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
    void postDeviceConnected(const QString& deviceId);
    void postDeviceDisconnected(const QString& deviceId);
    void postDeviceError(const QString& deviceId, const QString& errorMsg);
    void postJsonReceived(const QString& deviceId, const QJsonObject& doc);
    void postImageSnapped(const QString& deviceId, const QByteArray& jpegData, const QRect& location);
    void postPtzUpdated(const QString& deviceId, double pan, double tilt, double zoom);
    void postDeviceAiTimeout(const QString& deviceId);
    void postDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state);
    void postDeviceFrameReady(const QString& deviceId, const QImage& frame);
    void postDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc);

signals:
    // --- 订阅事件的信号 (业务层或 UI 层去监听) ---
    void sigDeviceConnected(const QString& deviceId);
    void sigDeviceDisconnected(const QString& deviceId);
    void sigDeviceError(const QString& deviceId, const QString& errorMsg);
    void sigJsonReceived(const QString& deviceId, const QJsonObject& doc);
    void sigImageSnapped(const QString& deviceId, const QByteArray& jpegData, const QRect& location);
    void sigPtzUpdated(const QString& deviceId, double pan, double tilt, double zoom);
    void sigDeviceAiTimeout(const QString& deviceId);
    void sigDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state);
    void sigDeviceFrameReady(const QString& deviceId, const QImage& frame);
    void sigDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc);

private:
    explicit EventBus(QObject *parent = nullptr);
    ~EventBus() override = default;
};

#endif // EVENTBUS_H
