/**
 * sync_pll.h — 软件锁相同步采样模块对外接口
 */
#ifndef SYNC_PLL_H
#define SYNC_PLL_H

#include <stdint.h>

/* 输入电流通道 rFFT 输出(实数打包格式), 返回基波频率估计值 Hz,
 * 并更新锁定状态(内部含一阶滤波与锁定计数器) */
float    PLL_UpdateFrequency(const float *spec_i);
/* 根据频率估计计算下一个 TIM3 ARR 值 */
uint32_t PLL_NextArr(float f0);
/* 锁定标志: 连续 N 个窗口 |df| < 0.05Hz 判定锁定; 锁定后用矩形窗, 失锁用汉宁窗 */
uint8_t  PLL_IsLocked(void);
/* 锁定频率估计值(滤波后) */
float    PLL_GetFrequency(void);

#endif /* SYNC_PLL_H */
