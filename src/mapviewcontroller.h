#ifndef MAPVIEWCONTROLLER_H
#define MAPVIEWCONTROLLER_H

#include <QObject>
#include <QPoint>
#include <QImage>
#include <QJsonObject>

class QWidget;
class QDialog;
class QPushButton;
class MapWidget;
class VideoGridWidget;
class VideoWidget;
class DeviceManager;
class TrackManager;
class ConfigManager;
struct CameraConfig;

class MapViewController : public QObject
{
    Q_OBJECT
public:
    MapViewController(QWidget *displayContainer,
                      VideoGridWidget *videoGrid,
                      QWidget *drawerPanel,
                      QPushButton *drawerToggleBtn,
                      DeviceManager *devMgr,
                      TrackManager *trackMgr,
                      ConfigManager *cfg,
                      QObject *parent = nullptr);

    void toggleVisibility();
    void toggleMode();
    void updateLayout();
    void updateDevicePosition(const QJsonObject &doc,
                              double visZoom, double irZoom,
                              double resX, double resY,
                              const CameraConfig &cam,
                              double aiDist, bool aiDistEstimated);
    void updateTargets(const QJsonObject &doc, int workMode,
                       double devLat, double devLon,
                       double pan, double tilt,
                       double px, double focal,
                       int halfW, int halfH, int algoModel,
                       double visZoom, double irZoom, int pipShow,
                       const CameraConfig &cam);
    void pipSetFrame(const QImage &frame);

    QDialog *pipDialog() const { return m_pipDialog; }
    VideoWidget *pipVideo() const { return m_pipVideo; }
    MapWidget *mapWidget() const { return m_mapWidget; }

signals:
    void visibilityChanged(bool visible);
    void modeChanged(bool expanded);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    MapWidget *m_mapWidget;
    QWidget *m_mapContainer;
    QWidget *m_mapOverlay;
    void createMapUi(QWidget *displayContainer);

    QDialog *m_pipDialog;
    QWidget *m_pipTitle;
    VideoWidget *m_pipVideo;
    QPoint m_pipPos{10, 10};
    QPoint m_pipDragStart;
    bool m_pipShown = false;
    void createPipUi();

    bool m_mapVisible = false;
    bool m_mapExpanded = false;
    QPoint m_miniMapPos{10, 10};
    bool m_dragging = false;
    QPoint m_dragStart;

    VideoGridWidget *m_videoGrid;
    QWidget *m_drawerPanel;
    QPushButton *m_drawerToggleBtn;
    DeviceManager *m_devMgr;
    TrackManager *m_trackMgr;
    ConfigManager *m_cfg;
};

#endif
