/**
 * pq_math.c — 电能质量分析核心算法（CMSIS-DSP 实数 FFT + 谐波/THD/功率）
 *
 * 窗口策略(与 sync_pll 联动):
 *   锁定态: 矩形窗 + 整周期截断(同步采样) —— 能量无损, 谐波精确落 bin;
 *   失锁态: 汉宁窗(旁瓣 -31.5dB/倍频程滚降) —— 抑制频谱泄漏, 幅值仍可测。
 * 幅值恢复: X_h = 2|X[bin_h]| / (N * CG), CG 为窗相干增益(矩形1.0/汉宁0.5)。
 * THD 定义(GB/T 14549): THD = sqrt(sum_{h=2..40} X_h^2) / X_1 x 100%。
 */
#include "pq_math.h"
#include "arm_math.h"
#include "sync_pll.h"
#include <math.h>

static arm_rfft_fast_instance_f32 s_rfft;

static float s_win[PQ_FFT_LEN];            /* 当前窗系数表 */
static float s_cg = 1.0f;                  /* 窗相干增益 Coherent Gain */
static float s_cal_i_gain = 1.0f, s_cal_i_off = 0.0f;
static float s_cal_u_gain = 1.0f, s_cal_u_off = 0.0f;

/* 工作缓冲(全部静态分配, 不用 malloc —— 嵌入式可靠性惯例) */
static float s_xi[PQ_FFT_LEN], s_xu[PQ_FFT_LEN];
static float s_spec_i[PQ_FFT_LEN], s_spec_u[PQ_FFT_LEN];  /* rFFT: N/2+1 复数 */

void PQ_MathInit(void)
{
    arm_rfft_fast_init_f32(&s_rfft, PQ_FFT_LEN);          /* 2048 点实数 FFT 初始化 */
    for (uint32_t n = 0; n < PQ_FFT_LEN; n++)             /* 预生成汉宁窗表(默认) */
        s_win[n] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * (float)n / (float)PQ_FFT_LEN));
    s_cg = 0.5f;
}

void PQ_SetCalibration(float i_gain, float i_off, float u_gain, float u_off)
{
    s_cal_i_gain = i_gain; s_cal_i_off = i_off;
    s_cal_u_gain = u_gain; s_cal_u_off = u_off;
}

/* 从 rFFT 实数打包频谱中提取 1..PQ_HARM_MAX 次谐波幅值与相位 */
static void harmonic_extract(const float *spec, float *harm, float *phase)
{
    const float norm = 2.0f / ((float)PQ_FFT_LEN * s_cg);
    for (uint32_t h = 1U; h <= PQ_HARM_MAX; h++) {
        uint32_t bin = h * PQ_FFT_BIN_H1;                 /* 同步采样: h次恰在第 4h bin */
        if (bin >= PQ_FFT_LEN / 2U) { harm[h] = 0.0f; phase[h] = 0.0f; continue; }
        float re = spec[2U * bin];
        float im = spec[2U * bin + 1U];
        harm[h]  = sqrtf(re * re + im * im) * norm;
        if (phase) phase[h] = atan2f(im, re);             /* 余弦参考相位 */
    }
}

void PQ_Analyze(const float *i_win, const float *u_win, PqResult *out)
{
    /* ---- 0. 标定(两点线性) ---- */
    for (uint32_t n = 0; n < PQ_FFT_LEN; n++) {
        s_xi[n] = (i_win[n] - s_cal_i_off) * s_cal_i_gain;
        s_xu[n] = (u_win[n] - s_cal_u_off) * s_cal_u_gain;
    }

    /* ---- 1. 时域统计: 去直流通量 + 真有效值 + 有功 ---- */
    float mi, mu, p_sum = 0.0f;
    arm_mean_f32(s_xi, PQ_FFT_LEN, &mi);
    arm_mean_f32(s_xu, PQ_FFT_LEN, &mu);
    for (uint32_t n = 0; n < PQ_FFT_LEN; n++) {
        s_xi[n] -= mi;
        s_xu[n] -= mu;
        p_sum += s_xi[n] * s_xu[n];                       /* Σu*i */
    }
    p_sum /= (float)PQ_FFT_LEN;                           /* 有功 P(含谐波功率) */

    arm_rms_f32(s_xi, PQ_FFT_LEN, &out->irms);
    arm_rms_f32(s_xu, PQ_FFT_LEN, &out->urms);
    out->s = out->irms * out->urms;
    out->p  = p_sum;
    out->pf = (out->s > 1e-9f) ? (p_sum / out->s) : 0.0f; /* 真功率因数(含畸变) */

    /* ---- 2. 窗函数选择: 锁定->矩形, 失锁->汉宁 ---- */
    if (PLL_IsLocked()) {
        for (uint32_t n = 0; n < PQ_FFT_LEN; n++) s_win[n] = 1.0f;
        s_cg = 1.0f;
    } else if (s_cg != 0.5f) {                            /* 仅在切换时重建, 省CPU */
        for (uint32_t n = 0; n < PQ_FFT_LEN; n++)
            s_win[n] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * (float)n / (float)PQ_FFT_LEN));
        s_cg = 0.5f;
    }
    for (uint32_t n = 0; n < PQ_FFT_LEN; n++) {
        s_xi[n] *= s_win[n];
        s_xu[n] *= s_win[n];
    }

    /* ---- 3. 实数 FFT (2048点, 输出 N/2+1 组复数打包) ---- */
    arm_rfft_fast_f32(&s_rfft, s_xi, s_spec_i, 0);
    arm_rfft_fast_f32(&s_rfft, s_xu, s_spec_u, 0);

    /* ---- 4. 谐波抽取 + 频率跟踪 ---- */
    harmonic_extract(s_spec_i, out->i_harm, out->i_phase);
    harmonic_extract(s_spec_u, out->u_harm, NULL);
    out->freq = PLL_UpdateFrequency(s_spec_i);            /* 软件锁相 */

    /* ---- 5. THD 与基波位移因数 ---- */
    float si = 0.0f, su = 0.0f;
    for (uint32_t h = 2U; h <= PQ_THD_HARM_MAX; h++) {
        si += out->i_harm[h] * out->i_harm[h];
        su += out->u_harm[h] * out->u_harm[h];
    }
    out->thdi = (out->i_harm[1] > 1e-9f) ? (sqrtf(si) / out->i_harm[1] * 100.0f) : 0.0f;
    out->thdu = (out->u_harm[1] > 1e-9f) ? (sqrtf(su) / out->u_harm[1] * 100.0f) : 0.0f;

    /* 基波相位差: phi_i - phi_u(同为余弦参考, 差值与参考系无关) */
    float phi_u1 = atan2f(s_spec_u[2U * PQ_FFT_BIN_H1 + 1U],
                          s_spec_u[2U * PQ_FFT_BIN_H1]);
    float dphi  = out->i_phase[1] - phi_u1;
    out->dpf = arm_cos_f32(dphi);                                   /* 位移因数 cosφ1 */
    out->q   = out->i_harm[1] * out->u_harm[1] * (-arm_sin_f32(dphi)); /* Q1 = U1*I1*sin(φu-φi) */
    out->locked = PLL_IsLocked();
}
