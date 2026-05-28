#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>
#include <QJsonArray>
#include "mapbridge.h"

class QWebChannel;

namespace Ui {
class MapWidget;
}

class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);
    ~MapWidget();

    void setDevicePosition(double lat, double lon);
    void setDeviceFov(double lat, double lon, double panDeg, double tiltDeg,
                      double hfovDeg, double vfovDeg, double rangeM);
    void clearFov();
    void setDeviceInfo(double lat, double lon, double alt, double pan, double tilt, double hfov, double vfov, double range);
    void appendTrackPoint(const QString& trackId, double lat, double lon);
    void clearTrack(const QString& trackId);
    void clearAllTracks();
    void updateTargetMarkers(const QJsonArray& targets);
    void setMapType(int type);
    void setZoom(int level);

signals:
    void mapClicked(double lat, double lon);

private:
    void runJS(const QString& js);

    Ui::MapWidget *ui;
    QWebChannel *m_channel;
    MapBridge *m_bridge;
};

#endif
