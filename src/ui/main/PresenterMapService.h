#ifndef PRESENTERMAPSERVICE_H
#define PRESENTERMAPSERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QHash>
#include <QDateTime>
#include "core/DeviceState.h"
#include "core/DeviceConfig.h"

class IMainView;
class ConfigManager;

class PresenterMapService : public QObject
{
    Q_OBJECT
public:
    explicit PresenterMapService(IMainView* view, ConfigManager* cfg, QObject *parent = nullptr);

    void updateAiInfo(const QString& deviceId, const QJsonObject& doc,
                      const DeviceState& state);
    void updateDevicePosition(const QString& deviceId, const DeviceState& state);
    double calculateVisualDistance(const QString& deviceId, const QJsonObject& obj,
                                   int cls, const DeviceState& state,
                                   bool updateTrackLabel);

    void resetDevice(const QString& deviceId);

private:
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

    struct DeviceMapState {
        TrackState track;
        double lastAiDist = 0;
        bool lastAiDistEstimated = false;
    };

    DeviceMapState& mapState(const QString& deviceId);
    const DeviceConfig& deviceCam(const QString& deviceId) const;
    IMainView* m_view;
    ConfigManager* m_cfg;
    QHash<QString, DeviceMapState> m_deviceStates;
};

#endif // PRESENTERMAPSERVICE_H
