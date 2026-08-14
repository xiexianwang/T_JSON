// ============================================================
// 文件: settingsdialog.cpp
// 描述: 系统参数设置对话框实现。读取/保存操作全部委托给
//       ConfigManager，不再直接操作 QSettings。
// ============================================================

#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "infrastructure/configmanager.h"
#include <QSerialPortInfo>

SettingsDialog::SettingsDialog(ConfigManager *cfg, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , m_cfg(cfg)
{
    qDebug() << "SettingsDialog: calling setupUi";
    ui->setupUi(this);
    connect(ui->comboMotorProtocol, &QComboBox::currentTextChanged, this, &SettingsDialog::updateProtocolControls);
    qDebug() << "SettingsDialog: calling loadSettings";
    loadSettings();
    updateProtocolControls(); // Initial call
    qDebug() << "SettingsDialog: constructor finished";
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::loadSettings()
{
    ui->comboPtzProtocol->setCurrentText(m_cfg->ptz().protocol);
    ui->spinPtzAddress->setValue(m_cfg->ptz().address);

    ui->comboVisProtocol->setCurrentText(m_cfg->lens().visProtocol);
    ui->spinVisAddress->setValue(m_cfg->lens().visAddress);

    ui->comboIrProtocol->setCurrentText(m_cfg->lens().irProtocol);
    ui->spinIrAddress->setValue(m_cfg->lens().irAddress);

    ui->editSerialIp->setText(m_cfg->serialIp());
    ui->spinSerialPort->setValue(m_cfg->serialPort());
    ui->spinMockServerPort->setValue(m_cfg->mockServerPort());
    ui->checkSerialServerEnabled->setChecked(m_cfg->serialServerEnabled());
    ui->checkTurntableIpEnabled->setChecked(m_cfg->turntableIpEnabled());
    ui->checkMotorSerialEnabled->setChecked(m_cfg->motorSerialEnabled());
    ui->checkSoftwarePtzCalibration->setChecked(m_cfg->softwarePtzCalibrationEnabled());

    ui->editMotorTcpIp->setText(m_cfg->motorTcpIp());
    ui->spinMotorTcpPort->setValue(m_cfg->motorTcpPort());

    for (const auto& info : QSerialPortInfo::availablePorts())
        ui->comboMotorPort->addItem(info.portName());

    ui->comboMotorCommandChannel->setCurrentText(m_cfg->motorCommandChannel());
    ui->comboMotorProtocol->setCurrentText(m_cfg->motorProtocol());
    ui->comboMotorPort->setCurrentText(m_cfg->motorComPort());

    ui->comboCloseAction->setCurrentIndex(static_cast<int>(m_cfg->closeAction()));

    const auto& cam = m_cfg->cam();
    ui->spinVisPixelSize->setValue(cam.visPixelSize);
    ui->editVisResolution->setText(QString("%1x%2").arg(cam.visResX).arg(cam.visResY));
    ui->spinVisMinFocal->setValue(cam.visMinFocal);

    ui->spinIrPixelSize->setValue(cam.irPixelSize);
    ui->editIrResolution->setText(QString("%1x%2").arg(cam.irResX).arg(cam.irResY));
    ui->spinIrMinFocal->setValue(cam.irMinFocal);

    auto loadRef = [&](const char* name, int ml, int cc) {
        auto *s = findChild<QDoubleSpinBox*>(name);
        if (s) s->setValue(cam.targetRefSize(ml, cc));
    };
    loadRef("spinRef_2_161", 2, 0xA1);
    loadRef("spinRef_2_162", 2, 0xA2);
    loadRef("spinRef_3_163", 3, 0xA3);
    loadRef("spinRef_4_164", 4, 0xA4);
    loadRef("spinRef_5_161", 5, 0xA1);
    loadRef("spinRef_5_162", 5, 0xA2);
    loadRef("spinRef_6_163", 6, 0xA3);
}

void SettingsDialog::saveSettings()
{
    // ---- 云台参数 ----
    m_cfg->ptz().protocol = ui->comboPtzProtocol->currentText();
    m_cfg->ptz().address  = static_cast<quint8>(ui->spinPtzAddress->value());

    // ---- 可见光通道 ----
    m_cfg->lens().visProtocol = ui->comboVisProtocol->currentText();
    m_cfg->lens().visAddress  = static_cast<quint8>(ui->spinVisAddress->value());

    // ---- 红外通道 ----
    m_cfg->lens().irProtocol = ui->comboIrProtocol->currentText();
    m_cfg->lens().irAddress  = static_cast<quint8>(ui->spinIrAddress->value());

    // ---- 串口连接 ----
    m_cfg->setSerialIp(ui->editSerialIp->text());
    m_cfg->setSerialPort(static_cast<quint16>(ui->spinSerialPort->value()));
    m_cfg->setMockServerPort(static_cast<quint16>(ui->spinMockServerPort->value()));
    m_cfg->setSerialServerEnabled(ui->checkSerialServerEnabled->isChecked());
    m_cfg->setTurntableIpEnabled(ui->checkTurntableIpEnabled->isChecked());
    m_cfg->setMotorSerialEnabled(ui->checkMotorSerialEnabled->isChecked());
    m_cfg->setSoftwarePtzCalibrationEnabled(ui->checkSoftwarePtzCalibration->isChecked());

    m_cfg->setMotorTcpIp(ui->editMotorTcpIp->text());
    m_cfg->setMotorTcpPort(static_cast<quint16>(ui->spinMotorTcpPort->value()));

    // ---- 电机协议 ----
    m_cfg->setMotorCommandChannel(ui->comboMotorCommandChannel->currentText());
    m_cfg->setMotorProtocol(ui->comboMotorProtocol->currentText());
    m_cfg->setMotorComPort(ui->comboMotorPort->currentText());

    // ---- 关闭按钮行为 ----
    m_cfg->setCloseAction(static_cast<ConfigManager::CloseAction>(ui->comboCloseAction->currentIndex()));

    // ---- 可见光相机参数 ----
    auto& cam = m_cfg->cam();
    cam.visPixelSize = ui->spinVisPixelSize->value();
    cam.visMinFocal  = ui->spinVisMinFocal->value();
    auto visRes = ui->editVisResolution->text().split('x');
    if (visRes.size() == 2) {
        cam.visResX = visRes[0].toInt();
        cam.visResY = visRes[1].toInt();
    }

    // ---- 红外相机参数 ----
    cam.irPixelSize = ui->spinIrPixelSize->value();
    cam.irMinFocal  = ui->spinIrMinFocal->value();
    auto irRes = ui->editIrResolution->text().split('x');
    if (irRes.size() == 2) {
        cam.irResX = irRes[0].toInt();
        cam.irResY = irRes[1].toInt();
    }

    // ---- 视觉测距参考尺寸 ----
    auto saveRef = [&](const char* name, int ml, int cc) {
        auto *s = findChild<QDoubleSpinBox*>(name);
        if (s) cam.targetRefMap[(ml << 8) | cc] = s->value();
    };
    saveRef("spinRef_2_161", 2, 0xA1);
    saveRef("spinRef_2_162", 2, 0xA2);
    saveRef("spinRef_3_163", 3, 0xA3);
    saveRef("spinRef_4_164", 4, 0xA4);
    saveRef("spinRef_5_161", 5, 0xA1);
    saveRef("spinRef_5_162", 5, 0xA2);
    saveRef("spinRef_6_163", 6, 0xA3);

    m_cfg->save();
}

void SettingsDialog::updateProtocolControls()
{
    QString protocol = ui->comboMotorProtocol->currentText();
    bool isModbus = (protocol == "MODBUS-RTU");
    bool isTcp = (protocol == "STM32-TCP-V4.0");
    bool isPelco = (protocol == "Pelco-D");

    // Motor COM Port and Command Channel are only for MODBUS-RTU
    ui->comboMotorPort->setEnabled(isModbus);
    ui->comboMotorCommandChannel->setEnabled(isModbus);
    
    // TCP settings for STM32-TCP-V4.0
    ui->editMotorTcpIp->setEnabled(isTcp);
    ui->spinMotorTcpPort->setEnabled(isTcp);
}

void SettingsDialog::on_buttonBox_accepted()
{
    saveSettings();
    accept();
}
