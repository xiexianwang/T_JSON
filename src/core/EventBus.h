#ifndef EVENTBUS_H
#define EVENTBUS_H

#include <QObject>
#include <QJsonObject>
#include <QByteArray>
#include <QImage>
#include <QRect>
#include <QString>
#include <memory>
#include "DeviceState.h"

// ============================================================================
// EventBus - 全局事件总线
// 基于 Qt 信号槽的跨模块通信枢纽。
// 用于彻底解耦底层通信(TCP/串口/RTSP)与表现层(UI)，支持多设备(DeviceId)。
// ============================================================================
class EventBus : public QObject
{
    Q_OBJECT
public:
    static EventBus* instance();

    // 禁用拷贝与赋值
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    // --- 发布事件的方法 (向总线投递) ---
    void postDeviceConnected(const QString& deviceId, quint64 generation = 0);
    void postDeviceDisconnected(const QString& deviceId, quint64 generation = 0);
    void postAckReceived(const QString& deviceId, quint8 statusCode, quint64 generation = 0);
    void postDeviceReconnecting(const QString& deviceId, int attempt, int maxRetries, quint64 generation = 0);
    void postDeviceReconnectFailed(const QString& deviceId, quint64 generation = 0);
    void postDeviceError(const QString& deviceId, const QString& errorMsg, quint64 generation = 0);
    void postRtspOpened(const QString& deviceId, quint64 generation = 0);
    void postRtspError(const QString& deviceId, const QString& errorMsg, quint64 generation = 0);
    void postJsonReceived(const QString& deviceId, const QJsonObject& doc, quint64 generation = 0);
    void postImageSnapped(const QString& deviceId, const QByteArray& jpegData, const QRect& location, quint64 generation = 0);
    void postPtzUpdated(const QString& deviceId, double pan, double tilt, double zoom, quint64 generation = 0);
    void postDeviceAiTimeout(const QString& deviceId, quint64 generation = 0);
    void postDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state, quint64 generation = 0);
    void postDeviceFrameReady(const QString& deviceId, const QImage& frame, quint64 generation = 0);
    void postDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc, quint64 generation = 0);

signals:
    // --- 订阅事件的信号 (业务层或 UI 层去监听) ---
    void sigDeviceConnected(const QString& deviceId, quint64 generation);
    void sigDeviceDisconnected(const QString& deviceId, quint64 generation);
    void sigAckReceived(const QString& deviceId, quint8 statusCode, quint64 generation);
    void sigDeviceReconnecting(const QString& deviceId, int attempt, int maxRetries, quint64 generation);
    void sigDeviceReconnectFailed(const QString& deviceId, quint64 generation);
    void sigDeviceError(const QString& deviceId, const QString& errorMsg, quint64 generation);
    void sigRtspOpened(const QString& deviceId, quint64 generation);
    void sigRtspError(const QString& deviceId, const QString& errorMsg, quint64 generation);
    void sigJsonReceived(const QString& deviceId, const QJsonObject& doc, quint64 generation);
    void sigImageSnapped(const QString& deviceId, const QByteArray& jpegData, const QRect& location, quint64 generation);
    void sigPtzUpdated(const QString& deviceId, double pan, double tilt, double zoom, quint64 generation);
    void sigDeviceAiTimeout(const QString& deviceId, quint64 generation);
    void sigDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state, quint64 generation);
    void sigDeviceFrameReady(const QString& deviceId, const QImage& frame, quint64 generation);
    void sigDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc, quint64 generation);

private:
    explicit EventBus(QObject *parent = nullptr);
    ~EventBus() override = default;
};

#endif // EVENTBUS_H
