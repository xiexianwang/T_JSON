// ============================================================
// 文件: settingsdialog.cpp
// 描述: 系统参数设置对话框实现。读取/保存操作全部委托给
//       ConfigManager，不再直接操作 QSettings。
// ============================================================

#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "configmanager.h"

SettingsDialog::SettingsDialog(ConfigManager *cfg, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , m_cfg(cfg)
{
    ui->setupUi(this);
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::buildTargetRefsForm()
{
    m_refSpins.clear();
    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);

    static const struct { int ml; int cc; const char* label; } kInfo[] = {
        {2, 0xA1, "人"},
        {2, 0xA2, "车"},
        {3, 0xA3, "船"},
        {4, 0xA4, "无人机"},
        {5, 0xA1, "飞机"},
        {5, 0xA2, "直升机"},
        {6, 0xA3, "鸟"},
    };
    for (auto& e : kInfo) {
        int key = (e.ml << 8) | e.cc;
        auto *spin = new QDoubleSpinBox;
        spin->setDecimals(2);
        spin->setRange(0.1, 100.0);
        spin->setSingleStep(0.05);
        spin->setSuffix(" m");
        spin->setValue(m_cfg->cam().targetRefSize(e.ml, e.cc));
        form->addRow(e.label, spin);
        m_refSpins[key] = spin;
    }

    auto *old = ui->groupTargetRefs->findChild<QFormLayout*>();
    if (old) delete old;
    ui->groupTargetRefs->setLayout(form);
}

void SettingsDialog::loadSettings()
{
    // ---- 云台参数 ----
    ui->comboPtzProtocol->setCurrentText(m_cfg->ptz().protocol);
    ui->spinPtzAddress->setValue(m_cfg->ptz().address);

    // ---- 可见光通道 ----
    ui->comboVisProtocol->setCurrentText(m_cfg->lens().visProtocol);
    ui->spinVisAddress->setValue(m_cfg->lens().visAddress);

    // ---- 红外通道 ----
    ui->comboIrProtocol->setCurrentText(m_cfg->lens().irProtocol);
    ui->spinIrAddress->setValue(m_cfg->lens().irAddress);

    // ---- 串口连接 ----
    ui->editSerialIp->setText(m_cfg->serialIp());
    ui->spinSerialPort->setValue(m_cfg->serialPort());

    // ---- 可见光相机参数 ----
    const auto& cam = m_cfg->cam();
    ui->spinVisPixelSize->setValue(cam.visPixelSize);
    ui->editVisResolution->setText(QString("%1x%2").arg(cam.visResX).arg(cam.visResY));
    ui->spinVisMinFocal->setValue(cam.visMinFocal);

    // ---- 红外相机参数 ----
    ui->spinIrPixelSize->setValue(cam.irPixelSize);
    ui->editIrResolution->setText(QString("%1x%2").arg(cam.irResX).arg(cam.irResY));
    ui->spinIrMinFocal->setValue(cam.irMinFocal);

    // ---- 视觉测距参考尺寸 ----
    buildTargetRefsForm();
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
    for (auto it = m_refSpins.constBegin(); it != m_refSpins.constEnd(); ++it)
        cam.targetRefMap[it.key()] = it.value()->value();

    m_cfg->save();
}

void SettingsDialog::on_buttonBox_accepted()
{
    saveSettings();
    accept();
}
