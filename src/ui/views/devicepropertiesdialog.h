#ifndef DEVICEPROPERTIESDIALOG_H
#define DEVICEPROPERTIESDIALOG_H

#include <QDialog>
#include "core/DeviceConfig.h"
#include "core/DeviceState.h"

class QComboBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QGroupBox;
class QScrollArea;
class DeviceContext;

class DevicePropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DevicePropertiesDialog(const QString& deviceIp, DeviceConfig* cfg,
                                    DeviceContext* ctx = nullptr, QWidget* parent = nullptr);

private slots:
    void onAccepted();
    void updateProtocolControls();

private:
    void setupUi();
    void loadFromConfig();
    void saveToConfig();
    void fillTofuFields(const DeviceState::TofuParams& p);
    DeviceState::TofuParams tofuFieldsToParams() const;

    QString m_deviceIp;
    DeviceConfig* m_cfg;
    DeviceContext* m_ctx = nullptr;   // 设备上下文（可为空：无上下文时仅本地配置）
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

    // 光学与相机参数（可见光像元/焦距改由设备上报，见下方 48M-Tofu7 分组）
    QLineEdit* m_editVisResolution;
    QDoubleSpinBox* m_spinIrPixelSize;
    QLineEdit* m_editIrResolution;
    QDoubleSpinBox* m_spinIrMinFocal;

    // 48M-Tofu7 参数（设备上报 / 下发）
    QComboBox* m_comboVisCameraModel;
    QDoubleSpinBox* m_spinTofu7PixSize;
    QSpinBox* m_spinTofu7MinFocal;
    QDoubleSpinBox* m_spinTofu6PixSize;
    QSpinBox* m_spinTofu6MinFocal;
    QSpinBox* m_spinTofu6ExpectedSize;
    QDoubleSpinBox* m_spinTofu6ZeroOffsetX;
    QDoubleSpinBox* m_spinTofu6ZeroOffsetY;
    QLineEdit* m_editPtzSerialIp;
    QLineEdit* m_editTofu6Ip;

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
