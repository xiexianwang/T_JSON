#include "MainPresenter.h"
#include "mainwindow.h"
#include "service/DeviceManager.h"
#include "core/EventBus.h"

MainPresenter::MainPresenter(MainWindow* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_cfg(cfg)
    , m_currentDeviceId("default_device")
{
    // 1. 初始化全局设备管理器
    DeviceManager::instance()->init(m_cfg);
    
    // 2. 预先创建一个默认设备（兼容旧版单设备架构）
    DeviceManager::instance()->addDevice(m_currentDeviceId);
    
    // 3. 挂载事件总线
    setupEventBus();
}

MainPresenter::~MainPresenter()
{
}

void MainPresenter::setupEventBus()
{
    EventBus* bus = EventBus::instance();
    
    // -- 连接事件 --
    // （注意：为了避免目前 MainWindow 还没完全解耦时的编译错误，我们暂时还是保留原有连接，
    //   这里只是铺垫，等 MainWindow 内的代码被剥离后，将会在这里回调 view 的方法）
    
    // connect(bus, &EventBus::sigDeviceConnected, this, [this](const QString& deviceId) {
    //     if (deviceId == m_currentDeviceId) m_view->onDeviceConnected();
    // });
}

void MainPresenter::connectToDevice(const QString& ip, quint16 port)
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        ctx->startConnection(ip, port);
    }
}

void MainPresenter::disconnectDevice()
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        ctx->stopConnection();
    }
}

void MainPresenter::ptzMove(int direction)
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        if (ctx->motorController()) {
            ctx->motorController()->ptzMove(static_cast<PtzDir>(direction));
        }
    }
}

void MainPresenter::ptzStop()
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        if (ctx->motorController()) {
            ctx->motorController()->ptzStop();
        }
    }
}

DeviceController* MainPresenter::motorController() const
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        return ctx->motorController();
    }
    return nullptr;
}

TJsonClient* MainPresenter::tcpClient() const
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        return ctx->tcpClient();
    }
    return nullptr;
}

RtspThread* MainPresenter::videoStream() const
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        return ctx->videoStream();
    }
    return nullptr;
}

PtzForwarder* MainPresenter::ptzForwarder() const
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        return ctx->ptzForwarder();
    }
    return nullptr;
}

