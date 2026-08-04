#ifndef TRACKMANAGER_H
#define TRACKMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>

struct CameraConfig;
struct AiInfoData;

class TrackManager : public QObject
{
    Q_OBJECT
public:
    struct TrackState {
        QString id;
        double lat = 0, lon = 0;
        int cls = 0;
        QDateTime lostSince;
        double prevLat = 0, prevLon = 0;
        QDateTime prevTime;
        double plotLat = 0, plotLon = 0;
        double plotHeading = -1;
        QDateTime plotTime;
    };

    explicit TrackManager(QObject *parent = nullptr);

    void reset();

    struct UpdateResult {
        bool hasLock = false;
        bool hasLost = false;
        bool cleared = false;
        bool targetChanged = false;
        QString id;
        int cls = 0;
        double lat = 0, lon = 0;
        double dist = 0;
        double speed = 0;
        bool shouldPlot = false;
        double plotBearing = -1;
    };

    UpdateResult processAiFrame(const AiInfoData& ai, int workMode,
                                 double devLat, double devLon, double pan, double tilt,
                                 double pixelSize, double focal,
                                 int halfW, int halfH,
                                 const CameraConfig& cam, int algoModel,
                                 double visZoom, double irZoom, int pipShow);

    TrackState& state() { return m_track; }
    double lastAiDist() const { return m_lastAiDist; }
    bool lastAiDistEstimated() const { return m_lastAiDistEstimated; }

signals:
    void stateChanged();

private:
    double calcVisualDistance(const QString& rawDist, int cls,
                               int boxPx, int algoModel,
                               double visZoom, double irZoom, int pipShow,
                               const CameraConfig& cam,
                               bool* estimated = nullptr);

    TrackState m_track;
    double m_lastAiDist = 0;
    bool m_lastAiDistEstimated = false;
};

#endif // TRACKMANAGER_H
