#include "MainWindowControlService.h"
#include "../main/MainPresenter.h"
#include "../../infrastructure/pelcodprotocol.h"
#include "../../infrastructure/configmanager.h"
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QStatusBar>
#include <QStyle>

MainWindowControlService::MainWindowControlService(QObject* parent)
    : QObject(parent)
{
}

void MainWindowControlService::setup(const ControlWidgets& widgets, ConfigManager* cfg,
                                     MainPresenter* presenter,
                                     std::function<bool()> requireConnected,
                                     std::function<bool()> requireMotorReady)
{
    m_w = widgets;
    m_cfg = cfg;
    m_presenter = presenter;
    m_requireConnected = requireConnected;
    m_requireMotorReady = requireMotorReady;

    // ── PTZ 八方向按钮 ──
    auto connectPtzBtn = [&](QPushButton* btn, PtzDir dir) {
        connect(btn, &QPushButton::pressed, this, [this, dir]() {
            if (!m_requireConnected()) return;
            m_presenter->ptzMove(static_cast<int>(dir));
        });
        connect(btn, &QPushButton::released, this, [this]() {
            if (!m_presenter->isDeviceConnected()) return;
            m_presenter->ptzStop();
        });
    };
    connectPtzBtn(m_w.ptzUp, PtzDir::Up);
    connectPtzBtn(m_w.ptzDown, PtzDir::Down);
    connectPtzBtn(m_w.ptzLeft, PtzDir::Left);
    connectPtzBtn(m_w.ptzRight, PtzDir::Right);
    connectPtzBtn(m_w.ptzTopLeft, PtzDir::UpLeft);
    connectPtzBtn(m_w.ptzTopRight, PtzDir::UpRight);
    connectPtzBtn(m_w.ptzBottomLeft, PtzDir::DownLeft);
    connectPtzBtn(m_w.ptzBottomRight, PtzDir::DownRight);

    // ── 云台速度控制 ──
    m_w.sliderSpeed->setValue(m_cfg->ptz().panSpeed);
    m_w.spinSpeed->setValue(m_cfg->ptz().panSpeed);
    connect(m_w.sliderSpeed, &QSlider::valueChanged, m_w.spinSpeed, &QSpinBox::setValue);
    connect(m_w.spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), m_w.sliderSpeed, &QSlider::setValue);
    connect(m_w.spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_cfg->ptz().panSpeed = static_cast<quint8>(val);
        m_cfg->ptz().tiltSpeed = static_cast<quint8>(val);
        m_cfg->save();
    });

    // ── 镜头控制 ──
    auto connectLensBtn = [&](QPushButton* btn, int op) {
        connect(btn, &QPushButton::pressed, this, [this, op]() {
            if (!m_requireConnected()) return;
            m_presenter->lensMove(op);
        });
        connect(btn, &QPushButton::released, this, [this]() {
            if (!m_presenter->isDeviceConnected()) return;
            m_presenter->lensStop();
        });
    };
    connectLensBtn(m_w.zoomIn, 0);
    connectLensBtn(m_w.zoomOut, 1);
    connectLensBtn(m_w.focusIn, 2);
    connectLensBtn(m_w.focusOut, 3);

    // ── 镜头速度控制 ──
    m_w.sliderZoomSpeed->setValue(m_cfg->lens().zoomSpeed);
    m_w.spinZoomSpeed->setValue(m_cfg->lens().zoomSpeed);
    connect(m_w.sliderZoomSpeed, &QSlider::valueChanged, m_w.spinZoomSpeed, &QSpinBox::setValue);
    connect(m_w.spinZoomSpeed, QOverload<int>::of(&QSpinBox::valueChanged), m_w.sliderZoomSpeed, &QSlider::setValue);
    connect(m_w.spinZoomSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_cfg->lens().zoomSpeed = static_cast<quint8>(val);
        m_cfg->save();
    });

    // ── 预置位控制 ──
    connect(m_w.callPreset, &QPushButton::clicked, m_presenter, &MainPresenter::on_btnCallPreset_clicked);
    connect(m_w.setPreset, &QPushButton::clicked, m_presenter, &MainPresenter::on_btnSetPreset_clicked);
    connect(m_w.delPreset, &QPushButton::clicked, m_presenter, &MainPresenter::on_btnDelPreset_clicked);
    connect(m_w.ptzReset, &QPushButton::clicked, m_presenter, &MainPresenter::on_btnPtzReset_clicked);

    // ── 附加功能开关 ──
    connect(m_w.checkDigitalZoom, &QCheckBox::toggled, m_presenter, &MainPresenter::onCheckDigitalZoomToggled);
    connect(m_w.checkAutoZoom, &QCheckBox::toggled, m_presenter, &MainPresenter::onCheckAutoZoomToggled);
    connect(m_w.checkCaptureUpload, &QCheckBox::toggled, m_presenter, &MainPresenter::onCheckCaptureUploadToggled);
    connect(m_w.checkPosReset, &QCheckBox::toggled, m_presenter, &MainPresenter::onCheckPosResetToggled);

    // ── 雨刷电机控制 ──
    connect(m_w.btnWiperStart, &QPushButton::clicked, m_presenter, &MainPresenter::onWiperStart);
    connect(m_w.btnWiperStop, &QPushButton::clicked, m_presenter, &MainPresenter::onWiperStop);

    connect(m_w.btnWiperLeft, &QPushButton::pressed, m_presenter, &MainPresenter::onWiperJogLeft);
    connect(m_w.btnWiperLeft, &QPushButton::released, m_presenter, &MainPresenter::onWiperJogStop);
    connect(m_w.btnWiperRight, &QPushButton::pressed, m_presenter, &MainPresenter::onWiperJogRight);
    connect(m_w.btnWiperRight, &QPushButton::released, m_presenter, &MainPresenter::onWiperJogStop);

    connect(m_w.btnWiperZeroCalib, &QPushButton::clicked, m_presenter, &MainPresenter::onWiperZeroCalib);
    connect(m_w.btnWiperMode, &QPushButton::clicked, m_presenter, &MainPresenter::onWiperMode);
    connect(m_w.btnWiperSilent, &QPushButton::clicked, m_presenter, &MainPresenter::onWiperSilent);
    connect(m_w.editWiperCurrent, &QLineEdit::editingFinished, this, [this]() {
        if (!m_requireMotorReady()) return;
        int ma = m_w.editWiperCurrent->text().toInt();
        m_presenter->onWiperCurrentSet();
        m_w.statusbar->showMessage(
            QString("正在下发并固化电机电流: %1 mA").arg(ma), 3000);
    });

    // ── 电机信号转发 ──
    connect(m_presenter, &MainPresenter::motorModeChanged, this, [this](bool isManual) {
        m_w.statWiperStatus->setText(isManual ? "手动" : "自动");
    });
    connect(m_presenter, &MainPresenter::motorSerialErrorOccurred, this, [this](const QString& msg) {
        m_w.statWiperStatus->setText("故障");
        qWarning() << "电机串口错误:" << msg;
    });
    connect(m_presenter, &MainPresenter::motorTcpErrorOccurred, this, [this](const QString& msg) {
        m_w.statWiperStatus->setText("故障");
        qWarning() << "电机TCP错误:" << msg;
    });
    connect(m_presenter, &MainPresenter::motorSilentChanged, this, [this](bool isSilent) {
        m_w.btnWiperSilent->setText(isSilent ? "狂暴模式" : "静音模式");
        m_w.statusbar->showMessage(isSilent
            ? "电机已切换为：静音模式 (StealthChop)"
            : "电机已切换为：狂暴模式 (SpreadCycle)", 3000);
    });
}

void MainWindowControlService::updateMotorButtons()
{
    bool isModbus = (m_cfg->motorProtocol() == "MODBUS-RTU");
    bool isTcp = (m_cfg->motorProtocol() == "STM32-TCP-V4.0");
    bool isPelco = (m_cfg->motorProtocol() == "Pelco-D");

    bool othersEnabled = !isPelco;
    m_w.btnWiperLeft->setEnabled(othersEnabled);
    m_w.btnWiperRight->setEnabled(othersEnabled);
    m_w.btnWiperZeroCalib->setEnabled(othersEnabled);
    m_w.btnWiperMode->setEnabled(othersEnabled);
    m_w.btnWiperSilent->setEnabled(isTcp);

    if (isModbus && m_presenter->isMotorSerialOpen()) {
        m_presenter->checkMotorMode();
    }
}

void MainWindowControlService::refreshStyle(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
}
