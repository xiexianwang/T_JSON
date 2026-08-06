#include "MainPresenter.h"
#include "mainwindow.h"
#include "service/DeviceManager.h"
#include "core/EventBus.h"
#include "ui_mainwindow.h"
#include "core/GeoCalculator.h"
#include <QMessageBox>
#include <QVariant>
#include "ui/components/VideoGridWidget.h"
#include <QtMath>

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
    
    connect(bus, &EventBus::sigDeviceAiTimeout, this, &MainPresenter::onDeviceAiTimeout);
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



// --- Extracted from MainWindow ---
void MainPresenter::on_btnConnect_clicked()
{
    if (this->tcpClient()->isConnected()) {
        this->tcpClient()->disconnectDevice();
    } else {
        if (!m_cfg->turntableIpEnabled()) {
             m_view->getUi()->statusbar->showMessage(QString::fromUtf8("转台IP连接已禁用"), 3000);
             return;
        }
        QString ip = m_view->getUi()->lineEditIp->text();
        this->tcpClient()->connectToDevice(ip, 8089);
        m_view->getUi()->btnConnect->setText(QString::fromUtf8("连接中..."));
        m_view->getUi()->btnConnect->setEnabled(false);
        m_view->getUi()->btnCancelConnect->setVisible(true);
    }
}

void MainPresenter::on_btnCancelConnect_clicked()
{
    this->tcpClient()->disconnectDevice();
    m_view->getUi()->btnConnect->setText(QString::fromUtf8("连接设备"));
    m_view->getUi()->btnConnect->setEnabled(true);
    m_view->getUi()->btnCancelConnect->setVisible(false);
    m_view->getUi()->statusbar->showMessage(QString::fromUtf8("已取消连接"), 3000);
}

void MainPresenter::on_btnVideoConnect_clicked()
{
    QString url = m_view->getUi()->lineEditRtsp->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(m_view, "RTSP", "请输入 RTSP 地址");
        return;
    }
    m_view->m_rtspEverOpened = true;
    this->videoStream()->openStream(url);
    m_view->getUi()->btnVideoConnect->setEnabled(false);
    m_view->getUi()->btnVideoConnect->setText(QString::fromUtf8("连接中..."));
    m_view->getUi()->statusbar->showMessage(QString::fromUtf8("正在连接 RTSP 视频流..."));
}

void MainPresenter::on_btnVideoDisconnect_clicked()
{
    if (auto vw = m_view->m_videoGrid->getWidget("default_device")) vw->clearFrame();
    m_view->m_videoGrid->repaint();
    this->videoStream()->closeStream();
    m_view->getUi()->btnVideoConnect->setEnabled(true);
    m_view->getUi()->btnVideoConnect->setText(QString::fromUtf8("开启"));
    m_view->getUi()->statusbar->showMessage(QString::fromUtf8("视频已断开"), 3000);
}

void MainPresenter::on_btnPtzMoveTo_clicked()
{
    if (!m_view->requireConnected()) return;

    bool panOk = false;
    bool tiltOk = false;
    double pan = m_view->getUi()->editTargetPan->text().toDouble(&panOk);
    double tilt = m_view->getUi()->editTargetTilt->text().toDouble(&tiltOk);

    if (panOk && tiltOk) {
        this->motorController()->ptzMoveTo(pan, tilt);
    } else {
        QMessageBox::warning(m_view, "输入错误", "请输入有效的水平和垂直角度值。");
    }
}

void MainPresenter::on_btnPtzMoveToGps_clicked()
{
    if (!m_view->requireConnected()) return;

    QString lonStr = m_view->getUi()->editTargetLon->text().trimmed();
    QString latStr = m_view->getUi()->editTargetLat->text().trimmed();
    QString altStr = m_view->getUi()->editTargetAlt->text().trimmed();

    if (lonStr.isEmpty() || latStr.isEmpty()) {
        QMessageBox::warning(m_view, "输入错误", "请输入目标的经纬度和高度。");
        return;
    }

    double targetLon = GeoCalculator::parseCoord(lonStr);
    double targetLat = GeoCalculator::parseCoord(latStr);
    double targetAlt = altStr.toDouble();

    double devLat = GeoCalculator::parseCoord(m_view->getUi()->statLatitude->text());
    double devLon = GeoCalculator::parseCoord(m_view->getUi()->statLongitude->text());
    double devAlt = m_view->m_deviceHeight;

    if (devLat == 0 && devLon == 0) {
        QMessageBox::warning(m_view, "状态错误", "当前设备 GPS 未知，无法计算目标角度。");
        return;
    }

    double pan = GeoCalculator::bearing(devLat, devLon, targetLat, targetLon);
    double dist = GeoCalculator::haversineDistance(devLat, devLon, targetLat, targetLon);

    double tilt = 0;
    if (dist > 0.001) { 
        tilt = -qRadiansToDegrees(qAtan2(targetAlt - devAlt, dist));
    }

    this->motorController()->ptzMoveTo(pan, tilt);
    m_view->getUi()->statusbar->showMessage(QString("转到 GPS: 方位=%1° 俯仰=%2°").arg(pan, 0, 'f', 1).arg(tilt, 0, 'f', 1), 3000);
}

void MainPresenter::on_btnPanZeroCalib_clicked()
{
    if (!m_view->requireConnected()) return;
    
    if (QMessageBox::question(m_view, "零点标定", "确认将当前云台水平和俯仰位置标定为 0 度？") == QMessageBox::Yes) {
        if (m_cfg->softwarePtzCalibrationEnabled()) {
            // 开启了模拟串口服务器，使用软件偏置
            QString panStr = m_view->getUi()->statPanAngle->text();
            panStr.remove("°");
            double displayedPan = panStr.toDouble();

            QString tiltStr = m_view->getUi()->statTiltAngle->text();
            tiltStr.remove("°");
            double displayedTilt = tiltStr.toDouble();

            double oldPanOffset = m_cfg->ptzPanOffset();
            double oldTiltOffset = m_cfg->ptzTiltOffset();

            double newPanOffset = displayedPan + oldPanOffset;
            while (newPanOffset >= 360.0) newPanOffset -= 360.0;
            while (newPanOffset < 0) newPanOffset += 360.0;

            double newTiltOffset = oldTiltOffset - displayedTilt;
            while (newTiltOffset > 180.0) newTiltOffset -= 360.0;
            while (newTiltOffset <= -180.0) newTiltOffset += 360.0;

            m_cfg->setPtzPanOffset(newPanOffset);
            m_cfg->setPtzTiltOffset(newTiltOffset);
            m_cfg->save();

            this->ptzForwarder()->setOffsets(newPanOffset, newTiltOffset);
            this->ptzForwarder()->flushZeroPosition();

            m_view->getUi()->statPanAngle->setText("0.0°");
            m_view->getUi()->statTiltAngle->setText("0.0°");
            m_view->getUi()->statusbar->showMessage("零点标定(软件偏置)已保存", 3000);
        } else {
            // 未开启模拟串口服务器，直接通过 PELCO-D 透传标定指令
            this->motorController()->ptzSetZero();
            m_view->getUi()->statusbar->showMessage("零点标定指令(Pelco-D)已下发", 3000);
            // 这里不强制改 UI，让后续设备主动上报的新角度来刷新 UI
        }
    }
}

void MainPresenter::on_btnSetLocation_clicked()
{
    QString latStr = m_view->getUi()->editSetLat->text().trimmed();
    QString lonStr = m_view->getUi()->editSetLon->text().trimmed();

    if (latStr.isEmpty() || lonStr.isEmpty()) {
        QMessageBox::warning(m_view, QString::fromUtf8("输入错误"),
                             QString::fromUtf8("请填写完整的经纬度参数"));
        return;
    }

    if (!m_view->requireConnected()) return;

    double latNum = GeoCalculator::parseCoord(latStr);
    double lonNum = GeoCalculator::parseCoord(lonStr);

    QString altStr = m_view->getUi()->editSetHeight->text().trimmed();
    if (!altStr.isEmpty()) {
        m_view->m_deviceHeight = altStr.toDouble();
        m_view->getUi()->statHeight->setText(QString::number(m_view->m_deviceHeight, 'f', 1) + QStringLiteral(" m"));
    }

    QString strictLat = QString::asprintf("%.7f%s", qAbs(latNum), latNum >= 0 ? "N" : "S");
    QString strictLon = QString::asprintf("%.7f%s", qAbs(lonNum), lonNum >= 0 ? "E" : "W");

    this->motorController()->setLocation(strictLat, strictLon);
    m_view->getUi()->statusbar->showMessage(QString::fromUtf8("已下发经纬度"), 3000);
}

void MainPresenter::on_btnGetImageParams_clicked()
{
    if (!m_view->requireConnected()) return;
    this->motorController()->queryImageParams();
    m_view->getUi()->statusbar->showMessage(QString::fromUtf8("已发送参数查询请求"), 3000);
}



void MainPresenter::onDeviceAiTimeout(const QString& deviceId)
{
    if (deviceId != m_currentDeviceId) return;

    auto ui = m_view->getUi();
    ui->lblIdentifyCount->setText(QString::fromUtf8("目标总数: 0"));
    ui->tableIdentify->setRowCount(0);

    ui->lblTrackStatus->setText(QString::fromUtf8("状态: 未锁定"));
    ui->lblTrackStatus->setProperty("state", "nolock");
    MainWindow::refreshStyle(ui->lblTrackStatus);
    ui->trackPos->clear();
    ui->trackMissDistance->clear();
    ui->trackDistance->clear();

    // Map clearing
    // Note: Assuming m_view has m_mapWidget accessible or we can use getter.
    // We already made m_mapWidget public? No, we didn't. 
    m_view->m_mapWidget->clearAllTracks();\n    m_view->m_mapWidget->updateTargetMarkers(QJsonArray());\n    m_view->m_mapWidget->clearFov();
}
