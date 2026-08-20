#include "PresenterMediaService.h"

#include <QMessageBox>

#include "IMainView.h"
#include "service/DeviceContext.h"

PresenterMediaService::PresenterMediaService(IMainView* view, QObject* parent)
    : QObject(parent), m_view(view)
{
}

void PresenterMediaService::connectStream(DeviceContext* device, const QString& url)
{
    if (!device || !m_view) return;
    const QString trimmedUrl = url.trimmed();
    if (trimmedUrl.isEmpty()) {
        QMessageBox::warning(m_view->asWidget(), "RTSP", "请输入 RTSP 地址");
        return;
    }
    device->startVideo(trimmedUrl);
    m_view->setVideoConnectButton(QString::fromUtf8("连接中..."), false);
    m_view->showStatusMessage(QString::fromUtf8("正在连接 RTSP 视频流..."));
}

void PresenterMediaService::disconnectStream(DeviceContext* device)
{
    if (!device || !m_view) return;
    device->stopVideo();
    m_view->repaintVideoGrid();
    m_view->setVideoConnectButton(QString::fromUtf8("开启"), true);
    m_view->showStatusMessage(QString::fromUtf8("视频已断开"), 3000);
}

void PresenterMediaService::startStream(DeviceContext* device, const QString& url)
{
    if (device) device->startVideo(url);
}

void PresenterMediaService::closeStream(DeviceContext* device)
{
    if (device) device->stopVideo();
}

bool PresenterMediaService::isStreamRunning(const DeviceContext* device) const
{
    return device && device->isVideoRunning();
}
