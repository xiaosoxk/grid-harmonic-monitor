/**
 * ui_manager.c — 工业级 HMI 管理器实现（LVGL v8.2）
 *
 * 架构原则(答辩讲点):
 *  1) 单核无RTOS: UI_OnResult 与 lv_task_handler 同在主循环调用, 天然串行,
 *     不存在 LVGL 线程安全问题 —— 与采集/分析共享一条执行流, 时序确定;
 *  2) 数据快照: 每窗口只拷一份 PqResult 快照, 页面控件按需读取,
 *     刷新节流 160ms(与 OLED 版一致), LVGL 重绘由脏区机制自动合并;
 *  3) 显示驱动: ILI9341 SPI DMA 刷屏(lv_port_disp 已按官方模板实现,
 *     本文件只做 UI 逻辑); 触摸 XPT2046 经 lv_port_inport 接入;
 *  4) 事件日志: 环形缓冲 32 条, 越限/失锁/恢复三类, 与事件页/黑匣子共用。
 *
 * 集成(main.c 管理节拍):
 *   UI_Init();                    // 初始化后默认停在监测页
 *   UI_OnResult(&s_result);       // 每分析窗口(80ms)一次
 *   lv_task_handler();            // 主循环里持续调用(非阻塞, 内部按需重绘)
 *   TIM 触摸扫描已由 lv_port_inport 定时器回调处理。
 */
#include "ui_manager.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

/* ---------------- 工业暗色主题(集中定义, 全 UI 统一) ---------------- */
#define COL_BG      lv_color_hex(0x141a21)   /* 页面底 */
#define COL_PANEL   lv_color_hex(0x1f2933)   /* 面板底 */
#define COL_ACC     lv_color_hex(0x35d5e0)   /* 强调青 */
#define COL_OK      lv_color_hex(0x3fae7a)   /* 正常绿 */
#define COL_WARN    lv_color_hex(0xe0a13d)   /* 警告黄 */
#define COL_ALARM   lv_color_hex(0xe04545)   /* 告警红 */
#define COL_TXT     lv_color_hex(0xdfe7ec)   /* 主文字 */
#define COL_TXT2    lv_color_hex(0x8a97a3)   /* 次文字 */

/* ---------------- 内部状态 ---------------- */
static PqResult  s_snap;                     /* 最新数据快照 */
static UiPageId  s_page = UI_PAGE_MONITOR;
static uint32_t  s_refresh_div = 0;          /* 2窗=160ms 节流 */
static uint8_t   s_thd_alarm = 0;            /* 越限状态(带迟滞) */

/* 事件日志环形缓冲 */
typedef struct { uint8_t type; float value; uint32_t utc; } UiEvent;
static UiEvent   s_evlog[32];
static uint8_t   s_ev_head = 0, s_ev_cnt = 0;

/* 告警横幅(所有页面共享, 置顶) */
static lv_obj_t *s_banner = NULL;
static lv_obj_t *s_banner_label = NULL;

/* 各页面构建/刷新由 ui_pages.c 提供 */
extern void Pages_BuildAll(lv_disp_t *disp, lv_theme_t *th);
extern void Pages_SetSnapshot(const PqResult *r);
extern void Pages_Refresh(UiPageId page);
extern void Pages_Switch(UiPageId page);
extern void Pages_AppendEvent(const void *ev, uint8_t type, float v, uint32_t utc);

/* ---------------- 事件日志 ---------------- */
void UI_LogEvent(uint8_t type, float value, uint32_t utc)
{
    s_evlog[s_ev_head] = (UiEvent){ type, value, utc };
    s_ev_head = (s_ev_head + 1U) % 32U;
    if (s_ev_cnt < 32U) s_ev_cnt++;
    Pages_AppendEvent(&s_evlog[(s_ev_head + 31U) % 32U], type, value, utc);
}

/* ---------------- 告警横幅(带迟滞, 防临界抖动) ---------------- */
static void banner_update(void)
{
    /* 迟滞: >5.2% 进入告警, <4.8% 才解除(工业仪表惯例) */
    if (!s_thd_alarm && s_snap.thdi > 5.2f)  s_thd_alarm = 1;
    if (s_thd_alarm && s_snap.thdi < 4.8f)   s_thd_alarm = 0;

    if (s_thd_alarm && s_banner == NULL) {
        s_banner = lv_btn_create(lv_scr_act());
        lv_obj_set_size(s_banner, 320, 26);
        lv_obj_align(s_banner, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(s_banner, COL_ALARM, 0);
        s_banner_label = lv_label_create(s_banner);
        lv_label_set_text(s_banner_label, "!! THD OVER LIMIT (GB/T 14549: 5.0%)");
        lv_obj_set_style_text_color(s_banner_label, lv_color_white(), 0);
        lv_obj_center(s_banner_label);
    } else if (!s_thd_alarm && s_banner) {
        lv_obj_del(s_banner);               /* LVGL 删除自动清理子对象 */
        s_banner = NULL; s_banner_label = NULL;
    }
}

/* ---------------- 对外接口 ---------------- */
void UI_Init(void)
{
    /* lv_init / lv_port_disp_init / lv_port_inport_init 已在 bsp 完成后调用 */
    static lv_style_t st;
    lv_style_init(&st);
    lv_style_set_bg_color(&st, COL_BG);
    lv_obj_add_style(lv_scr_act(), &st, 0);

    Pages_BuildAll(NULL, NULL);             /* 构建六页面(暗色样式内部统一) */
    Pages_Switch(UI_PAGE_MONITOR);
}

void UI_OnResult(const PqResult *r)
{
    s_snap = *r;                            /* 快照一次, 页面自取 */
    Pages_SetSnapshot(&s_snap);

    if (++s_refresh_div < 2U)               /* 160ms 节流: 2窗刷一次 */
        return;
    s_refresh_div = 0;

    banner_update();                        /* 告警横幅独立节流之外即时响应 */
    Pages_Refresh(s_page);
}

void UI_OnKeyNext(void)
{
    s_page = (UiPageId)((s_page + 1U) % UI_PAGE_NUM);
    Pages_Switch(s_page);
}

UiPageId UI_GetPage(void) { return s_page; }
