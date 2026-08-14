// ============================================================
// 文件: tjsonframecodec.h
// 描述: T-JSON 协议帧编解码器。负责帧头识别、长度解析、粘包/
//       半包缓冲、异常数据重同步，以及发送帧的组包。纯 C++ 类，
//       无 Qt 信号依赖，可直接进行单元测试。
// ============================================================

#ifndef TJSONFRAMECODEC_H
#define TJSONFRAMECODEC_H

#include "infrastructure/tjsonframe.h"
#include <QByteArray>

// 帧类别：区分标准协议帧与图像抓拍帧，二者载荷结构不同
enum class TJsonFrameKind {
    Standard,   // 标准帧 (0xEC 0x91)
    Snap        // 图像抓拍帧 (0xEB 0x92)
};

// T-JSON 协议帧编解码器
class TJsonFrameCodec
{
public:
    TJsonFrameCodec() = default;

    // 追加网络接收数据，内部缓冲等待完整帧
    void feed(const QByteArray& data);

    // 取出下一帧。返回 false 表示数据不足（半包）或已无可解析帧。
    //   - 标准帧: kind=Standard, type=帧类型, payload=载荷（不含帧头）
    //   - 抓拍帧: kind=Snap, type=ImageSnap, payload=完整帧数据（含帧头）
    bool nextFrame(TJsonFrameKind& kind, FrameType& type, QByteArray& payload);

    // 当前缓冲的字节数 / 清空缓冲
    int bufferedBytes() const { return m_buffer.size(); }
    void clear() { m_buffer.clear(); }

    // ---- 发送帧组包 ----
    // 组标准帧: [0xEC][0x91][type(1B)][length(4B big-endian)][payload]
    static QByteArray buildStandardFrame(FrameType type, const QByteArray& payload);

    // 心跳帧（固定字节: EC 91 11 00 00 00 00）
    static QByteArray buildHeartbeatFrame();

private:
    QByteArray m_buffer;

    void resync();              // 向后搜索下一个有效帧头并对齐
};

#endif // TJSONFRAMECODEC_H
