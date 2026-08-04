#include "devicecontext.h"
#include "tjsonclient.h"
#include "devicecontroller.h"
#include "configmanager.h"
#include "rtspthread.h"

DeviceContext::DeviceContext(const QString &ip, ConfigManager *cfg, QObject *parent)
    : QObject(parent)
    , tcpConnected(false)
    , m_ip(ip)
    , m_rtsp(nullptr)
{
    tcpClient = new TJsonClient(this);
    ctrl = new DeviceController(tcpClient, cfg, this);
}

DeviceContext::~DeviceContext()
{
    stopRtsp();
}

void DeviceContext::startRtsp(const QString &url)
{
    if (m_rtsp) {
        if (m_rtsp->isRunning())
            return;
        delete m_rtsp;
    }
    m_rtsp = new RtspThread(this);
    connect(m_rtsp, &RtspThread::frameReady, this, &DeviceContext::frameReady);
    m_rtsp->openStream(url);
}

void DeviceContext::stopRtsp()
{
    if (m_rtsp) {
        m_rtsp->closeStream();
        m_rtsp->wait(3000);
        delete m_rtsp;
        m_rtsp = nullptr;
    }
}

bool DeviceContext::isRtspRunning() const
{
    return m_rtsp && m_rtsp->isRunning();
}
