// ============================================================
// 文件: rtspbackoff.h
// 描述: 重连退避抖动（jitter）纯逻辑。抽离为无依赖头文件，便于单元测试。
//
//       目的：多设备同时断网时，避免所有客户端在同一时刻（1s/2s/4s…）
//       整齐重连造成的惊群。在指数退避基数上叠加 ±pct% 随机量。
// ============================================================

#ifndef RTSPBACKOFF_H
#define RTSPBACKOFF_H

#include <QtGlobal>
#include <QPair>

// 抖动上下界：baseMs ± clamp(pct,0,100)%。始终非负。
inline QPair<int, int> rtspJitterBounds(int baseMs, int jitterPercent)
{
    const int pct = qBound(0, jitterPercent, 100);
    const int span = (baseMs * pct) / 100;
    const int lo = qMax(0, baseMs - span);
    const int hi = baseMs + span;
    return { lo, hi };
}

// 应用抖动：seed 由调用方提供（便于确定性测试）。
// span = max(1, base*pct/100)，结果 = base + [-span, +span]，夹到 >= 0。
inline int rtspApplyJitter(int baseMs, int jitterPercent, quint32 seed)
{
    const int pct = qBound(0, jitterPercent, 100);
    if (pct == 0 || baseMs <= 0) return qMax(0, baseMs);

    const int span = qMax(1, (baseMs * pct) / 100);
    const quint32 range = static_cast<quint32>(2 * span + 1);
    const int delta = static_cast<int>(seed % range) - span;
    return qMax(0, baseMs + delta);
}

#endif // RTSPBACKOFF_H
