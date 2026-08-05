#include "DeviceContext.h"
#include "core/EventBus.h"

DeviceContext::DeviceContext(const QString& deviceId, ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_deviceId(deviceId)
    , m_cfg(cfg)
    , m_state(new DeviceState())
{
    // 初始化底层驱动组件
    m_tcp = new TJsonClient(this);
    m_motor = new DeviceController(m_tcp, m_cfg, this);
    m_video = new RtspThread(this);

    // 可以在这里建立 m_tcp 到 EventBus 之前的本地拦截（如果要处理状态更新）
    // 比如：
    // connect(m_tcp, &TJsonClient::jsonReceived, this, [this](const QJsonObject& doc){
    //     // 解析 doc，更新 m_state
    //     // ...
    // });
}

DeviceContext::~DeviceContext()
{
    stopConnection();
    delete m_state;
}

void DeviceContext::startConnection()
{
    if (m_cfg) {
        m_tcp->connectToDevice(m_cfg->deviceIp(), m_cfg->devicePort());
        // 这里的 RTSP url 获取逻辑依赖于 MainWindow，后续可通过 Config 统一获取
    }
}

void DeviceContext::stopConnection()
{
    if (m_tcp->isConnected()) {
        m_tcp->disconnectFromDevice();
    }
    m_video->stop();
    m_motor->closeMotorSerial();
}
