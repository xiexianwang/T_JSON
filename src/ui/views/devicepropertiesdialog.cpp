#include "devicepropertiesdialog.h"
#include "wheelredirectfilter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QAbstractSpinBox>
#include <QEvent>
#include <QCheckBox>
#include <QMessageBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QScrollArea>
#include <QSerialPortInfo>

DevicePropertiesDialog::DevicePropertiesDialog(const QString& deviceIp, DeviceConfig* cfg, QWidget* parent)
    : QDialog(parent)
    , m_deviceIp(deviceIp)
    , m_cfg(cfg)
{
    setWindowTitle(QStringLiteral("设备属性 - %1").arg(deviceIp));
    setMinimumWidth(460);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setupUi();
    loadFromConfig();
    // 滚轮只作用于滚动区域：悬停在任意输入控件上时，滚轮滚动面板而非改值。
    WheelRedirectFilter::install(m_scroll, this);
    connect(m_comboMotorProtocol, &QComboBox::currentTextChanged, this, &DevicePropertiesDialog::updateProtocolControls);
    updateProtocolControls();
}

void DevicePropertiesDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 把内容放进滚动区域，避免弹窗过高
    auto* scroll = new QScrollArea(this);
    m_scroll = scroll;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* content = new QWidget();
    content->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(content);
    mainLayout->addWidget(scroll);

    // ── 电机配置 ──
    auto* motorGroup = new QGroupBox(QStringLiteral("电机配置"));
    auto* motorLayout = new QFormLayout(motorGroup);

    m_checkMotorSerialEnabled = new QCheckBox(QStringLiteral("启用串口"));
    motorLayout->addRow(m_checkMotorSerialEnabled);

    m_checkMotorIpEnabled = new QCheckBox(QStringLiteral("启用 TCP"));
    motorLayout->addRow(m_checkMotorIpEnabled);

    m_comboMotorProtocol = new QComboBox();
    m_comboMotorProtocol->addItems({"Pelco-D", "MODBUS-RTU", "STM32-TCP-V4.0"});
    motorLayout->addRow(QStringLiteral("协议:"), m_comboMotorProtocol);

    m_comboMotorCommandChannel = new QComboBox();
    m_comboMotorCommandChannel->addItems({"Pelco-D", "串口"});
    motorLayout->addRow(QStringLiteral("指令通道:"), m_comboMotorCommandChannel);

    m_comboMotorComPort = new QComboBox();
    for (const auto& info : QSerialPortInfo::availablePorts())
        m_comboMotorComPort->addItem(info.portName());
    motorLayout->addRow(QStringLiteral("串口号:"), m_comboMotorComPort);

    m_editMotorTcpIp = new QLineEdit();
    motorLayout->addRow(QStringLiteral("TCP IP:"), m_editMotorTcpIp);

    m_spinMotorTcpPort = new QSpinBox();
    m_spinMotorTcpPort->setRange(1, 65535);
    motorLayout->addRow(QStringLiteral("TCP 端口:"), m_spinMotorTcpPort);

    contentLayout->addWidget(motorGroup);

    // ── 转台配置 ──
    auto* ptzGroup = new QGroupBox(QStringLiteral("转台配置"));
    auto* ptzLayout = new QFormLayout(ptzGroup);

    m_checkSerialServerEnabled = new QCheckBox(QStringLiteral("启用串口服务器"));
    ptzLayout->addRow(m_checkSerialServerEnabled);

    m_checkTurntableIpEnabled = new QCheckBox(QStringLiteral("启用转台 IP"));
    ptzLayout->addRow(m_checkTurntableIpEnabled);

    m_editSerialIp = new QLineEdit();
    ptzLayout->addRow(QStringLiteral("串口服务器 IP:"), m_editSerialIp);

    m_spinSerialPort = new QSpinBox();
    m_spinSerialPort->setRange(1, 65535);
    ptzLayout->addRow(QStringLiteral("串口服务器端口:"), m_spinSerialPort);

    m_spinMockServerPort = new QSpinBox();
    m_spinMockServerPort->setRange(1, 65535);
    ptzLayout->addRow(QStringLiteral("Mock 端口:"), m_spinMockServerPort);

    m_spinPtzPanOffset = new QDoubleSpinBox();
    m_spinPtzPanOffset->setRange(-360.0, 360.0);
    m_spinPtzPanOffset->setDecimals(2);
    m_spinPtzPanOffset->setSingleStep(0.5);
    ptzLayout->addRow(QStringLiteral("Pan 偏移 (°):"), m_spinPtzPanOffset);

    m_spinPtzTiltOffset = new QDoubleSpinBox();
    m_spinPtzTiltOffset->setRange(-360.0, 360.0);
    m_spinPtzTiltOffset->setDecimals(2);
    m_spinPtzTiltOffset->setSingleStep(0.5);
    ptzLayout->addRow(QStringLiteral("Tilt 偏移 (°):"), m_spinPtzTiltOffset);

    m_checkSoftwarePtzCalibration = new QCheckBox(QStringLiteral("软件标定"));
    ptzLayout->addRow(m_checkSoftwarePtzCalibration);

    contentLayout->addWidget(ptzGroup);

    // ── 云台(PTZ)配置 ──
    auto* ptzProtoGroup = new QGroupBox(QStringLiteral("云台协议配置"));
    auto* ptzProtoLayout = new QFormLayout(ptzProtoGroup);

    m_comboPtzProtocol = new QComboBox();
    m_comboPtzProtocol->addItems({"Pelco-D", "LPP", "N-MD"});
    ptzProtoLayout->addRow(QStringLiteral("协议:"), m_comboPtzProtocol);

    m_spinPtzAddress = new QSpinBox();
    m_spinPtzAddress->setRange(1, 255);
    ptzProtoLayout->addRow(QStringLiteral("地址:"), m_spinPtzAddress);

    m_spinPanSpeed = new QSpinBox();
    m_spinPanSpeed->setRange(0, 63);
    ptzProtoLayout->addRow(QStringLiteral("水平速度:"), m_spinPanSpeed);

    m_spinTiltSpeed = new QSpinBox();
    m_spinTiltSpeed->setRange(0, 63);
    ptzProtoLayout->addRow(QStringLiteral("垂直速度:"), m_spinTiltSpeed);

    contentLayout->addWidget(ptzProtoGroup);

    // ── 镜头配置 ──
    auto* lensGroup = new QGroupBox(QStringLiteral("镜头配置"));
    auto* lensLayout = new QFormLayout(lensGroup);

    m_comboVisProtocol = new QComboBox();
    m_comboVisProtocol->addItems({"VISCA", "Pelco-D"});
    lensLayout->addRow(QStringLiteral("可见光协议:"), m_comboVisProtocol);

    m_spinVisAddress = new QSpinBox();
    m_spinVisAddress->setRange(1, 255);
    lensLayout->addRow(QStringLiteral("可见光地址:"), m_spinVisAddress);

    m_comboIrProtocol = new QComboBox();
    m_comboIrProtocol->addItems({"Pelco-D", "IRAY"});
    lensLayout->addRow(QStringLiteral("红外协议:"), m_comboIrProtocol);

    m_spinIrAddress = new QSpinBox();
    m_spinIrAddress->setRange(1, 255);
    lensLayout->addRow(QStringLiteral("红外地址:"), m_spinIrAddress);

    m_spinZoomSpeed = new QSpinBox();
    m_spinZoomSpeed->setRange(0, 255);
    lensLayout->addRow(QStringLiteral("镜头速度:"), m_spinZoomSpeed);

    contentLayout->addWidget(lensGroup);

    // ── 光学与相机参数 ──
    auto* camGroup = new QGroupBox(QStringLiteral("光学与相机参数"));
    auto* camLayout = new QFormLayout(camGroup);

    m_spinVisPixelSize = new QDoubleSpinBox();
    m_spinVisPixelSize->setRange(0.1, 50.0);
    m_spinVisPixelSize->setDecimals(2);
    m_spinVisPixelSize->setSuffix(QStringLiteral(" μm"));
    camLayout->addRow(QStringLiteral("可见光像元尺寸:"), m_spinVisPixelSize);

    m_editVisResolution = new QLineEdit();
    m_editVisResolution->setPlaceholderText(QStringLiteral("2688x1520"));
    m_editVisResolution->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d{1,5}x\\d{1,5}$")),
                                        m_editVisResolution));
    camLayout->addRow(QStringLiteral("可见光分辨率:"), m_editVisResolution);

    m_spinVisMinFocal = new QDoubleSpinBox();
    m_spinVisMinFocal->setRange(1.0, 1000.0);
    m_spinVisMinFocal->setSuffix(QStringLiteral(" mm"));
    camLayout->addRow(QStringLiteral("可见光最小焦距:"), m_spinVisMinFocal);

    m_spinIrPixelSize = new QDoubleSpinBox();
    m_spinIrPixelSize->setRange(0.1, 50.0);
    m_spinIrPixelSize->setDecimals(2);
    m_spinIrPixelSize->setSuffix(QStringLiteral(" μm"));
    camLayout->addRow(QStringLiteral("红外像元尺寸:"), m_spinIrPixelSize);

    m_editIrResolution = new QLineEdit();
    m_editIrResolution->setPlaceholderText(QStringLiteral("640x512"));
    m_editIrResolution->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d{1,5}x\\d{1,5}$")),
                                        m_editIrResolution));
    camLayout->addRow(QStringLiteral("红外分辨率:"), m_editIrResolution);

    m_spinIrMinFocal = new QDoubleSpinBox();
    m_spinIrMinFocal->setRange(1.0, 1000.0);
    m_spinIrMinFocal->setSuffix(QStringLiteral(" mm"));
    camLayout->addRow(QStringLiteral("红外最小焦距:"), m_spinIrMinFocal);

    contentLayout->addWidget(camGroup);

    // ── 视觉测距参考尺寸 ──
    auto* refGroup = new QGroupBox(QStringLiteral("视觉测距参考尺寸"));
    auto* refLayout = new QFormLayout(refGroup);
    auto makeRefSpin = []() {
        auto* s = new QDoubleSpinBox();
        s->setRange(0.1, 100.0);
        s->setDecimals(2);
        s->setSingleStep(0.05);
        s->setSuffix(QStringLiteral(" m"));
        return s;
    };
    m_spinRef_2_161 = makeRefSpin(); refLayout->addRow(QStringLiteral("人:"), m_spinRef_2_161);
    m_spinRef_2_162 = makeRefSpin(); refLayout->addRow(QStringLiteral("车:"), m_spinRef_2_162);
    m_spinRef_3_163 = makeRefSpin(); refLayout->addRow(QStringLiteral("船:"), m_spinRef_3_163);
    m_spinRef_4_164 = makeRefSpin(); refLayout->addRow(QStringLiteral("无人机:"), m_spinRef_4_164);
    m_spinRef_5_161 = makeRefSpin(); refLayout->addRow(QStringLiteral("飞机:"), m_spinRef_5_161);
    m_spinRef_5_162 = makeRefSpin(); refLayout->addRow(QStringLiteral("直升机:"), m_spinRef_5_162);
    m_spinRef_6_163 = makeRefSpin(); refLayout->addRow(QStringLiteral("鸟:"), m_spinRef_6_163);

    contentLayout->addWidget(refGroup);

    // ── 附加功能开关 ──
    auto* switchesGroup = new QGroupBox(QStringLiteral("附加功能开关"));
    auto* switchesLayout = new QGridLayout(switchesGroup);
    m_checkDigitalZoom = new QCheckBox(QStringLiteral("数字变倍"));
    m_checkAutoZoom = new QCheckBox(QStringLiteral("自动变倍"));
    m_checkCaptureUpload = new QCheckBox(QStringLiteral("抓拍上传"));
    m_checkPosReset = new QCheckBox(QStringLiteral("位置复位"));
    switchesLayout->addWidget(m_checkDigitalZoom, 0, 0);
    switchesLayout->addWidget(m_checkAutoZoom, 0, 1);
    switchesLayout->addWidget(m_checkCaptureUpload, 1, 0);
    switchesLayout->addWidget(m_checkPosReset, 1, 1);

    contentLayout->addWidget(switchesGroup);

    // ── 按钮 ──
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &DevicePropertiesDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void DevicePropertiesDialog::loadFromConfig()
{
    m_checkMotorSerialEnabled->setChecked(m_cfg->motorSerialEnabled);
    m_checkMotorIpEnabled->setChecked(m_cfg->motorIpEnabled);
    m_comboMotorProtocol->setCurrentText(m_cfg->motorProtocol);
    m_comboMotorCommandChannel->setCurrentText(m_cfg->motorCommandChannel);
    m_comboMotorComPort->setCurrentText(m_cfg->motorComPort);
    m_editMotorTcpIp->setText(m_cfg->motorTcpIp);
    m_spinMotorTcpPort->setValue(m_cfg->motorTcpPort);

    m_checkSerialServerEnabled->setChecked(m_cfg->serialServerEnabled);
    m_checkTurntableIpEnabled->setChecked(m_cfg->turntableIpEnabled);
    m_editSerialIp->setText(m_cfg->serialIp);
    m_spinSerialPort->setValue(m_cfg->serialPort);
    m_spinMockServerPort->setValue(m_cfg->mockServerPort);
    m_spinPtzPanOffset->setValue(m_cfg->ptzPanOffset);
    m_spinPtzTiltOffset->setValue(m_cfg->ptzTiltOffset);
    m_checkSoftwarePtzCalibration->setChecked(m_cfg->softwarePtzCalibrationEnabled);

    m_comboPtzProtocol->setCurrentText(m_cfg->ptzProtocol);
    m_spinPtzAddress->setValue(m_cfg->ptzAddress);
    m_spinPanSpeed->setValue(m_cfg->panSpeed);
    m_spinTiltSpeed->setValue(m_cfg->tiltSpeed);

    m_comboVisProtocol->setCurrentText(m_cfg->visProtocol);
    m_spinVisAddress->setValue(m_cfg->visAddress);
    m_comboIrProtocol->setCurrentText(m_cfg->irProtocol);
    m_spinIrAddress->setValue(m_cfg->irAddress);
    m_spinZoomSpeed->setValue(m_cfg->zoomSpeed);

    m_spinVisPixelSize->setValue(m_cfg->visPixelSize);
    m_editVisResolution->setText(QString("%1x%2").arg(m_cfg->visResX).arg(m_cfg->visResY));
    m_spinVisMinFocal->setValue(m_cfg->visMinFocal);
    m_spinIrPixelSize->setValue(m_cfg->irPixelSize);
    m_editIrResolution->setText(QString("%1x%2").arg(m_cfg->irResX).arg(m_cfg->irResY));
    m_spinIrMinFocal->setValue(m_cfg->irMinFocal);

    m_spinRef_2_161->setValue(m_cfg->targetRefSize(2, 0xA1));
    m_spinRef_2_162->setValue(m_cfg->targetRefSize(2, 0xA2));
    m_spinRef_3_163->setValue(m_cfg->targetRefSize(3, 0xA3));
    m_spinRef_4_164->setValue(m_cfg->targetRefSize(4, 0xA4));
    m_spinRef_5_161->setValue(m_cfg->targetRefSize(5, 0xA1));
    m_spinRef_5_162->setValue(m_cfg->targetRefSize(5, 0xA2));
    m_spinRef_6_163->setValue(m_cfg->targetRefSize(6, 0xA3));
    m_checkDigitalZoom->setChecked(m_cfg->digitalZoom);
    m_checkAutoZoom->setChecked(m_cfg->autoZoom);
    m_checkCaptureUpload->setChecked(m_cfg->captureUpload);
    m_checkPosReset->setChecked(m_cfg->posReset);
}

void DevicePropertiesDialog::saveToConfig()
{
    m_cfg->motorSerialEnabled = m_checkMotorSerialEnabled->isChecked();
    m_cfg->motorIpEnabled = m_checkMotorIpEnabled->isChecked();
    m_cfg->motorProtocol = m_comboMotorProtocol->currentText();
    m_cfg->motorCommandChannel = m_comboMotorCommandChannel->currentText();
    m_cfg->motorComPort = m_comboMotorComPort->currentText();
    m_cfg->motorTcpIp = m_editMotorTcpIp->text();
    m_cfg->motorTcpPort = static_cast<quint16>(m_spinMotorTcpPort->value());

    m_cfg->serialServerEnabled = m_checkSerialServerEnabled->isChecked();
    m_cfg->turntableIpEnabled = m_checkTurntableIpEnabled->isChecked();
    m_cfg->serialIp = m_editSerialIp->text();
    m_cfg->serialPort = static_cast<quint16>(m_spinSerialPort->value());
    m_cfg->mockServerPort = static_cast<quint16>(m_spinMockServerPort->value());
    m_cfg->ptzPanOffset = m_spinPtzPanOffset->value();
    m_cfg->ptzTiltOffset = m_spinPtzTiltOffset->value();
    m_cfg->softwarePtzCalibrationEnabled = m_checkSoftwarePtzCalibration->isChecked();

    m_cfg->ptzProtocol = m_comboPtzProtocol->currentText();
    m_cfg->ptzAddress = static_cast<quint8>(m_spinPtzAddress->value());
    m_cfg->panSpeed = static_cast<quint8>(m_spinPanSpeed->value());
    m_cfg->tiltSpeed = static_cast<quint8>(m_spinTiltSpeed->value());

    m_cfg->visProtocol = m_comboVisProtocol->currentText();
    m_cfg->visAddress = static_cast<quint8>(m_spinVisAddress->value());
    m_cfg->irProtocol = m_comboIrProtocol->currentText();
    m_cfg->irAddress = static_cast<quint8>(m_spinIrAddress->value());
    m_cfg->zoomSpeed = static_cast<quint8>(m_spinZoomSpeed->value());

    m_cfg->visPixelSize = m_spinVisPixelSize->value();
    m_cfg->visMinFocal = m_spinVisMinFocal->value();
    auto visRes = m_editVisResolution->text().split('x');
    auto setRes = [](const QString& text, int& x, int& y) {
        auto p = text.split('x');
        if (p.size() == 2) { x = p[0].toInt(); y = p[1].toInt(); }
    };
    setRes(m_editVisResolution->text(), m_cfg->visResX, m_cfg->visResY);
    m_cfg->irPixelSize = m_spinIrPixelSize->value();
    m_cfg->irMinFocal = m_spinIrMinFocal->value();
    setRes(m_editIrResolution->text(), m_cfg->irResX, m_cfg->irResY);

    auto saveRef = [&](QDoubleSpinBox* s, int ml, int cc) {
        m_cfg->targetRefMap[(ml << 8) | cc] = s->value();
    };
    saveRef(m_spinRef_2_161, 2, 0xA1);
    saveRef(m_spinRef_2_162, 2, 0xA2);
    saveRef(m_spinRef_3_163, 3, 0xA3);
    saveRef(m_spinRef_4_164, 4, 0xA4);
    saveRef(m_spinRef_5_161, 5, 0xA1);
    saveRef(m_spinRef_5_162, 5, 0xA2);
    saveRef(m_spinRef_6_163, 6, 0xA3);

    m_cfg->digitalZoom = m_checkDigitalZoom->isChecked();
    m_cfg->autoZoom = m_checkAutoZoom->isChecked();
    m_cfg->captureUpload = m_checkCaptureUpload->isChecked();
    m_cfg->posReset = m_checkPosReset->isChecked();
}

void DevicePropertiesDialog::onAccepted()
{
    // 提交期复校验：分辨率必须是 "宽x高" 形式（QValidator 允许空中间态）
    for (QLineEdit* le : {m_editVisResolution, m_editIrResolution}) {
        if (!le->hasAcceptableInput()) {
            QMessageBox::warning(this, QStringLiteral("输入错误"),
                                 QStringLiteral("分辨率格式应为 “宽x高”，例如 2688x1520。"));
            le->setFocus();
            return;
        }
    }
    saveToConfig();
    accept();
}

void DevicePropertiesDialog::updateProtocolControls()
{
    QString protocol = m_comboMotorProtocol->currentText();
    bool isModbus = (protocol == "MODBUS-RTU");
    bool isTcp = (protocol == "STM32-TCP-V4.0");

    m_comboMotorComPort->setEnabled(isModbus);
    m_comboMotorCommandChannel->setEnabled(isModbus);
    m_editMotorTcpIp->setEnabled(isTcp);
    m_spinMotorTcpPort->setEnabled(isTcp);
}
