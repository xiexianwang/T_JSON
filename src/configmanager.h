// ============================================================
// 文件: configmanager.h
// 描述: 配置管理器模块。通过 QSettings 将云台(PTZ)、镜头(Lens)
//       和相机(Camera)参数持久化到本地存储，并提供运行时的读写接口。
// ============================================================

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QMap>

// PTZ（云台）配置结构体
// 包含地址、Pan/Tilt 速度以及协议类型
struct PtzConfig {
    quint8 address = 1;        // 云台设备地址，默认 1
    quint8 panSpeed = 63;      // 水平旋转速度 (0-63)，默认 63
    quint8 tiltSpeed = 63;     // 垂直俯仰速度 (0-63)，默认 63
    QString protocol = "PELCO_D";  // 通信协议，默认 Pelco-D

    // 从 QSettings 中加载 PTZ 配置
    void load(QSettings& s) {
        address    = static_cast<quint8>(s.value("PtzAddress", 1).toUInt());
        panSpeed   = static_cast<quint8>(s.value("PtzPanSpeed", 63).toUInt());
        tiltSpeed  = static_cast<quint8>(s.value("PtzTiltSpeed", 63).toUInt());
        protocol   = s.value("PtzProtocol", "Pelco-D").toString();
    }

    // 将 PTZ 配置保存到 QSettings
    void save(QSettings& s) const {
        s.setValue("PtzAddress",   address);
        s.setValue("PtzPanSpeed",  panSpeed);
        s.setValue("PtzTiltSpeed", tiltSpeed);
        s.setValue("PtzProtocol",  protocol);
    }
};

// 镜头配置结构体
// 包含变倍/变焦速度、可见光与红外相机各自的设备地址
struct LensConfig {
    quint8 zoomSpeed = 5;
    quint8 visAddress = 1;
    quint8 irAddress = 2;
    QString visProtocol = "VISCA";
    QString irProtocol = "Pelco-D";

    void load(QSettings& s) {
        zoomSpeed   = static_cast<quint8>(s.value("LensZoomSpeed", 5).toUInt());
        visAddress  = static_cast<quint8>(s.value("VisAddress", 1).toUInt());
        irAddress   = static_cast<quint8>(s.value("IrAddress", 2).toUInt());
        visProtocol = s.value("VisProtocol", "VISCA").toString();
        irProtocol  = s.value("IrProtocol", "Pelco-D").toString();
    }

    void save(QSettings& s) const {
        s.setValue("LensZoomSpeed",  zoomSpeed);
        s.setValue("VisAddress", visAddress);
        s.setValue("IrAddress",  irAddress);
        s.setValue("VisProtocol", visProtocol);
        s.setValue("IrProtocol",  irProtocol);
    }
};

// 相机传感器参数配置结构体
// 包含可见光与红外传感器的像素尺寸、分辨率和最短焦距
struct CameraConfig {
    double visPixelSize = 2.92;   // 可见光像元尺寸 (μm)
    double irPixelSize = 12.0;    // 红外像元尺寸 (μm)
    int visResX = 2688;           // 可见光水平分辨率 (px)
    int visResY = 1520;           // 可见光垂直分辨率 (px)
    int irResX = 640;             // 红外水平分辨率 (px)
    int irResY = 512;             // 红外垂直分辨率 (px)
    double visMinFocal = 6.1;     // 可见光最短焦距 (mm)
    double irMinFocal = 25.0;     // 红外最短焦距 (mm)

    // 视觉法参考尺寸表，key = (modelLow << 8) | classCode
    QMap<int, double> targetRefMap;

    double targetRefSize(int modelLow, int classCode) const {
        return targetRefMap.value((modelLow << 8) | classCode, -1.0);
    }

    // 从 QSettings 中加载相机参数
    void load(QSettings& s) {
        visPixelSize = s.value("VisPixelSize", 2.92).toDouble();
        irPixelSize  = s.value("IrPixelSize", 12.0).toDouble();
        visMinFocal  = s.value("VisMinFocal", 6.1).toDouble();
        irMinFocal   = s.value("IrMinFocal", 25.0).toDouble();

        targetRefMap.clear();
        auto loadRef = [&](int ml, int cc, const char* sk, double def) {
            targetRefMap[(ml << 8) | cc] = s.value(sk, def).toDouble();
        };
        loadRef(2, 0xA1, "TargetRef_2_161", 1.7);
        loadRef(2, 0xA2, "TargetRef_2_162", 4.5);
        loadRef(3, 0xA3, "TargetRef_3_163", 10.0);
        loadRef(4, 0xA4, "TargetRef_4_164", 0.35);
        loadRef(5, 0xA1, "TargetRef_5_161", 15.0);
        loadRef(5, 0xA2, "TargetRef_5_162", 10.0);
        loadRef(6, 0xA3, "TargetRef_6_163", 0.3);

        auto parseRes = [](const QString& str, int& x, int& y) {
            QStringList p = str.split('x');
            if (p.size() == 2) { x = p[0].toInt(); y = p[1].toInt(); }
        };
        parseRes(s.value("VisResolution", "2688x1520").toString(), visResX, visResY);
        parseRes(s.value("IrResolution", "640x512").toString(), irResX, irResY);
    }

    void save(QSettings& s) const {
        s.setValue("VisPixelSize", visPixelSize);
        s.setValue("IrPixelSize",  irPixelSize);
        s.setValue("VisMinFocal",  visMinFocal);
        s.setValue("IrMinFocal",   irMinFocal);
        s.setValue("VisResolution", QString("%1x%2").arg(visResX).arg(visResY));
        s.setValue("IrResolution",  QString("%1x%2").arg(irResX).arg(irResY));

        auto saveRef = [&](int ml, int cc, const char* sk) {
            s.setValue(sk, targetRefMap.value((ml << 8) | cc));
        };
        saveRef(2, 0xA1, "TargetRef_2_161");
        saveRef(2, 0xA2, "TargetRef_2_162");
        saveRef(3, 0xA3, "TargetRef_3_163");
        saveRef(4, 0xA4, "TargetRef_4_164");
        saveRef(5, 0xA1, "TargetRef_5_161");
        saveRef(5, 0xA2, "TargetRef_5_162");
        saveRef(6, 0xA3, "TargetRef_6_163");
    }
};

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    enum CloseAction { Ask = 0, Exit = 1, Minimize = 2 };

    explicit ConfigManager(QObject *parent = nullptr);

    void load();                // 从本地存储加载所有配置
    void reload();              // 重新加载配置（委托给 load）
    void save();                // 将所有配置写入本地存储并发射变更信号

    PtzConfig& ptz() { return m_ptz; }       // 获取 PTZ 配置引用
    LensConfig& lens() { return m_lens; }    // 获取镜头配置引用
    CameraConfig& cam() { return m_cam; }    // 获取相机参数配置引用
    QString serialIp() const { return m_serialIp; }        // 串口服务器 IP
    quint16 serialPort() const { return m_serialPort; }    // 串口服务器端口
    void setSerialIp(const QString& ip) { m_serialIp = ip; }
    void setSerialPort(quint16 port) { m_serialPort = port; }

    CloseAction closeAction() const { return m_closeAction; }
    void setCloseAction(CloseAction action) { m_closeAction = action; }

    bool captureUploadEnabled() const { return m_captureUploadEnabled; }
    void setCaptureUploadEnabled(bool enabled) { m_captureUploadEnabled = enabled; }
    bool digitalZoomEnabled() const { return m_digitalZoomEnabled; }
    void setDigitalZoomEnabled(bool enabled) { m_digitalZoomEnabled = enabled; }
    bool autoZoomEnabled() const { return m_autoZoomEnabled; }
    void setAutoZoomEnabled(bool enabled) { m_autoZoomEnabled = enabled; }
    bool posResetEnabled() const { return m_posResetEnabled; }
    void setPosResetEnabled(bool enabled) { m_posResetEnabled = enabled; }

signals:
    void ptzConfigChanged();     // PTZ 配置变更时发射
    void lensConfigChanged();    // 镜头配置变更时发射
    void cameraConfigChanged();  // 相机参数配置变更时发射

private:
    PtzConfig m_ptz;             // PTZ 配置实例
    LensConfig m_lens;           // 镜头配置实例
    CameraConfig m_cam;          // 相机参数配置实例
    QString m_serialIp = "192.168.1.66";   // 串口服务器 IP 地址
    quint16 m_serialPort = 4001;           // 串口服务器端口号
    CloseAction m_closeAction = Ask;
    bool m_captureUploadEnabled = false;
    bool m_digitalZoomEnabled = false;
    bool m_autoZoomEnabled = false;
    bool m_posResetEnabled = false;
};

#endif // CONFIGMANAGER_H
