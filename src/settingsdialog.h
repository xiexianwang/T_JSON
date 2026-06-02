// ============================================================
// 文件: settingsdialog.h
// 描述: 系统参数设置对话框，用于编辑串口、云台协议、镜头协议
//       以及相机传感器参数。通过 ConfigManager 统一读写，不
//       直接操作 QSettings。
// ============================================================

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QMap>
#include <QDoubleSpinBox>

class ConfigManager;

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(ConfigManager *cfg, QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    void on_buttonBox_accepted();

private:
    Ui::SettingsDialog *ui;
    ConfigManager *m_cfg;

    QMap<int, QDoubleSpinBox*> m_refSpins; // key = (modelLow << 8) | classCode

    void loadSettings();
    void saveSettings();
    void buildTargetRefsForm();
};

#endif // SETTINGSDIALOG_H
