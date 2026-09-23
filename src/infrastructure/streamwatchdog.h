// ============================================================
// 文件: streamwatchdog.h
// 描述: RTSP 停帧看门狗纯逻辑判定。抽离为无依赖头文件，便于单元测试
//       （rtspthread.cpp 依赖 FFmpeg，无法进入轻量测试目标）。
//
//       判定语义：会话已成功打开（armed）且距最近一帧活动超过 timeoutMs
//       即视为僵死，应通过 FFmpeg 中断回调打断阻塞读并重连。
// ============================================================

#ifndef STREAMWATCHDOG_H
#define STREAMWATCHDOG_H

#include <QtGlobal>

// 停帧看门狗判定：
//   armed          - 是否已武装（仅在流成功打开后为 true）
//   lastActivityMs - 最近一帧/流打开时刻（毫秒，<=0 表示尚无活动）
//   nowMs          - 当前时刻（毫秒）
//   timeoutMs      - 停帧阈值（毫秒）
// 返回 true 表示会话僵死。
inline bool rtspIsStalled(bool armed, qint64 lastActivityMs, qint64 nowMs, qint64 timeoutMs)
{
    return armed && lastActivityMs > 0 && (nowMs - lastActivityMs) > timeoutMs;
}

#endif // STREAMWATCHDOG_H
