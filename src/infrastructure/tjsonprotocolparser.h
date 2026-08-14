// ============================================================
// 文件: tjsonprotocolparser.h
// 描述: T-JSON 协议载荷解析器。负责将 TJsonFrameCodec 切出的
//       完整帧按类型解析为业务数据（JSON 状态帧 / ACK / 图像抓拍）。
//       纯 C++ 类，无 Qt 信号依赖，可直接进行单元测试。
// ============================================================

#ifndef TJSONPROTOCOLPARSER_H
#define TJSONPROTOCOLPARSER_H

#include "infrastructure/tjsonframe.h"
#include <QJsonObject>
#include <QByteArray>
#include <QRect>

// T-JSON 协议载荷解析器
class TJsonProtocolParser
{
public:
    // 抓拍帧解析结果
    struct SnapResult {
        QByteArray jpegData;    // JPEG 数据块
        QRect location;         // 图像在原始画面中的位置
    };

    // 解析标准帧 JSON 载荷，成功返回 true 并填充 out
    static bool parseJson(const QByteArray& payload, QJsonObject& out);

    // 解析 ACK 帧状态码（载荷 2 字节大端整数），载荷非法时返回 0
    static quint8 parseAck(const QByteArray& payload);

    // 解析完整抓拍帧（含帧头），校验 checksum 与帧尾标识，成功返回 true
    static bool parseImageSnap(const QByteArray& frame, SnapResult& out);
};

#endif // TJSONPROTOCOLPARSER_H
