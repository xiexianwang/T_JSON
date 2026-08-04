#include "devicemanager.h"
#include "devicecontext.h"
#include "tjsonclient.h"
#include "devicecontroller.h"
#include "configmanager.h"
#include "devicetreewidget.h"
#include "videogridwidget.h"
#include "videowidget.h"
#include "cmdlogdialog.h"
#include <QStandardItemModel>
#include <functional>

DeviceManager::DeviceManager(ConfigManager *cfg, DeviceTreeWidget *deviceTree,
                             VideoGridWidget *videoGrid, CmdLogDialog *cmdLog,
                             QObject *parent)
    : QObject(parent)
    , m_cfg(cfg)
    , m_deviceTree(deviceTree)
    , m_videoGrid(videoGrid)
    , m_cmdLog(cmdLog)
{
}

DeviceContext *DeviceManager::activeDevice() const
{
    return m_activeDeviceIp.isEmpty() ? nullptr : m_devices.value(m_activeDeviceIp, nullptr);
}

TJsonClient *DeviceManager::activeClient() const
{
    auto *d = activeDevice();
    return d ? d->tcpClient : nullptr;
}

DeviceController *DeviceManager::activeCtrl() const
{
    auto *d = activeDevice();
    return d ? d->ctrl : nullptr;
}

bool DeviceManager::isConnected() const
{
    auto *d = activeDevice();
    return d && d->tcpConnected;
}

DeviceContext *DeviceManager::createDeviceContext(const QString &ip, const QString &name)
{
    if (m_devices.contains(ip)) return m_devices.value(ip);
    const int port = 8089;

    auto *ctx = new DeviceContext(ip, m_cfg, this);
    ctx->state.deviceName = name;
    m_devices[ip] = ctx;

    connect(ctx->tcpClient, &TJsonClient::deviceConnected, this, [this, ip]() {
        auto *dc = m_devices.value(ip);
        if (!dc) return;
        dc->tcpConnected = true;
        m_deviceTree->setDeviceConnected(ip, true);
        emit deviceConnected(ip);
    });

    connect(ctx->tcpClient, &TJsonClient::deviceDisconnected, this, [this, ip]() {
        auto *dc = m_devices.value(ip);
        if (!dc) return;
        dc->stopRtsp();
        dc->tcpConnected = false;
        m_deviceTree->setDeviceConnected(ip, false);
        emit deviceDisconnected(ip);
    });

    connect(ctx->tcpClient, &TJsonClient::jsonReceived, this,
        [this, ip](const QJsonObject &doc) {
            emit jsonReceived(ip, doc);
        });

    connect(ctx->tcpClient, &TJsonClient::ackReceived, this,
        [this, ip](quint8 code) {
            if (ip != m_activeDeviceIp) return;
            QString msg;
            switch (code) {
            case 0: msg = QString::fromUtf8("指令执行成功"); break;
            case 1: msg = QString::fromUtf8("指令不完整");   break;
            case 2: msg = QString::fromUtf8("指令内容错误");  break;
            default: msg = QString::fromUtf8("未知状态码: %1").arg(code);
            }
            auto *dc = m_devices.value(ip);
            if (!dc || !dc->tcpClient) return;
            Q_UNUSED(dc);
        });

    connect(ctx->tcpClient, &TJsonClient::imageSnapped, this,
        [](const QByteArray &data, const QRect &rect) {
            Q_UNUSED(data); Q_UNUSED(rect);
        });

    connect(ctx->tcpClient, &TJsonClient::errorOccurred, this,
        [this, ip](const QString &err) {
            if (ip != m_activeDeviceIp) return;
            Q_UNUSED(err);
        });

    connect(ctx->ctrl, &DeviceController::commandSent, m_cmdLog, &CmdLogDialog::appendLog);

    ctx->tcpClient->connectToDevice(ip, port);
    return ctx;
}

int DeviceManager::assignFreeCell(const QString &ip)
{
    if (!m_videoGrid) return -1;
    auto *root = m_deviceTree->model()->invisibleRootItem();
    for (int i = 0; i < root->rowCount(); ++i) {
        auto *devItem = root->child(i);
        if (!devItem || devItem->data(DeviceTreeWidget::RoleIp).toString() != ip) continue;
        for (int j = 0; j < devItem->rowCount(); ++j) {
            auto *chItem = devItem->child(j);
            if (!chItem) continue;
            QString rtspUrl = chItem->data(DeviceTreeWidget::RoleRtspUrl).toString();
            if (rtspUrl.isEmpty()) continue;
            QString chName = chItem->text();
            int idx = -1;
            for (int k = 0; k < m_videoGrid->cellCount(); ++k) {
                if (!m_videoGrid->cellAt(k)->channelLabel().isEmpty()) continue;
                idx = k;
                break;
            }
            if (idx < 0) return -1;
            m_videoGrid->assignChannel(idx, chName, rtspUrl);
            auto *dc = m_devices.value(ip);
            if (dc) {
                disconnect(dc, &DeviceContext::frameReady, nullptr, nullptr);
                connect(dc, &DeviceContext::frameReady, this, [this, idx, ip](const QImage &frame) {
                    auto *vw = m_videoGrid->cellAt(idx);
                    if (vw) vw->setFrame(frame);
                    emit frameReady(ip, frame);
                });
                dc->startRtsp(rtspUrl);
            }
            return idx;
        }
    }
    return -1;
}

void DeviceManager::setupDeviceContext(DeviceContext *ctx, const QString &ip, const QString &name)
{
    if (!ctx || m_devices.contains(ip)) return;
    ctx->state.deviceName = name;
    m_devices[ip] = ctx;

    connect(ctx->tcpClient, &TJsonClient::deviceConnected, this, [this, ip]() {
        auto *dc = m_devices.value(ip);
        if (!dc) return;
        dc->tcpConnected = true;
        m_deviceTree->setDeviceConnected(ip, true);
        emit deviceConnected(ip);
        if (ip == m_activeDeviceIp) {
            dc->ctrl->queryImageParams();
        }
    });

    connect(ctx->tcpClient, &TJsonClient::deviceDisconnected, this, [this, ip]() {
        auto *dc = m_devices.value(ip);
        if (!dc) return;
        dc->tcpConnected = false;
        m_deviceTree->setDeviceConnected(ip, false);
        emit deviceDisconnected(ip);
    });

    connect(ctx->tcpClient, &TJsonClient::jsonReceived, this,
        [this, ip](const QJsonObject &doc) {
            emit jsonReceived(ip, doc);
        });

    connect(ctx->tcpClient, &TJsonClient::ackReceived, this,
        [this, ip](quint8 code) {
            if (ip != m_activeDeviceIp) return;
            QString msg;
            switch (code) {
            case 0: msg = QString::fromUtf8("指令执行成功"); break;
            case 1: msg = QString::fromUtf8("指令不完整");   break;
            case 2: msg = QString::fromUtf8("指令内容错误");  break;
            default: msg = QString::fromUtf8("未知状态码: %1").arg(code);
            }
        });

    connect(ctx->tcpClient, &TJsonClient::imageSnapped, this,
        [](const QByteArray &data, const QRect &rect) {
            Q_UNUSED(data); Q_UNUSED(rect);
        });

    connect(ctx->tcpClient, &TJsonClient::errorOccurred, this,
        [this, ip](const QString &err) {
            if (ip != m_activeDeviceIp) return;
            Q_UNUSED(err);
        });

    connect(ctx->ctrl, &DeviceController::commandSent, m_cmdLog, &CmdLogDialog::appendLog);
    ctx->tcpClient->connectToDevice(ip, 8089);
}

void DeviceManager::connectAllDevices()
{
    const int port = 8089;
    std::function<void(QStandardItem*)> walk = [&](QStandardItem *parent) {
        for (int i = 0; i < parent->rowCount(); ++i) {
            auto *item = parent->child(i);
            if (!item) continue;
            QString ip = item->data(Qt::UserRole + 2).toString();
            if (!ip.isEmpty() && !m_devices.contains(ip)) {
                QString name = item->text().section(' ', 0, 0);
                auto *ctx = new DeviceContext(ip, m_cfg, this);
                setupDeviceContext(ctx, ip, name);
            }
            if (item->hasChildren())
                walk(item);
        }
    };
    walk(m_deviceTree->model()->invisibleRootItem());
}

void DeviceManager::switchActiveDevice(const QString &ip)
{
    if (ip == m_activeDeviceIp) return;
    auto *oldDc = m_devices.value(m_activeDeviceIp);
    if (oldDc) oldDc->stopRtsp();

    m_activeDeviceIp = ip;
    emit activeDeviceChanged(ip);
}

void DeviceManager::disconnectAll()
{
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        auto *dc = it.value();
        dc->tcpClient->disconnectDevice();
        dc->stopRtsp();
    }
}
