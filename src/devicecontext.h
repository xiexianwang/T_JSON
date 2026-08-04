#ifndef DEVICECONTEXT_H
#define DEVICECONTEXT_H

#include <QObject>
#include <QString>
#include <QImage>

class TJsonClient;
class DeviceController;
class ConfigManager;
class RtspThread;

class DeviceContext : public QObject
{
    Q_OBJECT
public:
    struct State {
        int workMode = 0;
        int algoModel = 0;
        int displayMode = 0;
        double visZoom = 1.0;
        double irZoom = 1.0;
        int resX = 2688;
        int resY = 1520;
        double lastVisFocal = 0;
        double lastIrFocal = 0;
        double lastPan = 0;
        double lastTilt = 0;
        double lastLat = 0;
        double lastLon = 0;
        double lastAlt = 0;
        double lastLaserRange = 0;
        QString deviceName;
    };

    DeviceContext(const QString &ip, ConfigManager *cfg, QObject *parent = nullptr);
    ~DeviceContext() override;

    void startRtsp(const QString &url);
    void stopRtsp();
    bool isRtspRunning() const;

    QString ip() const { return m_ip; }
    bool tcpConnected;

    TJsonClient *tcpClient;
    DeviceController *ctrl;
    State state;

signals:
    void frameReady(const QImage &frame);
    void tcpConnectedChanged(const QString &ip, bool connected);

private:
    QString m_ip;
    RtspThread *m_rtsp;
};

#endif // DEVICECONTEXT_H
