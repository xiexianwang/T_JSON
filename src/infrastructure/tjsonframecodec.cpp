// ============================================================
// 文件: tjsonframecodec.cpp
// 描述: TJsonFrameCodec 类的实现。从字节流中切分 T-JSON 协议帧，
//       支持标准帧 (0xEC 0x91) 与图像抓拍帧 (0xEB 0x92) 两种格式，
//       处理粘包、半包、非法长度以及无法识别的数据重同步。
// ============================================================

#include "tjsonframecodec.h"
#include <QtEndian>
#include <QDebug>
#include <cstring>

// 追加网络数据到缓冲
void TJsonFrameCodec::feed(const QByteArray& data)
{
    m_buffer.append(data);
}

// 切分下一帧。内部循环处理缓冲中的帧头与异常数据，直到切出一帧或数据不足。
bool TJsonFrameCodec::nextFrame(TJsonFrameKind& kind, FrameType& type, QByteArray& payload)
{
    // 最小标准帧为 7 字节（2 字节帧头 + 1 字节类型 + 4 字节长度）
    while (m_buffer.size() >= TJsonFrame::kHeaderSize) {
        const quint8 b1 = static_cast<quint8>(m_buffer.at(0));
        const quint8 b2 = static_cast<quint8>(m_buffer.at(1));

        if (b1 == TJsonFrame::kHeaderB1 && b2 == TJsonFrame::kHeaderB2) {
            // ----- 标准帧 (0xEC 0x91) -----
            quint32 length;
            memcpy(&length, m_buffer.constData() + 3, 4);
            length = qFromBigEndian(length);            // 大端转主机字节序

            // 长度合法性检查，防止恶意或异常数据导致内存问题
            if (length > TJsonFrame::kMaxJsonLength) {
                qWarning() << "Abnormal JSON length detected:" << length << ". Discarding header.";
                m_buffer.remove(0, 2);                  // 丢弃无效帧头的前 2 字节
                continue;
            }

            // 检查缓冲是否已收齐完整帧（7 字节头 + 载荷长度）
            if (static_cast<quint32>(m_buffer.size()) < TJsonFrame::kHeaderSize + length) {
                return false;                           // 半包，等待更多数据
            }

            type = static_cast<FrameType>(static_cast<quint8>(m_buffer.at(2)));
            payload = m_buffer.mid(TJsonFrame::kHeaderSize, length);
            m_buffer.remove(0, TJsonFrame::kHeaderSize + length);
            kind = TJsonFrameKind::Standard;
            return true;
        }

        if (b1 == TJsonFrame::kSnapHeaderB1 && b2 == TJsonFrame::kSnapHeaderB2) {
            // ----- 图像抓拍帧 (0xEB 0x92) -----
            // 固定帧头 18 字节：2(帧头) + 1(类型) + 4(JPEG大小) + 11(坐标/保留)
            if (m_buffer.size() < TJsonFrame::kSnapHeaderSize) {
                return false;                           // 帧头不完整
            }

            quint32 jpegSize;
            memcpy(&jpegSize, m_buffer.constData() + 3, 4);
            jpegSize = qFromBigEndian(jpegSize);

            // JPEG 大小合法性检查
            if (jpegSize > TJsonFrame::kMaxJpegLength) {
                qWarning() << "Abnormal JPEG length detected:" << jpegSize << ". Discarding header.";
                m_buffer.remove(0, 2);
                continue;
            }

            const quint32 totalFrameSize = TJsonFrame::kSnapHeaderSize + jpegSize;
            if (static_cast<quint32>(m_buffer.size()) < totalFrameSize) {
                return false;                           // 数据不足
            }

            payload = m_buffer.left(totalFrameSize);    // 完整帧数据（含帧头，交 Parser 校验）
            m_buffer.remove(0, totalFrameSize);
            kind = TJsonFrameKind::Snap;
            type = FrameType::ImageSnap;
            return true;
        }

        // ----- 未知数据：向后搜索下一个有效帧头 -----
        resync();
    }
    return false;
}

// 未知数据重同步：向后搜索最近的有效帧头（0xEC91 / 0xEB92）并对齐
void TJsonFrameCodec::resync()
{
    int nextEc = m_buffer.indexOf(QByteArray::fromHex("EC91"), 1);
    int nextEb = m_buffer.indexOf(QByteArray::fromHex("EB92"), 1);

    // 取两个帧头中较近的一个作为对齐位置
    int nextHeader = -1;
    if (nextEc != -1 && nextEb != -1) nextHeader = qMin(nextEc, nextEb);
    else if (nextEc != -1) nextHeader = nextEc;
    else if (nextEb != -1) nextHeader = nextEb;

    if (nextHeader != -1) {
        m_buffer.remove(0, nextHeader);                 // 跳到下一个帧头位置
    } else {
        m_buffer.clear();                               // 无可识别帧头，清空缓冲区
    }
}

// 组标准协议帧
QByteArray TJsonFrameCodec::buildStandardFrame(FrameType type, const QByteArray& payload)
{
    QByteArray frame;
    frame.append(static_cast<char>(TJsonFrame::kHeaderB1)); // 帧头字节 1
    frame.append(static_cast<char>(TJsonFrame::kHeaderB2)); // 帧头字节 2
    frame.append(static_cast<char>(type));                  // 帧类型

    // 载荷长度，以大端序写入 4 字节
    quint32 lenBE = qToBigEndian(static_cast<quint32>(payload.size()));
    frame.append(reinterpret_cast<const char*>(&lenBE), 4);

    if (!payload.isEmpty()) {
        frame.append(payload);                              // 附加载荷
    }
    return frame;
}

// 心跳帧（固定字节：EC 91 11 00 00 00 00）
QByteArray TJsonFrameCodec::buildHeartbeatFrame()
{
    return QByteArray::fromHex("EC911100000000");
}
