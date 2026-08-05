#ifndef DEVICECONTEXT_H
#define DEVICECONTEXT_H

#include <QObject>
#include <QString>
#include "core/DeviceState.h"
#include "tjsonclient.h"
#include "devicecontroller.h"
#include "rtspthread.h"
#include "ptzforwarder.h"
#include "configmanager.h"

// ============================================================================
// DeviceContext - 设备运行时上下文 (聚合根)
// 代表一个真实物理设备的所有控制模块与状态。
// ============================================================================
class DeviceContext : public QObject
{
    Q_OBJECT
public:
    explicit DeviceContext(const QString& deviceId, ConfigManager* cfg, QObject *parent = nullptr);
    ~DeviceContext() override;

    QString deviceId() const { return m_deviceId; }

    // 启动与停止网络和视频连接
    void startConnection();
    void stopConnection();

    // 模块访问器
    TJsonClient* tcpClient() const { return m_tcp; }
    DeviceController* motorController() const { return m_motor; }
    RtspThread* videoStream() const { return m_video; }
    PtzForwarder* ptzForwarder() const { return m_ptz; }
    DeviceState* state() const { return m_state; }

private:
    QString m_deviceId;
    ConfigManager* m_cfg;

    // 底层驱动与组件
    TJsonClient* m_tcp;
    DeviceController* m_motor;
    RtspThread* m_video;
    PtzForwarder* m_ptz;
    
    // 设备数据状态
    DeviceState* m_state;
};

#endif // DEVICECONTEXT_H
