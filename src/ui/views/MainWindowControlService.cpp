#include "MainWindowControlService.h"
#include "../main/MainPresenter.h"
#include "../../infrastructure/pelcodprotocol.h"
#include "../../infrastructure/configmanager.h"
#include "../../service/DeviceManager.h"
#include "../../service/DeviceContext.h"
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QIntValidator>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>

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
    auto currentDevCfg = [this]() -> DeviceConfig* {
        auto* ctx = DeviceManager::instance().getDevice(m_presenter->currentDeviceId());
        return ctx ? &ctx->deviceConfig() : nullptr;
    };
    m_w.sliderSpeed->setValue(currentDevCfg() ? currentDevCfg()->panSpeed : 63);
    m_w.spinSpeed->setValue(currentDevCfg() ? currentDevCfg()->panSpeed : 63);
    connect(m_w.sliderSpeed, &QSlider::valueChanged, m_w.spinSpeed, &QSpinBox::setValue);
    connect(m_w.spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), m_w.sliderSpeed, &QSlider::setValue);
    connect(m_w.spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, currentDevCfg](int val) {
        if (auto* cfg = currentDevCfg()) {
            cfg->panSpeed = static_cast<quint8>(val);
            cfg->tiltSpeed = static_cast<quint8>(val);
        }
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
    m_w.sliderZoomSpeed->setValue(currentDevCfg() ? currentDevCfg()->zoomSpeed : 5);
    m_w.spinZoomSpeed->setValue(currentDevCfg() ? currentDevCfg()->zoomSpeed : 5);
    connect(m_w.sliderZoomSpeed, &QSlider::valueChanged, m_w.spinZoomSpeed, &QSpinBox::setValue);
    connect(m_w.spinZoomSpeed, QOverload<int>::of(&QSpinBox::valueChanged), m_w.sliderZoomSpeed, &QSlider::setValue);
    connect(m_w.spinZoomSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, currentDevCfg](int val) {
        if (auto* cfg = currentDevCfg())
            cfg->zoomSpeed = static_cast<quint8>(val);
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
    const auto onWiperCurrentEdited = [this]() {
        if (!m_requireMotorReady()) return;
        // 提交期复校验：QValidator 允许为空中间态，需在真正下发前拦住非法/空值
        const auto acceptable = [](QLineEdit* le) {
            return le && le->isEnabled() && le->hasAcceptableInput();
        };
        if (!acceptable(m_w.editWiperRunCurrent)
            || (m_motorProtocol == "STM32-TCP-V4.0"
                && (!acceptable(m_w.editWiperHoldCurrent) || !acceptable(m_w.editWiperHoldDelay)))) {
            QMessageBox::warning(m_w.editWiperRunCurrent->window(), QStringLiteral("输入错误"),
                                 QStringLiteral("请输入有效的电流参数。"));
            return;
        }
        m_presenter->onWiperCurrentSet();
        // MODBUS：下发固化后回读实际电流刷新仪表盘
        if (m_motorProtocol == "MODBUS-RTU")
            QTimer::singleShot(200, this, [this]() { m_presenter->readMotorCurrent(); });
        const QString msg = (m_motorProtocol == "STM32-TCP-V4.0")
            ? QString("正在下发电流: 运行%1 保持%2 延迟%3")
                  .arg(m_w.editWiperRunCurrent->text())
                  .arg(m_w.editWiperHoldCurrent->text())
                  .arg(m_w.editWiperHoldDelay->text())
            : QString("正在下发电流: 运行%1 mA")
                  .arg(m_w.editWiperRunCurrent->text());
        m_w.statusbar->showMessage(msg, 3000);
    };
    // 电流设置：仅回车确认才下发（编辑途中失焦不发送）
    connect(m_w.editWiperRunCurrent, &QLineEdit::returnPressed, this, onWiperCurrentEdited);
    connect(m_w.editWiperHoldCurrent, &QLineEdit::returnPressed, this, onWiperCurrentEdited);
    connect(m_w.editWiperHoldDelay, &QLineEdit::returnPressed, this, onWiperCurrentEdited);

    // ── 电机信号转发 ──
    connect(m_presenter, &MainPresenter::motorModeChanged, this, [this](bool isManual) {
        m_motorManual = isManual;
        updateMotorModeLabel();
    });
    connect(m_presenter, &MainPresenter::motorSerialErrorOccurred, this, [this](const QString& msg) {
        if (m_w.editMotorMode) m_w.editMotorMode->setText("故障");
        qWarning() << "电机串口错误:" << msg;
    });
    connect(m_presenter, &MainPresenter::motorTcpErrorOccurred, this, [this](const QString& msg) {
        if (m_w.editMotorMode) m_w.editMotorMode->setText("故障");
        qWarning() << "电机TCP错误:" << msg;
    });
    connect(m_presenter, &MainPresenter::motorSilentChanged, this, [this](bool isSilent) {
        m_w.btnWiperSilent->setText(isSilent ? "狂暴模式" : "静音模式");
        m_motorSilent = isSilent;
        updateMotorModeLabel();
        m_w.statusbar->showMessage(isSilent
            ? "电机已切换为：静音模式 (StealthChop)"
            : "电机已切换为：狂暴模式 (SpreadCycle)", 3000);
    });
    // 仪表盘电机参数：电机上报电流 motor_ack(run/hold/iholddelay) → 只读显示
    connect(m_presenter, &MainPresenter::motorCurrentChanged, this,
            [this](int run, int hold, int delay) {
        if (m_w.editMotorRunCurrent && run >= 0) m_w.editMotorRunCurrent->setText(QString::number(run));
        // hold/delay 为负表示协议不适用（如 MODBUS 仅运行电流），保持原样
        if (m_w.editMotorHoldCurrent && hold >= 0) m_w.editMotorHoldCurrent->setText(QString::number(hold));
        if (m_w.editMotorHoldDelay && delay >= 0) m_w.editMotorHoldDelay->setText(QString::number(delay));
    });
}

void MainWindowControlService::updateMotorModeLabel()
{
    if (!m_w.editMotorMode) return;
    m_w.editMotorMode->setText(m_motorManual
        ? (m_motorSilent ? "手动/静音" : "手动/狂暴")
        : (m_motorSilent ? "自动/静音" : "自动/狂暴"));
}

void MainWindowControlService::updateMotorButtons()
{
    // 从当前设备获取协议（未连接也可用其配置），否则用全局默认
    QString protocol = m_cfg->motorProtocol();
    DeviceContext* ctx = DeviceManager::instance().getDevice(m_presenter->currentDeviceId());
    if (ctx) {
        protocol = ctx->deviceConfig().motorProtocol;
    }
    bool isModbus = (protocol == "MODBUS-RTU");
    bool isTcp = (protocol == "STM32-TCP-V4.0");
    bool isPelco = (protocol == "Pelco-D");

    m_motorProtocol = protocol;
    const bool holdSupported = isTcp;
    if (m_w.editWiperHoldCurrent) m_w.editWiperHoldCurrent->setEnabled(holdSupported);
    if (m_w.editWiperHoldDelay) m_w.editWiperHoldDelay->setEnabled(holdSupported);
    if (m_w.editWiperRunCurrent) {
        m_w.editWiperRunCurrent->setPlaceholderText(holdSupported ? QStringLiteral("1-31")
                                                                  : QStringLiteral("0-2000"));
        // 运行电流范围随协议变化：STM32 → 1-31，其它 → 0-2000
        // 复用已有 validator（避免每次重建泄漏），仅在缺失时新建
        const int lo = holdSupported ? 1 : 0;
        const int hi = holdSupported ? 31 : 2000;
        if (auto* iv = qobject_cast<QIntValidator*>(
                const_cast<QValidator*>(m_w.editWiperRunCurrent->validator()))) {
            iv->setRange(lo, hi);
        } else {
            m_w.editWiperRunCurrent->setValidator(new QIntValidator(lo, hi, m_w.editWiperRunCurrent));
        }
        if (!holdSupported) {
            if (m_w.editWiperHoldCurrent) m_w.editWiperHoldCurrent->clear();
            if (m_w.editWiperHoldDelay) m_w.editWiperHoldDelay->clear();
        }
    }

    bool othersEnabled = !isPelco;
    m_w.btnWiperLeft->setEnabled(othersEnabled);
    m_w.btnWiperRight->setEnabled(othersEnabled);
    m_w.btnWiperZeroCalib->setEnabled(othersEnabled);
    m_w.btnWiperMode->setEnabled(othersEnabled);
    m_w.btnWiperSilent->setEnabled(isTcp);

    if (isModbus && m_presenter->isMotorSerialOpen()) {
        m_presenter->checkMotorMode();
        // 切到 MODBUS 协议时读取实际电流（寄存器 0x000D）刷新仪表盘
        m_presenter->readMotorCurrent();
    }
}

void MainWindowControlService::refreshStyle(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
}
