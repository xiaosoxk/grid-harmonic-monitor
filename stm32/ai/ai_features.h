/**
 * ai_features.h — 边缘AI特征提取（与 train_load_id.py 完全一致的28维向量）
 */
#ifndef AI_FEATURES_H
#define AI_FEATURES_H

#include "pq_math.h"

#define AI_N_FEAT 28

typedef struct {
    int8_t   class_id;      /* 识别类别 */
    float    confidence;    /* 投票置信度 0..1 */
} AiResult;

/* 从分析结果提取28维特征(顺序与训练端一致):
 * [0..23] h2..h25 相对基波幅值比 x100
 * [24] THDi%  [25] PF
 * [26] 奇偶次能量比 clip(0..50)  [27] 3n次谐波占比 x100 */
void AI_Features(const PqResult *r, float feat[AI_N_FEAT]);

/* 推理: 随机森林投票(模型由训练脚本生成 rf_model.c) */
void AI_Classify(const float feat[AI_N_FEAT], AiResult *out);

/* 中文标签(TFT显示用) */
const char *AI_ClassNameCn(int8_t id);

#endif /* AI_FEATURES_H */
