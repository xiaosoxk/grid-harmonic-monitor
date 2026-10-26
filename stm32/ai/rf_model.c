/* rf_model.c — 由 train_load_id.py 自动生成 */
#include "rf_model.h"
const char * const RF_CLASS_NAMES[RF_N_CLASSES] = {"CHARGER", "FAN_DIMMER", "INVERTER_AC", "LAPTOP_PSU", "LED_LAMP", "RESISTIVE"};
const float RF_FEAT_MEAN[RF_N_FEAT] = {0.00000f, 24.04724f, 0.00000f, 27.01578f, 0.00000f, 14.83523f, 0.00000f, 3.49342f, 0.00000f, 5.06897f, 0.00000f, 2.84774f, 0.00000f, 0.49934f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 43.08627f, 0.72241f, 0.00000f, 58.52443f};
const float RF_FEAT_STD[RF_N_FEAT] = {0.00000f, 20.62874f, 0.00000f, 15.39646f, 0.00000f, 9.05720f, 0.00000f, 3.76325f, 0.00000f, 4.20874f, 0.00000f, 3.10372f, 0.00000f, 1.12330f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f, 22.48192f, 0.19970f, 0.00000f, 30.12123f};
const int16_t RF_T0_FEAT[] = {1, 5, -1, 24, 7, -1, 27, -1, -1, -1, -1};
const float RF_T0_THR[] = {1.1688f, -1.2077f, -2.0000f, 0.5780f, -0.1861f, -2.0000f, 0.2263f, -2.0000f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T0_LEFT[] = {1, 2, -6, 4, 5, -3, 7, -2, -5, -4, -1};
const int16_t RF_T0_RIGHT[] = {10, 3, -1, 9, 6, -1, 8, -1, -1, -1, -1};
const int RF_T0_NNODES = 11;
static int16_t rf_walk_0(const float *x) {
    int i = 0;
    while (RF_T0_FEAT[i] >= 0)
        i = (x[RF_T0_FEAT[i]] <= RF_T0_THR[i]) ? RF_T0_LEFT[i] : RF_T0_RIGHT[i];
    return (int16_t)(-RF_T0_LEFT[i] - 1);
}
const int16_t RF_T1_FEAT[] = {27, -1, 1, -1, 11, 9, -1, 1, -1, -1, -1};
const float RF_T1_THR[] = {-1.3287f, -2.0000f, -0.7767f, -2.0000f, 0.5139f, -0.6865f, -2.0000f, -0.0944f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T1_LEFT[] = {1, -3, 3, -6, 5, 6, -1, 8, -2, -5, -4};
const int16_t RF_T1_RIGHT[] = {2, -1, 4, -1, 10, 7, -1, 9, -1, -1, -1};
const int RF_T1_NNODES = 11;
static int16_t rf_walk_1(const float *x) {
    int i = 0;
    while (RF_T1_FEAT[i] >= 0)
        i = (x[RF_T1_FEAT[i]] <= RF_T1_THR[i]) ? RF_T1_LEFT[i] : RF_T1_RIGHT[i];
    return (int16_t)(-RF_T1_LEFT[i] - 1);
}
const int16_t RF_T2_FEAT[] = {3, -1, 7, 9, -1, -1, 1, 13, -1, -1, -1};
const float RF_T2_THR[] = {-1.3143f, -2.0000f, -0.3083f, 1.0529f, -2.0000f, -2.0000f, 0.8380f, 0.6574f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T2_LEFT[] = {1, -6, 3, 4, -4, -3, 7, 8, -2, -5, -1};
const int16_t RF_T2_RIGHT[] = {2, -1, 6, 5, -1, -1, 10, 9, -1, -1, -1};
const int RF_T2_NNODES = 11;
static int16_t rf_walk_2(const float *x) {
    int i = 0;
    while (RF_T2_FEAT[i] >= 0)
        i = (x[RF_T2_FEAT[i]] <= RF_T2_THR[i]) ? RF_T2_LEFT[i] : RF_T2_RIGHT[i];
    return (int16_t)(-RF_T2_LEFT[i] - 1);
}
const int16_t RF_T3_FEAT[] = {25, 24, -1, 27, -1, -1, 24, -1, 27, -1, -1};
const float RF_T3_THR[] = {-0.1244f, 0.2826f, -2.0000f, 0.4832f, -2.0000f, -2.0000f, -1.0630f, -2.0000f, -1.3736f, -2.0000f, -2.0000f};
const int16_t RF_T3_LEFT[] = {1, 2, -5, 4, -4, -1, 7, -6, 9, -3, -2};
const int16_t RF_T3_RIGHT[] = {6, 3, -1, 5, -1, -1, 8, -1, 10, -1, -1};
const int RF_T3_NNODES = 11;
static int16_t rf_walk_3(const float *x) {
    int i = 0;
    while (RF_T3_FEAT[i] >= 0)
        i = (x[RF_T3_FEAT[i]] <= RF_T3_THR[i]) ? RF_T3_LEFT[i] : RF_T3_RIGHT[i];
    return (int16_t)(-RF_T3_LEFT[i] - 1);
}
const int16_t RF_T4_FEAT[] = {13, 27, -1, 3, -1, 27, 7, -1, -1, -1, -1};
const float RF_T4_THR[] = {0.6687f, -1.3260f, -2.0000f, -0.7664f, -2.0000f, 0.5140f, 0.2424f, -2.0000f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T4_LEFT[] = {1, 2, -3, 4, -6, 6, 7, -4, -2, -1, -5};
const int16_t RF_T4_RIGHT[] = {10, 3, -1, 5, -1, 9, 8, -1, -1, -1, -1};
const int RF_T4_NNODES = 11;
static int16_t rf_walk_4(const float *x) {
    int i = 0;
    while (RF_T4_FEAT[i] >= 0)
        i = (x[RF_T4_FEAT[i]] <= RF_T4_THR[i]) ? RF_T4_LEFT[i] : RF_T4_RIGHT[i];
    return (int16_t)(-RF_T4_LEFT[i] - 1);
}
const int16_t RF_T5_FEAT[] = {13, 1, 1, -1, 11, 5, -1, -1, -1, -1, -1};
const float RF_T5_THR[] = {0.6687f, 1.1688f, -1.1554f, -2.0000f, -0.1789f, -0.6658f, -2.0000f, -2.0000f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T5_LEFT[] = {1, 2, 3, -3, 5, 6, -6, -2, -4, -1, -5};
const int16_t RF_T5_RIGHT[] = {10, 9, 4, -1, 8, 7, -1, -1, -1, -1, -1};
const int RF_T5_NNODES = 11;
static int16_t rf_walk_5(const float *x) {
    int i = 0;
    while (RF_T5_FEAT[i] >= 0)
        i = (x[RF_T5_FEAT[i]] <= RF_T5_THR[i]) ? RF_T5_LEFT[i] : RF_T5_RIGHT[i];
    return (int16_t)(-RF_T5_LEFT[i] - 1);
}
const int16_t RF_T6_FEAT[] = {27, -1, 11, 3, 3, -1, -1, 5, -1, -1, -1};
const float RF_T6_THR[] = {-1.3736f, -2.0000f, 0.5237f, -0.2528f, -1.3143f, -2.0000f, -2.0000f, 0.0927f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T6_LEFT[] = {1, -3, 3, 4, 5, -6, -5, 8, -1, -2, -4};
const int16_t RF_T6_RIGHT[] = {2, -1, 10, 7, 6, -1, -1, 9, -1, -1, -1};
const int RF_T6_NNODES = 11;
static int16_t rf_walk_6(const float *x) {
    int i = 0;
    while (RF_T6_FEAT[i] >= 0)
        i = (x[RF_T6_FEAT[i]] <= RF_T6_THR[i]) ? RF_T6_LEFT[i] : RF_T6_RIGHT[i];
    return (int16_t)(-RF_T6_LEFT[i] - 1);
}
const int16_t RF_T7_FEAT[] = {9, 5, -1, 5, 9, -1, -1, 27, -1, -1, -1};
const float RF_T7_THR[] = {1.0620f, -1.1861f, -2.0000f, 0.0882f, -0.6865f, -2.0000f, -2.0000f, -0.1951f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T7_LEFT[] = {1, 2, -6, 4, 5, -1, -5, 8, -2, -4, -3};
const int16_t RF_T7_RIGHT[] = {10, 3, -1, 7, 6, -1, -1, 9, -1, -1, -1};
const int RF_T7_NNODES = 11;
static int16_t rf_walk_7(const float *x) {
    int i = 0;
    while (RF_T7_FEAT[i] >= 0)
        i = (x[RF_T7_FEAT[i]] <= RF_T7_THR[i]) ? RF_T7_LEFT[i] : RF_T7_RIGHT[i];
    return (int16_t)(-RF_T7_LEFT[i] - 1);
}
const int16_t RF_T8_FEAT[] = {1, 1, 27, 11, -1, -1, 3, -1, -1, 27, -1, -1, -1};
const float RF_T8_THR[] = {1.1812f, 0.4550f, 0.0337f, 0.1857f, -2.0000f, -2.0000f, -1.3101f, -2.0000f, -2.0000f, 0.6319f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T8_LEFT[] = {1, 2, 3, 4, -2, -3, 7, -6, -5, 10, -4, -5, -1};
const int16_t RF_T8_RIGHT[] = {12, 9, 6, 5, -1, -1, 8, -1, -1, 11, -1, -1, -1};
const int RF_T8_NNODES = 13;
static int16_t rf_walk_8(const float *x) {
    int i = 0;
    while (RF_T8_FEAT[i] >= 0)
        i = (x[RF_T8_FEAT[i]] <= RF_T8_THR[i]) ? RF_T8_LEFT[i] : RF_T8_RIGHT[i];
    return (int16_t)(-RF_T8_LEFT[i] - 1);
}
const int16_t RF_T9_FEAT[] = {5, -1, 1, 9, -1, -1, 11, -1, 3, -1, -1};
const float RF_T9_THR[] = {-1.2077f, -2.0000f, -0.0893f, 0.8763f, -2.0000f, -2.0000f, -0.3530f, -2.0000f, 0.0297f, -2.0000f, -2.0000f};
const int16_t RF_T9_LEFT[] = {1, -6, 3, 4, -2, -3, 7, -1, 9, -5, -4};
const int16_t RF_T9_RIGHT[] = {2, -1, 6, 5, -1, -1, 8, -1, 10, -1, -1};
const int RF_T9_NNODES = 11;
static int16_t rf_walk_9(const float *x) {
    int i = 0;
    while (RF_T9_FEAT[i] >= 0)
        i = (x[RF_T9_FEAT[i]] <= RF_T9_THR[i]) ? RF_T9_LEFT[i] : RF_T9_RIGHT[i];
    return (int16_t)(-RF_T9_LEFT[i] - 1);
}
const int16_t RF_T10_FEAT[] = {9, 25, -1, -1, 9, 1, -1, 27, -1, -1, 25, -1, 25, -1, -1};
const float RF_T10_THR[] = {-0.6865f, 0.4284f, -2.0000f, -2.0000f, 0.4478f, -0.0990f, -2.0000f, 0.4381f, -2.0000f, -2.0000f, 0.1775f, -2.0000f, 0.7006f, -2.0000f, -2.0000f};
const int16_t RF_T10_LEFT[] = {1, 2, -1, -6, 5, 6, -2, 8, -4, -5, 11, -4, 13, -2, -3};
const int16_t RF_T10_RIGHT[] = {4, 3, -1, -1, 10, 7, -1, 9, -1, -1, 12, -1, 14, -1, -1};
const int RF_T10_NNODES = 15;
static int16_t rf_walk_10(const float *x) {
    int i = 0;
    while (RF_T10_FEAT[i] >= 0)
        i = (x[RF_T10_FEAT[i]] <= RF_T10_THR[i]) ? RF_T10_LEFT[i] : RF_T10_RIGHT[i];
    return (int16_t)(-RF_T10_LEFT[i] - 1);
}
const int16_t RF_T11_FEAT[] = {24, -1, 1, -1, 27, -1, 11, 7, -1, 13, -1, -1, -1};
const float RF_T11_THR[] = {-1.2030f, -2.0000f, -0.7904f, -2.0000f, -0.1715f, -2.0000f, 0.5139f, 0.5495f, -2.0000f, 0.6574f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T11_LEFT[] = {1, -6, 3, -3, 5, -2, 7, 8, -1, 10, -1, -5, -4};
const int16_t RF_T11_RIGHT[] = {2, -1, 4, -1, 6, -1, 12, 9, -1, 11, -1, -1, -1};
const int RF_T11_NNODES = 13;
static int16_t rf_walk_11(const float *x) {
    int i = 0;
    while (RF_T11_FEAT[i] >= 0)
        i = (x[RF_T11_FEAT[i]] <= RF_T11_THR[i]) ? RF_T11_LEFT[i] : RF_T11_RIGHT[i];
    return (int16_t)(-RF_T11_LEFT[i] - 1);
}
const int16_t RF_T12_FEAT[] = {5, -1, 9, -1, 13, 24, 27, -1, -1, -1, -1};
const float RF_T12_THR[] = {-1.2077f, -2.0000f, -0.6865f, -2.0000f, 0.6574f, 0.5775f, -1.3736f, -2.0000f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T12_LEFT[] = {1, -6, 3, -1, 5, 6, 7, -3, -2, -4, -5};
const int16_t RF_T12_RIGHT[] = {2, -1, 4, -1, 10, 9, 8, -1, -1, -1, -1};
const int RF_T12_NNODES = 11;
static int16_t rf_walk_12(const float *x) {
    int i = 0;
    while (RF_T12_FEAT[i] >= 0)
        i = (x[RF_T12_FEAT[i]] <= RF_T12_THR[i]) ? RF_T12_LEFT[i] : RF_T12_RIGHT[i];
    return (int16_t)(-RF_T12_LEFT[i] - 1);
}
const int16_t RF_T13_FEAT[] = {13, 1, -1, 25, 9, -1, -1, 7, 1, -1, 9, -1, -1, -1, -1};
const float RF_T13_THR[] = {0.6687f, -1.1554f, -2.0000f, -0.6601f, -0.3311f, -2.0000f, -2.0000f, 0.9468f, -0.3134f, -2.0000f, -0.4222f, -2.0000f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T13_LEFT[] = {1, 2, -3, 4, 5, -1, -4, 8, 9, -6, 11, -1, -4, -2, -5};
const int16_t RF_T13_RIGHT[] = {14, 3, -1, 7, 6, -1, -1, 13, 10, -1, 12, -1, -1, -1, -1};
const int RF_T13_NNODES = 15;
static int16_t rf_walk_13(const float *x) {
    int i = 0;
    while (RF_T13_FEAT[i] >= 0)
        i = (x[RF_T13_FEAT[i]] <= RF_T13_THR[i]) ? RF_T13_LEFT[i] : RF_T13_RIGHT[i];
    return (int16_t)(-RF_T13_LEFT[i] - 1);
}
const int16_t RF_T14_FEAT[] = {27, 3, 5, -1, -1, 11, -1, 25, -1, -1, 3, 25, -1, -1, -1};
const float RF_T14_THR[] = {0.3317f, 0.3114f, 0.7108f, -2.0000f, -2.0000f, -0.1873f, -2.0000f, 0.3092f, -2.0000f, -2.0000f, -0.2106f, 0.2460f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T14_LEFT[] = {1, 2, 3, -2, -3, 6, -2, 8, -4, -3, 11, 12, -5, -6, -1};
const int16_t RF_T14_RIGHT[] = {10, 5, 4, -1, -1, 7, -1, 9, -1, -1, 14, 13, -1, -1, -1};
const int RF_T14_NNODES = 15;
static int16_t rf_walk_14(const float *x) {
    int i = 0;
    while (RF_T14_FEAT[i] >= 0)
        i = (x[RF_T14_FEAT[i]] <= RF_T14_THR[i]) ? RF_T14_LEFT[i] : RF_T14_RIGHT[i];
    return (int16_t)(-RF_T14_LEFT[i] - 1);
}
const int16_t RF_T15_FEAT[] = {5, -1, 9, -1, 24, 27, 7, -1, -1, -1, -1};
const float RF_T15_THR[] = {-1.2044f, -2.0000f, -0.6865f, -2.0000f, 0.6073f, 0.2291f, 0.2327f, -2.0000f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T15_LEFT[] = {1, -6, 3, -1, 5, 6, 7, -3, -2, -5, -4};
const int16_t RF_T15_RIGHT[] = {2, -1, 4, -1, 10, 9, 8, -1, -1, -1, -1};
const int RF_T15_NNODES = 11;
static int16_t rf_walk_15(const float *x) {
    int i = 0;
    while (RF_T15_FEAT[i] >= 0)
        i = (x[RF_T15_FEAT[i]] <= RF_T15_THR[i]) ? RF_T15_LEFT[i] : RF_T15_RIGHT[i];
    return (int16_t)(-RF_T15_LEFT[i] - 1);
}
const int16_t RF_T16_FEAT[] = {3, -1, 9, 24, -1, 25, 1, -1, -1, -1, -1};
const float RF_T16_THR[] = {-1.3143f, -2.0000f, 1.0684f, -0.1000f, -2.0000f, -0.1244f, 1.1688f, -2.0000f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T16_LEFT[] = {1, -6, 3, 4, -5, 6, 7, -4, -1, -2, -3};
const int16_t RF_T16_RIGHT[] = {2, -1, 10, 5, -1, 9, 8, -1, -1, -1, -1};
const int RF_T16_NNODES = 11;
static int16_t rf_walk_16(const float *x) {
    int i = 0;
    while (RF_T16_FEAT[i] >= 0)
        i = (x[RF_T16_FEAT[i]] <= RF_T16_THR[i]) ? RF_T16_LEFT[i] : RF_T16_RIGHT[i];
    return (int16_t)(-RF_T16_LEFT[i] - 1);
}
const int16_t RF_T17_FEAT[] = {24, 7, 25, -1, 3, -1, -1, -1, 27, -1, -1};
const float RF_T17_THR[] = {0.5928f, 1.1423f, -0.1121f, -2.0000f, -0.8537f, -2.0000f, -2.0000f, -2.0000f, 0.5140f, -2.0000f, -2.0000f};
const int16_t RF_T17_LEFT[] = {1, 2, 3, -5, 5, -6, -3, -2, 9, -4, -1};
const int16_t RF_T17_RIGHT[] = {8, 7, 4, -1, 6, -1, -1, -1, 10, -1, -1};
const int RF_T17_NNODES = 11;
static int16_t rf_walk_17(const float *x) {
    int i = 0;
    while (RF_T17_FEAT[i] >= 0)
        i = (x[RF_T17_FEAT[i]] <= RF_T17_THR[i]) ? RF_T17_LEFT[i] : RF_T17_RIGHT[i];
    return (int16_t)(-RF_T17_LEFT[i] - 1);
}
const int16_t RF_T18_FEAT[] = {13, 1, 9, 27, 7, -1, -1, -1, -1, -1, -1};
const float RF_T18_THR[] = {0.6696f, 1.2310f, 1.0620f, 0.3402f, 0.2327f, -2.0000f, -2.0000f, -2.0000f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T18_LEFT[] = {1, 2, 3, 4, 5, -4, -2, -6, -3, -1, -5};
const int16_t RF_T18_RIGHT[] = {10, 9, 8, 7, 6, -1, -1, -1, -1, -1, -1};
const int RF_T18_NNODES = 11;
static int16_t rf_walk_18(const float *x) {
    int i = 0;
    while (RF_T18_FEAT[i] >= 0)
        i = (x[RF_T18_FEAT[i]] <= RF_T18_THR[i]) ? RF_T18_LEFT[i] : RF_T18_RIGHT[i];
    return (int16_t)(-RF_T18_LEFT[i] - 1);
}
const int16_t RF_T19_FEAT[] = {3, -1, 7, 24, 24, -1, 27, -1, -1, 27, -1, -1, -1};
const float RF_T19_THR[] = {-1.3101f, -2.0000f, 1.1438f, 0.4319f, -0.1828f, -2.0000f, -0.4837f, -2.0000f, -2.0000f, 0.4700f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T19_LEFT[] = {1, -6, 3, 4, 5, -5, 7, -3, -5, 10, -4, -1, -2};
const int16_t RF_T19_RIGHT[] = {2, -1, 12, 9, 6, -1, 8, -1, -1, 11, -1, -1, -1};
const int RF_T19_NNODES = 13;
static int16_t rf_walk_19(const float *x) {
    int i = 0;
    while (RF_T19_FEAT[i] >= 0)
        i = (x[RF_T19_FEAT[i]] <= RF_T19_THR[i]) ? RF_T19_LEFT[i] : RF_T19_RIGHT[i];
    return (int16_t)(-RF_T19_LEFT[i] - 1);
}
const int16_t RF_T20_FEAT[] = {11, 3, -1, 27, -1, -1, 25, -1, 11, -1, -1};
const float RF_T20_THR[] = {-0.3484f, -0.7663f, -2.0000f, 0.2240f, -2.0000f, -2.0000f, -0.8498f, -2.0000f, 1.1161f, -2.0000f, -2.0000f};
const int16_t RF_T20_LEFT[] = {1, 2, -6, 4, -2, -1, 7, -5, 9, -4, -3};
const int16_t RF_T20_RIGHT[] = {6, 3, -1, 5, -1, -1, 8, -1, 10, -1, -1};
const int RF_T20_NNODES = 11;
static int16_t rf_walk_20(const float *x) {
    int i = 0;
    while (RF_T20_FEAT[i] >= 0)
        i = (x[RF_T20_FEAT[i]] <= RF_T20_THR[i]) ? RF_T20_LEFT[i] : RF_T20_RIGHT[i];
    return (int16_t)(-RF_T20_LEFT[i] - 1);
}
const int16_t RF_T21_FEAT[] = {1, 3, -1, -1, 1, 25, -1, -1, 7, -1, 3, -1, -1};
const float RF_T21_THR[] = {-0.7767f, -0.8537f, -2.0000f, -2.0000f, 0.4544f, -0.4597f, -2.0000f, -2.0000f, -0.3083f, -2.0000f, -0.3002f, -2.0000f, -2.0000f};
const int16_t RF_T21_LEFT[] = {1, 2, -6, -3, 5, 6, -5, -2, 9, -4, 11, -5, -1};
const int16_t RF_T21_RIGHT[] = {4, 3, -1, -1, 8, 7, -1, -1, 10, -1, 12, -1, -1};
const int RF_T21_NNODES = 13;
static int16_t rf_walk_21(const float *x) {
    int i = 0;
    while (RF_T21_FEAT[i] >= 0)
        i = (x[RF_T21_FEAT[i]] <= RF_T21_THR[i]) ? RF_T21_LEFT[i] : RF_T21_RIGHT[i];
    return (int16_t)(-RF_T21_LEFT[i] - 1);
}
const int16_t RF_T22_FEAT[] = {3, -1, 7, 3, -1, -1, 25, 1, -1, -1, -1};
const float RF_T22_THR[] = {-1.3098f, -2.0000f, -0.3083f, 0.5568f, -2.0000f, -2.0000f, -0.2655f, 0.8783f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T22_LEFT[] = {1, -6, 3, 4, -3, -4, 7, 8, -5, -1, -2};
const int16_t RF_T22_RIGHT[] = {2, -1, 6, 5, -1, -1, 10, 9, -1, -1, -1};
const int RF_T22_NNODES = 11;
static int16_t rf_walk_22(const float *x) {
    int i = 0;
    while (RF_T22_FEAT[i] >= 0)
        i = (x[RF_T22_FEAT[i]] <= RF_T22_THR[i]) ? RF_T22_LEFT[i] : RF_T22_RIGHT[i];
    return (int16_t)(-RF_T22_LEFT[i] - 1);
}
const int16_t RF_T23_FEAT[] = {7, 5, -1, 24, 3, -1, -1, 27, -1, -1, -1};
const float RF_T23_THR[] = {1.1423f, -1.2077f, -2.0000f, 0.4319f, -0.3401f, -2.0000f, -2.0000f, 0.5050f, -2.0000f, -2.0000f, -2.0000f};
const int16_t RF_T23_LEFT[] = {1, 2, -6, 4, 5, -5, -3, 8, -4, -1, -2};
const int16_t RF_T23_RIGHT[] = {10, 3, -1, 7, 6, -1, -1, 9, -1, -1, -1};
const int RF_T23_NNODES = 11;
static int16_t rf_walk_23(const float *x) {
    int i = 0;
    while (RF_T23_FEAT[i] >= 0)
        i = (x[RF_T23_FEAT[i]] <= RF_T23_THR[i]) ? RF_T23_LEFT[i] : RF_T23_RIGHT[i];
    return (int16_t)(-RF_T23_LEFT[i] - 1);
}
const int16_t RF_T24_FEAT[] = {24, 1, -1, 3, 3, -1, -1, -1, 7, -1, -1};
const float RF_T24_THR[] = {0.5928f, -1.1554f, -2.0000f, -0.2528f, -1.3143f, -2.0000f, -2.0000f, -2.0000f, -0.3083f, -2.0000f, -2.0000f};
const int16_t RF_T24_LEFT[] = {1, 2, -3, 4, 5, -6, -5, -2, 9, -4, -1};
const int16_t RF_T24_RIGHT[] = {8, 3, -1, 7, 6, -1, -1, -1, 10, -1, -1};
const int RF_T24_NNODES = 11;
static int16_t rf_walk_24(const float *x) {
    int i = 0;
    while (RF_T24_FEAT[i] >= 0)
        i = (x[RF_T24_FEAT[i]] <= RF_T24_THR[i]) ? RF_T24_LEFT[i] : RF_T24_RIGHT[i];
    return (int16_t)(-RF_T24_LEFT[i] - 1);
}
int16_t rf_tree_walk(int tree, const float *x)
{
    switch (tree) {
    case 0: return rf_walk_0(x);
    case 1: return rf_walk_1(x);
    case 2: return rf_walk_2(x);
    case 3: return rf_walk_3(x);
    case 4: return rf_walk_4(x);
    case 5: return rf_walk_5(x);
    case 6: return rf_walk_6(x);
    case 7: return rf_walk_7(x);
    case 8: return rf_walk_8(x);
    case 9: return rf_walk_9(x);
    case 10: return rf_walk_10(x);
    case 11: return rf_walk_11(x);
    case 12: return rf_walk_12(x);
    case 13: return rf_walk_13(x);
    case 14: return rf_walk_14(x);
    case 15: return rf_walk_15(x);
    case 16: return rf_walk_16(x);
    case 17: return rf_walk_17(x);
    case 18: return rf_walk_18(x);
    case 19: return rf_walk_19(x);
    case 20: return rf_walk_20(x);
    case 21: return rf_walk_21(x);
    case 22: return rf_walk_22(x);
    case 23: return rf_walk_23(x);
    case 24: return rf_walk_24(x);
    default: return -1;
    }
}