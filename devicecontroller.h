#ifndef DEVICECONTROLLER_H
#define DEVICECONTROLLER_H

#include <QObject>
#include <QByteArray>
#include "tjsonclient.h"
#include "configmanager.h"

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

    // VISCA 变倍 (速度 0-7)
    static QByteArray buildViscaZoom(quint8 addr, bool tele, quint8 speed) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0x80 | addr));
        pkt.append(static_cast<char>(0x01));
        pkt.append(static_cast<char>(0x04));
        pkt.append(static_cast<char>(0x07));
        pkt.append(static_cast<char>((tele ? 0x20 : 0x30) | (speed & 0x07)));
        pkt.append(static_cast<char>(0xFF));
        return pkt;
    }

    // VISCA 变焦 (速度 0-7)
    static QByteArray buildViscaFocus(quint8 addr, bool far, quint8 speed) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0x80 | addr));
        pkt.append(static_cast<char>(0x01));
        pkt.append(static_cast<char>(0x04));
        pkt.append(static_cast<char>(0x08));
        pkt.append(static_cast<char>((far ? 0x20 : 0x30) | (speed & 0x07)));
        pkt.append(static_cast<char>(0xFF));
        return pkt;
    }

    // VISCA 变倍/变焦停止
    static QByteArray buildViscaStop(quint8 addr, bool zoom) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0x80 | addr));
        pkt.append(static_cast<char>(0x01));
        pkt.append(static_cast<char>(0x04));
        pkt.append(static_cast<char>(zoom ? 0x07 : 0x08));
        pkt.append(static_cast<char>(0x00));
        pkt.append(static_cast<char>(0xFF));
        return pkt;
    }
};

enum class PtzDir : quint8 {
    Up = 0x08,
    Down = 0x10,
    Left = 0x04,
    Right = 0x02,
    UpLeft = 0x0C,
    UpRight = 0x0A,
    DownLeft = 0x14,
    DownRight = 0x12
};

class DeviceController : public QObject
{
    Q_OBJECT
public:
    explicit DeviceController(TJsonClient* client, ConfigManager* cfg, QObject *parent = nullptr);

    // ================= 基础控制 =================
    void setWorkMode(int mode);
    void queryImageParams();

    // ================= 算法与显示控制 =================
    void setAlgoModel(int model);
    void setDisplayMode(int mode);
    void setLocation(const QString& lat, const QString& lon);

    // ================= 云台控制 (Pelco-D) =================
    void ptzMove(PtzDir dir);
    void ptzStop();

    // ================= 框选跟踪 =================
    void setBoxTrack(int centerX, int centerY, int width, int height);

    // ================= 预置位控制 (Pelco-D) =================
    void setPreset(int preset);
    void callPreset(int preset);
    void delPreset(int preset);

    // ================= 附加功能开关 =================
    void setDigitalZoom(bool enable);
    void setAutoZoom(bool enable);
    void setCaptureUpload(bool enable);
    void posReset(bool enable);

    // ================= 镜头控制 (VISCA / Pelco-D) =================
    // target: 0=可见光(VISCA), 1=红外(Pelco-D)
    void lensZoomIn(int target);
    void lensZoomOut(int target);
    void lensFocusIn(int target);
    void lensFocusOut(int target);
    void lensStop();

    // ================= 串口透传通用网关 =================
    // 无论是哪种协议（Pelco-D/VISCA/自定义），也无论是云台还是镜头，
    // 只要是在 T-JSON 架构下透传，统一调用该网关。
    // serialType 对应文档中的 "PELCO_D", "VISCA", "VISCAIR" 等
    void sendTransparentData(const QString& serialType, const QByteArray& data);

private:
    TJsonClient* m_client;
    ConfigManager* m_cfg;
};

#endif // DEVICECONTROLLER_H