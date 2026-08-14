// ============================================================
// 文件: tjsonprotocolparser.cpp
// 描述: TJsonProtocolParser 类的实现。解析 JSON 状态帧、ACK
//       应答帧与图像抓拍帧，抓拍帧包含校验和与帧尾的校验。
// ============================================================

#include "tjsonprotocolparser.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtEndian>
#include <QDebug>
#include <cstring>

// 解析 JSON 载荷
bool TJsonProtocolParser::parseJson(const QByteArray& payload, QJsonObject& out)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        out = doc.object();
        return true;
    }
    qDebug() << "Failed to parse JSON:" << err.errorString();   // 解析失败日志
    return false;
}

// 解析 ACK 状态码：载荷为 2 字节大端整数
quint8 TJsonProtocolParser::parseAck(const QByteArray& payload)
{
    if (payload.size() < 2) return 0;
    quint16 statusCode = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(payload.constData()));
    return static_cast<quint8>(statusCode);
}

// 解析完整抓拍帧
// 帧结构:
//   [0xEB][0x92][0x04][jpegSize(4B)][left(2B)][top(2B)][width(2B)][height(2B)]
//   [jpegData(NB)][checksum(1B)][0xFB][0x92]
// 帧校验 = (0xEB + 0x92 + 0x04 + jpegSize 的 4 字节) & 0xFF
bool TJsonProtocolParser::parseImageSnap(const QByteArray& frame, SnapResult& out)
{
    if (frame.size() < TJsonFrame::kSnapHeaderSize) return false;

    // JPEG 数据大小（偏移 3 处）
    quint32 jpegSize;
    memcpy(&jpegSize, frame.constData() + 3, 4);
    jpegSize = qFromBigEndian(jpegSize);

    const quint32 totalFrameSize = TJsonFrame::kSnapHeaderSize + jpegSize;
    if (static_cast<quint32>(frame.size()) < totalFrameSize) return false;

    // 校验帧校验和：前 7 字节累加
    quint8 expectedSum = 0;
    for (int i = 0; i < 7; ++i)
        expectedSum += static_cast<quint8>(frame.at(i));
    quint8 actualSum = static_cast<quint8>(frame.at(TJsonFrame::kSnapJpegOffset + jpegSize));
    if (actualSum != expectedSum) {
        qWarning() << "Image snap checksum mismatch: expected" << expectedSum << "got" << actualSum;
        return false;
    }

    // 校验帧尾标识 0xFB 0x92
    if (static_cast<quint8>(frame.at(TJsonFrame::kSnapJpegOffset + 1 + jpegSize)) != 0xFB ||
        static_cast<quint8>(frame.at(TJsonFrame::kSnapJpegOffset + 2 + jpegSize)) != 0x92) {
        qWarning() << "Image snap footer mismatch";
        return false;
    }

    // 从偏移 7 处读取画面区域坐标（大端序）
    quint16 left, top, width, height;
    memcpy(&left, frame.constData() + TJsonFrame::kSnapCoordOffset, 2);
    memcpy(&top, frame.constData() + TJsonFrame::kSnapCoordOffset + 2, 2);
    memcpy(&width, frame.constData() + TJsonFrame::kSnapCoordOffset + 4, 2);
    memcpy(&height, frame.constData() + TJsonFrame::kSnapCoordOffset + 6, 2);

    // JPEG 数据从偏移 15 处开始
    out.jpegData = frame.mid(TJsonFrame::kSnapJpegOffset, jpegSize);
    out.location = QRect(qFromBigEndian(left), qFromBigEndian(top),
                         qFromBigEndian(width), qFromBigEndian(height));
    return true;
}
