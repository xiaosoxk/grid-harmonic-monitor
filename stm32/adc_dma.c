/**
 * adc_dma.c — 定时器触发 ADC 双通道扫描 + DMA 循环采集（硬件双缓冲）
 * 平台: STM32F407 (168MHz), HAL 库
 *
 * 采集链路: TIM3(TRGO, 25.6kHz) -> ADC1 双rank扫描(CH1电流, CH4电压)
 *           -> DMA2_Stream0 循环搬运 -> HT/TC 半区中断
 * 双缓冲原理: DMA 循环模式下每填满一半缓冲区触发一次中断(HT/TC)。
 *            主循环处理前半区时, DMA 正在写后半区, 互不覆盖 -> 不丢采样点。
 * 时序校验: 2 rank x (15采样+12转换)周期 / 21MHz ADC时钟 = 2.57us << 39.06us 触发周期
 */
#include "adc_dma.h"
#include "sync_pll.h"

ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim3;
DMA_HandleTypeDef hdma_adc1;

uint16_t g_adc_raw[ADC_BUF_LEN];            /* [I0,U0,I1,U1,...] 交织存放 */
volatile uint8_t g_adc_half_flag = 0;

HAL_StatusTypeDef ADC_DMA_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* ---- 时钟 ---- */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* ---- 引脚: PA1/PA4 模拟输入(无上下拉) ---- */
    gpio.Pin  = GPIO_PIN_1 | GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* ---- TIM3: 更新事件经 TRGO 触发 ADC, 频率 = fs, 由锁相环动态修 ARR ---- */
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 0;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = (TIM3_CLK_HZ / (uint32_t)PQ_FS_NOMINAL) - 1U; /* 3280 */
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.RepetitionCounter = 0;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) return HAL_ERROR;

    TIM_MasterConfigTypeDef mst = {0};
    mst.MasterOutputTrigger = TIM_TRGO_UPDATE;      /* 更新事件 -> TRGO */
    mst.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &mst) != HAL_OK) return HAL_ERROR;

    /* ---- ADC1: 双rank扫描, 禁止连续模式, 定时器上升沿触发 ---- */
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;   /* 84/4 = 21MHz */
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = ENABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;   /* 关键: 软件连续模式关闭, 由TIM节拍 */
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T3_TRGO;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = PQ_ADC_CHANNELS;
    hadc1.Init.DMAContinuousRequests = ENABLE;    /* 允许 DMA 循环请求 */
    hadc1.Init.EOCSelection          = ADC_EOC_SEQ_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) return HAL_ERROR;

    /* rank1: 电流通道 PA1(ADC1_IN1), rank2: 电压通道 PA4(ADC1_IN4)
     * 采样时间 15 周期: 保证前端源阻抗向采样电容充电充分并留裕量 */
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel      = ADC_CHANNEL_1;
    ch.Rank         = 1;
    ch.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) return HAL_ERROR;
    ch.Channel      = ADC_CHANNEL_4;
    ch.Rank         = 2;
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) return HAL_ERROR;

    /* ---- DMA2_Stream0 / Channel0: 外设->内存, 半字, 循环模式, 高优先级 ---- */
    hdma_adc1.Instance                 = DMA2_Stream0;
    hdma_adc1.Init.Channel             = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode                = DMA_CIRCULAR;   /* 关键: 循环模式 */
    hdma_adc1.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_adc1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) return HAL_ERROR;
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    /* 半传输/全传输中断优先级: 低于SysTick, 高于普通任务 */
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    return HAL_OK;
}

HAL_StatusTypeDef ADC_DMA_Start(void)
{
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc_raw, ADC_BUF_LEN) != HAL_OK)
        return HAL_ERROR;
    return HAL_TIM_Base_Start(&htim3);
}

/* 锁相环给出新基波估计后, 重设定时器分频: fs = 512 x f0
 * ARR 范围: f0=45Hz -> 3644, f0=55Hz -> 2989, 均在16位计数器范围内 */
void ADC_DMA_TrackFrequency(float f0)
{
    float fs_target = (float)PQ_SAMPLES_PER_CYC * f0;
    uint32_t arr = (uint32_t)((float)TIM3_CLK_HZ / fs_target + 0.5f) - 1U;
    if (arr < 2989U) arr = 2989U;
    if (arr > 3644U) arr = 3644U;
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
}

/* 拆交织并归一化: (code - 2048) / 2048 -> [-1, 1), 后级统一做标定增益 */
void ADC_DMA_Deinterleave(const uint16_t *src, float *i_out, float *u_out,
                          uint32_t n_pairs)
{
    const float k = 1.0f / PQ_ADC_MID;
    for (uint32_t n = 0; n < n_pairs; n++) {
        i_out[n] = ((float)src[2U * n]     - PQ_ADC_MID) * k;   /* rank1: 电流 */
        u_out[n] = ((float)src[2U * n + 1U] - PQ_ADC_MID) * k;  /* rank2: 电压 */
    }
}

/* ---------------- HAL 中断回调: 硬件双缓冲的落点 ---------------- */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h) { (void)h; g_adc_half_flag = 1; }
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)     { (void)h; g_adc_half_flag = 2; }

void DMA2_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_adc1); }
void ADC_IRQHandler(void)          { HAL_ADC_IRQHandler(&hadc1); }
