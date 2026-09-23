//============================================================================
// mainwindow_imainview.cpp - IMainView 接口实现
// MainWindow 对 IMainView 纯虚函数的具体实现，
// 包括状态展示、镜头统计、AI 识别、跟踪信息、地图操作、视频帧路由。
//============================================================================
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui/components/VideoGridWidget.h"
#include "ui/components/DeviceTreeWidget.h"
#include "ui/views/videowidget.h"
#include "ui/views/mapwidget.h"
#include "ui/views/MainWindowDialogService.h"
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QApplication>
#include <QMessageBox>

//============================================================================
// requireConnected / requireMotorReady
//============================================================================
bool MainWindow::requireConnected()
{
    return m_dialogService->requireConnected();
}

bool MainWindow::requireMotorReady()
{
    return m_dialogService->requireMotorReady();
}

//============================================================================
// IMainView - 基础查询
//============================================================================
QWidget* MainWindow::asWidget() { return this; }

void MainWindow::showStatusMessage(const QString& msg, int timeoutMs)
{
    ui->statusbar->showMessage(msg, timeoutMs);
}

QString MainWindow::targetPanText() const { return ui->editTargetPan->text(); }
QString MainWindow::targetTiltText() const { return ui->editTargetTilt->text(); }
QString MainWindow::targetLonText() const { return ui->editTargetLon->text(); }
QString MainWindow::targetLatText() const { return ui->editTargetLat->text(); }
QString MainWindow::targetAltText() const { return ui->editTargetAlt->text(); }
QString MainWindow::setLatText() const { return ui->editSetLat->text(); }
QString MainWindow::setLonText() const { return ui->editSetLon->text(); }
QString MainWindow::setHeightText() const { return ui->editSetHeight->text(); }
int MainWindow::wiperRunCurrent() const { return ui->editWiperRunCurrent->text().toInt(); }
int MainWindow::wiperHoldCurrent() const { return ui->editWiperHoldCurrent->text().toInt(); }
int MainWindow::wiperHoldDelay() const { return ui->editWiperHoldDelay->text().toInt(); }
int MainWindow::presetValue() const { return ui->spinPreset->value(); }
int MainWindow::workModeIndex() const { return ui->comboWorkMode->currentIndex(); }
int MainWindow::algoModel2Index() const { return ui->comboAlgoModel2->currentIndex(); }
int MainWindow::displayModeIndex() const { return ui->comboDisplayMode->currentIndex(); }

QString MainWindow::statLatitudeText() const { return ui->statLatitude->text(); }
QString MainWindow::statLongitudeText() const { return ui->statLongitude->text(); }
QString MainWindow::statPanAngleText() const { return ui->statPanAngle->text(); }
QString MainWindow::statTiltAngleText() const { return ui->statTiltAngle->text(); }

//============================================================================
// IMainView - 状态展示
//============================================================================
void MainWindow::showDeviceState(const QString& lat, const QString& lon,
                                 const QString& height, const QString& pan, const QString& tilt)
{
    if (!lat.isNull())
        ui->statLatitude->setText(lat);
    if (!lon.isNull())
        ui->statLongitude->setText(lon);
    if (!height.isNull()) {
        if (height.isEmpty()) ui->statHeight->clear();
        else ui->statHeight->setText(height);
    }
    if (!pan.isNull()) {
        if (pan.isEmpty()) ui->statPanAngle->clear();
        else ui->statPanAngle->setText(pan);
    }
    if (!tilt.isNull()) {
        if (tilt.isEmpty()) ui->statTiltAngle->clear();
        else ui->statTiltAngle->setText(tilt);
    }
}

void MainWindow::showLensStats(double visZoom, double visFocal, double visHfov,
                               double irZoom, double irFocal, double irHfov)
{
    if (visZoom == 0 && irZoom == 0) {
        ui->statZoomVis->clear();
        ui->statFocalVis->clear();
        ui->statFocusVis->clear();
        ui->statFovVis->clear();
        ui->statZoomIR->clear();
        ui->statFocalIR->clear();
        ui->statFocusIR->clear();
        ui->statFovIR->clear();
        return;
    }
    ui->statZoomVis->setText(QString::number(visZoom, 'f', 2) + QStringLiteral("x"));
    ui->statFocalVis->setText(QString::number(visFocal, 'f', 2) + QStringLiteral(" mm"));
    ui->statFocusVis->clear();
    ui->statFovVis->setText(QString::number(visHfov, 'f', 2) + QStringLiteral("°"));

    ui->statZoomIR->setText(QString::number(irZoom, 'f', 2) + QStringLiteral("x"));
    ui->statFocalIR->setText(QString::number(irFocal, 'f', 2) + QStringLiteral(" mm"));
    ui->statFocusIR->clear();
    ui->statFovIR->setText(QString::number(irHfov, 'f', 2) + QStringLiteral("°"));
}

//============================================================================
// IMainView - AI 识别
//============================================================================
void MainWindow::setIdentifyCount(const QString& text)
{
    ui->lblIdentifyCount->setText(text);
}

void MainWindow::clearIdentifyTable()
{
    ui->tableIdentify->setRowCount(0);
}

void MainWindow::addIdentifyRow(const QString& id, const QString& typeName, double dist,
                                const QString& pos, const QString& miss)
{
    int r = ui->tableIdentify->rowCount();
    ui->tableIdentify->insertRow(r);
    ui->tableIdentify->setItem(r, 0, new QTableWidgetItem(id));
    ui->tableIdentify->setItem(r, 1, new QTableWidgetItem(typeName));
    ui->tableIdentify->setItem(r, 2, new QTableWidgetItem(QString::number(dist, 'f', 1)));
    if (!pos.isNull())
        ui->tableIdentify->setItem(r, 3, new QTableWidgetItem(pos));
    if (!miss.isNull())
        ui->tableIdentify->setItem(r, 4, new QTableWidgetItem(miss));
}

//============================================================================
// IMainView - 跟踪信息
//============================================================================
void MainWindow::showTrackStatus(const QString& text, const QString& state)
{
    ui->lblTrackStatus->setText(text);
    ui->lblTrackStatus->setProperty("state", state);
    refreshStyle(ui->lblTrackStatus);
}

void MainWindow::setTrackDistance(const QString& text)
{
    if (text.isEmpty()) ui->trackDistance->clear();
    else ui->trackDistance->setText(text);
}

void MainWindow::setTrackPos(const QString& text)
{
    if (text.isEmpty()) ui->trackPos->clear();
    else ui->trackPos->setText(text);
}

void MainWindow::setTrackMissDistance(const QString& text)
{
    if (text.isEmpty()) ui->trackMissDistance->clear();
    else ui->trackMissDistance->setText(text);
}

void MainWindow::setTrackTargetType(const QString& text)
{
    if (text.isEmpty()) ui->trackTargetType->clear();
    else ui->trackTargetType->setText(text);
}

void MainWindow::setTrackAngle(const QString& text)
{
    if (text.isEmpty()) ui->trackAngle->clear();
    else ui->trackAngle->setText(text);
}

//============================================================================
// IMainView - 图像参数
//============================================================================
void MainWindow::showImageParams(const QString& resolution, const QString& bitrate,
                                 const QString& codec, const QString& workMode,
                                 const QString& pipShow, const QString& algoModel,
                                 const QString& maxVisFL, const QString& maxIRFL)
{
    ui->paramResolution->setText(resolution);
    ui->paramBitrate->setText(bitrate);
    ui->paramCodec->setText(codec);
    ui->paramWorkMode->setText(workMode);
    ui->paramPipShow->setText(pipShow);
    ui->paramAlgoModel->setText(algoModel);
    ui->paramMaxVisFL->setText(maxVisFL);
    ui->paramMaxIRFL->setText(maxIRFL);
}

//============================================================================
// IMainView - 下拉框/复选框状态设置（带 blockSignals）
//============================================================================
void MainWindow::setAlgoModel1Index(int high)
{
    ui->comboAlgoModel1->blockSignals(true);
    ui->comboAlgoModel1->setCurrentIndex(high);
    ui->comboAlgoModel1->blockSignals(false);
}

void MainWindow::setAlgoModel2Index(int low)
{
    ui->comboAlgoModel2->blockSignals(true);
    ui->comboAlgoModel2->setCurrentIndex(low);
    ui->comboAlgoModel2->blockSignals(false);
}

void MainWindow::setDisplayModeIndex(int index)
{
    ui->comboDisplayMode->blockSignals(true);
    ui->comboDisplayMode->setCurrentIndex(index);
    ui->comboDisplayMode->blockSignals(false);
}

void MainWindow::setWorkModeIndex(int index)
{
    ui->comboWorkMode->blockSignals(true);
    ui->comboWorkMode->setCurrentIndex(index);
    ui->comboWorkMode->blockSignals(false);
}

void MainWindow::setDigitalZoomChecked(bool checked)
{
    ui->checkDigitalZoom->blockSignals(true);
    ui->checkDigitalZoom->setChecked(checked);
    ui->checkDigitalZoom->blockSignals(false);
}

void MainWindow::setAutoZoomChecked(bool checked)
{
    ui->checkAutoZoom->blockSignals(true);
    ui->checkAutoZoom->setChecked(checked);
    ui->checkAutoZoom->blockSignals(false);
}

void MainWindow::setCaptureUploadChecked(bool checked)
{
    ui->checkCaptureUpload->blockSignals(true);
    ui->checkCaptureUpload->setChecked(checked);
    ui->checkCaptureUpload->blockSignals(false);
}

void MainWindow::setPosResetChecked(bool checked)
{
    ui->checkPosReset->blockSignals(true);
    ui->checkPosReset->setChecked(checked);
    ui->checkPosReset->blockSignals(false);
}

//============================================================================
// IMainView - 视频帧路由
//============================================================================
bool MainWindow::setVideoFrame(const QString& deviceId, const QImage& frame)
{
    if (!m_videoGrid) return false;
    if (VideoWidget* vw = m_videoGrid->bindDevice(deviceId)) {
        vw->setFrame(frame);
        return true;
    }
    return false;
}

void MainWindow::clearVideoFrame(const QString& deviceId)
{
    if (!m_videoGrid) return;
    m_videoGrid->unbindDevice(deviceId);
}

void MainWindow::setVideoSelectionEnabled(const QString& deviceId, bool enabled)
{
    if (!m_videoGrid) return;
    if (VideoWidget* vw = m_videoGrid->getWidget(deviceId)) {
        vw->setSelectionEnabled(enabled);
    }
}

void MainWindow::setVideoStatusText(const QString& deviceId, const QString& text)
{
    if (!m_videoGrid) return;
    if (VideoWidget* vw = m_videoGrid->getWidget(deviceId)) {
        vw->setStatusText(text);
    }
}

void MainWindow::repaintVideoGrid()
{
    if (!m_videoGrid) return;
    m_videoGrid->repaint();
}

//============================================================================
// IMainView - 地图操作
//============================================================================
void MainWindow::mapClearAllTracks()
{
    if (!m_mapWidget) return;
    m_mapWidget->clearAllTracks();
}

void MainWindow::mapUpdateTargetMarkers(const QJsonArray& targets)
{
    if (!m_mapWidget) return;
    m_mapWidget->updateTargetMarkers(targets);
}

void MainWindow::mapClearFov()
{
    if (!m_mapWidget) return;
    m_mapWidget->clearFov();
}

void MainWindow::mapAppendTrackPoint(const QString& trackId, double lat, double lon, double speed)
{
    if (!m_mapWidget) return;
    m_mapWidget->appendTrackPoint(trackId, lat, lon, speed);
}

void MainWindow::mapSetDevicePosition(double lat, double lon)
{
    if (!m_mapWidget) return;
    m_mapWidget->setDevicePosition(lat, lon);
}

void MainWindow::mapSetVisFov(double lat, double lon, double panDeg, double tiltDeg,
                              double hfov, double vfov, double distance)
{
    if (!m_mapWidget) return;
    m_mapWidget->setVisFov(lat, lon, panDeg, tiltDeg, hfov, vfov, distance);
}

void MainWindow::mapSetIrFov(double lat, double lon, double panDeg, double tiltDeg,
                             double hfov, double vfov, double distance)
{
    if (!m_mapWidget) return;
    m_mapWidget->setIrFov(lat, lon, panDeg, tiltDeg, hfov, vfov, distance);
}

void MainWindow::mapSetDeviceInfo(double lat, double lon, double alt, double pan, double tilt,
                                  double visHfov, double visVfov, double range, bool rangeEstimated)
{
    if (!m_mapWidget) return;
    m_mapWidget->setDeviceInfo(lat, lon, alt, pan, tilt, visHfov, visVfov, range, rangeEstimated);
}

//============================================================================
// IMainView - 抓拍
//============================================================================
void MainWindow::onImageSnapped(const QByteArray& jpegData, const QRect& location)
{
    QString dirPath = qApp->applicationDirPath() + QStringLiteral("/snapshots");
    QDir().mkpath(dirPath);

    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString path = dirPath + QStringLiteral("/snap_") + ts + QStringLiteral(".jpg");
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(jpegData);
        f.close();
        ui->statusbar->showMessage(
            QString::fromUtf8("已保存抓拍: %1  位置: (%2,%3 %4x%5)")
                .arg(path)
                .arg(location.x()).arg(location.y())
                .arg(location.width()).arg(location.height()),
            5000);
    }
}
