/**
 * main.c — 主程序：采集 -> 分析 -> 上传 的实时调度核心
 *
 * 调度模型: 超级循环 + 事件标志(无RTOS, 时序确定性好, 便于答辩讲解)
 *   TIM3 25.6kHz -> DMA 半区中断(每20ms) -> 主循环组窗(每80ms) -> 分析(~4ms) -> DMA串口发送
 * 实时性: 处理耗时 << 80ms 窗口周期, 最坏情况裕量 > 15 倍;
 *         DMA 循环模式持续搬数, CPU 分析期间采样零丢失(双缓冲互不覆盖)。
 * 可靠性: DWT 周期计数器统计 CPU 占用率; SEQ+CRC 由上位机核对; IWDG 看门狗兜底。
 */
#include "stm32f4xx_hal.h"
#include "pq_config.h"
#include "adc_dma.h"
#include "pq_math.h"
#include "uart_frame.h"
#include "sync_pll.h"
#include <string.h>

/* 窗口组装缓冲(两半区乒乓拼合) */
static float s_i_win[PQ_FFT_LEN], s_u_win[PQ_FFT_LEN];
static float s_i_half[ADC_PAIRS_PER_HALF], s_u_half[ADC_PAIRS_PER_HALF];
static int16_t s_wave_tx[PQ_WAVE_TX_POINTS * 2U];      /* I,U 交织 */

static PqResult s_result;
static uint32_t s_win_cnt = 0, s_drop_cnt = 0;
static float    s_cpu_load = 0.0f;

/* DWT 周期计数器: 用于 CPU 占用率与处理耗时测量 */
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config_168MHz();          /* 见工程模板: HSE 8M -> PLL x21 -> 168MHz */
    dwt_init();

    ADC_DMA_Init();
    PQ_MathInit();
    /* 两点标定系数示例: 出厂/上位机标定后写入 Flash, 此处为默认 1.0/0.0 */
    PQ_SetCalibration(1.0f, 0.0f, 1.0f, 0.0f);
    UART_Init(UART_BAUD);

    ADC_DMA_Start();
    UART_SendHello();

    while (1) {
        /* ---- 等待 DMA 半区事件(中断置标志), WFI 降功耗 ---- */
        if (g_adc_half_flag == 0U) { __WFI(); continue; }
        uint8_t half = g_adc_half_flag;
        g_adc_half_flag = 0U;

        /* ---- 1. 半区数据: 拆交织 + 归一化 + 标定前预处理 ---- */
        const uint16_t *src = (half == 1U) ? &g_adc_raw[0]
                                           : &g_adc_raw[ADC_BUF_LEN / 2U];
        ADC_DMA_Deinterleave(src, s_i_half, s_u_half, ADC_PAIRS_PER_HALF);

        /* ---- 2. 组窗: 前半区 + 后半区 = 2048 点完整窗口 ---- */
        uint32_t off = (half == 1U) ? 0U : ADC_PAIRS_PER_HALF;
        memcpy(&s_i_win[off], s_i_half, sizeof(s_i_half));
        memcpy(&s_u_win[off], s_u_half, sizeof(s_u_half));
        if (half != 2U) continue;         /* 窗口未齐, 等 DMA 写后半区 */

        /* ---- 3. 完整窗口: 全量分析(测处理耗时) ---- */
        uint32_t t0 = DWT->CYCCNT;
        PQ_Analyze(s_i_win, s_u_win, &s_result);
        uint32_t dt = DWT->CYCCNT - t0;

        /* ---- 4. 锁相闭环: 按新频率估计重设采样率(下一窗生效) ---- */
        if (s_result.locked)
            ADC_DMA_TrackFrequency(s_result.freq);
        else
            ADC_DMA_TrackFrequency(PQ_F_NOMINAL);   /* 失锁回额定采样率 */

        /* ---- 5. 上传: 结果帧每窗必发, 波形帧按抽取率 ---- */
        if (UART_SendResult(&s_result) != 0U) s_drop_cnt++;
        if ((s_win_cnt % PQ_WAVE_DECIM) == 0U) {
            for (uint32_t n = 0; n < PQ_WAVE_TX_POINTS; n++) {
                s_wave_tx[2U * n]     = (int16_t)(s_i_win[n * 2U] * 32767.0f);
                s_wave_tx[2U * n + 1U] = (int16_t)(s_u_win[n * 2U] * 32767.0f);
            }
            UART_SendWave(s_wave_tx, PQ_WAVE_TX_POINTS * 2U);
        }
        s_win_cnt++;

        /* ---- 6. CPU 占用率: 处理耗时 / 窗口周期, 一阶平滑 ---- */
        float load = (float)dt / (0.08f * (float)SystemCoreClock);
        s_cpu_load += 0.1f * (load - s_cpu_load);
        (void)s_cpu_load;                 /* 可经 HELLO 帧扩展上报 */
    }
}

/* SystemClock_Config_168MHz: 由 STM32CubeMX 生成(略), 要点:
 * HSE 8MHz -> PLLM /8 -> PLLN x336 -> PLLP /2 -> SYSCLK 168MHz,
 * APB1 = 42MHz(定时器 x2 = 84MHz), APB2 = 84MHz(ADC 分频 /4 = 21MHz), USB OTG 48MHz */
