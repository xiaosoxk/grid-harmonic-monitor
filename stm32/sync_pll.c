/**
 * sync_pll.c — 软件锁相同步采样（频谱反馈式 PLL）
 *
 * 思路: 不用硬件过零比较器, 而是从上一窗口的 FFT 频谱中直接测出基波频率:
 *   1) 在理论基波 bin(PQ_FFT_BIN_H1=4) 附近 ±2bin 内搜索谱峰;
 *   2) 抛物线插值获得亚 bin 精度的峰位 -> 频率分辨率远优于 fs/N=12.5Hz;
 *   3) 一阶低通滤波抑制抖动, 连续稳定 N 窗判定"锁定";
 *   4) 锁定后按 fs = 512 x f0 重设 TIM3 分频, 使整数次谐波恰好落在整数 bin 上,
 *      从机理上同时消除"栅栏效应"与"频谱泄漏"(矩形窗+整周期截断)。
 * 失锁保护: 电网频率异常(45~55Hz 之外)或频率突跳超过 0.2Hz 立即判失锁,
 *           回退到汉宁窗 + 额定采样率, 保证测幅值精度仍可用。
 */
#include "sync_pll.h"
#include "pq_config.h"
#include <math.h>

#define PLL_LOCK_WINDOWS   5U      /* 连续5窗稳定判锁定 */
#define PLL_LOCK_DELTA     0.05f   /* 判锁定: |df| < 0.05 Hz */
#define PLL_UNLOCK_DELTA   0.20f   /* 判失锁: |df| > 0.20 Hz */
#define PLL_FILT_ALPHA     0.10f   /* 一阶低通系数 */

static float    s_freq    = PQ_F_NOMINAL;   /* 滤波后的频率估计 */
static float    s_fs      = PQ_FS_NOMINAL;  /* 当前实际采样率 */
static uint8_t  s_lock_cnt = 0;
static uint8_t  s_locked   = 0;

float PLL_UpdateFrequency(const float *spec_i)
{
    /* 理论基波 bin: 同步后恒为 4, 失锁时按上一帧频率推算 */
    int32_t b1 = (int32_t)(s_freq * (float)PQ_FFT_LEN / s_fs + 0.5f);
    if (b1 < 3) b1 = 3;
    if (b1 > 8) b1 = 8;                       /* 55Hz x 2048/28160 < 5, 留裕量 */

    /* ±2 bin 内搜谱峰(复数模平方比较, 免开方) */
    int32_t bp = b1;
    for (int32_t k = b1 - 2; k <= b1 + 2; k++) {
        float pk = spec_i[2*k] * spec_i[2*k] + spec_i[2*k+1] * spec_i[2*k+1];
        float pp = spec_i[2*bp] * spec_i[2*bp] + spec_i[2*bp+1] * spec_i[2*bp+1];
        if (pk > pp) bp = k;
    }

    /* 抛物线插值: 峰值三点 (bp-1, bp, bp+1)
     * delta = 0.5*(y[-1]-y[+1]) / (y[-1]-2*y[0]+y[+1]), 范围 (-0.5, 0.5) */
    float ym = spec_i[2*(bp-1)] * spec_i[2*(bp-1)] + spec_i[2*(bp-1)+1] * spec_i[2*(bp-1)+1];
    float y0 = spec_i[2*bp]     * spec_i[2*bp]     + spec_i[2*bp+1]     * spec_i[2*bp+1];
    float yp = spec_i[2*(bp+1)] * spec_i[2*(bp+1)] + spec_i[2*(bp+1)+1] * spec_i[2*(bp+1)+1];
    float ym_s = sqrtf(ym), y0_s = sqrtf(y0), yp_s = sqrtf(yp);   /* 幅值域插值更准 */
    float den  = ym_s - 2.0f * y0_s + yp_s;
    float delta = (fabsf(den) > 1e-9f) ? 0.5f * (ym_s - yp_s) / den : 0.0f;
    if (delta > 0.5f)  delta = 0.5f;
    if (delta < -0.5f) delta = -0.5f;

    float bin_hz = s_fs / (float)PQ_FFT_LEN;          /* 每 bin 对应频率 */
    float f_raw  = ((float)bp + delta) * bin_hz;

    /* 有效性判断: 工频合理区间 45~55Hz, 否则保留旧值 */
    if (f_raw >= 45.0f && f_raw <= 55.0f) {
        float df = f_raw - s_freq;
        s_freq += PLL_FILT_ALPHA * df;                /* 一阶低通 */

        if (fabsf(df) < PLL_LOCK_DELTA) {
            if (s_lock_cnt < 255U) s_lock_cnt++;
        } else if (fabsf(df) > PLL_UNLOCK_DELTA) {
            s_lock_cnt = 0;
        }
        s_locked = (s_lock_cnt >= PLL_LOCK_WINDOWS) ? 1U : 0U;
    }
    return s_freq;
}

uint32_t PLL_NextArr(float f0)
{
    float fs_target = (float)PQ_SAMPLES_PER_CYC * f0;
    uint32_t arr = (uint32_t)((float)TIM3_CLK_HZ / fs_target + 0.5f) - 1U;
    if (arr < 2989U) arr = 2989U;    /* f0 = 55Hz 上限 */
    if (arr > 3644U) arr = 3644U;    /* f0 = 45Hz 下限 */
    s_fs = (float)TIM3_CLK_HZ / (float)(arr + 1U);  /* 记录真实采样率 */
    return arr;
}

uint8_t PLL_IsLocked(void)  { return s_locked; }
float   PLL_GetFrequency(void) { return s_freq; }
