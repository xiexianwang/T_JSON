#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>
#include <QJsonArray>
#include <QVector>
#include "mapbridge.h"

class QWebChannel;
class QTimer;

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
    void setVisFov(double lat, double lon, double panDeg, double tiltDeg,
                   double hfovDeg, double vfovDeg, double rangeM);
    void setIrFov(double lat, double lon, double panDeg, double tiltDeg,
                  double hfovDeg, double vfovDeg, double rangeM);
    void clearFov();
    void setDeviceInfo(double lat, double lon, double alt, double pan,
                       double tilt, double hfov, double vfov, double range);
    void setDeviceInfoVisible(bool visible);
    void appendTrackPoint(const QString& trackId, double lat, double lon, double speed = 0);
    void clearTrack(const QString& trackId);
    void clearAllTracks();
    void updateTargetMarkers(const QJsonArray& targets);
    void setMapType(int type);
    void setZoom(int level);
    void reloadMap();

signals:
    void mapClicked(double lat, double lon);
    void mapZoomChanged(int zoom);

private:
    void runJS(const QString& js);
    void flushAiData();

    static const int DIRTY_TARGETS = 1 << 0;
    static const int DIRTY_TRACKS  = 1 << 1;
    static const int DIRTY_TRK_CLR = 1 << 2;

    int m_dirtyFlags = 0;
    QTimer *m_aiFlushTimer;
    QJsonArray m_cacheTargets;
    struct TrackPt { QString id; double lat, lon, speed; };
    QVector<TrackPt> m_cacheTrackPts;

    Ui::MapWidget *ui;
    QWebChannel *m_channel;
    MapBridge *m_bridge;
};

#endif
