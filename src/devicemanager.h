#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QMap>
#include <QImage>

class DeviceContext;
class TJsonClient;
class DeviceController;
class ConfigManager;
class DeviceTreeWidget;
class VideoGridWidget;
class CmdLogDialog;
class QJsonObject;
class QStandardItem;

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    DeviceManager(ConfigManager *cfg, DeviceTreeWidget *deviceTree,
                  VideoGridWidget *videoGrid, CmdLogDialog *cmdLog,
                  QObject *parent = nullptr);

    DeviceContext *activeDevice() const;
    TJsonClient *activeClient() const;
    DeviceController *activeCtrl() const;
    QString activeDeviceIp() const { return m_activeDeviceIp; }
    bool isConnected() const;
    void switchActiveDevice(const QString &ip);
    void connectAllDevices();
    DeviceContext *device(const QString &ip) const { return m_devices.value(ip); }
    void disconnectAll();
    void setupDeviceContext(DeviceContext *ctx, const QString &ip, const QString &name);

signals:
    void jsonReceived(const QString &ip, const QJsonObject &doc);
    void activeDeviceChanged(const QString &ip);
    void deviceConnected(const QString &ip);
    void deviceDisconnected(const QString &ip);
    void frameReady(const QString &ip, const QImage &frame);

private:
    DeviceContext *createDeviceContext(const QString &ip, const QString &name);
    int assignFreeCell(const QString &ip);

    QMap<QString, DeviceContext*> m_devices;
    QString m_activeDeviceIp;
    ConfigManager *m_cfg;
    DeviceTreeWidget *m_deviceTree;
    VideoGridWidget *m_videoGrid;
    CmdLogDialog *m_cmdLog;
};

#endif // DEVICEMANAGER_H
