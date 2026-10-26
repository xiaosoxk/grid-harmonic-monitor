/**
 * ui_manager.h — 工业级触摸屏 HMI 管理器（LVGL v8, 2.8" SPI TFT + XPT2046）
 * 页面: 监测/谐波/趋势/事件/设置/诊断 六页面, 暗色工业主题
 */
#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "pq_math.h"
#include <stdint.h>

/* 触摸屏选型(两种路径, 见手册第10章):
 *   路径A: F407 + 2.8" SPI TFT(ILI9341 320x240) + XPT2046 电阻触摸 —— 本代码默认
 *   路径B: 换 F429ZGT6 + 4.3" RGB 800x480 电容屏(LTDC+SDRAM) —— 代码层完全兼容 */

typedef enum {
    UI_PAGE_MONITOR = 0,    /* 实时监测: 波形+频谱缩略+大字指标 */
    UI_PAGE_HARM,           /* 谐波详情: 1~25次柱状图+数值表 */
    UI_PAGE_TREND,          /* THD 趋势: 240点滚动曲线 */
    UI_PAGE_EVENTS,         /* 事件记录: 越限/失锁历史(带时间戳) */
    UI_PAGE_SETTINGS,       /* 设置: 阈值/校准/通信(密码保护) */
    UI_PAGE_DIAG,           /* 诊断: 帧计数/丢帧/CRC/版本/锁相状态 */
    UI_PAGE_NUM
} UiPageId;

void UI_Init(void);                    /* LVGL 初始化+六页面构建+进主循环 task */
void UI_OnResult(const PqResult *r);   /* 每个分析窗口调用: 数据快照+刷新当前页 */
void UI_OnKeyNext(void);               /* 物理按键翻页(与触摸等效) */
UiPageId UI_GetPage(void);

/* 事件日志(供事件页与黑匣子共用): type=0越限 1失锁 2恢复 */
void UI_LogEvent(uint8_t type, float value, uint32_t utc);

#endif /* UI_MANAGER_H */
