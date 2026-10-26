/**
 * uart_frame.h — STM32 <-> Linux 串口帧协议（发送端）
 * 帧结构: AA 55 | TYPE | LEN(2,LE) | SEQ | PAYLOAD... | CRC16(2,LE)
 * CRC16-MODBUS 校验范围: TYPE .. PAYLOAD(含), 小端发送。
 */
#ifndef UART_FRAME_H
#define UART_FRAME_H

#include "stm32f4xx_hal.h"
#include "pq_math.h"

#define FRAME_TYPE_WAVE    0x01U   /* 原始波形: int16 交织 I,U x 1024 点对 */
#define FRAME_TYPE_RESULT  0x02U   /* 分析结果: float32 LE 数组(定义见 .c) */
#define FRAME_TYPE_HELLO   0x03U   /* 上线心跳: 版本号 + 配置参数 */
#define FRAME_TYPE_TIME    0x04U   /* v1.1 对时帧: 设备侧 UTC 秒(u32) */
#define FRAME_TYPE_EVENT   0x05U   /* v1.1 事件帧: 越限触发元数据(16B) */

HAL_StatusTypeDef UART_Init(uint32_t baud);
/* 发送一帧分析结果(11 + 2*PQ_HARM_MAX 个 float32), 返回 0=已提交, 1=忙丢弃 */
uint8_t UART_SendResult(const PqResult *r);
/* 发送一帧原始波形(供上位机显示时域曲线), decim 为抽取率计数 */
void    UART_SendWave(const int16_t *iu_interleaved, uint32_t n_halfwords);
void    UART_SendHello(void);
/* v1.1: 对时帧 —— 设备周期性上报自身 UTC, 上位机比对偏差 */
uint8_t UART_SendTime(uint32_t utc);
/* v1.1: 事件帧 —— THD越限触发元数据; 波形紧随其后以 WAVE 帧发送 */
uint8_t UART_SendEvent(uint32_t utc, float thdi_trig, float irms,
                       uint16_t seq, uint8_t locked);
/* 统计信息, 供论文"可靠性设计"一节引用 */
uint32_t UART_GetTxDropCount(void);
uint32_t UART_GetTxFrameCount(void);

#endif /* UART_FRAME_H */
