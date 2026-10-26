/**
 * adc_dma.h — ADC+DMA 硬件双缓冲采集模块对外接口
 */
#ifndef ADC_DMA_H
#define ADC_DMA_H

#include "stm32f4xx_hal.h"
#include "pq_config.h"

/* DMA 循环缓冲: 存放 1 个完整分析窗口的交织采样
 * 总长度 = FFT点数 x 通道数 = 2048 x 2 = 4096 个半字
 * 每半缓冲 = 1024 个点对, 对应 2 个周波 */
#define ADC_PAIRS_PER_HALF  (PQ_FFT_LEN / 2U)
#define ADC_BUF_LEN         (PQ_FFT_LEN * PQ_ADC_CHANNELS)

/* 半区就绪标志: 1 = 前半区(含2周波)就绪, 2 = 后半区就绪 */
extern volatile uint8_t g_adc_half_flag;
extern uint16_t         g_adc_raw[ADC_BUF_LEN];

HAL_StatusTypeDef ADC_DMA_Init(void);
HAL_StatusTypeDef ADC_DMA_Start(void);
/* 按锁相输出的基波频率更新 TIM3 分频, 实现 fs = 512 x f0 同步采样 */
void              ADC_DMA_TrackFrequency(float f0);
/* 拆交织 + 去偏置 + 定标: raw(0~4095) -> float(-1.0~1.0 归一化) */
void              ADC_DMA_Deinterleave(const uint16_t *src,
                                       float *i_out, float *u_out,
                                       uint32_t n_pairs);

#endif /* ADC_DMA_H */
