#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>
#include <QWebEngineView>
#include <QWebChannel>
#include "mapbridge.h"

class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

    void setDevicePosition(double lat, double lon);
    void setDeviceFov(double lat, double lon, double panDeg, double tiltDeg,
                      double hfovDeg, double vfovDeg, double rangeM);
    void clearFov();
    void appendTrackPoint(const QString& trackId, double lat, double lon);
    void clearTrack(const QString& trackId);
    void clearAllTracks();
    void updateTargetMarkers(const QJsonArray& targets);

signals:
    void mapClicked(double lat, double lon);

private:
    void runJS(const QString& js);
    void flushPending();

    QWebEngineView *m_webView;
    QWebChannel *m_channel;
    MapBridge *m_bridge;
    bool m_mapReady = false;
    QStringList m_pendingJs;
};

#endif
