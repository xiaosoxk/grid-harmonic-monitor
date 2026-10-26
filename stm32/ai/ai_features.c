/**
 * ai_features.c — 边缘AI：特征提取 + 随机森林推理（Cortex-M4F, 纯float, 无依赖）
 *
 * 特征算式与训练脚本逐维对照(改任何一端必须同步另一端并重训!):
 *   归一化: x = (raw - MEAN) / STD   ← MEAN/STD 在 rf_model.c(训练导出)
 *   推理:   25棵树投票, 单树 = 沿嵌套阈值走到叶
 * 耗时: 25树×~15节点比较 ≈ 400次浮点比较 < 0.1ms @168MHz(DWT实测口径)
 *
 * 集成(main.c, PQ_Analyze之后两行):
 *   float feat[AI_N_FEAT]; AiResult ai;
 *   AI_Features(&s_result, feat);
 *   AI_Classify(feat, &ai);              // ai.class_id / ai.confidence
 *   if (ai.confidence > 0.6f) UI_LogEvent(3, (float)ai.class_id, utc);  // 设备切换入事件日志
 * 上传: RESULT帧可扩展3个float(class/conf/reserved), 协议v1.2兼容新增。
 */
#include "ai_features.h"
#include "rf_model.h"
#include <math.h>

void AI_Features(const PqResult *r, float feat[AI_N_FEAT])
{
    const float u1 = (r->i_harm[1] > 1e-9f) ? r->i_harm[1] : 1e-9f;
    float odd = 0.0f, even = 0.0f, tri = 0.0f, all = 0.0f;
    for (int h = 2; h <= 25; h++) {
        float a = r->i_harm[h];
        all += a * a;
        if (h & 1) odd += a * a; else even += a * a;
        if (h % 3 == 0) tri += a * a;
        feat[h - 2] = a / u1 * 100.0f;                 /* [0..23] h2..h25 */
    }
    feat[24] = r->thdi;                                 /* THDi % */
    feat[25] = r->pf;                                   /* PF */
    float ratio = sqrtf(odd) / (sqrtf(even) + 1e-9f);
    feat[26] = (ratio > 50.0f) ? 50.0f : ratio;         /* 奇偶比 clip */
    feat[27] = sqrtf(tri) / (sqrtf(all) + 1e-9f) * 100.0f;  /* 3n占比% */
}

void AI_Classify(const float feat[AI_N_FEAT], AiResult *out)
{
    float x[AI_N_FEAT];
    for (int i = 0; i < AI_N_FEAT; i++)                 /* 训练端同款标准化 */
        x[i] = (feat[i] - RF_FEAT_MEAN[i]) / RF_FEAT_STD[i];

    uint8_t votes[RF_N_CLASSES] = {0};
    for (int t = 0; t < RF_N_TREES; t++) {
        /* 树数组按训练脚本导出的符号链接(弱符号表, 见 rf_model.c 生成说明):
         * 为免动态分发, rf_model.c 生成时已把树展开为 switch-case 宏 —— 直接调用 */
        extern int16_t rf_tree_walk(int tree, const float *x);   /* 生成的辅助 */
        int cls = rf_tree_walk(t, x);
        if (cls >= 0 && cls < RF_N_CLASSES) votes[cls]++;
    }
    int best = 0;
    for (int c = 1; c < RF_N_CLASSES; c++)
        if (votes[c] > votes[best]) best = c;
    out->class_id = (int8_t)best;
    out->confidence = (float)votes[best] / (float)RF_N_TREES;
}

const char *AI_ClassNameCn(int8_t id)
{
    static const char *CN[6] = {
        "阻性负载", "LED灯", "充电器", "笔记本电源", "风扇/调光", "变频空调"
    };
    return (id >= 0 && id < 6) ? CN[id] : "未知";
}
