/**
 * oled_ui.h — STM32 本地人机交互（0.96寸 SSD1306 OLED 页面轮播管理器）
 */
#ifndef OLED_UI_H
#define OLED_UI_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* 每个分析窗口(80ms)由主循环回调一次的数据包 */
typedef struct {
    float    freq;              /* 基波频率 Hz */
    float    irms, urms;        /* 有效值(标定后) */
    float    thdi;              /* 电流THD % */
    uint8_t  harm_top[4];       /* 前4大谐波次数(1次除外) */
    float    harm_amp[4];       /* 对应相对基波幅值 % */
    uint32_t frames, drops;     /* 发送帧数/丢弃帧数 */
    uint32_t crc_err;           /* 上位机回传或本地累计CRC错 */
    uint8_t  locked;            /* 同步采样锁定标志 */
} OuiData;

void OUI_Init(I2C_HandleTypeDef *hi2c);
/* 主循环每个分析窗口调用一次(约12.5Hz), 内部按 OUI_REFRESH_WINDOWS 节流 */
void OUI_OnResult(const OuiData *d);
/* 按键 EXTI 回调: 短按翻页 */
void OUI_OnKey(void);

#define OUI_PAGE_NUM            3U
#define OUI_REFRESH_WINDOWS     2U   /* 每2窗刷新一次 ≈ 160ms, I2C占空比~14% */

#endif /* OLED_UI_H */
