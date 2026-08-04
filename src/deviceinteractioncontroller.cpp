#include "deviceinteractioncontroller.h"
#include "ui_mainwindow.h"
#include "devicemanager.h"
#include "trackmanager.h"
#include "mapviewcontroller.h"
#include "configmanager.h"
#include "devicestate.h"
#include "devicecontext.h"
#include "devicecontroller.h"
#include "tjsonclient.h"
#include "rtspthread.h"
#include "videowidget.h"
#include "videogridwidget.h"
#include "devicetreewidget.h"
#include "geoutils.h"
#include "jsonframeparser.h"
#include "cmdlogdialog.h"
#include "settingsdialog.h"
#include "mapwidget.h"
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QSlider>
#include <QSpinBox>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QApplication>
#include <QStatusBar>
#include <QtMath>

//============================================================================
// 辅助函数
//============================================================================
static void refreshStyle(QWidget *w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

static void revertRadioMode(QRadioButton* off, QRadioButton* id, QRadioButton* track, int prevWm)
{
    off->blockSignals(true);
    id->blockSignals(true);
    track->blockSignals(true);
    if (prevWm == 0)      off->setChecked(true);
    else if (prevWm == 1) id->setChecked(true);
    else                  track->setChecked(true);
    off->blockSignals(false);
    id->blockSignals(false);
    track->blockSignals(false);
}

static const int kResTab[][2] = {{1920,1080},{1280,720},{704,576},{2566,1520}};
static const char* kResMap[] = {"1080P", "720P", "D1", "1440P"};
static const char* kCodecMap[] = {"H264", "H265"};
static const char* kWmMap[] = {"关闭AI", "识别", "自动跟踪", "点选跟踪", "波门/框选跟踪"};
static const char* kPipMap[] = {"大图可见光", "红外", "可见光", "融合", "大图红外"};
static const char* kHighMap[] = {"可见光", "红外"};
static const char* kLowMap[] = {"", "", "人车识别", "船识别", "无人机识别", "飞机直升机识别", "鸟识别"};

DeviceInteractionController::DeviceInteractionController(Ui::MainWindow *ui,
                                                         VideoGridWidget *videoGrid,
                                                         QWidget *drawerPanel,
                                                         DeviceTreeWidget *deviceTree,
                                                         QPushButton *drawerToggleBtn,
                                                         QWidget *mainWindow,
                                                         DeviceManager *devMgr,
                                                         TrackManager *trackMgr,
                                                         MapViewController *mapCtrl,
                                                         ConfigManager *cfg,
                                                         DeviceState *devState,
                                                         QStatusBar *statusBar,
                                                         QObject *parent)
    : QObject(parent)
    , ui(ui)
    , m_mainWindow(mainWindow)
    , m_devMgr(devMgr)
    , m_trackMgr(trackMgr)
    , m_mapCtrl(mapCtrl)
    , m_cfg(cfg)
    , m_devState(devState)
    , m_statusBar(statusBar)
    , m_videoGrid(videoGrid)
    , m_drawerPanel(drawerPanel)
    , m_deviceTree(deviceTree)
    , m_drawerToggleBtn(drawerToggleBtn)
{
    // 左侧抽屉布局
    {
        auto *drawerLay = new QVBoxLayout(m_drawerPanel);
        drawerLay->setContentsMargins(0, 0, 0, 0);
        drawerLay->setSpacing(2);
        drawerLay->addWidget(m_deviceTree, 1);

        auto *splitBar = new QWidget(m_drawerPanel);
        splitBar->setObjectName("splitBar");
        auto *splitLay = new QHBoxLayout(splitBar);
        splitLay->setContentsMargins(4, 0, 4, 4);
        splitLay->setSpacing(2);

        m_splitGroup = new QButtonGroup(this);
        auto makeSplitBtn = [&](const QString &text, int id) {
            auto *btn = new QPushButton(text, splitBar);
            btn->setCheckable(true);
            btn->setFixedHeight(24);
            btn->setObjectName(QString("splitBtn%1").arg(id));
            m_splitGroup->addButton(btn, id);
            splitLay->addWidget(btn);
        };
        makeSplitBtn("1", 0); makeSplitBtn("4", 1);
        makeSplitBtn("9", 2); makeSplitBtn("16", 3);
        m_splitGroup->button(0)->setChecked(true);
        drawerLay->addWidget(splitBar);

        auto *bodyLay = new QHBoxLayout(ui->videoGridContainer);
        bodyLay->setContentsMargins(0, 0, 0, 0);
        bodyLay->setSpacing(0);
        bodyLay->addWidget(m_drawerPanel);
        bodyLay->addWidget(m_videoGrid, 1);

        connect(m_splitGroup, &QButtonGroup::idClicked, this, [this](int id) {
            m_videoGrid->setSplitMode(static_cast<VideoGridWidget::SplitMode>(id));
        });
        connect(m_deviceTree, &DeviceTreeWidget::channelDoubleClicked,
                this, &DeviceInteractionController::onDeviceTreeDoubleClicked);
        connect(m_videoGrid, &VideoGridWidget::cellSelected,
                this, &DeviceInteractionController::onGridCellSelected);
        connect(m_videoGrid, &VideoGridWidget::cellSelectionFinished,
                this, [this](int, int cx, int cy, int pw, int ph) {
            onVideoSelection(cx, cy, pw, ph);
        });
        connect(m_drawerToggleBtn, &QPushButton::clicked,
                this, &DeviceInteractionController::onDrawerToggled);

        connect(m_deviceTree, &DeviceTreeWidget::treeModified,
                this, [this]() {
            m_deviceTree->saveToDisk(QStringLiteral("config/device_tree.json"));
        });

        m_deviceTree->loadFromDisk(QStringLiteral("config/device_tree.json"));
    }

    m_drawerToggleBtn->raise();
    m_drawerToggleBtn->move(240, 0);
    m_drawerToggleBtn->setText(QStringLiteral("\u25C0"));

    // 系统参数轮询
    m_sysParamTimer = new QTimer(this);
    m_sysParamTimer->setInterval(500);
    connect(m_sysParamTimer, &QTimer::timeout,
            this, &DeviceInteractionController::onSysParamTimerTimeout);

    // 表格表头初始化
    ui->tableIdentify->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    //============================================================================
    // 设备管理器信号 → UI 更新
    //============================================================================
    connect(m_devMgr, &DeviceManager::deviceConnected, this, [this](const QString &ip) {
        if (ip == m_devMgr->activeDeviceIp()) {
            m_statusBar->showMessage(QString::fromUtf8("%1 \u5df2\u8fde\u63a5").arg(ip), 3000);
            if (auto *dc = m_devMgr->device(ip)) {
                dc->ctrl->queryImageParams();
            }
            m_sysParamTimer->start();
        }
    });

    connect(m_devMgr, &DeviceManager::deviceDisconnected, this, [this](const QString &ip) {
        if (ip == m_devMgr->activeDeviceIp()) {
            m_sysParamTimer->stop();
            m_statusBar->showMessage(QString::fromUtf8("%1 \u5df2\u65ad\u5f00").arg(ip), 3000);
        }
    });

    connect(m_devMgr, &DeviceManager::jsonReceived, this,
        [this](const QString &ip, const QJsonObject &doc) {
            onDeviceJsonReceived(ip, doc);
        });

    connect(m_devMgr, &DeviceManager::activeDeviceChanged, this, [this](const QString &ip) {
        auto *dc = m_devMgr->device(ip);
        if (dc && dc->tcpConnected) {
            m_sysParamTimer->start();
            dc->ctrl->queryImageParams();
        } else {
            m_sysParamTimer->stop();
        }
        m_devState->visZoom = dc ? dc->state.visZoom : 1.0;
        m_devState->irZoom = dc ? dc->state.irZoom : 1.0;
        m_devState->resX = dc ? dc->state.resX : m_cfg->cam().visResX;
        m_devState->resY = dc ? dc->state.resY : m_cfg->cam().visResY;
        m_devState->previousWorkMode = dc ? dc->state.workMode : 0;
        m_devState->previousAlgoModel = dc ? dc->state.algoModel : 0;
        m_devState->previousDisplayMode = dc ? dc->state.displayMode : 0;
        m_statusBar->showMessage(QString::fromUtf8("\u5df2\u5207\u6362\u5230\u8bbe\u5907 %1").arg(ip), 3000);
    });

    // 设备树右键 → 连接/断开
    connect(m_deviceTree, &DeviceTreeWidget::deviceToggleConnect, this, [this](const QString &ip) {
        auto *dc = m_devMgr->device(ip);
        if (!dc) {
            m_statusBar->showMessage(QString::fromUtf8("\u8bbe\u5907 %1 \u4e0d\u5b58\u5728").arg(ip), 3000);
            return;
        }
        if (dc->tcpConnected) {
            dc->stopRtsp();
            dc->tcpClient->disconnectDevice();
            m_statusBar->showMessage(QString::fromUtf8("\u5df2\u65ad\u5f00 %1").arg(ip), 3000);
        } else {
            dc->tcpClient->connectToDevice(ip, 8089);
            m_devMgr->switchActiveDevice(ip);
            m_statusBar->showMessage(QString::fromUtf8("\u6b63\u5728\u8fde\u63a5 %1...").arg(ip), 3000);
        }
    });

    // 新建设备时自动创建上下文
    connect(m_deviceTree, &DeviceTreeWidget::deviceAdded, this,
        [this](const QString &ip, const QString &name) {
            if (m_devMgr->device(ip)) return;
            auto *ctx = new DeviceContext(ip, m_cfg, m_devMgr);
            m_devMgr->setupDeviceContext(ctx, ip, name);
        });

    //============================================================================
    // 云台八方向控制
    //============================================================================
    auto connectPtzBtn = [this](QPushButton* btn, PtzDir dir) {
        connect(btn, &QPushButton::pressed, this, [this, dir]() {
            if (!requireConnected()) return;
            m_devMgr->activeCtrl()->ptzMove(dir);
        });
        connect(btn, &QPushButton::released, this, [this]() {
            auto *c = m_devMgr->activeClient();
            if (!c || !c->isConnected()) return;
            m_devMgr->activeCtrl()->ptzStop();
        });
    };

    connectPtzBtn(ui->btnPtzUp, PtzDir::Up);
    connectPtzBtn(ui->btnPtzDown, PtzDir::Down);
    connectPtzBtn(ui->btnPtzLeft, PtzDir::Left);
    connectPtzBtn(ui->btnPtzRight, PtzDir::Right);
    connectPtzBtn(ui->btnPtzTopLeft, PtzDir::UpLeft);
    connectPtzBtn(ui->btnPtzTopRight, PtzDir::UpRight);
    connectPtzBtn(ui->btnPtzBottomLeft, PtzDir::DownLeft);
    connectPtzBtn(ui->btnPtzBottomRight, PtzDir::DownRight);

    //============================================================================
    // 云台速度控制
    //============================================================================
    ui->sliderSpeed->setValue(m_cfg->ptz().panSpeed);
    ui->spinSpeed->setValue(m_cfg->ptz().panSpeed);

    connect(ui->sliderSpeed, &QSlider::valueChanged, ui->spinSpeed, &QSpinBox::setValue);
    connect(ui->spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderSpeed, &QSlider::setValue);
    connect(ui->spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_cfg->ptz().panSpeed = static_cast<quint8>(val);
        m_cfg->ptz().tiltSpeed = static_cast<quint8>(val);
        m_cfg->save();
    });

    //============================================================================
    // 镜头控制
    //============================================================================
    auto connectLensBtn = [this, ui](QPushButton* btn, int op) {
        connect(btn, &QPushButton::pressed, this, [this, op, ui]() {
            if (!requireConnected()) return;
            int t = ui->comboLensTarget->currentIndex();
            if (op == 0) m_devMgr->activeCtrl()->lensZoomIn(t);
            else if (op == 1) m_devMgr->activeCtrl()->lensZoomOut(t);
            else if (op == 2) m_devMgr->activeCtrl()->lensFocusIn(t);
            else m_devMgr->activeCtrl()->lensFocusOut(t);
        });
        connect(btn, &QPushButton::released, this, [this]() {
            auto *c = m_devMgr->activeClient();
            if (!c || !c->isConnected()) return;
            m_devMgr->activeCtrl()->lensStop();
        });
    };

    connectLensBtn(ui->btnZoomIn, 0);
    connectLensBtn(ui->btnZoomOut, 1);
    connectLensBtn(ui->btnFocusIn, 2);
    connectLensBtn(ui->btnFocusOut, 3);

    //============================================================================
    // 镜头速度控制
    //============================================================================
    ui->sliderZoomSpeed->setValue(m_cfg->lens().zoomSpeed);
    ui->spinZoomSpeed->setValue(m_cfg->lens().zoomSpeed);
    ui->sliderFocusSpeed->setValue(m_cfg->lens().focusSpeed);
    ui->spinFocusSpeed->setValue(m_cfg->lens().focusSpeed);

    connect(ui->sliderZoomSpeed, &QSlider::valueChanged, ui->spinZoomSpeed, &QSpinBox::setValue);
    connect(ui->spinZoomSpeed, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderZoomSpeed, &QSlider::setValue);
    connect(ui->spinZoomSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_cfg->lens().zoomSpeed = static_cast<quint8>(val);
        m_cfg->save();
    });

    connect(ui->sliderFocusSpeed, &QSlider::valueChanged, ui->spinFocusSpeed, &QSpinBox::setValue);
    connect(ui->spinFocusSpeed, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderFocusSpeed, &QSlider::setValue);
    connect(ui->spinFocusSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_cfg->lens().focusSpeed = static_cast<quint8>(val);
        m_cfg->save();
    });

    //============================================================================
    // 预置位控制
    //============================================================================
    connect(ui->btnCallPreset, &QPushButton::clicked, this, [this, ui]() {
        if (!requireConnected()) return;
        m_devMgr->activeCtrl()->callPreset(ui->spinPreset->value());
    });
    connect(ui->btnSetPreset, &QPushButton::clicked, this, [this, ui]() {
        if (!requireConnected()) return;
        m_devMgr->activeCtrl()->setPreset(ui->spinPreset->value());
    });
    connect(ui->btnDelPreset, &QPushButton::clicked, this, [this, ui]() {
        if (!requireConnected()) return;
        m_devMgr->activeCtrl()->delPreset(ui->spinPreset->value());
    });

    //============================================================================
    // 附加功能开关
    //============================================================================
    connect(ui->checkDigitalZoom, &QCheckBox::toggled, this, [this, ui](bool checked) {
        if (!requireConnected()) { ui->checkDigitalZoom->blockSignals(true); ui->checkDigitalZoom->setChecked(!checked); ui->checkDigitalZoom->blockSignals(false); return; }
        m_devMgr->activeCtrl()->setDigitalZoom(checked);
    });
    connect(ui->checkAutoZoom, &QCheckBox::toggled, this, [this, ui](bool checked) {
        if (!requireConnected()) { ui->checkAutoZoom->blockSignals(true); ui->checkAutoZoom->setChecked(!checked); ui->checkAutoZoom->blockSignals(false); return; }
        m_devMgr->activeCtrl()->setAutoZoom(checked);
    });
    connect(ui->checkCaptureUpload, &QCheckBox::toggled, this, [this, ui](bool checked) {
        if (!requireConnected()) { ui->checkCaptureUpload->blockSignals(true); ui->checkCaptureUpload->setChecked(!checked); ui->checkCaptureUpload->blockSignals(false); return; }
        m_devMgr->activeCtrl()->setCaptureUpload(checked);
    });
    connect(ui->checkPosReset, &QCheckBox::toggled, this, [this, ui](bool checked) {
        if (!requireConnected()) { ui->checkPosReset->blockSignals(true); ui->checkPosReset->setChecked(!checked); ui->checkPosReset->blockSignals(false); return; }
        m_devMgr->activeCtrl()->posReset(checked);
    });
    connect(ui->btnPtzReset, &QPushButton::clicked, this, [this]() {
        if (!requireConnected()) return;
        m_devMgr->activeCtrl()->callPreset(0);
    });

    //============================================================================
    // 工作模式 + 算法模型 + 显示模式 + 镜头目标
    //============================================================================
    connect(ui->radioModeOff, &QRadioButton::clicked, this, &DeviceInteractionController::onRadioModeOffClicked);
    connect(ui->radioModeIdentify, &QRadioButton::clicked, this, &DeviceInteractionController::onRadioModeIdentifyClicked);
    connect(ui->radioModeAutoTrack, &QRadioButton::clicked, this, &DeviceInteractionController::onRadioModeAutoTrackClicked);
    connect(ui->comboAlgoModel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DeviceInteractionController::onAlgoModelChanged);
    connect(ui->comboDisplayMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DeviceInteractionController::onDisplayModeChanged);
    connect(ui->comboLensTarget, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DeviceInteractionController::onLensTargetChanged);
    connect(ui->btnSetLocation, &QPushButton::clicked, this, &DeviceInteractionController::onSetLocationClicked);
    connect(ui->btnGetImageParams, &QPushButton::clicked, this, &DeviceInteractionController::onGetImageParamsClicked);
    connect(ui->btnPtzMoveTo, &QPushButton::clicked, this, &DeviceInteractionController::onPtzMoveToClicked);
    connect(ui->btnSettings, &QPushButton::clicked, this, &DeviceInteractionController::onSettingsClicked);
    connect(ui->btnMapToggle, &QPushButton::clicked, this, [this]() { m_mapCtrl->toggleVisibility(); });

    m_devMgr->connectAllDevices();
    m_mapCtrl->updateLayout();
}

//============================================================================
// requireConnected
//============================================================================
bool DeviceInteractionController::requireConnected()
{
    if (m_devMgr->activeDeviceIp().isEmpty()) {
        QMessageBox::information(m_mainWindow, QStringLiteral("\u63d0\u793a"),
                                 QStringLiteral("\u8bf7\u9009\u62e9\u753b\u9762\u4e2d\u7684\u8bbe\u5907"));
        return false;
    }
    auto *dc = m_devMgr->activeDevice();
    if (!dc || !dc->tcpConnected) {
        QMessageBox::information(m_mainWindow, QStringLiteral("\u63d0\u793a"),
                                 QString::fromUtf8("\u8bbe\u5907 %1 \u672a\u8fde\u63a5").arg(m_devMgr->activeDeviceIp()));
        return false;
    }
    return true;
}

//============================================================================
// onDeviceJsonReceived
//============================================================================
void DeviceInteractionController::onDeviceJsonReceived(const QString &ip, const QJsonObject &doc)
{
    auto *dc = m_devMgr->device(ip);
    if (!dc) return;

    QString ctrlType = doc.value("ControlType").toString();
    if (ctrlType == "ZoomInfo") {
        auto z = ZoomInfoData::parse(doc);
        dc->state.visZoom = z.visZoom;
        dc->state.irZoom = z.irZoom;
        dc->state.lastPan = z.pan;
        dc->state.lastTilt = z.tilt;
        dc->state.lastLaserRange = z.laserRange;
        dc->state.lastLat = z.latitude.toDouble();
        dc->state.lastLon = z.longitude.toDouble();
        dc->state.lastAlt = z.height;
    } else if (ctrlType == "ImageSetting") {
        auto is = ImageSettingData::parse(doc);
        if (is.imgSize >= 0 && is.imgSize < 4) {
            dc->state.resX = kResTab[is.imgSize][0];
            dc->state.resY = kResTab[is.imgSize][1];
        }
        dc->state.workMode = is.workMode;
        dc->state.displayMode = is.pipShow;
        dc->state.algoModel = is.model;
    }

    if (ip == m_devMgr->activeDeviceIp()) {
        m_devState->visZoom = dc->state.visZoom;
        m_devState->irZoom = dc->state.irZoom;
        m_devState->resX = dc->state.resX;
        m_devState->resY = dc->state.resY;
        updateStatusFromJson(doc);
    }
}

void DeviceInteractionController::onSysParamTimerTimeout()
{
    auto *c = m_devMgr->activeClient();
    if (c && c->isConnected()) m_devMgr->activeCtrl()->queryImageParams();
}

//============================================================================
// switchActiveDevice
//============================================================================
void DeviceInteractionController::switchActiveDevice(const QString &ip)
{
    m_devMgr->switchActiveDevice(ip);
}

//============================================================================
// onImageSnapped
//============================================================================
void DeviceInteractionController::onImageSnapped(const QByteArray &jpegData, const QRect &location)
{
    QString dirPath = qApp->applicationDirPath() + QStringLiteral("/snapshots");
    QDir().mkpath(dirPath);

    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString path = dirPath + QStringLiteral("/snap_") + ts + QStringLiteral(".jpg");
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(jpegData);
        f.close();
        m_statusBar->showMessage(
            QString::fromUtf8("\u5df2\u4fdd\u5b58\u6293\u62cd: %1  \u4f4d\u7f6e: (%2,%3 %4x%5)")
                .arg(path)
                .arg(location.x()).arg(location.y())
                .arg(location.width()).arg(location.height()),
            5000);
    }
}

//============================================================================
// updateStatusFromJson
//============================================================================
void DeviceInteractionController::updateStatusFromJson(const QJsonObject &doc)
{
    QString controlType = doc.value("ControlType").toString();
    CameraConfig& cam = m_cfg->cam();

    // 1) AIInfo
    if (controlType == "AIInfo") {
        int workMode = doc.value("WorkMode").toInt();
        int count = doc.value("ObjectCount").toInt();

        if (workMode == 1) {
            ui->lblIdentifyCount->setText(QString::fromUtf8("\u76ee\u6807\u603b\u6570: %1").arg(count));
            ui->tableIdentify->setRowCount(0);

            bool isVis = (m_devState->pipShow != 1 && m_devState->pipShow != 4);
            double px = isVis ? cam.visPixelSize : cam.irPixelSize;
            double fl = isVis ? cam.visMinFocal * m_devState->visZoom
                              : cam.irMinFocal * m_devState->irZoom;
            int halfW = (isVis ? m_devState->resX : cam.irResX) / 2;
            int halfH = (isVis ? m_devState->resY : cam.irResY) / 2;

            if (doc.contains("Object") && doc.value("Object").isObject()) {
                QJsonObject objMap = doc.value("Object").toObject();
                for (auto it = objMap.begin(); it != objMap.end(); ++it) {
                    QString id = it.key();
                    QJsonObject obj = it.value().toObject();

                    int r = ui->tableIdentify->rowCount();
                    ui->tableIdentify->insertRow(r);
                    ui->tableIdentify->setItem(r, 0, new QTableWidgetItem(id));
                    ui->tableIdentify->setItem(r, 1, new QTableWidgetItem(QString::number(obj.value("Class").toInt())));
                    ui->tableIdentify->setItem(r, 2, new QTableWidgetItem(QString::number(obj.value("Distance").toDouble(), 'f', 1)));

                    if (obj.contains("Points")) {
                        QJsonObject pts = obj.value("Points").toObject();
                        int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                        int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                        QString pos = QString("(%1,%2)").arg(l).arg(t);
                        ui->tableIdentify->setItem(r, 3, new QTableWidgetItem(pos));

                        double cx = (l + r2) / 2.0, cy = (t + b) / 2.0;
                        QString miss = GeoUtils::missMradStr(cx - halfW, cy - halfH, px, fl);
                        ui->tableIdentify->setItem(r, 4, new QTableWidgetItem(miss));
                    }
                }
            }
        }

        if ((workMode == 1) || (workMode >= 2 && workMode <= 4)) {
            bool isVis = (m_devState->pipShow != 1 && m_devState->pipShow != 4);
            m_mapCtrl->updateTargets(doc, workMode,
                GeoUtils::parseCoord(ui->statLatitude->text()),
                GeoUtils::parseCoord(ui->statLongitude->text()),
                ui->statPanAngle->text().toDouble(),
                ui->statTiltAngle->text().toDouble(),
                isVis ? cam.visPixelSize : cam.irPixelSize,
                isVis ? cam.visMinFocal * m_devState->visZoom : cam.irMinFocal * m_devState->irZoom,
                (isVis ? m_devState->resX : cam.irResX) / 2,
                (isVis ? m_devState->resY : cam.irResY) / 2,
                ui->comboAlgoModel->currentIndex(),
                m_devState->visZoom, m_devState->irZoom, m_devState->pipShow,
                cam);
        }

        if (workMode >= 2 && workMode <= 4) {
            bool hasObj = doc.contains("Object") && doc.value("Object").isObject()
                          && !doc.value("Object").toObject().isEmpty();

            if (hasObj) {
                QJsonObject objMap = doc.value("Object").toObject();
                QJsonObject obj = objMap.begin().value().toObject();
                int cls = obj.value("Class").toInt();

                bool locked = (cls == 0xB1);
                QString statusText = locked ? QString::fromUtf8("\u9501\u5b9a\u4e2d") : QString::fromUtf8("\u4e22\u5931");
                QString statusFull = QString::fromUtf8("\u72b6\u6001: %1").arg(statusText);
                ui->lblTrackStatus->setText(statusFull);
                ui->lblTrackStatus->setProperty("state", locked ? "locked" : "missed");
                refreshStyle(ui->lblTrackStatus);

                if (obj.contains("Distance")) {
                    double rawDist = obj.value("Distance").toDouble(0);
                    if (rawDist > 0)
                        ui->trackDistance->setText(QString::number(rawDist, 'f', 1));
                } else
                    ui->trackDistance->clear();

                if (obj.contains("Points")) {
                    QJsonObject pts = obj.value("Points").toObject();
                    int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                    int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                    int cx = (l + r2) / 2, cy = (t + b) / 2;
                    int pw = r2 - l, ph = b - t;
                    ui->trackPos->setText(QString("(%1,%2) %3\u00d7%4").arg(cx).arg(cy).arg(pw).arg(ph));

                    bool isVis = (m_devState->pipShow != 1 && m_devState->pipShow != 4);
                    double px = isVis ? cam.visPixelSize : cam.irPixelSize;
                    double fl = isVis ? cam.visMinFocal * m_devState->visZoom
                                      : cam.irMinFocal * m_devState->irZoom;
                    int halfW = (isVis ? m_devState->resX : cam.irResX) / 2;
                    int halfH = (isVis ? m_devState->resY : cam.irResY) / 2;
                    double objCx = (l + r2) / 2.0, objCy = (t + b) / 2.0;
                    double dx = objCx - halfW, dy = objCy - halfH;
                    double dxMrad = dx * px / fl;
                    double dyMrad = dy * px / fl;
                    ui->trackMissDistance->setText(QString("H: %1  V: %2")
                        .arg(dxMrad, 0, 'f', 2).arg(dyMrad, 0, 'f', 2));
                } else {
                    ui->trackPos->clear();
                    ui->trackMissDistance->clear();
                }
            } else {
                ui->lblTrackStatus->setText(QString::fromUtf8("\u72b6\u6001: \u672a\u9501\u5b9a"));
                ui->lblTrackStatus->setProperty("state", "nolock");
                refreshStyle(ui->lblTrackStatus);
                ui->trackPos->clear();
                ui->trackMissDistance->clear();
                ui->trackDistance->clear();
            }
        }

    // 2) ZoomInfo
    } else if (controlType == "ZoomInfo") {
        m_devState->visZoom = doc.value("ZoomInfo").toDouble(1.0);
        m_devState->irZoom = doc.value("ZoomInfoIR").toDouble(1.0);

        ui->statCamMode->setText(QString::number(doc.value("CamShowMode").toInt()));
        ui->statLatitude->setText(doc.value("Latitude").toString());
        ui->statLongitude->setText(doc.value("Longitude").toString());
        ui->statHeight->setText(QString::number(doc.value("Height").toDouble(), 'f', 1));
        {
            double laserRange = doc.value("LaserRange").toDouble(0);
            if (laserRange > 0) {
                ui->statDistance->setText(QString::number(laserRange, 'f', 1));
            } else if (m_trackMgr->lastAiDist() > 0) {
                QString est = m_trackMgr->lastAiDistEstimated() ? QStringLiteral(" (\u4f30\u7b97)") : QString();
                ui->statDistance->setText(QString::number(m_trackMgr->lastAiDist(), 'f', 1) + est);
            } else {
                ui->statDistance->setText(QStringLiteral("--"));
            }
        }
        ui->statPanAngle->setText(QString::number(doc.value("PTZInfoH").toDouble(), 'f', 1));
        ui->statTiltAngle->setText(QString::number(doc.value("PTZInfoV").toDouble(), 'f', 1));

        updateLensStats();
        m_mapCtrl->updateDevicePosition(doc,
            m_devState->visZoom, m_devState->irZoom,
            m_devState->resX, m_devState->resY, m_cfg->cam(),
            m_trackMgr->lastAiDist(), m_trackMgr->lastAiDistEstimated());

    // 3) ImageSetting
    } else if (controlType == "ImageSetting") {
        int imgSize = doc.value("ImageSize").toInt();
        ui->paramResolution->setText(imgSize >= 0 && imgSize < 4 ? kResMap[imgSize] : QString::number(imgSize));
        {
            if (imgSize >= 0 && imgSize < 4) {
                m_devState->resX = kResTab[imgSize][0];
                m_devState->resY = kResTab[imgSize][1];
            }
        }

        ui->paramBitrate->setText(QString("%1 Kb/s").arg(doc.value("ImageBit").toInt()));

        int codec = doc.value("ImageCode").toInt();
        ui->paramCodec->setText(codec >= 0 && codec < 2 ? kCodecMap[codec] : QString::number(codec));

        int wm = doc.value("WorkMode").toInt();
        ui->paramWorkMode->setText(wm >= 0 && wm < 5 ? QString::fromUtf8(kWmMap[wm]) : QString::number(wm));
        m_devState->previousWorkMode = wm;

        int pip = doc.value("PipShow").toInt();
        ui->paramPipShow->setText(pip >= 0 && pip < 5 ? QString::fromUtf8(kPipMap[pip]) : QString::number(pip));

        int model = doc.value("Model").toInt();
        int high = model / 10;
        int low  = model % 10;
        QString modelStr;
        if (high >= 0 && high < 2)
            modelStr = QString::fromUtf8(kHighMap[high]);
        if (low >= 2 && low <= 6)
            modelStr += QString(" / %1").arg(QString::fromUtf8(kLowMap[low]));
        ui->paramAlgoModel->setText(modelStr.isEmpty() ? QString::number(model) : modelStr);
        m_devState->previousAlgoModel = model;

        ui->paramMaxVisFL->setText(doc.value("MaxVisFL").toString());
        ui->paramMaxIRFL->setText(doc.value("MaxIRFL").toString());

        m_devState->pipShow = doc.value("PipShow").toInt();
        m_devState->previousDisplayMode = m_devState->pipShow;

        m_devState->updatingFromDevice = true;
        int lowIdx = model % 10;
        if (lowIdx >= 0 && lowIdx < ui->comboAlgoModel->count())
            ui->comboAlgoModel->setCurrentIndex(lowIdx);
        int pipShow = doc.value("PipShow").toInt();
        if (pipShow >= 0 && pipShow < ui->comboDisplayMode->count())
            ui->comboDisplayMode->setCurrentIndex(pipShow);
        syncLensTargetByDisplayMode(pipShow);
        if (wm == 0)      ui->radioModeOff->setChecked(true);
        else if (wm == 1) ui->radioModeIdentify->setChecked(true);
        else if (wm >= 2) ui->radioModeAutoTrack->setChecked(true);
        m_devState->updatingFromDevice = false;
    }
}

//============================================================================
// updateLensStats
//============================================================================
void DeviceInteractionController::updateLensStats()
{
    CameraConfig& cam = m_cfg->cam();
    const double kRad2Deg = 180.0 / 3.14159265358979323846;

    double visFocal = cam.visMinFocal * m_devState->visZoom;
    double irFocal  = cam.irMinFocal * m_devState->irZoom;

    ui->statZoomVis->setText(QString::number(m_devState->visZoom, 'f', 2));
    ui->statFocalVis->setText(QString::number(visFocal, 'f', 2));
    ui->statFocusVis->clear();

    double visHfov = 2.0 * qAtan((cam.visPixelSize * cam.visResX / 1000.0) / (2.0 * visFocal));
    ui->statFovVis->setText(QString::number(visHfov * kRad2Deg, 'f', 2));

    ui->statZoomIR->setText(QString::number(m_devState->irZoom, 'f', 2));
    ui->statFocalIR->setText(QString::number(irFocal, 'f', 2));
    ui->statFocusIR->clear();

    double irHfov = 2.0 * qAtan((cam.irPixelSize * cam.irResX / 1000.0) / (2.0 * irFocal));
    ui->statFovIR->setText(QString::number(irHfov * kRad2Deg, 'f', 2));
}

//============================================================================
// 工作模式切换
//============================================================================
void DeviceInteractionController::onRadioModeOffClicked() {
    if (!requireConnected()) { revertRadioMode(ui->radioModeOff, ui->radioModeIdentify, ui->radioModeAutoTrack, m_devState->previousWorkMode); return; }
    if (m_devState->updatingFromDevice) return;
    m_devMgr->activeCtrl()->setWorkMode(0);
    m_devMgr->activeCtrl()->queryImageParams();
}

void DeviceInteractionController::onRadioModeIdentifyClicked() {
    if (!requireConnected()) { revertRadioMode(ui->radioModeOff, ui->radioModeIdentify, ui->radioModeAutoTrack, m_devState->previousWorkMode); return; }
    if (m_devState->updatingFromDevice) return;
    m_devMgr->activeCtrl()->setWorkMode(1);
    m_devMgr->activeCtrl()->queryImageParams();
}

void DeviceInteractionController::onRadioModeAutoTrackClicked() {
    if (!requireConnected()) { revertRadioMode(ui->radioModeOff, ui->radioModeIdentify, ui->radioModeAutoTrack, m_devState->previousWorkMode); return; }
    if (m_devState->updatingFromDevice) return;
    m_devMgr->activeCtrl()->setWorkMode(2);
    m_devMgr->activeCtrl()->queryImageParams();
}

//============================================================================
// 下拉框与按钮
//============================================================================
void DeviceInteractionController::onAlgoModelChanged(int index)
{
    if (!requireConnected()) { ui->comboAlgoModel->blockSignals(true); ui->comboAlgoModel->setCurrentIndex(m_devState->previousAlgoModel % 10); ui->comboAlgoModel->blockSignals(false); return; }
    if (m_devState->updatingFromDevice) return;
    m_devMgr->activeCtrl()->setAlgoModel(index);
    m_devMgr->activeCtrl()->queryImageParams();
}

void DeviceInteractionController::onDisplayModeChanged(int index)
{
    if (!requireConnected()) { ui->comboDisplayMode->blockSignals(true); ui->comboDisplayMode->setCurrentIndex(m_devState->previousDisplayMode); ui->comboDisplayMode->blockSignals(false); return; }
    if (m_devState->updatingFromDevice) return;
    syncLensTargetByDisplayMode(index);
    m_devMgr->activeCtrl()->setDisplayMode(index);
}

void DeviceInteractionController::onLensTargetChanged(int)
{
}

void DeviceInteractionController::onPtzMoveToClicked()
{
}

void DeviceInteractionController::onSettingsClicked()
{
    SettingsDialog dlg(m_cfg, m_devMgr->activeDeviceIp(), m_mainWindow);
    dlg.exec();
}

void DeviceInteractionController::onSetLocationClicked()
{
    QString lat = ui->editSetLat->text().trimmed();
    QString lon = ui->editSetLon->text().trimmed();

    if (lat.isEmpty() || lon.isEmpty()) {
        QMessageBox::warning(m_mainWindow, QString::fromUtf8("\u53c2\u6570\u9519\u8bef"),
                             QString::fromUtf8("\u8bf7\u586b\u5199\u5b8c\u6574\u7684\u7ecf\u7eac\u5ea6\u53c2\u6570"));
        return;
    }

    if (!requireConnected()) return;
    m_devMgr->activeCtrl()->setLocation(lat, lon);
    m_statusBar->showMessage(QString::fromUtf8("\u5df2\u4e0b\u53d1\u7ecf\u7eac\u5ea6"), 3000);
}

void DeviceInteractionController::onGetImageParamsClicked()
{
    if (!requireConnected()) return;
    m_devMgr->activeCtrl()->queryImageParams();
    m_statusBar->showMessage(QString::fromUtf8("\u5df2\u53d1\u9001\u53c2\u6570\u67e5\u8be2\u8bf7\u6c42"), 3000);
}

//============================================================================
// syncLensTargetByDisplayMode
//============================================================================
void DeviceInteractionController::syncLensTargetByDisplayMode(int pipShow)
{
    int target = 0;
    if (pipShow == 1 || pipShow == 4)
        target = 1;

    if (ui->comboLensTarget->currentIndex() != target)
        ui->comboLensTarget->setCurrentIndex(target);
}

//============================================================================
// 设备树双击 - 分配网格 + 启动 RTSP
//============================================================================
void DeviceInteractionController::onDeviceTreeDoubleClicked(const QString &name, const QString &ip, const QString &rtspUrl)
{
    auto *dc = m_devMgr->device(ip);
    if (!dc) {
        dc = new DeviceContext(ip, m_cfg, m_devMgr);
        m_devMgr->setupDeviceContext(dc, ip, name);
    }

    for (int i = 0; i < m_videoGrid->cellCount(); ++i) {
        if (m_videoGrid->cellAt(i)->channelLabel() == name && m_gridCellMap.value(i) == ip) {
            m_videoGrid->selectCell(i);
            if (!rtspUrl.isEmpty())
                dc->startRtsp(rtspUrl);
            switchActiveDevice(ip);
            m_statusBar->showMessage(QStringLiteral("\u5df2\u5207\u6362 %1 \u2192 \u753b\u9762%2").arg(name).arg(i + 1), 2000);
            return;
        }
    }

    int idx = m_videoGrid->selectedCell();
    if (idx < 0 || m_gridCellMap.contains(idx)) {
        for (int i = 0; i < m_videoGrid->cellCount(); ++i) {
            if (m_gridCellMap.contains(i)) continue;
            idx = i;
            break;
        }
        if (idx < 0) {
            m_statusBar->showMessage(QStringLiteral("\u65e0\u7a7a\u4f59\u753b\u9762"), 2000);
            return;
        }
    }

    m_gridCellMap[idx] = ip;
    m_videoGrid->assignChannel(idx, name, rtspUrl);

    if (!rtspUrl.isEmpty())
        dc->startRtsp(rtspUrl);
    disconnect(dc, &DeviceContext::frameReady, nullptr, nullptr);
    connect(dc, &DeviceContext::frameReady, this, [this, idx, ip](const QImage &frame) {
        auto *vw = m_videoGrid->cellAt(idx);
        if (vw) vw->setFrame(frame);
        if (m_mapCtrl->pipDialog()->isVisible())
            m_mapCtrl->pipVideo()->setFrame(frame);
    });

    switchActiveDevice(ip);
    m_statusBar->showMessage(QStringLiteral("\u5df2\u5206\u914d %1 \u2192 \u753b\u9762%2").arg(name).arg(idx + 1), 2000);
}

//============================================================================
// onGridCellSelected
//============================================================================
void DeviceInteractionController::onGridCellSelected(int cellIndex)
{
    QString ip = m_gridCellMap.value(cellIndex);
    if (!ip.isEmpty())
        switchActiveDevice(ip);
}

//============================================================================
// onDrawerToggled
//============================================================================
void DeviceInteractionController::onDrawerToggled()
{
    m_drawerVisible = !m_drawerVisible;
    m_drawerPanel->setVisible(m_drawerVisible);
    m_drawerToggleBtn->setText(m_drawerVisible ? QStringLiteral("\u25C0") : QStringLiteral("\u25B6"));
    if (ui->videoGridContainer) {
        int ch = ui->videoGridContainer->height();
        int bh = m_drawerToggleBtn->height();
        m_drawerToggleBtn->move(m_drawerVisible ? 240 : 0, ch > bh ? (ch - bh) / 2 : 0);
    }
}

//============================================================================
// onVideoSelection
//============================================================================
void DeviceInteractionController::onVideoSelection(int cx, int cy, int pw, int ph)
{
    m_statusBar->showMessage(
        QString::fromUtf8("\u6846\u9009\u8ddf\u8e2a: \u50cf\u7d20\u4e2d\u5fc3(%1,%2) \u5bbd%3\u9ad8%4")
            .arg(cx).arg(cy).arg(pw).arg(ph));

    if (!requireConnected()) return;
    m_devMgr->activeCtrl()->setBoxTrack(cx, cy, pw, ph);
}

//============================================================================
// handleResize
//============================================================================
void DeviceInteractionController::handleResize()
{
    if (m_drawerToggleBtn && ui->videoGridContainer) {
        int x = m_drawerVisible ? 240 : 0;
        int h = ui->videoGridContainer->height();
        int bh = m_drawerToggleBtn->height();
        m_drawerToggleBtn->move(x, h > bh ? (h - bh) / 2 : 0);
    }
}
