#ifndef DEVICECONTROLLER_H
#define DEVICECONTROLLER_H

#include <QObject>
#include <QByteArray>
#include "tjsonclient.h"

// 专门用于组装各类底层通信协议的独立工具箱
class ProtocolBuilder {
public:
    // Pelco-D 协议组包 (7字节，含 Checksum 计算)
    static QByteArray buildPelcoD(quint8 address, quint8 cmd1, quint8 cmd2, quint8 data1, quint8 data2) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0xFF));
        pkt.append(static_cast<char>(address));
        pkt.append(static_cast<char>(cmd1));
        pkt.append(static_cast<char>(cmd2));
        pkt.append(static_cast<char>(data1));
        pkt.append(static_cast<char>(data2));
        quint8 checksum = (address + cmd1 + cmd2 + data1 + data2) % 256;
        pkt.append(static_cast<char>(checksum));
        return pkt;
    }
};

class DeviceController : public QObject
{
    Q_OBJECT
public:
    explicit DeviceController(TJsonClient* client, QObject *parent = nullptr);

    // ================= 基础控制 =================
    void setWorkMode(int mode);
    void queryImageParams();

    // ================= 串口透传通用网关 =================
    // 无论是哪种协议（Pelco-D/VISCA/自定义），也无论是云台还是镜头，
    // 只要是在 T-JSON 架构下透传，统一调用该网关。
    // serialType 对应文档中的 "PELCO_D", "VISCA", "VISCAIR" 等
    void sendTransparentData(const QString& serialType, const QByteArray& data);

private:
    TJsonClient* m_client;
};

#endif // DEVICECONTROLLER_H