# -*- coding: utf-8 -*-
"""
train_load_id.py — 谐波指纹负载识别：训练 + 评估 + 导出 C 部署
流程: 读CSV -> 28维特征(与固件ai_features.c完全一致) -> 按会话划分训练/测试
      -> 随机森林基线 -> 导出 rf_model.c/.h 到固件目录 -> C导出回环校验
特征定义(两端必须一致, 顺序固定):
  [0..23]  h2..h25 相对基波幅值比 x100
  [24]     THDi (%)
  [25]     PF
  [26]     奇偶次能量比 (odd/even, clip 0..50)
  [27]     3n次谐波占比 (3,6,..24 / 全部2..25, x100)
用法:
  python gen_synthetic_dataset.py    # 先造数据(实测CSV到位后跳过)
  python train_load_id.py            # 训练+导出+回环校验
实测数据放置: data/ai_dataset/<CLASS>_<session>.csv (格式同合成器, 可分批追加)
"""
import sys, os, glob, re
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "data", "ai_dataset")
FW = os.path.abspath(os.path.join(HERE, "..", "..", "stm32", "ai"))   # 导出目标: 固件目录

# ---------------- 特征(与 ai_features.c 一致) ----------------
def features(h1_h25, thdi, pf):
    h = np.asarray(h1_h25, dtype=float)      # h[0]=h1 ... h[24]=h25
    u1 = max(h[0], 1e-9)
    rel = h[1:25] / u1 * 100.0               # h2..h25 (%)
    odd = np.sqrt(np.sum(h[1:25:2] ** 2))    # 3,5,...,25
    even = np.sqrt(np.sum(h[2:25:2] ** 2))   # 2,4,...,24
    tri = np.sqrt(np.sum(h[[2, 5, 8, 11, 14, 17, 20, 23]] ** 2))  # 3,6,..24
    allh = np.sqrt(np.sum(h[1:25] ** 2))
    return np.concatenate([rel, [thdi, pf,
                                 float(np.clip(odd / max(even, 1e-9), 0, 50)),
                                 float(tri / max(allh, 1e-9) * 100.0)]])

# ---------------- 读数据集 ----------------
X, y, sessions, classes = [], [], [], []
for f in sorted(glob.glob(os.path.join(DATA, "*.csv"))):
    m = re.match(r"([A-Z_]+)_s(\d+)\.csv", os.path.basename(f))
    if not m:
        continue
    cls, ses = m.group(1), int(m.group(2))
    if cls not in classes:
        classes.append(cls)
    arr = np.loadtxt(f, delimiter=",", skiprows=1)
    for row in arr:
        X.append(features(row[:25], row[25], row[26]))
        y.append(classes.index(cls))
        sessions.append((cls, ses))
X, y = np.array(X), np.array(y)
print("dataset: %d windows, %d classes: %s" % (len(X), len(classes), classes))
assert len(classes) >= 2, "至少两类"

# ---------------- 按会话划分(防泄漏): 每类偶数会话训练/奇数测试 ----------------
tr = np.array([s[1] % 2 == 0 for s in sessions])
Xtr, ytr, Xte, yte = X[tr], y[tr], X[~tr], y[~tr]
print("split by session: train %d / test %d" % (len(Xtr), len(Xte)))

# ---------------- 标准化(均值/方差导出到C) ----------------
mu, sd = Xtr.mean(0), Xtr.std(0) + 1e-9
Xtr_n, Xte_n = (Xtr - mu) / sd, (Xte - mu) / sd

# ---------------- 随机森林 ----------------
from sklearn.ensemble import RandomForestClassifier
rf = RandomForestClassifier(n_estimators=25, max_depth=8, random_state=7)
rf.fit(Xtr_n, ytr)
acc = float((rf.predict(Xte_n) == yte).mean())
print("RF test accuracy: %.2f%%" % (acc * 100))

# ---------------- 导出 C: 树数组 + 标准化常数 + 类名 ----------------
def export_c():
    trees = rf.estimators_
    lines_h, lines_c = [], []
    lines_h.append("/* rf_model.h — 由 train_load_id.py 自动生成, 勿手改 */")
    lines_h.append("#ifndef RF_MODEL_H")
    lines_h.append("#define RF_MODEL_H")
    lines_h.append("#include <stdint.h>")
    lines_h.append("#define RF_N_TREES   %d" % len(trees))
    lines_h.append("#define RF_N_FEAT    28")
    lines_h.append("#define RF_N_CLASSES %d" % len(classes))
    lines_h.append("extern const char * const RF_CLASS_NAMES[RF_N_CLASSES];")
    lines_h.append("extern const float  RF_FEAT_STD[RF_N_FEAT];   /* 标准化均值/方差 */")
    lines_h.append("extern const float  RF_FEAT_MEAN[RF_N_FEAT];")
    lines_h.append("/* 每棵树: 扁平数组表示, node i 为叶则 left[i]<0, class=-left[i]-1 */")
    lines_h.append("typedef struct { int n_nodes; const int16_t feat[]; } RFTree;")
    for t in range(len(trees)):
        lines_h.append("extern const int16_t RF_T%d_FEAT[];" % t)
        lines_h.append("extern const float   RF_T%d_THR[];" % t)
        lines_h.append("extern const int16_t RF_T%d_LEFT[];" % t)
        lines_h.append("extern const int16_t RF_T%d_RIGHT[];" % t)
        lines_h.append("extern const int     RF_T%d_NNODES;" % t)
    lines_h.append("int16_t rf_tree_walk(int tree, const float *x);   /* 推理入口 */")
    lines_h.append("#endif")

    lines_c.append("/* rf_model.c — 由 train_load_id.py 自动生成 */")
    lines_c.append('#include "rf_model.h"')
    lines_c.append("const char * const RF_CLASS_NAMES[RF_N_CLASSES] = {%s};"
                   % ", ".join('"%s"' % c for c in classes))
    lines_c.append("const float RF_FEAT_MEAN[RF_N_FEAT] = {%s};"
                   % ", ".join("%.5ff" % v for v in mu))
    lines_c.append("const float RF_FEAT_STD[RF_N_FEAT] = {%s};"
                   % ", ".join("%.5ff" % v for v in sd))
    total_nodes = 0
    for t, est in enumerate(trees):
        tree = est.tree_
        n = tree.node_count
        total_nodes += n
        def idx_arr(a):
            return ", ".join(str(int(v)) for v in a)
        feat = [int(f) if f >= 0 else -1 for f in tree.feature]
        left, right = [], []
        for i in range(n):
            if tree.children_left[i] == -1:          # 叶: class存负数编码
                cls = int(np.argmax(tree.value[i][0]))
                left.append(-(cls + 1)); right.append(-1)
            else:
                left.append(int(tree.children_left[i])); right.append(int(tree.children_right[i]))
        lines_c.append("const int16_t RF_T%d_FEAT[] = {%s};" % (t, idx_arr(feat)))
        lines_c.append("const float RF_T%d_THR[] = {%s};"
                       % (t, ", ".join("%.4ff" % v for v in tree.threshold)))
        lines_c.append("const int16_t RF_T%d_LEFT[] = {%s};" % (t, idx_arr(left)))
        lines_c.append("const int16_t RF_T%d_RIGHT[] = {%s};" % (t, idx_arr(right)))
        lines_c.append("const int RF_T%d_NNODES = %d;" % (t, n))
        lines_c.append("static int16_t rf_walk_%d(const float *x) {" % t)
        lines_c.append("    int i = 0;")
        lines_c.append("    while (RF_T%d_FEAT[i] >= 0)" % t)
        lines_c.append("        i = (x[RF_T%d_FEAT[i]] <= RF_T%d_THR[i]) ? RF_T%d_LEFT[i] : RF_T%d_RIGHT[i];"
                       % (t, t, t, t))
        lines_c.append("    return (int16_t)(-RF_T%d_LEFT[i] - 1);" % t)
        lines_c.append("}")
    lines_c.append("int16_t rf_tree_walk(int tree, const float *x)")
    lines_c.append("{")
    lines_c.append("    switch (tree) {")
    for t in range(len(trees)):
        lines_c.append("    case %d: return rf_walk_%d(x);" % (t, t))
    lines_c.append("    default: return -1;")
    lines_c.append("    }")
    lines_c.append("}")
    open(os.path.join(FW, "rf_model.h"), "w", encoding="utf-8").write("\n".join(lines_h))
    open(os.path.join(FW, "rf_model.c"), "w", encoding="utf-8").write("\n".join(lines_c))
    return total_nodes

n_nodes = export_c()
print("exported -> %s (rf_model.c/.h, 25 trees, %d nodes total)" % (FW, n_nodes))

# ---------------- 回环校验: 用导出的数组在Python重实现推理, 必须与sklearn一致 ----------------
def infer_c_model(x28):
    x = [(v - mu[i]) / sd[i] for i, v in enumerate(x28)]
    votes = [0] * len(classes)
    src = open(os.path.join(FW, "rf_model.c"), encoding="utf-8").read()
    import re as _re
    feats = [[float(v) for v in _re.search(r"RF_T%d_THR\[\] = \{([^}]*)\}" % t, src).group(1).replace("f", "").split(",")]
             for t in range(len(rf.estimators_))]
    F = [[int(v) for v in _re.search(r"RF_T%d_FEAT\[\] = \{([^}]*)\}" % t, src).group(1).split(",")]
         for t in range(len(rf.estimators_))]
    L = [[int(v) for v in _re.search(r"RF_T%d_LEFT\[\] = \{([^}]*)\}" % t, src).group(1).split(",")]
         for t in range(len(rf.estimators_))]
    R = [[int(v) for v in _re.search(r"RF_T%d_RIGHT\[\] = \{([^}]*)\}" % t, src).group(1).split(",")]
         for t in range(len(rf.estimators_))]
    for t in range(len(F)):
        i = 0
        while F[t][i] >= 0:
            i = L[t][i] if x[F[t][i]] <= feats[t][i] else R[t][i]
        votes[-L[t][i] - 1] += 1
    best = max(range(len(votes)), key=lambda k: votes[k])
    return best, votes[best] / float(len(F))

ok = 0
for xi in range(len(Xte)):
    pred, _ = infer_c_model(Xte[xi])
    ok += (pred == yte[xi])
print("C-export round-trip accuracy: %.2f%% (%d/%d)" % (ok / len(Xte) * 100, ok, len(Xte)))

# ---------------- 混淆矩阵(论文图直接用) ----------------
cm = np.zeros((len(classes), len(classes)), int)
for xi in range(len(Xte)):
    pred, _ = infer_c_model(Xte[xi])
    cm[yte[xi], pred] += 1
print("confusion matrix (rows=true):")
print("%-13s %s" % ("", " ".join("%6s" % c[:6] for c in classes)))
for i, c in enumerate(classes):
    print("%-13s %s" % (c, " ".join("%6d" % v for v in cm[i])))
