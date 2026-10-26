/**
 * pq_config.h — 系统核心参数配置（全工程唯一参数源）
 * 平台: STM32F407VET6 + ZMCT103C(电流) + ZMPT101B(电压, 扩展)
 * 原理: 50Hz 工频谐波分析, 频率跟踪式同步采样 + CMSIS-DSP FFT
 */
#ifndef PQ_CONFIG_H
#define PQ_CONFIG_H

/* ---------------- 采样参数 ---------------- */
#define PQ_F_NOMINAL        50.0f     /* 额定工频 Hz */
#define PQ_SAMPLES_PER_CYC  512U      /* 额定每周波采样点数 */
#define PQ_FS_NOMINAL       25600.0f  /* 额定采样率 = 512 x 50 Hz */
#define PQ_FFT_LEN          2048U     /* FFT 点数 = 4 个周波窗口(80ms) */
#define PQ_CYCLES_PER_WIN   4U        /* 每分析窗口包含的周波数 */
#define PQ_FFT_BIN_H1       (PQ_FFT_LEN / PQ_SAMPLES_PER_CYC)  /* 基波所在 bin = 4 */
#define PQ_HARM_MAX         50U       /* 最高分析谐波次数 */
#define PQ_THD_HARM_MAX     40U       /* THD 求和上限(GB/T 14549 惯例) */

/* ---------------- ADC 参数 ---------------- */
#define PQ_ADC_CHANNELS     2U        /* rank1: 电流 CH1(PA1), rank2: 电压 CH4(PA4) */
#define PQ_ADC_BITS         12U
#define PQ_ADC_MID          2048.0f   /* 前端 1.65V 偏置对应的理论码值 */
#define PQ_VREF             3.3f      /* ADC 参考电压 */

/* ---------------- 串口协议 ---------------- */
#define UART_BAUD           921600U   /* 波特率(带宽预算见论文 5.4 节) */
#define FRAME_SYNC0         0xAAU     /* 帧头同步字节 0 */
#define FRAME_SYNC1         0x55U     /* 帧头同步字节 1 */
#define PQ_WAVE_TX_POINTS   1024U     /* 原始波形帧上传的点对数(I,U 各1024点) */
#define PQ_WAVE_DECIM       8U        /* 每隔 8 个分析窗口上传 1 帧波形 */

/* ---------------- 定时器 ---------------- */
#define TIM3_CLK_HZ         84000000U /* APB1=42MHz 时定时器时钟 x2 = 84MHz */

#endif /* PQ_CONFIG_H */
