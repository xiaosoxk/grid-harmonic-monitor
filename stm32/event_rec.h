/**
 * event_rec.h — 事件录波：THD 越限触发 + 防抖 + 冷却
 */
#ifndef EVENT_REC_H
#define EVENT_REC_H

#include <stdint.h>

#define EVENT_THDI_HIGH        20.0f   /* 触发阈值: THDi > 20% (可按现场配置) */
#define EVENT_CONFIRM_WINDOWS  2U      /* 连续N窗超限才确认(防单窗毛刺) */
#define EVENT_COOLDOWN_WINDOWS 250U    /* 冷却期: 约20s内不重复触发(防风暴) */

/* 每个分析窗口调用一次。返回1=本次触发事件(调用方应立即:
 *   1) UART_SendEvent(utc, thdi, irms, seq, locked) 发事件元数据帧;
 *   2) 紧接着发送当前窗口与上一窗口的 WAVE 帧(事件前后波形)。 */
uint8_t EVENT_OnResult(float thdi, float irms, uint8_t locked,
                       uint16_t seq, uint32_t utc);
/* 最近一次触发信息(供事件帧填充) */
float    EVENT_LastTrigThdi(void);
uint16_t EVENT_LastTrigSeq(void);

#endif /* EVENT_REC_H */
