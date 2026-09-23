#ifndef DEVICECONFIG_H
#define DEVICECONFIG_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

struct DeviceConfig {
    // ================= 电机配置 =================
    bool motorSerialEnabled = false;
    bool motorIpEnabled = true;
    QString motorProtocol = "Pelco-D";
    QString motorCommandChannel = "Pelco-D";
    QString motorComPort = "COM1";
    QString motorTcpIp = "192.168.1.55";
    quint16 motorTcpPort = 5000;

    // ================= 转台配置 =================
    bool serialServerEnabled = true;
    bool turntableIpEnabled = true;
    QString serialIp = "192.168.1.66";
    quint16 serialPort = 4001;
    quint16 mockServerPort = 5001;
    double ptzPanOffset = 0.0;
    double ptzTiltOffset = 0.0;
    bool softwarePtzCalibrationEnabled = false;

    // ================= 云台(PTZ)配置 =================
    QString ptzProtocol = "Pelco-D";
    quint8 ptzAddress = 1;
    quint8 panSpeed = 63;      // 水平旋转速度 (0-63)
    quint8 tiltSpeed = 63;     // 垂直俯仰速度 (0-63)

    // ================= 镜头配置 =================
    quint8 zoomSpeed = 5;
    quint8 visAddress = 1;     // 可见光设备地址
    quint8 irAddress = 2;      // 红外设备地址
    QString visProtocol = "VISCA";
    QString irProtocol = "Pelco-D";

    // ================= 附加功能开关 =================
    bool digitalZoom = false;    // 数字变倍
    bool autoZoom = false;       // 自动变倍
    bool captureUpload = false;  // 抓拍上传
    bool posReset = false;       // 位置复位

    // ================= 相机光学参数 =================
    double visPixelSize = 2.92;   // 可见光像元尺寸 (μm)
    double irPixelSize = 12.0;    // 红外像元尺寸 (μm)
    int visResX = 2688;           // 可见光水平分辨率 (px)
    int visResY = 1520;           // 可见光垂直分辨率 (px)
    int irResX = 640;             // 红外水平分辨率 (px)
    int irResY = 512;             // 红外垂直分辨率 (px)
    double visMinFocal = 6.1;     // 可见光最短焦距 (mm)
    double irMinFocal = 25.0;     // 红外最短焦距 (mm)

    // 视觉法参考尺寸表，key = (modelLow << 8) | classCode；单位 m。
    // 为空表示未显式配置，targetRefSize 会回退到 defaultTargetRefSize。
    QMap<int, double> targetRefMap;

    // 各类目标的典型参考尺寸（特征长/高/翼展，单位 m）。
    // key 与 targetRefMap 一致：低段模型 × Class 码。
    static double defaultTargetRefSize(int modelLow, int classCode) {
        switch ((modelLow << 8) | classCode) {
        case (2 << 8) | 0xA1: return 1.7;    // 人：身高
        case (2 << 8) | 0xA2: return 4.5;    // 车：车长
        case (3 << 8) | 0xA3: return 15.0;   // 船：船长
        case (4 << 8) | 0xA4: return 0.5;    // 无人机：轴距/翼展
        case (5 << 8) | 0xA1: return 30.0;   // 飞机：机身长
        case (5 << 8) | 0xA2: return 12.0;   // 直升机：机身长
        case (6 << 8) | 0xA3: return 0.5;    // 鸟：翼展
        default: return -1.0;
        }
    }

    // 取有效参考尺寸：显式配置值（> 0.1m）优先，否则回退默认；无对应类型返回 -1。
    double targetRefSize(int modelLow, int classCode) const {
        const double v = targetRefMap.value((modelLow << 8) | classCode, -1.0);
        return v > 0.1 ? v : defaultTargetRefSize(modelLow, classCode);
    }

    // ================= JSON 序列化 =================
    QJsonObject toJson() const {
        QJsonObject obj{
            {"motorSerialEnabled", motorSerialEnabled},
            {"motorIpEnabled", motorIpEnabled},
            {"motorProtocol", motorProtocol},
            {"motorCommandChannel", motorCommandChannel},
            {"motorComPort", motorComPort},
            {"motorTcpIp", motorTcpIp},
            {"motorTcpPort", motorTcpPort},
            {"serialServerEnabled", serialServerEnabled},
            {"turntableIpEnabled", turntableIpEnabled},
            {"serialIp", serialIp},
            {"serialPort", serialPort},
            {"mockServerPort", mockServerPort},
            {"ptzPanOffset", ptzPanOffset},
            {"ptzTiltOffset", ptzTiltOffset},
            {"softwarePtzCalibrationEnabled", softwarePtzCalibrationEnabled},
            {"ptzProtocol", ptzProtocol},
            {"ptzAddress", ptzAddress},
            {"panSpeed", panSpeed},
            {"tiltSpeed", tiltSpeed},
            {"zoomSpeed", zoomSpeed},
            {"visAddress", visAddress},
            {"irAddress", irAddress},
            {"visProtocol", visProtocol},
            {"irProtocol", irProtocol},
            {"digitalZoom", digitalZoom},
            {"autoZoom", autoZoom},
            {"captureUpload", captureUpload},
            {"posReset", posReset},
            {"visPixelSize", visPixelSize},
            {"irPixelSize", irPixelSize},
            {"visMinFocal", visMinFocal},
            {"irMinFocal", irMinFocal},
            {"visResX", visResX},
            {"visResY", visResY},
            {"irResX", irResX},
            {"irResY", irResY}
        };

        QJsonObject refs;
        for (auto it = targetRefMap.constBegin(); it != targetRefMap.constEnd(); ++it)
            refs[QString::number(it.key())] = it.value();
        obj["targetRefMap"] = refs;
        return obj;
    }

    static DeviceConfig fromJson(const QJsonObject& obj) {
        DeviceConfig c;
        if (obj.contains("motorSerialEnabled")) c.motorSerialEnabled = obj["motorSerialEnabled"].toBool();
        if (obj.contains("motorIpEnabled")) c.motorIpEnabled = obj["motorIpEnabled"].toBool();
        if (obj.contains("motorProtocol")) c.motorProtocol = obj["motorProtocol"].toString();
        if (obj.contains("motorCommandChannel")) c.motorCommandChannel = obj["motorCommandChannel"].toString();
        if (obj.contains("motorComPort")) c.motorComPort = obj["motorComPort"].toString();
        if (obj.contains("motorTcpIp")) c.motorTcpIp = obj["motorTcpIp"].toString();
        if (obj.contains("motorTcpPort")) c.motorTcpPort = static_cast<quint16>(obj["motorTcpPort"].toInt());
        if (obj.contains("serialServerEnabled")) c.serialServerEnabled = obj["serialServerEnabled"].toBool();
        if (obj.contains("turntableIpEnabled")) c.turntableIpEnabled = obj["turntableIpEnabled"].toBool();
        if (obj.contains("serialIp")) c.serialIp = obj["serialIp"].toString();
        if (obj.contains("serialPort")) c.serialPort = static_cast<quint16>(obj["serialPort"].toInt());
        if (obj.contains("mockServerPort")) c.mockServerPort = static_cast<quint16>(obj["mockServerPort"].toInt());
        if (obj.contains("ptzPanOffset")) c.ptzPanOffset = obj["ptzPanOffset"].toDouble();
        if (obj.contains("ptzTiltOffset")) c.ptzTiltOffset = obj["ptzTiltOffset"].toDouble();
        if (obj.contains("softwarePtzCalibrationEnabled")) c.softwarePtzCalibrationEnabled = obj["softwarePtzCalibrationEnabled"].toBool();

        if (obj.contains("ptzProtocol")) c.ptzProtocol = obj["ptzProtocol"].toString();
        if (obj.contains("ptzAddress")) c.ptzAddress = static_cast<quint8>(obj["ptzAddress"].toInt());
        if (obj.contains("panSpeed")) c.panSpeed = static_cast<quint8>(obj["panSpeed"].toInt());
        if (obj.contains("tiltSpeed")) c.tiltSpeed = static_cast<quint8>(obj["tiltSpeed"].toInt());
        if (obj.contains("zoomSpeed")) c.zoomSpeed = static_cast<quint8>(obj["zoomSpeed"].toInt());
        if (obj.contains("visAddress")) c.visAddress = static_cast<quint8>(obj["visAddress"].toInt());
        if (obj.contains("irAddress")) c.irAddress = static_cast<quint8>(obj["irAddress"].toInt());
        if (obj.contains("visProtocol")) c.visProtocol = obj["visProtocol"].toString();
        if (obj.contains("irProtocol")) c.irProtocol = obj["irProtocol"].toString();
        if (obj.contains("digitalZoom")) c.digitalZoom = obj["digitalZoom"].toBool();
        if (obj.contains("autoZoom")) c.autoZoom = obj["autoZoom"].toBool();
        if (obj.contains("captureUpload")) c.captureUpload = obj["captureUpload"].toBool();
        if (obj.contains("posReset")) c.posReset = obj["posReset"].toBool();

        if (obj.contains("visPixelSize")) c.visPixelSize = obj["visPixelSize"].toDouble();
        if (obj.contains("irPixelSize")) c.irPixelSize = obj["irPixelSize"].toDouble();
        if (obj.contains("visMinFocal")) c.visMinFocal = obj["visMinFocal"].toDouble();
        if (obj.contains("irMinFocal")) c.irMinFocal = obj["irMinFocal"].toDouble();
        if (obj.contains("visResX")) c.visResX = obj["visResX"].toInt();
        if (obj.contains("visResY")) c.visResY = obj["visResY"].toInt();
        if (obj.contains("irResX")) c.irResX = obj["irResX"].toInt();
        if (obj.contains("irResY")) c.irResY = obj["irResY"].toInt();

        if (obj.contains("targetRefMap")) {
            const QJsonObject refs = obj["targetRefMap"].toObject();
            for (auto it = refs.constBegin(); it != refs.constEnd(); ++it)
                c.targetRefMap[it.key().toInt()] = it.value().toDouble();
        }
        return c;
    }
};

#endif // DEVICECONFIG_H
