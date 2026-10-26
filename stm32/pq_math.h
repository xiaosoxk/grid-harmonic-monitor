/**
 * pq_math.h — 电能质量分析核心算法模块对外接口
 */
#ifndef PQ_MATH_H
#define PQ_MATH_H

#include "pq_config.h"

typedef struct {
    float freq;                       /* 基波频率 Hz */
    float irms, urms;                 /* 真有效值(归一化, 标定后为A/V) */
    float p, q, s;                    /* 有功/无功/视在功率(单相) */
    float pf, dpf;                    /* 真功率因数 / 基波位移因数 */
    float thdi, thdu;                 /* 电流/电压总谐波畸变率(%) */
    float i_harm[PQ_HARM_MAX + 1U];   /* 下标 = 谐波次数, [0]未用 */
    float u_harm[PQ_HARM_MAX + 1U];
    float i_phase[PQ_HARM_MAX + 1U];  /* 各次电流相位(余弦参考, rad) */
    uint8_t locked;                   /* 同步采样锁定标志 */
} PqResult;

void  PQ_MathInit(void);
void  PQ_Analyze(const float *i_win, const float *u_win, PqResult *out);
/* 两点标定: x_cal = (x_raw - offset) * gain, 由上位机下发/出厂写入 */
void  PQ_SetCalibration(float i_gain, float i_off, float u_gain, float u_off);

#endif /* PQ_MATH_H */
