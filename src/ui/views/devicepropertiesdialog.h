#ifndef DEVICEPROPERTIESDIALOG_H
#define DEVICEPROPERTIESDIALOG_H

#include <QDialog>
#include "core/DeviceConfig.h"

class QComboBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QGroupBox;
class QScrollArea;

class DevicePropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DevicePropertiesDialog(const QString& deviceIp, DeviceConfig* cfg, QWidget* parent = nullptr);

private slots:
    void onAccepted();
    void updateProtocolControls();

private:
    void setupUi();
    void loadFromConfig();
    void saveToConfig();

    QString m_deviceIp;
    DeviceConfig* m_cfg;
    QScrollArea* m_scroll = nullptr;

    // 电机
    QCheckBox* m_checkMotorSerialEnabled;
    QCheckBox* m_checkMotorIpEnabled;
    QComboBox* m_comboMotorProtocol;
    QComboBox* m_comboMotorCommandChannel;
    QComboBox* m_comboMotorComPort;
    QLineEdit* m_editMotorTcpIp;
    QSpinBox* m_spinMotorTcpPort;

    // 转台
    QCheckBox* m_checkSerialServerEnabled;
    QCheckBox* m_checkTurntableIpEnabled;
    QLineEdit* m_editSerialIp;
    QSpinBox* m_spinSerialPort;
    QSpinBox* m_spinMockServerPort;
    QDoubleSpinBox* m_spinPtzPanOffset;
    QDoubleSpinBox* m_spinPtzTiltOffset;
    QCheckBox* m_checkSoftwarePtzCalibration;

    // 云台(PTZ)
    QComboBox* m_comboPtzProtocol;
    QSpinBox* m_spinPtzAddress;
    QSpinBox* m_spinPanSpeed;
    QSpinBox* m_spinTiltSpeed;

    // 镜头
    QComboBox* m_comboVisProtocol;
    QSpinBox* m_spinVisAddress;
    QComboBox* m_comboIrProtocol;
    QSpinBox* m_spinIrAddress;
    QSpinBox* m_spinZoomSpeed;

    // 光学与相机参数
    QDoubleSpinBox* m_spinVisPixelSize;
    QLineEdit* m_editVisResolution;
    QDoubleSpinBox* m_spinVisMinFocal;
    QDoubleSpinBox* m_spinIrPixelSize;
    QLineEdit* m_editIrResolution;
    QDoubleSpinBox* m_spinIrMinFocal;

    // 视觉测距参考尺寸
    QDoubleSpinBox* m_spinRef_2_161;
    QDoubleSpinBox* m_spinRef_2_162;
    QDoubleSpinBox* m_spinRef_3_163;
    QDoubleSpinBox* m_spinRef_4_164;
    QDoubleSpinBox* m_spinRef_5_161;
    QDoubleSpinBox* m_spinRef_5_162;
    QDoubleSpinBox* m_spinRef_6_163;

    // 附加功能开关
    QCheckBox* m_checkDigitalZoom;
    QCheckBox* m_checkAutoZoom;
    QCheckBox* m_checkCaptureUpload;
    QCheckBox* m_checkPosReset;
};

#endif // DEVICEPROPERTIESDIALOG_H
