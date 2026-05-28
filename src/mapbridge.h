#ifndef MAPBRIDGE_H
#define MAPBRIDGE_H

#include <QObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class MapBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool mapReady READ mapReady NOTIFY mapReadyChanged)
public:
    explicit MapBridge(QObject *parent = nullptr);

    bool mapReady() const { return m_ready; }

public slots:
    void onMapInitialized();
    void onMapClick(double lat, double lon);

signals:
    void mapReadyChanged();
    void mapClicked(double lat, double lon);
    void jsDevicePos(double lat, double lon);
    void jsDeviceFov(double lat, double lon, double pan, double tilt, double hfov, double vfov, double range);
    void jsClearFov();
    void jsAddTrackPoint(const QString& trackId, double lat, double lon);
    void jsClearTrack(const QString& trackId);
    void jsClearAllTracks();
    void jsUpdateTargets(const QString& jsonStr);
    void jsSetMapType(int type);
    void jsSetZoom(int level);
    void jsDeviceInfo(double lat, double lon, double alt, double pan, double tilt, double hfov, double vfov, double range);

private:
    bool m_ready = false;
};

#endif
