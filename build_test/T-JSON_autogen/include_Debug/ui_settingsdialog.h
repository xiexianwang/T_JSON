/********************************************************************************
** Form generated from reading UI file 'settingsdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGSDIALOG_H
#define UI_SETTINGSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_SettingsDialog
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *groupComm;
    QGridLayout *gridLayout_comm;
    QLabel *label;
    QComboBox *comboPtzProtocol;
    QLabel *label_2;
    QSpinBox *spinPtzAddress;
    QLabel *label_3;
    QComboBox *comboVisProtocol;
    QLabel *label_4;
    QSpinBox *spinVisAddress;
    QLabel *label_5;
    QComboBox *comboIrProtocol;
    QLabel *label_6;
    QSpinBox *spinIrAddress;
    QLabel *label_7;
    QLineEdit *editSerialIp;
    QLabel *label_8;
    QSpinBox *spinSerialPort;
    QLabel *label_16;
    QComboBox *comboMotorProtocol;
    QLabel *label_17;
    QComboBox *comboMotorPort;
    QLabel *label_18;
    QSpinBox *spinMockServerPort;
    QLabel *label_22;
    QLineEdit *editMotorTcpIp;
    QLabel *label_23;
    QSpinBox *spinMotorTcpPort;
    QLabel *label_motor_channel;
    QComboBox *comboMotorCommandChannel;
    QHBoxLayout *horizontalLayout_enabled;
    QCheckBox *checkSerialServerEnabled;
    QCheckBox *checkTurntableIpEnabled;
    QCheckBox *checkMotorSerialEnabled;
    QCheckBox *checkSoftwarePtzCalibration;
    QGroupBox *groupCloseAction;
    QFormLayout *formLayout_closeAction;
    QLabel *label_15;
    QComboBox *comboCloseAction;
    QGroupBox *groupCamera;
    QFormLayout *formLayout_camera;
    QLabel *label_9;
    QDoubleSpinBox *spinVisPixelSize;
    QLabel *label_10;
    QLineEdit *editVisResolution;
    QLabel *label_11;
    QDoubleSpinBox *spinVisMinFocal;
    QLabel *label_12;
    QDoubleSpinBox *spinIrPixelSize;
    QLabel *label_13;
    QLineEdit *editIrResolution;
    QLabel *label_14;
    QDoubleSpinBox *spinIrMinFocal;
    QGroupBox *groupTargetRefs;
    QFormLayout *formLayout_targetRefs;
    QLabel *labelRef_ren;
    QDoubleSpinBox *spinRef_2_161;
    QLabel *labelRef_che;
    QDoubleSpinBox *spinRef_2_162;
    QLabel *labelRef_chuan;
    QDoubleSpinBox *spinRef_3_163;
    QLabel *labelRef_wurenji;
    QDoubleSpinBox *spinRef_4_164;
    QLabel *labelRef_feiji;
    QDoubleSpinBox *spinRef_5_161;
    QLabel *labelRef_zhishengji;
    QDoubleSpinBox *spinRef_5_162;
    QLabel *labelRef_niao;
    QDoubleSpinBox *spinRef_6_163;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *SettingsDialog)
    {
        if (SettingsDialog->objectName().isEmpty())
            SettingsDialog->setObjectName("SettingsDialog");
        SettingsDialog->resize(450, 791);
        verticalLayout = new QVBoxLayout(SettingsDialog);
        verticalLayout->setSpacing(10);
        verticalLayout->setObjectName("verticalLayout");
        groupComm = new QGroupBox(SettingsDialog);
        groupComm->setObjectName("groupComm");
        gridLayout_comm = new QGridLayout(groupComm);
        gridLayout_comm->setObjectName("gridLayout_comm");
        label = new QLabel(groupComm);
        label->setObjectName("label");

        gridLayout_comm->addWidget(label, 0, 0, 1, 1);

        comboPtzProtocol = new QComboBox(groupComm);
        comboPtzProtocol->addItem(QString());
        comboPtzProtocol->addItem(QString());
        comboPtzProtocol->addItem(QString());
        comboPtzProtocol->setObjectName("comboPtzProtocol");

        gridLayout_comm->addWidget(comboPtzProtocol, 0, 1, 1, 1);

        label_2 = new QLabel(groupComm);
        label_2->setObjectName("label_2");

        gridLayout_comm->addWidget(label_2, 0, 2, 1, 1);

        spinPtzAddress = new QSpinBox(groupComm);
        spinPtzAddress->setObjectName("spinPtzAddress");
        spinPtzAddress->setMinimum(1);
        spinPtzAddress->setMaximum(255);

        gridLayout_comm->addWidget(spinPtzAddress, 0, 3, 1, 1);

        label_3 = new QLabel(groupComm);
        label_3->setObjectName("label_3");

        gridLayout_comm->addWidget(label_3, 1, 0, 1, 1);

        comboVisProtocol = new QComboBox(groupComm);
        comboVisProtocol->addItem(QString());
        comboVisProtocol->setObjectName("comboVisProtocol");

        gridLayout_comm->addWidget(comboVisProtocol, 1, 1, 1, 1);

        label_4 = new QLabel(groupComm);
        label_4->setObjectName("label_4");

        gridLayout_comm->addWidget(label_4, 1, 2, 1, 1);

        spinVisAddress = new QSpinBox(groupComm);
        spinVisAddress->setObjectName("spinVisAddress");
        spinVisAddress->setMinimum(1);
        spinVisAddress->setMaximum(255);

        gridLayout_comm->addWidget(spinVisAddress, 1, 3, 1, 1);

        label_5 = new QLabel(groupComm);
        label_5->setObjectName("label_5");

        gridLayout_comm->addWidget(label_5, 2, 0, 1, 1);

        comboIrProtocol = new QComboBox(groupComm);
        comboIrProtocol->addItem(QString());
        comboIrProtocol->addItem(QString());
        comboIrProtocol->setObjectName("comboIrProtocol");

        gridLayout_comm->addWidget(comboIrProtocol, 2, 1, 1, 1);

        label_6 = new QLabel(groupComm);
        label_6->setObjectName("label_6");

        gridLayout_comm->addWidget(label_6, 2, 2, 1, 1);

        spinIrAddress = new QSpinBox(groupComm);
        spinIrAddress->setObjectName("spinIrAddress");
        spinIrAddress->setMinimum(1);
        spinIrAddress->setMaximum(255);

        gridLayout_comm->addWidget(spinIrAddress, 2, 3, 1, 1);

        label_7 = new QLabel(groupComm);
        label_7->setObjectName("label_7");

        gridLayout_comm->addWidget(label_7, 3, 0, 1, 1);

        editSerialIp = new QLineEdit(groupComm);
        editSerialIp->setObjectName("editSerialIp");

        gridLayout_comm->addWidget(editSerialIp, 3, 1, 1, 1);

        label_8 = new QLabel(groupComm);
        label_8->setObjectName("label_8");

        gridLayout_comm->addWidget(label_8, 3, 2, 1, 1);

        spinSerialPort = new QSpinBox(groupComm);
        spinSerialPort->setObjectName("spinSerialPort");
        spinSerialPort->setMinimum(1);
        spinSerialPort->setMaximum(65535);
        spinSerialPort->setValue(4001);

        gridLayout_comm->addWidget(spinSerialPort, 3, 3, 1, 1);

        label_16 = new QLabel(groupComm);
        label_16->setObjectName("label_16");

        gridLayout_comm->addWidget(label_16, 4, 0, 1, 1);

        comboMotorProtocol = new QComboBox(groupComm);
        comboMotorProtocol->addItem(QString());
        comboMotorProtocol->addItem(QString());
        comboMotorProtocol->addItem(QString());
        comboMotorProtocol->setObjectName("comboMotorProtocol");

        gridLayout_comm->addWidget(comboMotorProtocol, 4, 1, 1, 1);

        label_17 = new QLabel(groupComm);
        label_17->setObjectName("label_17");

        gridLayout_comm->addWidget(label_17, 4, 2, 1, 1);

        comboMotorPort = new QComboBox(groupComm);
        comboMotorPort->setObjectName("comboMotorPort");

        gridLayout_comm->addWidget(comboMotorPort, 4, 3, 1, 1);

        label_18 = new QLabel(groupComm);
        label_18->setObjectName("label_18");

        gridLayout_comm->addWidget(label_18, 5, 0, 1, 1);

        spinMockServerPort = new QSpinBox(groupComm);
        spinMockServerPort->setObjectName("spinMockServerPort");
        spinMockServerPort->setMinimum(1);
        spinMockServerPort->setMaximum(65535);
        spinMockServerPort->setValue(5001);

        gridLayout_comm->addWidget(spinMockServerPort, 5, 1, 1, 1);

        label_22 = new QLabel(groupComm);
        label_22->setObjectName("label_22");

        gridLayout_comm->addWidget(label_22, 5, 2, 1, 1);

        editMotorTcpIp = new QLineEdit(groupComm);
        editMotorTcpIp->setObjectName("editMotorTcpIp");

        gridLayout_comm->addWidget(editMotorTcpIp, 5, 3, 1, 1);

        label_23 = new QLabel(groupComm);
        label_23->setObjectName("label_23");

        gridLayout_comm->addWidget(label_23, 6, 0, 1, 1);

        spinMotorTcpPort = new QSpinBox(groupComm);
        spinMotorTcpPort->setObjectName("spinMotorTcpPort");
        spinMotorTcpPort->setMinimum(1);
        spinMotorTcpPort->setMaximum(65535);
        spinMotorTcpPort->setValue(5000);

        gridLayout_comm->addWidget(spinMotorTcpPort, 6, 1, 1, 1);

        label_motor_channel = new QLabel(groupComm);
        label_motor_channel->setObjectName("label_motor_channel");

        gridLayout_comm->addWidget(label_motor_channel, 6, 2, 1, 1);

        comboMotorCommandChannel = new QComboBox(groupComm);
        comboMotorCommandChannel->addItem(QString());
        comboMotorCommandChannel->addItem(QString());
        comboMotorCommandChannel->setObjectName("comboMotorCommandChannel");

        gridLayout_comm->addWidget(comboMotorCommandChannel, 6, 3, 1, 1);

        horizontalLayout_enabled = new QHBoxLayout();
        horizontalLayout_enabled->setObjectName("horizontalLayout_enabled");
        checkSerialServerEnabled = new QCheckBox(groupComm);
        checkSerialServerEnabled->setObjectName("checkSerialServerEnabled");

        horizontalLayout_enabled->addWidget(checkSerialServerEnabled);

        checkTurntableIpEnabled = new QCheckBox(groupComm);
        checkTurntableIpEnabled->setObjectName("checkTurntableIpEnabled");

        horizontalLayout_enabled->addWidget(checkTurntableIpEnabled);

        checkMotorSerialEnabled = new QCheckBox(groupComm);
        checkMotorSerialEnabled->setObjectName("checkMotorSerialEnabled");

        horizontalLayout_enabled->addWidget(checkMotorSerialEnabled);

        checkSoftwarePtzCalibration = new QCheckBox(groupComm);
        checkSoftwarePtzCalibration->setObjectName("checkSoftwarePtzCalibration");

        horizontalLayout_enabled->addWidget(checkSoftwarePtzCalibration);


        gridLayout_comm->addLayout(horizontalLayout_enabled, 7, 0, 1, 4);


        verticalLayout->addWidget(groupComm);

        groupCloseAction = new QGroupBox(SettingsDialog);
        groupCloseAction->setObjectName("groupCloseAction");
        formLayout_closeAction = new QFormLayout(groupCloseAction);
        formLayout_closeAction->setObjectName("formLayout_closeAction");
        label_15 = new QLabel(groupCloseAction);
        label_15->setObjectName("label_15");

        formLayout_closeAction->setWidget(0, QFormLayout::ItemRole::LabelRole, label_15);

        comboCloseAction = new QComboBox(groupCloseAction);
        comboCloseAction->addItem(QString());
        comboCloseAction->addItem(QString());
        comboCloseAction->addItem(QString());
        comboCloseAction->setObjectName("comboCloseAction");

        formLayout_closeAction->setWidget(0, QFormLayout::ItemRole::FieldRole, comboCloseAction);


        verticalLayout->addWidget(groupCloseAction);

        groupCamera = new QGroupBox(SettingsDialog);
        groupCamera->setObjectName("groupCamera");
        formLayout_camera = new QFormLayout(groupCamera);
        formLayout_camera->setObjectName("formLayout_camera");
        label_9 = new QLabel(groupCamera);
        label_9->setObjectName("label_9");

        formLayout_camera->setWidget(0, QFormLayout::ItemRole::LabelRole, label_9);

        spinVisPixelSize = new QDoubleSpinBox(groupCamera);
        spinVisPixelSize->setObjectName("spinVisPixelSize");
        spinVisPixelSize->setDecimals(2);
        spinVisPixelSize->setMinimum(0.100000000000000);
        spinVisPixelSize->setMaximum(50.000000000000000);

        formLayout_camera->setWidget(0, QFormLayout::ItemRole::FieldRole, spinVisPixelSize);

        label_10 = new QLabel(groupCamera);
        label_10->setObjectName("label_10");

        formLayout_camera->setWidget(1, QFormLayout::ItemRole::LabelRole, label_10);

        editVisResolution = new QLineEdit(groupCamera);
        editVisResolution->setObjectName("editVisResolution");

        formLayout_camera->setWidget(1, QFormLayout::ItemRole::FieldRole, editVisResolution);

        label_11 = new QLabel(groupCamera);
        label_11->setObjectName("label_11");

        formLayout_camera->setWidget(2, QFormLayout::ItemRole::LabelRole, label_11);

        spinVisMinFocal = new QDoubleSpinBox(groupCamera);
        spinVisMinFocal->setObjectName("spinVisMinFocal");
        spinVisMinFocal->setMinimum(1.000000000000000);
        spinVisMinFocal->setMaximum(1000.000000000000000);

        formLayout_camera->setWidget(2, QFormLayout::ItemRole::FieldRole, spinVisMinFocal);

        label_12 = new QLabel(groupCamera);
        label_12->setObjectName("label_12");

        formLayout_camera->setWidget(3, QFormLayout::ItemRole::LabelRole, label_12);

        spinIrPixelSize = new QDoubleSpinBox(groupCamera);
        spinIrPixelSize->setObjectName("spinIrPixelSize");
        spinIrPixelSize->setDecimals(2);
        spinIrPixelSize->setMinimum(0.100000000000000);
        spinIrPixelSize->setMaximum(50.000000000000000);

        formLayout_camera->setWidget(3, QFormLayout::ItemRole::FieldRole, spinIrPixelSize);

        label_13 = new QLabel(groupCamera);
        label_13->setObjectName("label_13");

        formLayout_camera->setWidget(4, QFormLayout::ItemRole::LabelRole, label_13);

        editIrResolution = new QLineEdit(groupCamera);
        editIrResolution->setObjectName("editIrResolution");

        formLayout_camera->setWidget(4, QFormLayout::ItemRole::FieldRole, editIrResolution);

        label_14 = new QLabel(groupCamera);
        label_14->setObjectName("label_14");

        formLayout_camera->setWidget(5, QFormLayout::ItemRole::LabelRole, label_14);

        spinIrMinFocal = new QDoubleSpinBox(groupCamera);
        spinIrMinFocal->setObjectName("spinIrMinFocal");
        spinIrMinFocal->setMinimum(1.000000000000000);
        spinIrMinFocal->setMaximum(1000.000000000000000);

        formLayout_camera->setWidget(5, QFormLayout::ItemRole::FieldRole, spinIrMinFocal);


        verticalLayout->addWidget(groupCamera);

        groupTargetRefs = new QGroupBox(SettingsDialog);
        groupTargetRefs->setObjectName("groupTargetRefs");
        formLayout_targetRefs = new QFormLayout(groupTargetRefs);
        formLayout_targetRefs->setObjectName("formLayout_targetRefs");
        labelRef_ren = new QLabel(groupTargetRefs);
        labelRef_ren->setObjectName("labelRef_ren");

        formLayout_targetRefs->setWidget(0, QFormLayout::ItemRole::LabelRole, labelRef_ren);

        spinRef_2_161 = new QDoubleSpinBox(groupTargetRefs);
        spinRef_2_161->setObjectName("spinRef_2_161");
        spinRef_2_161->setDecimals(2);
        spinRef_2_161->setMinimum(0.100000000000000);
        spinRef_2_161->setMaximum(100.000000000000000);
        spinRef_2_161->setSingleStep(0.050000000000000);
        spinRef_2_161->setValue(1.700000000000000);

        formLayout_targetRefs->setWidget(0, QFormLayout::ItemRole::FieldRole, spinRef_2_161);

        labelRef_che = new QLabel(groupTargetRefs);
        labelRef_che->setObjectName("labelRef_che");

        formLayout_targetRefs->setWidget(1, QFormLayout::ItemRole::LabelRole, labelRef_che);

        spinRef_2_162 = new QDoubleSpinBox(groupTargetRefs);
        spinRef_2_162->setObjectName("spinRef_2_162");
        spinRef_2_162->setDecimals(2);
        spinRef_2_162->setMinimum(0.100000000000000);
        spinRef_2_162->setMaximum(100.000000000000000);
        spinRef_2_162->setSingleStep(0.050000000000000);
        spinRef_2_162->setValue(4.500000000000000);

        formLayout_targetRefs->setWidget(1, QFormLayout::ItemRole::FieldRole, spinRef_2_162);

        labelRef_chuan = new QLabel(groupTargetRefs);
        labelRef_chuan->setObjectName("labelRef_chuan");

        formLayout_targetRefs->setWidget(2, QFormLayout::ItemRole::LabelRole, labelRef_chuan);

        spinRef_3_163 = new QDoubleSpinBox(groupTargetRefs);
        spinRef_3_163->setObjectName("spinRef_3_163");
        spinRef_3_163->setDecimals(2);
        spinRef_3_163->setMinimum(0.100000000000000);
        spinRef_3_163->setMaximum(100.000000000000000);
        spinRef_3_163->setSingleStep(0.050000000000000);
        spinRef_3_163->setValue(10.000000000000000);

        formLayout_targetRefs->setWidget(2, QFormLayout::ItemRole::FieldRole, spinRef_3_163);

        labelRef_wurenji = new QLabel(groupTargetRefs);
        labelRef_wurenji->setObjectName("labelRef_wurenji");

        formLayout_targetRefs->setWidget(3, QFormLayout::ItemRole::LabelRole, labelRef_wurenji);

        spinRef_4_164 = new QDoubleSpinBox(groupTargetRefs);
        spinRef_4_164->setObjectName("spinRef_4_164");
        spinRef_4_164->setDecimals(2);
        spinRef_4_164->setMinimum(0.100000000000000);
        spinRef_4_164->setMaximum(100.000000000000000);
        spinRef_4_164->setSingleStep(0.050000000000000);
        spinRef_4_164->setValue(0.350000000000000);

        formLayout_targetRefs->setWidget(3, QFormLayout::ItemRole::FieldRole, spinRef_4_164);

        labelRef_feiji = new QLabel(groupTargetRefs);
        labelRef_feiji->setObjectName("labelRef_feiji");

        formLayout_targetRefs->setWidget(4, QFormLayout::ItemRole::LabelRole, labelRef_feiji);

        spinRef_5_161 = new QDoubleSpinBox(groupTargetRefs);
        spinRef_5_161->setObjectName("spinRef_5_161");
        spinRef_5_161->setDecimals(2);
        spinRef_5_161->setMinimum(0.100000000000000);
        spinRef_5_161->setMaximum(100.000000000000000);
        spinRef_5_161->setSingleStep(0.050000000000000);
        spinRef_5_161->setValue(15.000000000000000);

        formLayout_targetRefs->setWidget(4, QFormLayout::ItemRole::FieldRole, spinRef_5_161);

        labelRef_zhishengji = new QLabel(groupTargetRefs);
        labelRef_zhishengji->setObjectName("labelRef_zhishengji");

        formLayout_targetRefs->setWidget(5, QFormLayout::ItemRole::LabelRole, labelRef_zhishengji);

        spinRef_5_162 = new QDoubleSpinBox(groupTargetRefs);
        spinRef_5_162->setObjectName("spinRef_5_162");
        spinRef_5_162->setDecimals(2);
        spinRef_5_162->setMinimum(0.100000000000000);
        spinRef_5_162->setMaximum(100.000000000000000);
        spinRef_5_162->setSingleStep(0.050000000000000);
        spinRef_5_162->setValue(10.000000000000000);

        formLayout_targetRefs->setWidget(5, QFormLayout::ItemRole::FieldRole, spinRef_5_162);

        labelRef_niao = new QLabel(groupTargetRefs);
        labelRef_niao->setObjectName("labelRef_niao");

        formLayout_targetRefs->setWidget(6, QFormLayout::ItemRole::LabelRole, labelRef_niao);

        spinRef_6_163 = new QDoubleSpinBox(groupTargetRefs);
        spinRef_6_163->setObjectName("spinRef_6_163");
        spinRef_6_163->setDecimals(2);
        spinRef_6_163->setMinimum(0.100000000000000);
        spinRef_6_163->setMaximum(100.000000000000000);
        spinRef_6_163->setSingleStep(0.050000000000000);
        spinRef_6_163->setValue(0.350000000000000);

        formLayout_targetRefs->setWidget(6, QFormLayout::ItemRole::FieldRole, spinRef_6_163);


        verticalLayout->addWidget(groupTargetRefs);

        buttonBox = new QDialogButtonBox(SettingsDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Save);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(SettingsDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, SettingsDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, SettingsDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(SettingsDialog);
    } // setupUi

    void retranslateUi(QDialog *SettingsDialog)
    {
        SettingsDialog->setWindowTitle(QCoreApplication::translate("SettingsDialog", "\347\263\273\347\273\237\345\217\202\346\225\260\350\256\276\347\275\256", nullptr));
        groupComm->setTitle(QCoreApplication::translate("SettingsDialog", "\351\200\232\344\277\241\344\270\216\345\215\217\350\256\256\351\205\215\347\275\256", nullptr));
        label->setText(QCoreApplication::translate("SettingsDialog", "\344\272\221\345\217\260\345\215\217\350\256\256:", nullptr));
        comboPtzProtocol->setItemText(0, QCoreApplication::translate("SettingsDialog", "Pelco-D", nullptr));
        comboPtzProtocol->setItemText(1, QCoreApplication::translate("SettingsDialog", "LPP", nullptr));
        comboPtzProtocol->setItemText(2, QCoreApplication::translate("SettingsDialog", "N-MD", nullptr));

        label_2->setText(QCoreApplication::translate("SettingsDialog", "\344\272\221\345\217\260\345\234\260\345\235\200:", nullptr));
        label_3->setText(QCoreApplication::translate("SettingsDialog", "\345\217\257\350\247\201\345\205\211\345\215\217\350\256\256:", nullptr));
        comboVisProtocol->setItemText(0, QCoreApplication::translate("SettingsDialog", "VISCA", nullptr));

        label_4->setText(QCoreApplication::translate("SettingsDialog", "\345\217\257\350\247\201\345\205\211\345\234\260\345\235\200:", nullptr));
        label_5->setText(QCoreApplication::translate("SettingsDialog", "\347\272\242\345\244\226\345\215\217\350\256\256:", nullptr));
        comboIrProtocol->setItemText(0, QCoreApplication::translate("SettingsDialog", "Pelco-D", nullptr));
        comboIrProtocol->setItemText(1, QCoreApplication::translate("SettingsDialog", "IRAY", nullptr));

        label_6->setText(QCoreApplication::translate("SettingsDialog", "\347\272\242\345\244\226\345\234\260\345\235\200:", nullptr));
        label_7->setText(QCoreApplication::translate("SettingsDialog", "\350\275\254\345\217\260 IP:", nullptr));
        editSerialIp->setPlaceholderText(QCoreApplication::translate("SettingsDialog", "192.168.1.66", nullptr));
        label_8->setText(QCoreApplication::translate("SettingsDialog", "\350\275\254\345\217\260\347\253\257\345\217\243:", nullptr));
        label_16->setText(QCoreApplication::translate("SettingsDialog", "\347\224\265\346\234\272\345\215\217\350\256\256:", nullptr));
        comboMotorProtocol->setItemText(0, QCoreApplication::translate("SettingsDialog", "Pelco-D", nullptr));
        comboMotorProtocol->setItemText(1, QCoreApplication::translate("SettingsDialog", "MODBUS-RTU", nullptr));
        comboMotorProtocol->setItemText(2, QCoreApplication::translate("SettingsDialog", "STM32-TCP-V4.0", nullptr));

        label_17->setText(QCoreApplication::translate("SettingsDialog", "\347\224\265\346\234\272\344\270\262\345\217\243:", nullptr));
        label_18->setText(QCoreApplication::translate("SettingsDialog", "\346\250\241\346\213\237\346\234\215\345\212\241\345\231\250\347\253\257\345\217\243:", nullptr));
        label_22->setText(QCoreApplication::translate("SettingsDialog", "\347\224\265\346\234\272TCP IP:", nullptr));
        editMotorTcpIp->setPlaceholderText(QCoreApplication::translate("SettingsDialog", "192.168.1.55", nullptr));
        label_23->setText(QCoreApplication::translate("SettingsDialog", "\347\224\265\346\234\272TCP \347\253\257\345\217\243:", nullptr));
        label_motor_channel->setText(QCoreApplication::translate("SettingsDialog", "\344\270\213\345\217\221\351\200\232\351\201\223:", nullptr));
        comboMotorCommandChannel->setItemText(0, QCoreApplication::translate("SettingsDialog", "Pelco-D", nullptr));
        comboMotorCommandChannel->setItemText(1, QCoreApplication::translate("SettingsDialog", "\344\270\262\345\217\243", nullptr));

        checkSerialServerEnabled->setText(QCoreApplication::translate("SettingsDialog", "\346\250\241\346\213\237\344\270\262\345\217\243\346\234\215\345\212\241\345\231\250", nullptr));
        checkTurntableIpEnabled->setText(QCoreApplication::translate("SettingsDialog", "\350\275\254\345\217\260IP\350\277\236\346\216\245", nullptr));
        checkMotorSerialEnabled->setText(QCoreApplication::translate("SettingsDialog", "\347\224\265\346\234\272\344\270\262\345\217\243\350\277\236\346\216\245", nullptr));
        checkSoftwarePtzCalibration->setText(QCoreApplication::translate("SettingsDialog", "\350\275\257\344\273\266\345\201\217\347\275\256\346\240\207\345\256\232", nullptr));
        groupCloseAction->setTitle(QCoreApplication::translate("SettingsDialog", "\345\205\263\351\227\255\346\214\211\351\222\256\350\241\214\344\270\272", nullptr));
        label_15->setText(QCoreApplication::translate("SettingsDialog", "\345\205\263\351\227\255\346\214\211\351\222\256\346\223\215\344\275\234:", nullptr));
        comboCloseAction->setItemText(0, QCoreApplication::translate("SettingsDialog", "\346\257\217\346\254\241\350\257\242\351\227\256", nullptr));
        comboCloseAction->setItemText(1, QCoreApplication::translate("SettingsDialog", "\351\200\200\345\207\272\347\250\213\345\272\217", nullptr));
        comboCloseAction->setItemText(2, QCoreApplication::translate("SettingsDialog", "\346\234\200\345\260\217\345\214\226\345\210\260\346\211\230\347\233\230", nullptr));

        groupCamera->setTitle(QCoreApplication::translate("SettingsDialog", "\345\205\211\345\255\246\344\270\216\347\233\270\346\234\272\345\217\202\346\225\260\351\205\215\347\275\256", nullptr));
        label_9->setText(QCoreApplication::translate("SettingsDialog", "\345\217\257\350\247\201\345\205\211\345\203\217\345\205\203\345\260\272\345\257\270:", nullptr));
        spinVisPixelSize->setSuffix(QCoreApplication::translate("SettingsDialog", " \316\274m", nullptr));
        label_10->setText(QCoreApplication::translate("SettingsDialog", "\345\217\257\350\247\201\345\205\211\347\211\251\347\220\206\345\210\206\350\276\250\347\216\207:", nullptr));
        editVisResolution->setPlaceholderText(QCoreApplication::translate("SettingsDialog", "2688x1520", nullptr));
        label_11->setText(QCoreApplication::translate("SettingsDialog", "\345\217\257\350\247\201\345\205\211\346\234\200\345\260\217\347\204\246\350\267\235:", nullptr));
        spinVisMinFocal->setSuffix(QCoreApplication::translate("SettingsDialog", " mm", nullptr));
        label_12->setText(QCoreApplication::translate("SettingsDialog", "\347\272\242\345\244\226\345\203\217\345\205\203\345\260\272\345\257\270:", nullptr));
        spinIrPixelSize->setSuffix(QCoreApplication::translate("SettingsDialog", " \316\274m", nullptr));
        label_13->setText(QCoreApplication::translate("SettingsDialog", "\347\272\242\345\244\226\347\211\251\347\220\206\345\210\206\350\276\250\347\216\207:", nullptr));
        editIrResolution->setPlaceholderText(QCoreApplication::translate("SettingsDialog", "640x512", nullptr));
        label_14->setText(QCoreApplication::translate("SettingsDialog", "\347\272\242\345\244\226\346\234\200\345\260\217\347\204\246\350\267\235:", nullptr));
        spinIrMinFocal->setSuffix(QCoreApplication::translate("SettingsDialog", " mm", nullptr));
        groupTargetRefs->setTitle(QCoreApplication::translate("SettingsDialog", "\350\247\206\350\247\211\346\265\213\350\267\235\345\217\202\350\200\203\345\260\272\345\257\270", nullptr));
        labelRef_ren->setText(QCoreApplication::translate("SettingsDialog", "\344\272\272", nullptr));
        spinRef_2_161->setSuffix(QCoreApplication::translate("SettingsDialog", " m", nullptr));
        labelRef_che->setText(QCoreApplication::translate("SettingsDialog", "\350\275\246", nullptr));
        spinRef_2_162->setSuffix(QCoreApplication::translate("SettingsDialog", " m", nullptr));
        labelRef_chuan->setText(QCoreApplication::translate("SettingsDialog", "\350\210\271", nullptr));
        spinRef_3_163->setSuffix(QCoreApplication::translate("SettingsDialog", " m", nullptr));
        labelRef_wurenji->setText(QCoreApplication::translate("SettingsDialog", "\346\227\240\344\272\272\346\234\272", nullptr));
        spinRef_4_164->setSuffix(QCoreApplication::translate("SettingsDialog", " m", nullptr));
        labelRef_feiji->setText(QCoreApplication::translate("SettingsDialog", "\351\243\236\346\234\272", nullptr));
        spinRef_5_161->setSuffix(QCoreApplication::translate("SettingsDialog", " m", nullptr));
        labelRef_zhishengji->setText(QCoreApplication::translate("SettingsDialog", "\347\233\264\345\215\207\346\234\272", nullptr));
        spinRef_5_162->setSuffix(QCoreApplication::translate("SettingsDialog", " m", nullptr));
        labelRef_niao->setText(QCoreApplication::translate("SettingsDialog", "\351\270\237", nullptr));
        spinRef_6_163->setSuffix(QCoreApplication::translate("SettingsDialog", " m", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SettingsDialog: public Ui_SettingsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGSDIALOG_H
