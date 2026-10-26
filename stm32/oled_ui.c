/**
 * oled_ui.c — 本地显示页面轮播管理器（0.96" SSD1306, 128x64, I2C @400kHz）
 *
 * 交互设计决策(论文/答辩素材):
 *  1) 轮播而非同屏堆叠: 128x64 点阵只有 8行x16列(8x16字号)的容量,
 *     按任务分层拆成 3 页, 用单按键循环切换, 认知负荷最低;
 *  2) 英文缩写标签: 8x16 ASCII 字库仅占 1KB Flash, 中文取模字库需 10KB+
 *     且 5x8 点阵中文不可读 —— 资源受限屏的信息密度权衡;
 *  3) 刷新节流: 每 OUI_REFRESH_WINDOWS 个分析窗口(约160ms)刷一次,
 *     一帧全屏 1024B+命令在 400kHz I2C 上约 23ms, 占空比约 14%,
 *     且与 FFT 计算异步(同在主循环但 FFT 每窗仅 2ms), 互不拖慢;
 *  4) 数据快照: OUI_OnResult 由主循环调用(与 PQ_Analyze 同上下文),
 *     不存在并发, 直接拷贝快照后渲染, 无需加锁。
 *
 * 集成方法(在 main.c 分析完成后追加):
 *   OuiData d = {...从 s_result 填充...}; OUI_OnResult(&d);
 *   按键 PA0 EXTI 下降沿回调中调用 OUI_OnKey();
 * 驱动依赖: 工程需含 0.96寸 SSD1306 通用驱动(OLED_Init/Clear/ShowString),
 *          该层只管"显示什么、何时显示", 不管"怎么点亮像素"。
 */
#include "oled_ui.h"
#include <stdio.h>
#include <string.h>

/* ---------- 依赖的 SSD1306 驱动接口(工程通用库, 略) ---------- */
extern void OLED_Init(void);
extern void OLED_Clear(void);
extern void OLED_ShowString(uint8_t row, uint8_t col, const char *str);  /* 8x16, row:0~3, col:0~15 */
extern void OLED_Refresh(void);                                          /* 把显存整帧推到 I2C */

/* ---------- 内部状态 ---------- */
static I2C_HandleTypeDef *s_hi2c;
static OuiData  s_snap;                  /* 最新数据快照 */
static uint8_t  s_page = 0;              /* 当前页 */
static uint8_t  s_win_cnt = 0;           /* 窗口计数(节流) */
static uint8_t  s_dirty = 1;             /* 需要重绘标志 */

/* ---------- 页面渲染: 每页一个函数, 只负责布字符 ---------- */
static void page_main(const OuiData *d)          /* P1: 核心指标 */
{
    char buf[20];
    OLED_ShowString(0, 0, d->locked ? "== LOCK  P1/3 ==" : "== FREE  P1/3 ==");
    snprintf(buf, sizeof buf, "f0   %7.2f Hz", d->freq);
    OLED_ShowString(1, 0, buf);
    snprintf(buf, sizeof buf, "Irms %7.3f A", d->irms);
    OLED_ShowString(2, 0, buf);
    snprintf(buf, sizeof buf, "THDi %6.1f %%", d->thdi);
    OLED_ShowString(3, 0, buf);
}

static void page_harm(const OuiData *d)          /* P2: 前4大谐波(字符条) */
{
    /* 字符化"条形图": 每 10% 一个 '#' */
    OLED_ShowString(0, 0, "== HARMONIC P2/3 ==");
    for (uint8_t i = 0; i < 4; i++) {
        char buf[20];
        int bars = (int)(d->harm_amp[i] / 5.0f);         /* 5%/格 */
        if (bars > 10) bars = 10;
        char bar[12];
        uint8_t k = 0;
        for (; k < bars; k++) bar[k] = '#';
        bar[k] = '\0';
        snprintf(buf, sizeof buf, "h%-2d %4.1f%% %s",
                 d->harm_top[i], d->harm_amp[i], bar);
        OLED_ShowString(i + 1, 0, buf);
    }
}

static void page_comm(const OuiData *d)          /* P3: 通信与系统状态 */
{
    char buf[20];
    OLED_ShowString(0, 0, "== LINK    P3/3 ==");
    snprintf(buf, sizeof buf, "FRM  %8lu", (unsigned long)d->frames);
    OLED_ShowString(1, 0, buf);
    snprintf(buf, sizeof buf, "DROP %6lu", (unsigned long)d->drops);
    OLED_ShowString(2, 0, buf);
    snprintf(buf, sizeof buf, "CRC  %6lu %s", (unsigned long)d->crc_err,
             d->locked ? "LOCK" : "FREE");
    OLED_ShowString(3, 0, buf);
}

static const void (*s_pages[OUI_PAGE_NUM])(const OuiData *) =
    { page_main, page_harm, page_comm };

/* ---------- 对外接口 ---------- */
void OUI_Init(I2C_HandleTypeDef *hi2c)
{
    s_hi2c = hi2c;
    OLED_Init();
    OLED_Clear();
    OLED_Refresh();
    s_page = 0; s_win_cnt = 0; s_dirty = 1;
}

void OUI_OnResult(const OuiData *d)
{
    s_snap = *d;                                 /* 数据快照 */
    if (++s_win_cnt < OUI_REFRESH_WINDOWS && !s_dirty)
        return;                                  /* 节流: 未到刷新周期且无按键 */
    s_win_cnt = 0;
    s_dirty = 0;
    OLED_Clear();
    s_pages[s_page](&s_snap);
    OLED_Refresh();                              /* 整帧上屏约 23ms @400kHz */
}

void OUI_OnKey(void)                             /* 按键: 翻页并立即重绘 */
{
    s_page = (s_page + 1U) % OUI_PAGE_NUM;
    s_dirty = 1;                                 /* 下次 OnResult 立即刷 */
}
