#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>

struct PtzConfig {
    quint8 address = 1;
    quint8 panSpeed = 63;
    quint8 tiltSpeed = 63;
    QString protocol = "PELCO_D";

    void load(QSettings& s) {
        address    = static_cast<quint8>(s.value("PtzAddress", 1).toUInt());
        panSpeed   = static_cast<quint8>(s.value("PtzPanSpeed", 63).toUInt());
        tiltSpeed  = static_cast<quint8>(s.value("PtzTiltSpeed", 63).toUInt());
        protocol   = s.value("PtzProtocol", "Pelco-D").toString();
    }

    void save(QSettings& s) const {
        s.setValue("PtzAddress",   address);
        s.setValue("PtzPanSpeed",  panSpeed);
        s.setValue("PtzTiltSpeed", tiltSpeed);
        s.setValue("PtzProtocol",  protocol);
    }
};

struct LensConfig {
    quint8 zoomSpeed = 5;
    quint8 focusSpeed = 5;
    quint8 visAddress = 1;
    quint8 irAddress = 2;

    void load(QSettings& s) {
        zoomSpeed  = static_cast<quint8>(s.value("LensZoomSpeed", 5).toUInt());
        focusSpeed = static_cast<quint8>(s.value("LensFocusSpeed", 5).toUInt());
        visAddress = static_cast<quint8>(s.value("VisAddress", 1).toUInt());
        irAddress  = static_cast<quint8>(s.value("IrAddress", 2).toUInt());
    }

    void save(QSettings& s) const {
        s.setValue("LensZoomSpeed",  zoomSpeed);
        s.setValue("LensFocusSpeed", focusSpeed);
        s.setValue("VisAddress", visAddress);
        s.setValue("IrAddress",  irAddress);
    }
};

struct CameraConfig {
    double visPixelSize = 2.9;
    double irPixelSize = 12.0;
    int visResX = 2688;
    int visResY = 1520;
    int irResX = 640;
    int irResY = 512;
    double visMinFocal = 6.0;
    double irMinFocal = 25.0;

    void load(QSettings& s) {
        visPixelSize = s.value("VisPixelSize", 2.9).toDouble();
        irPixelSize  = s.value("IrPixelSize", 12.0).toDouble();
        visMinFocal  = s.value("VisMinFocal", 6.0).toDouble();
        irMinFocal   = s.value("IrMinFocal", 25.0).toDouble();

        auto parseRes = [](const QString& str, int& x, int& y) {
            QStringList p = str.split('x');
            if (p.size() == 2) { x = p[0].toInt(); y = p[1].toInt(); }
        };
        parseRes(s.value("VisResolution", "2688x1520").toString(), visResX, visResY);
        parseRes(s.value("IrResolution", "640x512").toString(), irResX, irResY);
    }
};

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);

    void load();
    void reload();
    void save();

    PtzConfig& ptz() { return m_ptz; }
    LensConfig& lens() { return m_lens; }
    CameraConfig& cam() { return m_cam; }

signals:
    void ptzConfigChanged();
    void lensConfigChanged();

private:
    PtzConfig m_ptz;
    LensConfig m_lens;
    CameraConfig m_cam;
};

#endif // CONFIGMANAGER_H
