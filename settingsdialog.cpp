#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QFile>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    
    QFile qssFile(":/style.qss");
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        this->setStyleSheet(QLatin1String(qssFile.readAll()));
        qssFile.close();
    }
    
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::loadSettings()
{
    QSettings settings("Tofu", "T-JSON_Settings");
    
    ui->comboPtzProtocol->setCurrentText(settings.value("PtzProtocol", "Pelco-D").toString());
    ui->spinPtzAddress->setValue(settings.value("PtzAddress", 1).toInt());
    ui->comboVisProtocol->setCurrentText(settings.value("VisProtocol", "VISCA").toString());
    ui->spinVisAddress->setValue(settings.value("VisAddress", 1).toInt());
    ui->comboIrProtocol->setCurrentText(settings.value("IrProtocol", "Pelco-D").toString());
    ui->spinIrAddress->setValue(settings.value("IrAddress", 2).toInt());
    ui->editSerialIp->setText(settings.value("SerialIp", "192.168.1.66").toString());
    ui->spinSerialPort->setValue(settings.value("SerialPort", 4001).toInt());

    ui->spinVisPixelSize->setValue(settings.value("VisPixelSize", 2.9).toDouble());
    ui->editVisResolution->setText(settings.value("VisResolution", "2688x1520").toString());
    ui->spinVisMinFocal->setValue(settings.value("VisMinFocal", 6.0).toDouble());
    
    ui->spinIrPixelSize->setValue(settings.value("IrPixelSize", 12.0).toDouble());
    ui->editIrResolution->setText(settings.value("IrResolution", "640x512").toString());
    ui->spinIrMinFocal->setValue(settings.value("IrMinFocal", 25.0).toDouble());
}

void SettingsDialog::saveSettings()
{
    QSettings settings("Tofu", "T-JSON_Settings");
    
    settings.setValue("PtzProtocol", ui->comboPtzProtocol->currentText());
    settings.setValue("PtzAddress", ui->spinPtzAddress->value());
    settings.setValue("VisProtocol", ui->comboVisProtocol->currentText());
    settings.setValue("VisAddress", ui->spinVisAddress->value());
    settings.setValue("IrProtocol", ui->comboIrProtocol->currentText());
    settings.setValue("IrAddress", ui->spinIrAddress->value());
    settings.setValue("SerialIp", ui->editSerialIp->text());
    settings.setValue("SerialPort", ui->spinSerialPort->value());

    settings.setValue("VisPixelSize", ui->spinVisPixelSize->value());
    settings.setValue("VisResolution", ui->editVisResolution->text());
    settings.setValue("VisMinFocal", ui->spinVisMinFocal->value());
    
    settings.setValue("IrPixelSize", ui->spinIrPixelSize->value());
    settings.setValue("IrResolution", ui->editIrResolution->text());
    settings.setValue("IrMinFocal", ui->spinIrMinFocal->value());
}

void SettingsDialog::on_buttonBox_accepted()
{
    saveSettings();
}