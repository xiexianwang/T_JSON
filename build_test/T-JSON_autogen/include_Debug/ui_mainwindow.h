/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout_main;
    QWidget *titleBar;
    QHBoxLayout *horizontalLayout_title;
    QLabel *labelAppIcon;
    QLabel *labelAppTitle;
    QPushButton *btnNavMonitor;
    QPushButton *btnNavPlayback;
    QPushButton *btnNavLog;
    QPushButton *btnNavSettings;
    QSpacerItem *spacerTitle;
    QPushButton *btnMenu_Min;
    QPushButton *btnMenu_Max;
    QPushButton *btnMenu_Close;
    QStackedWidget *contentStack;
    QWidget *pageMonitor;
    QVBoxLayout *verticalLayout_pageMonitor;
    QHBoxLayout *horizontalLayout_top;
    QPushButton *btnMapToggle;
    QSpacerItem *spacerTop;
    QHBoxLayout *horizontalLayout_middle;
    QWidget *widgetDisplay;
    QVBoxLayout *verticalLayout_display;
    QWidget *videoGridContainer;
    QScrollArea *scrollAreaControl;
    QWidget *widgetControl;
    QVBoxLayout *verticalLayout_control;
    QGroupBox *groupAiAlgo;
    QGridLayout *gridLayout_aiAlgo;
    QLabel *lblAiMode;
    QComboBox *comboWorkMode;
    QLabel *lblAlgo;
    QComboBox *comboAlgoModel1;
    QComboBox *comboAlgoModel2;
    QLabel *lblDisplayMode;
    QComboBox *comboDisplayMode;
    QGroupBox *groupPTZ;
    QVBoxLayout *verticalLayout_ptz;
    QGridLayout *gridLayout_8way;
    QPushButton *btnPtzTopLeft;
    QPushButton *btnPtzUp;
    QPushButton *btnPtzTopRight;
    QPushButton *btnPtzLeft;
    QPushButton *btnPtzReset;
    QPushButton *btnPtzRight;
    QPushButton *btnPtzBottomLeft;
    QPushButton *btnPtzDown;
    QPushButton *btnPtzBottomRight;
    QHBoxLayout *horizontalLayout_speed;
    QLabel *lblSpeed;
    QSlider *sliderSpeed;
    QSpinBox *spinSpeed;
    QHBoxLayout *horizontalLayout_angleMove;
    QLabel *label;
    QLineEdit *editTargetPan;
    QLabel *label1;
    QLineEdit *editTargetTilt;
    QPushButton *btnPtzMoveTo;
    QGridLayout *gridLayout_ptzGps;
    QLabel *labelTargetLat;
    QLineEdit *editTargetLat;
    QLabel *labelTargetLon;
    QLineEdit *editTargetLon;
    QLabel *labelTargetAlt;
    QLineEdit *editTargetAlt;
    QPushButton *btnPtzMoveToGps;
    QPushButton *btnPanZeroCalib;
    QHBoxLayout *horizontalLayout_wiper;
    QLabel *lblWiper;
    QPushButton *btnWiperStart;
    QPushButton *btnWiperStop;
    QPushButton *btnWiperLeft;
    QPushButton *btnWiperRight;
    QPushButton *btnWiperZeroCalib;
    QPushButton *btnWiperMode;
    QPushButton *btnWiperSilent;
    QHBoxLayout *horizontalLayout_wiper_current;
    QLabel *lblWiperCurrent;
    QLineEdit *editWiperCurrent;
    QSpacerItem *horizontalSpacer_wiper_current;
    QGroupBox *groupLens;
    QGridLayout *gridLayout_lens;
    QLabel *label2;
    QPushButton *btnZoomOut;
    QPushButton *btnZoomIn;
    QLabel *label3;
    QPushButton *btnFocusOut;
    QPushButton *btnFocusIn;
    QLabel *lblZoomSpeed;
    QHBoxLayout *horizontalLayout_zoomSpeed;
    QSlider *sliderZoomSpeed;
    QSpinBox *spinZoomSpeed;
    QGroupBox *groupPreset;
    QHBoxLayout *horizontalLayout_preset;
    QLabel *lblPreset;
    QSpinBox *spinPreset;
    QPushButton *btnCallPreset;
    QPushButton *btnSetPreset;
    QPushButton *btnDelPreset;
    QGroupBox *groupLocationSet;
    QFormLayout *formLayout_location;
    QLabel *lblSetLat;
    QLineEdit *editSetLat;
    QLabel *lblSetLon;
    QLineEdit *editSetLon;
    QLabel *lblSetHeight;
    QLineEdit *editSetHeight;
    QPushButton *btnSetLocation;
    QGroupBox *groupExtraSwitches;
    QGridLayout *gridLayout_extraSwitches;
    QCheckBox *checkDigitalZoom;
    QCheckBox *checkAutoZoom;
    QCheckBox *checkCaptureUpload;
    QCheckBox *checkPosReset;
    QSpacerItem *spacerRightPanel;
    QWidget *widgetBottomDashboard;
    QHBoxLayout *horizontalLayout_dashboard;
    QGroupBox *dashGroupPTZ;
    QGridLayout *gridLayout_statPtz;
    QLabel *label4;
    QLineEdit *statPanAngle;
    QLabel *label5;
    QLineEdit *statTiltAngle;
    QLabel *label6;
    QLineEdit *statDistance;
    QLabel *label7;
    QLineEdit *statCamMode;
    QLabel *label8;
    QLineEdit *statLatitude;
    QLabel *label9;
    QLineEdit *statLongitude;
    QLabel *label10;
    QLineEdit *statHeight;
    QGroupBox *dashGroupLens;
    QGridLayout *gridLayout_statLens;
    QLabel *label11;
    QLineEdit *statZoomVis;
    QLabel *label12;
    QLineEdit *statFocalVis;
    QLabel *label13;
    QLineEdit *statFovVis;
    QLabel *label14;
    QLineEdit *statFocusVis;
    QLabel *label15;
    QLineEdit *statZoomIR;
    QLabel *label16;
    QLineEdit *statFocalIR;
    QLabel *label17;
    QLineEdit *statFovIR;
    QLabel *label18;
    QLineEdit *statFocusIR;
    QGroupBox *dashGroupIdentify;
    QVBoxLayout *verticalLayout_identify;
    QLabel *lblIdentifyCount;
    QTableWidget *tableIdentify;
    QGroupBox *dashGroupTrack;
    QGridLayout *gridLayout_track;
    QLabel *lblTrackStatus;
    QLabel *label19;
    QLineEdit *trackPos;
    QLabel *label20;
    QLineEdit *trackDistance;
    QLabel *label21;
    QLineEdit *trackMissDistance;
    QGroupBox *dashGroupSysParams;
    QVBoxLayout *verticalLayout_sysParams;
    QPushButton *btnGetImageParams;
    QGridLayout *gridLayout_imgParamsInner;
    QLabel *label22;
    QLineEdit *paramResolution;
    QLabel *label23;
    QLineEdit *paramBitrate;
    QLabel *label24;
    QLineEdit *paramCodec;
    QLabel *label25;
    QLineEdit *paramWorkMode;
    QLabel *label26;
    QLineEdit *paramPipShow;
    QLabel *label27;
    QLineEdit *paramAlgoModel;
    QLabel *label28;
    QLineEdit *paramMaxVisFL;
    QLabel *label29;
    QLineEdit *paramMaxIRFL;
    QWidget *pagePlayback;
    QVBoxLayout *verticalLayout_pagePlayback;
    QLabel *labelPlaceholder1;
    QWidget *pageLog;
    QVBoxLayout *verticalLayout_pageLog;
    QLabel *labelPlaceholder2;
    QWidget *pageSettings;
    QVBoxLayout *verticalLayout_pageSettings;
    QLabel *labelPlaceholder3;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1440, 1027);
        MainWindow->setMinimumSize(QSize(960, 600));
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout_main = new QVBoxLayout(centralwidget);
        verticalLayout_main->setObjectName("verticalLayout_main");
        verticalLayout_main->setContentsMargins(0, 0, 0, 0);
        titleBar = new QWidget(centralwidget);
        titleBar->setObjectName("titleBar");
        titleBar->setMinimumSize(QSize(0, 32));
        titleBar->setMaximumSize(QSize(16777215, 32));
        titleBar->setCursor(QCursor(Qt::CursorShape::ArrowCursor));
        horizontalLayout_title = new QHBoxLayout(titleBar);
        horizontalLayout_title->setSpacing(0);
        horizontalLayout_title->setObjectName("horizontalLayout_title");
        horizontalLayout_title->setContentsMargins(0, 0, 0, 0);
        labelAppIcon = new QLabel(titleBar);
        labelAppIcon->setObjectName("labelAppIcon");
        labelAppIcon->setMinimumSize(QSize(20, 20));
        labelAppIcon->setMaximumSize(QSize(20, 20));

        horizontalLayout_title->addWidget(labelAppIcon);

        labelAppTitle = new QLabel(titleBar);
        labelAppTitle->setObjectName("labelAppTitle");
        labelAppTitle->setStyleSheet(QString::fromUtf8("font-size:12px;font-weight:bold;"));

        horizontalLayout_title->addWidget(labelAppTitle);

        btnNavMonitor = new QPushButton(titleBar);
        btnNavMonitor->setObjectName("btnNavMonitor");
        btnNavMonitor->setMinimumSize(QSize(80, 28));
        btnNavMonitor->setCursor(QCursor(Qt::CursorShape::ArrowCursor));
        btnNavMonitor->setFocusPolicy(Qt::FocusPolicy::NoFocus);
        btnNavMonitor->setCheckable(true);
        btnNavMonitor->setChecked(true);

        horizontalLayout_title->addWidget(btnNavMonitor);

        btnNavPlayback = new QPushButton(titleBar);
        btnNavPlayback->setObjectName("btnNavPlayback");
        btnNavPlayback->setMinimumSize(QSize(80, 28));
        btnNavPlayback->setCursor(QCursor(Qt::CursorShape::ArrowCursor));
        btnNavPlayback->setFocusPolicy(Qt::FocusPolicy::NoFocus);
        btnNavPlayback->setCheckable(true);

        horizontalLayout_title->addWidget(btnNavPlayback);

        btnNavLog = new QPushButton(titleBar);
        btnNavLog->setObjectName("btnNavLog");
        btnNavLog->setMinimumSize(QSize(80, 28));
        btnNavLog->setCursor(QCursor(Qt::CursorShape::ArrowCursor));
        btnNavLog->setFocusPolicy(Qt::FocusPolicy::NoFocus);
        btnNavLog->setCheckable(true);

        horizontalLayout_title->addWidget(btnNavLog);

        btnNavSettings = new QPushButton(titleBar);
        btnNavSettings->setObjectName("btnNavSettings");
        btnNavSettings->setMinimumSize(QSize(80, 28));
        btnNavSettings->setCursor(QCursor(Qt::CursorShape::ArrowCursor));
        btnNavSettings->setFocusPolicy(Qt::FocusPolicy::NoFocus);
        btnNavSettings->setCheckable(true);

        horizontalLayout_title->addWidget(btnNavSettings);

        spacerTitle = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_title->addItem(spacerTitle);

        btnMenu_Min = new QPushButton(titleBar);
        btnMenu_Min->setObjectName("btnMenu_Min");
        btnMenu_Min->setMinimumSize(QSize(46, 28));
        btnMenu_Min->setMaximumSize(QSize(46, 28));
        btnMenu_Min->setCursor(QCursor(Qt::CursorShape::ArrowCursor));
        btnMenu_Min->setFocusPolicy(Qt::FocusPolicy::NoFocus);

        horizontalLayout_title->addWidget(btnMenu_Min);

        btnMenu_Max = new QPushButton(titleBar);
        btnMenu_Max->setObjectName("btnMenu_Max");
        btnMenu_Max->setMinimumSize(QSize(46, 28));
        btnMenu_Max->setMaximumSize(QSize(46, 28));
        btnMenu_Max->setCursor(QCursor(Qt::CursorShape::ArrowCursor));
        btnMenu_Max->setFocusPolicy(Qt::FocusPolicy::NoFocus);

        horizontalLayout_title->addWidget(btnMenu_Max);

        btnMenu_Close = new QPushButton(titleBar);
        btnMenu_Close->setObjectName("btnMenu_Close");
        btnMenu_Close->setMinimumSize(QSize(46, 28));
        btnMenu_Close->setMaximumSize(QSize(46, 28));
        btnMenu_Close->setCursor(QCursor(Qt::CursorShape::ArrowCursor));
        btnMenu_Close->setFocusPolicy(Qt::FocusPolicy::NoFocus);

        horizontalLayout_title->addWidget(btnMenu_Close);


        verticalLayout_main->addWidget(titleBar);

        contentStack = new QStackedWidget(centralwidget);
        contentStack->setObjectName("contentStack");
        pageMonitor = new QWidget();
        pageMonitor->setObjectName("pageMonitor");
        verticalLayout_pageMonitor = new QVBoxLayout(pageMonitor);
        verticalLayout_pageMonitor->setSpacing(0);
        verticalLayout_pageMonitor->setObjectName("verticalLayout_pageMonitor");
        verticalLayout_pageMonitor->setContentsMargins(10, 0, 10, 10);
        horizontalLayout_top = new QHBoxLayout();
        horizontalLayout_top->setObjectName("horizontalLayout_top");
        btnMapToggle = new QPushButton(pageMonitor);
        btnMapToggle->setObjectName("btnMapToggle");
        btnMapToggle->setMinimumSize(QSize(36, 26));
        btnMapToggle->setMaximumSize(QSize(36, 26));
        btnMapToggle->setCheckable(true);

        horizontalLayout_top->addWidget(btnMapToggle);

        spacerTop = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_top->addItem(spacerTop);


        verticalLayout_pageMonitor->addLayout(horizontalLayout_top);

        horizontalLayout_middle = new QHBoxLayout();
        horizontalLayout_middle->setObjectName("horizontalLayout_middle");
        widgetDisplay = new QWidget(pageMonitor);
        widgetDisplay->setObjectName("widgetDisplay");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(widgetDisplay->sizePolicy().hasHeightForWidth());
        widgetDisplay->setSizePolicy(sizePolicy);
        verticalLayout_display = new QVBoxLayout(widgetDisplay);
        verticalLayout_display->setSpacing(0);
        verticalLayout_display->setObjectName("verticalLayout_display");
        verticalLayout_display->setContentsMargins(0, 0, 0, 0);
        videoGridContainer = new QWidget(widgetDisplay);
        videoGridContainer->setObjectName("videoGridContainer");

        verticalLayout_display->addWidget(videoGridContainer);


        horizontalLayout_middle->addWidget(widgetDisplay);

        scrollAreaControl = new QScrollArea(pageMonitor);
        scrollAreaControl->setObjectName("scrollAreaControl");
        scrollAreaControl->setMinimumSize(QSize(360, 0));
        scrollAreaControl->setMaximumSize(QSize(440, 16777215));
        scrollAreaControl->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        scrollAreaControl->setWidgetResizable(true);
        widgetControl = new QWidget();
        widgetControl->setObjectName("widgetControl");
        widgetControl->setGeometry(QRect(0, 0, 564, 790));
        verticalLayout_control = new QVBoxLayout(widgetControl);
        verticalLayout_control->setSpacing(4);
        verticalLayout_control->setObjectName("verticalLayout_control");
        verticalLayout_control->setContentsMargins(5, 0, 20, 0);
        groupAiAlgo = new QGroupBox(widgetControl);
        groupAiAlgo->setObjectName("groupAiAlgo");
        gridLayout_aiAlgo = new QGridLayout(groupAiAlgo);
        gridLayout_aiAlgo->setObjectName("gridLayout_aiAlgo");
        gridLayout_aiAlgo->setVerticalSpacing(4);
        lblAiMode = new QLabel(groupAiAlgo);
        lblAiMode->setObjectName("lblAiMode");

        gridLayout_aiAlgo->addWidget(lblAiMode, 0, 0, 1, 1);

        comboWorkMode = new QComboBox(groupAiAlgo);
        comboWorkMode->addItem(QString());
        comboWorkMode->addItem(QString());
        comboWorkMode->addItem(QString());
        comboWorkMode->addItem(QString());
        comboWorkMode->addItem(QString());
        comboWorkMode->setObjectName("comboWorkMode");

        gridLayout_aiAlgo->addWidget(comboWorkMode, 0, 1, 1, 2);

        lblAlgo = new QLabel(groupAiAlgo);
        lblAlgo->setObjectName("lblAlgo");

        gridLayout_aiAlgo->addWidget(lblAlgo, 1, 0, 1, 1);

        comboAlgoModel1 = new QComboBox(groupAiAlgo);
        comboAlgoModel1->addItem(QString());
        comboAlgoModel1->addItem(QString());
        comboAlgoModel1->setObjectName("comboAlgoModel1");

        gridLayout_aiAlgo->addWidget(comboAlgoModel1, 1, 1, 1, 1);

        comboAlgoModel2 = new QComboBox(groupAiAlgo);
        comboAlgoModel2->addItem(QString());
        comboAlgoModel2->addItem(QString());
        comboAlgoModel2->addItem(QString());
        comboAlgoModel2->addItem(QString());
        comboAlgoModel2->addItem(QString());
        comboAlgoModel2->setObjectName("comboAlgoModel2");

        gridLayout_aiAlgo->addWidget(comboAlgoModel2, 1, 2, 1, 1);

        lblDisplayMode = new QLabel(groupAiAlgo);
        lblDisplayMode->setObjectName("lblDisplayMode");

        gridLayout_aiAlgo->addWidget(lblDisplayMode, 2, 0, 1, 1);

        comboDisplayMode = new QComboBox(groupAiAlgo);
        comboDisplayMode->addItem(QString());
        comboDisplayMode->addItem(QString());
        comboDisplayMode->addItem(QString());
        comboDisplayMode->addItem(QString());
        comboDisplayMode->addItem(QString());
        comboDisplayMode->setObjectName("comboDisplayMode");

        gridLayout_aiAlgo->addWidget(comboDisplayMode, 2, 1, 1, 2);


        verticalLayout_control->addWidget(groupAiAlgo);

        groupPTZ = new QGroupBox(widgetControl);
        groupPTZ->setObjectName("groupPTZ");
        verticalLayout_ptz = new QVBoxLayout(groupPTZ);
        verticalLayout_ptz->setSpacing(4);
        verticalLayout_ptz->setObjectName("verticalLayout_ptz");
        gridLayout_8way = new QGridLayout();
        gridLayout_8way->setObjectName("gridLayout_8way");
        gridLayout_8way->setVerticalSpacing(2);
        btnPtzTopLeft = new QPushButton(groupPTZ);
        btnPtzTopLeft->setObjectName("btnPtzTopLeft");

        gridLayout_8way->addWidget(btnPtzTopLeft, 0, 0, 1, 1);

        btnPtzUp = new QPushButton(groupPTZ);
        btnPtzUp->setObjectName("btnPtzUp");

        gridLayout_8way->addWidget(btnPtzUp, 0, 1, 1, 1);

        btnPtzTopRight = new QPushButton(groupPTZ);
        btnPtzTopRight->setObjectName("btnPtzTopRight");

        gridLayout_8way->addWidget(btnPtzTopRight, 0, 2, 1, 1);

        btnPtzLeft = new QPushButton(groupPTZ);
        btnPtzLeft->setObjectName("btnPtzLeft");

        gridLayout_8way->addWidget(btnPtzLeft, 1, 0, 1, 1);

        btnPtzReset = new QPushButton(groupPTZ);
        btnPtzReset->setObjectName("btnPtzReset");

        gridLayout_8way->addWidget(btnPtzReset, 1, 1, 1, 1);

        btnPtzRight = new QPushButton(groupPTZ);
        btnPtzRight->setObjectName("btnPtzRight");

        gridLayout_8way->addWidget(btnPtzRight, 1, 2, 1, 1);

        btnPtzBottomLeft = new QPushButton(groupPTZ);
        btnPtzBottomLeft->setObjectName("btnPtzBottomLeft");

        gridLayout_8way->addWidget(btnPtzBottomLeft, 2, 0, 1, 1);

        btnPtzDown = new QPushButton(groupPTZ);
        btnPtzDown->setObjectName("btnPtzDown");

        gridLayout_8way->addWidget(btnPtzDown, 2, 1, 1, 1);

        btnPtzBottomRight = new QPushButton(groupPTZ);
        btnPtzBottomRight->setObjectName("btnPtzBottomRight");

        gridLayout_8way->addWidget(btnPtzBottomRight, 2, 2, 1, 1);


        verticalLayout_ptz->addLayout(gridLayout_8way);

        horizontalLayout_speed = new QHBoxLayout();
        horizontalLayout_speed->setObjectName("horizontalLayout_speed");
        lblSpeed = new QLabel(groupPTZ);
        lblSpeed->setObjectName("lblSpeed");

        horizontalLayout_speed->addWidget(lblSpeed);

        sliderSpeed = new QSlider(groupPTZ);
        sliderSpeed->setObjectName("sliderSpeed");
        sliderSpeed->setMinimum(1);
        sliderSpeed->setMaximum(63);
        sliderSpeed->setValue(63);
        sliderSpeed->setOrientation(Qt::Orientation::Horizontal);

        horizontalLayout_speed->addWidget(sliderSpeed);

        spinSpeed = new QSpinBox(groupPTZ);
        spinSpeed->setObjectName("spinSpeed");
        spinSpeed->setMinimum(1);
        spinSpeed->setMaximum(63);
        spinSpeed->setValue(63);

        horizontalLayout_speed->addWidget(spinSpeed);


        verticalLayout_ptz->addLayout(horizontalLayout_speed);

        horizontalLayout_angleMove = new QHBoxLayout();
        horizontalLayout_angleMove->setObjectName("horizontalLayout_angleMove");
        label = new QLabel(groupPTZ);
        label->setObjectName("label");

        horizontalLayout_angleMove->addWidget(label);

        editTargetPan = new QLineEdit(groupPTZ);
        editTargetPan->setObjectName("editTargetPan");

        horizontalLayout_angleMove->addWidget(editTargetPan);

        label1 = new QLabel(groupPTZ);
        label1->setObjectName("label1");

        horizontalLayout_angleMove->addWidget(label1);

        editTargetTilt = new QLineEdit(groupPTZ);
        editTargetTilt->setObjectName("editTargetTilt");

        horizontalLayout_angleMove->addWidget(editTargetTilt);

        btnPtzMoveTo = new QPushButton(groupPTZ);
        btnPtzMoveTo->setObjectName("btnPtzMoveTo");

        horizontalLayout_angleMove->addWidget(btnPtzMoveTo);


        verticalLayout_ptz->addLayout(horizontalLayout_angleMove);

        gridLayout_ptzGps = new QGridLayout();
        gridLayout_ptzGps->setObjectName("gridLayout_ptzGps");
        labelTargetLat = new QLabel(groupPTZ);
        labelTargetLat->setObjectName("labelTargetLat");

        gridLayout_ptzGps->addWidget(labelTargetLat, 0, 0, 1, 1);

        editTargetLat = new QLineEdit(groupPTZ);
        editTargetLat->setObjectName("editTargetLat");

        gridLayout_ptzGps->addWidget(editTargetLat, 0, 1, 1, 1);

        labelTargetLon = new QLabel(groupPTZ);
        labelTargetLon->setObjectName("labelTargetLon");

        gridLayout_ptzGps->addWidget(labelTargetLon, 0, 2, 1, 1);

        editTargetLon = new QLineEdit(groupPTZ);
        editTargetLon->setObjectName("editTargetLon");

        gridLayout_ptzGps->addWidget(editTargetLon, 0, 3, 1, 1);

        labelTargetAlt = new QLabel(groupPTZ);
        labelTargetAlt->setObjectName("labelTargetAlt");

        gridLayout_ptzGps->addWidget(labelTargetAlt, 1, 0, 1, 1);

        editTargetAlt = new QLineEdit(groupPTZ);
        editTargetAlt->setObjectName("editTargetAlt");

        gridLayout_ptzGps->addWidget(editTargetAlt, 1, 1, 1, 1);

        btnPtzMoveToGps = new QPushButton(groupPTZ);
        btnPtzMoveToGps->setObjectName("btnPtzMoveToGps");

        gridLayout_ptzGps->addWidget(btnPtzMoveToGps, 1, 2, 1, 2);


        verticalLayout_ptz->addLayout(gridLayout_ptzGps);

        btnPanZeroCalib = new QPushButton(groupPTZ);
        btnPanZeroCalib->setObjectName("btnPanZeroCalib");

        verticalLayout_ptz->addWidget(btnPanZeroCalib);

        horizontalLayout_wiper = new QHBoxLayout();
        horizontalLayout_wiper->setObjectName("horizontalLayout_wiper");
        lblWiper = new QLabel(groupPTZ);
        lblWiper->setObjectName("lblWiper");

        horizontalLayout_wiper->addWidget(lblWiper);

        btnWiperStart = new QPushButton(groupPTZ);
        btnWiperStart->setObjectName("btnWiperStart");

        horizontalLayout_wiper->addWidget(btnWiperStart);

        btnWiperStop = new QPushButton(groupPTZ);
        btnWiperStop->setObjectName("btnWiperStop");

        horizontalLayout_wiper->addWidget(btnWiperStop);

        btnWiperLeft = new QPushButton(groupPTZ);
        btnWiperLeft->setObjectName("btnWiperLeft");

        horizontalLayout_wiper->addWidget(btnWiperLeft);

        btnWiperRight = new QPushButton(groupPTZ);
        btnWiperRight->setObjectName("btnWiperRight");

        horizontalLayout_wiper->addWidget(btnWiperRight);

        btnWiperZeroCalib = new QPushButton(groupPTZ);
        btnWiperZeroCalib->setObjectName("btnWiperZeroCalib");

        horizontalLayout_wiper->addWidget(btnWiperZeroCalib);

        btnWiperMode = new QPushButton(groupPTZ);
        btnWiperMode->setObjectName("btnWiperMode");

        horizontalLayout_wiper->addWidget(btnWiperMode);

        btnWiperSilent = new QPushButton(groupPTZ);
        btnWiperSilent->setObjectName("btnWiperSilent");

        horizontalLayout_wiper->addWidget(btnWiperSilent);


        verticalLayout_ptz->addLayout(horizontalLayout_wiper);

        horizontalLayout_wiper_current = new QHBoxLayout();
        horizontalLayout_wiper_current->setObjectName("horizontalLayout_wiper_current");
        lblWiperCurrent = new QLabel(groupPTZ);
        lblWiperCurrent->setObjectName("lblWiperCurrent");

        horizontalLayout_wiper_current->addWidget(lblWiperCurrent);

        editWiperCurrent = new QLineEdit(groupPTZ);
        editWiperCurrent->setObjectName("editWiperCurrent");
        editWiperCurrent->setMinimumSize(QSize(150, 0));

        horizontalLayout_wiper_current->addWidget(editWiperCurrent);

        horizontalSpacer_wiper_current = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_wiper_current->addItem(horizontalSpacer_wiper_current);


        verticalLayout_ptz->addLayout(horizontalLayout_wiper_current);


        verticalLayout_control->addWidget(groupPTZ);

        groupLens = new QGroupBox(widgetControl);
        groupLens->setObjectName("groupLens");
        gridLayout_lens = new QGridLayout(groupLens);
        gridLayout_lens->setObjectName("gridLayout_lens");
        gridLayout_lens->setVerticalSpacing(4);
        label2 = new QLabel(groupLens);
        label2->setObjectName("label2");

        gridLayout_lens->addWidget(label2, 0, 0, 1, 1);

        btnZoomOut = new QPushButton(groupLens);
        btnZoomOut->setObjectName("btnZoomOut");

        gridLayout_lens->addWidget(btnZoomOut, 0, 1, 1, 1);

        btnZoomIn = new QPushButton(groupLens);
        btnZoomIn->setObjectName("btnZoomIn");

        gridLayout_lens->addWidget(btnZoomIn, 0, 2, 1, 1);

        label3 = new QLabel(groupLens);
        label3->setObjectName("label3");

        gridLayout_lens->addWidget(label3, 1, 0, 1, 1);

        btnFocusOut = new QPushButton(groupLens);
        btnFocusOut->setObjectName("btnFocusOut");

        gridLayout_lens->addWidget(btnFocusOut, 1, 1, 1, 1);

        btnFocusIn = new QPushButton(groupLens);
        btnFocusIn->setObjectName("btnFocusIn");

        gridLayout_lens->addWidget(btnFocusIn, 1, 2, 1, 1);

        lblZoomSpeed = new QLabel(groupLens);
        lblZoomSpeed->setObjectName("lblZoomSpeed");

        gridLayout_lens->addWidget(lblZoomSpeed, 2, 0, 1, 1);

        horizontalLayout_zoomSpeed = new QHBoxLayout();
        horizontalLayout_zoomSpeed->setObjectName("horizontalLayout_zoomSpeed");
        sliderZoomSpeed = new QSlider(groupLens);
        sliderZoomSpeed->setObjectName("sliderZoomSpeed");
        sliderZoomSpeed->setMinimum(1);
        sliderZoomSpeed->setMaximum(7);
        sliderZoomSpeed->setValue(5);
        sliderZoomSpeed->setOrientation(Qt::Orientation::Horizontal);

        horizontalLayout_zoomSpeed->addWidget(sliderZoomSpeed);

        spinZoomSpeed = new QSpinBox(groupLens);
        spinZoomSpeed->setObjectName("spinZoomSpeed");
        spinZoomSpeed->setMinimum(1);
        spinZoomSpeed->setMaximum(7);
        spinZoomSpeed->setValue(5);

        horizontalLayout_zoomSpeed->addWidget(spinZoomSpeed);


        gridLayout_lens->addLayout(horizontalLayout_zoomSpeed, 2, 1, 1, 2);


        verticalLayout_control->addWidget(groupLens);

        groupPreset = new QGroupBox(widgetControl);
        groupPreset->setObjectName("groupPreset");
        horizontalLayout_preset = new QHBoxLayout(groupPreset);
        horizontalLayout_preset->setSpacing(4);
        horizontalLayout_preset->setObjectName("horizontalLayout_preset");
        lblPreset = new QLabel(groupPreset);
        lblPreset->setObjectName("lblPreset");

        horizontalLayout_preset->addWidget(lblPreset);

        spinPreset = new QSpinBox(groupPreset);
        spinPreset->setObjectName("spinPreset");
        spinPreset->setMinimum(1);
        spinPreset->setMaximum(255);

        horizontalLayout_preset->addWidget(spinPreset);

        btnCallPreset = new QPushButton(groupPreset);
        btnCallPreset->setObjectName("btnCallPreset");

        horizontalLayout_preset->addWidget(btnCallPreset);

        btnSetPreset = new QPushButton(groupPreset);
        btnSetPreset->setObjectName("btnSetPreset");

        horizontalLayout_preset->addWidget(btnSetPreset);

        btnDelPreset = new QPushButton(groupPreset);
        btnDelPreset->setObjectName("btnDelPreset");

        horizontalLayout_preset->addWidget(btnDelPreset);


        verticalLayout_control->addWidget(groupPreset);

        groupLocationSet = new QGroupBox(widgetControl);
        groupLocationSet->setObjectName("groupLocationSet");
        formLayout_location = new QFormLayout(groupLocationSet);
        formLayout_location->setObjectName("formLayout_location");
        formLayout_location->setVerticalSpacing(4);
        lblSetLat = new QLabel(groupLocationSet);
        lblSetLat->setObjectName("lblSetLat");

        formLayout_location->setWidget(0, QFormLayout::ItemRole::LabelRole, lblSetLat);

        editSetLat = new QLineEdit(groupLocationSet);
        editSetLat->setObjectName("editSetLat");

        formLayout_location->setWidget(0, QFormLayout::ItemRole::FieldRole, editSetLat);

        lblSetLon = new QLabel(groupLocationSet);
        lblSetLon->setObjectName("lblSetLon");

        formLayout_location->setWidget(1, QFormLayout::ItemRole::LabelRole, lblSetLon);

        editSetLon = new QLineEdit(groupLocationSet);
        editSetLon->setObjectName("editSetLon");

        formLayout_location->setWidget(1, QFormLayout::ItemRole::FieldRole, editSetLon);

        lblSetHeight = new QLabel(groupLocationSet);
        lblSetHeight->setObjectName("lblSetHeight");

        formLayout_location->setWidget(2, QFormLayout::ItemRole::LabelRole, lblSetHeight);

        editSetHeight = new QLineEdit(groupLocationSet);
        editSetHeight->setObjectName("editSetHeight");

        formLayout_location->setWidget(2, QFormLayout::ItemRole::FieldRole, editSetHeight);

        btnSetLocation = new QPushButton(groupLocationSet);
        btnSetLocation->setObjectName("btnSetLocation");

        formLayout_location->setWidget(3, QFormLayout::ItemRole::SpanningRole, btnSetLocation);


        verticalLayout_control->addWidget(groupLocationSet);

        groupExtraSwitches = new QGroupBox(widgetControl);
        groupExtraSwitches->setObjectName("groupExtraSwitches");
        gridLayout_extraSwitches = new QGridLayout(groupExtraSwitches);
        gridLayout_extraSwitches->setObjectName("gridLayout_extraSwitches");
        gridLayout_extraSwitches->setVerticalSpacing(4);
        checkDigitalZoom = new QCheckBox(groupExtraSwitches);
        checkDigitalZoom->setObjectName("checkDigitalZoom");

        gridLayout_extraSwitches->addWidget(checkDigitalZoom, 0, 0, 1, 1);

        checkAutoZoom = new QCheckBox(groupExtraSwitches);
        checkAutoZoom->setObjectName("checkAutoZoom");

        gridLayout_extraSwitches->addWidget(checkAutoZoom, 0, 1, 1, 1);

        checkCaptureUpload = new QCheckBox(groupExtraSwitches);
        checkCaptureUpload->setObjectName("checkCaptureUpload");

        gridLayout_extraSwitches->addWidget(checkCaptureUpload, 1, 0, 1, 1);

        checkPosReset = new QCheckBox(groupExtraSwitches);
        checkPosReset->setObjectName("checkPosReset");

        gridLayout_extraSwitches->addWidget(checkPosReset, 1, 1, 1, 1);


        verticalLayout_control->addWidget(groupExtraSwitches);

        spacerRightPanel = new QSpacerItem(20, 5, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_control->addItem(spacerRightPanel);

        scrollAreaControl->setWidget(widgetControl);

        horizontalLayout_middle->addWidget(scrollAreaControl);

        horizontalLayout_middle->setStretch(0, 4);
        horizontalLayout_middle->setStretch(1, 1);

        verticalLayout_pageMonitor->addLayout(horizontalLayout_middle);

        widgetBottomDashboard = new QWidget(pageMonitor);
        widgetBottomDashboard->setObjectName("widgetBottomDashboard");
        widgetBottomDashboard->setMaximumSize(QSize(16777215, 150));
        horizontalLayout_dashboard = new QHBoxLayout(widgetBottomDashboard);
        horizontalLayout_dashboard->setSpacing(4);
        horizontalLayout_dashboard->setObjectName("horizontalLayout_dashboard");
        horizontalLayout_dashboard->setContentsMargins(0, 5, 0, 0);
        dashGroupPTZ = new QGroupBox(widgetBottomDashboard);
        dashGroupPTZ->setObjectName("dashGroupPTZ");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(dashGroupPTZ->sizePolicy().hasHeightForWidth());
        dashGroupPTZ->setSizePolicy(sizePolicy1);
        gridLayout_statPtz = new QGridLayout(dashGroupPTZ);
        gridLayout_statPtz->setSpacing(2);
        gridLayout_statPtz->setObjectName("gridLayout_statPtz");
        label4 = new QLabel(dashGroupPTZ);
        label4->setObjectName("label4");

        gridLayout_statPtz->addWidget(label4, 0, 0, 1, 1);

        statPanAngle = new QLineEdit(dashGroupPTZ);
        statPanAngle->setObjectName("statPanAngle");
        statPanAngle->setReadOnly(true);

        gridLayout_statPtz->addWidget(statPanAngle, 0, 1, 1, 1);

        label5 = new QLabel(dashGroupPTZ);
        label5->setObjectName("label5");

        gridLayout_statPtz->addWidget(label5, 0, 2, 1, 1);

        statTiltAngle = new QLineEdit(dashGroupPTZ);
        statTiltAngle->setObjectName("statTiltAngle");
        statTiltAngle->setReadOnly(true);

        gridLayout_statPtz->addWidget(statTiltAngle, 0, 3, 1, 1);

        label6 = new QLabel(dashGroupPTZ);
        label6->setObjectName("label6");

        gridLayout_statPtz->addWidget(label6, 1, 0, 1, 1);

        statDistance = new QLineEdit(dashGroupPTZ);
        statDistance->setObjectName("statDistance");
        statDistance->setReadOnly(true);

        gridLayout_statPtz->addWidget(statDistance, 1, 1, 1, 1);

        label7 = new QLabel(dashGroupPTZ);
        label7->setObjectName("label7");

        gridLayout_statPtz->addWidget(label7, 1, 2, 1, 1);

        statCamMode = new QLineEdit(dashGroupPTZ);
        statCamMode->setObjectName("statCamMode");
        statCamMode->setReadOnly(true);

        gridLayout_statPtz->addWidget(statCamMode, 1, 3, 1, 1);

        label8 = new QLabel(dashGroupPTZ);
        label8->setObjectName("label8");

        gridLayout_statPtz->addWidget(label8, 2, 0, 1, 1);

        statLatitude = new QLineEdit(dashGroupPTZ);
        statLatitude->setObjectName("statLatitude");
        statLatitude->setReadOnly(true);

        gridLayout_statPtz->addWidget(statLatitude, 2, 1, 1, 1);

        label9 = new QLabel(dashGroupPTZ);
        label9->setObjectName("label9");

        gridLayout_statPtz->addWidget(label9, 2, 2, 1, 1);

        statLongitude = new QLineEdit(dashGroupPTZ);
        statLongitude->setObjectName("statLongitude");
        statLongitude->setReadOnly(true);

        gridLayout_statPtz->addWidget(statLongitude, 2, 3, 1, 1);

        label10 = new QLabel(dashGroupPTZ);
        label10->setObjectName("label10");

        gridLayout_statPtz->addWidget(label10, 3, 0, 1, 1);

        statHeight = new QLineEdit(dashGroupPTZ);
        statHeight->setObjectName("statHeight");
        statHeight->setReadOnly(true);

        gridLayout_statPtz->addWidget(statHeight, 3, 1, 1, 1);


        horizontalLayout_dashboard->addWidget(dashGroupPTZ);

        dashGroupLens = new QGroupBox(widgetBottomDashboard);
        dashGroupLens->setObjectName("dashGroupLens");
        sizePolicy1.setHeightForWidth(dashGroupLens->sizePolicy().hasHeightForWidth());
        dashGroupLens->setSizePolicy(sizePolicy1);
        gridLayout_statLens = new QGridLayout(dashGroupLens);
        gridLayout_statLens->setSpacing(2);
        gridLayout_statLens->setObjectName("gridLayout_statLens");
        label11 = new QLabel(dashGroupLens);
        label11->setObjectName("label11");

        gridLayout_statLens->addWidget(label11, 0, 0, 1, 1);

        statZoomVis = new QLineEdit(dashGroupLens);
        statZoomVis->setObjectName("statZoomVis");
        statZoomVis->setMaximumSize(QSize(60, 16777215));
        statZoomVis->setReadOnly(true);

        gridLayout_statLens->addWidget(statZoomVis, 0, 1, 1, 1);

        label12 = new QLabel(dashGroupLens);
        label12->setObjectName("label12");

        gridLayout_statLens->addWidget(label12, 0, 2, 1, 1);

        statFocalVis = new QLineEdit(dashGroupLens);
        statFocalVis->setObjectName("statFocalVis");
        statFocalVis->setMaximumSize(QSize(60, 16777215));
        statFocalVis->setReadOnly(true);

        gridLayout_statLens->addWidget(statFocalVis, 0, 3, 1, 1);

        label13 = new QLabel(dashGroupLens);
        label13->setObjectName("label13");

        gridLayout_statLens->addWidget(label13, 1, 0, 1, 1);

        statFovVis = new QLineEdit(dashGroupLens);
        statFovVis->setObjectName("statFovVis");
        statFovVis->setMaximumSize(QSize(60, 16777215));
        statFovVis->setReadOnly(true);

        gridLayout_statLens->addWidget(statFovVis, 1, 1, 1, 1);

        label14 = new QLabel(dashGroupLens);
        label14->setObjectName("label14");

        gridLayout_statLens->addWidget(label14, 1, 2, 1, 1);

        statFocusVis = new QLineEdit(dashGroupLens);
        statFocusVis->setObjectName("statFocusVis");
        statFocusVis->setMaximumSize(QSize(60, 16777215));
        statFocusVis->setReadOnly(true);

        gridLayout_statLens->addWidget(statFocusVis, 1, 3, 1, 1);

        label15 = new QLabel(dashGroupLens);
        label15->setObjectName("label15");

        gridLayout_statLens->addWidget(label15, 2, 0, 1, 1);

        statZoomIR = new QLineEdit(dashGroupLens);
        statZoomIR->setObjectName("statZoomIR");
        statZoomIR->setMaximumSize(QSize(60, 16777215));
        statZoomIR->setReadOnly(true);

        gridLayout_statLens->addWidget(statZoomIR, 2, 1, 1, 1);

        label16 = new QLabel(dashGroupLens);
        label16->setObjectName("label16");

        gridLayout_statLens->addWidget(label16, 2, 2, 1, 1);

        statFocalIR = new QLineEdit(dashGroupLens);
        statFocalIR->setObjectName("statFocalIR");
        statFocalIR->setMaximumSize(QSize(60, 16777215));
        statFocalIR->setReadOnly(true);

        gridLayout_statLens->addWidget(statFocalIR, 2, 3, 1, 1);

        label17 = new QLabel(dashGroupLens);
        label17->setObjectName("label17");

        gridLayout_statLens->addWidget(label17, 3, 0, 1, 1);

        statFovIR = new QLineEdit(dashGroupLens);
        statFovIR->setObjectName("statFovIR");
        statFovIR->setMaximumSize(QSize(60, 16777215));
        statFovIR->setReadOnly(true);

        gridLayout_statLens->addWidget(statFovIR, 3, 1, 1, 1);

        label18 = new QLabel(dashGroupLens);
        label18->setObjectName("label18");

        gridLayout_statLens->addWidget(label18, 3, 2, 1, 1);

        statFocusIR = new QLineEdit(dashGroupLens);
        statFocusIR->setObjectName("statFocusIR");
        statFocusIR->setMaximumSize(QSize(60, 16777215));
        statFocusIR->setReadOnly(true);

        gridLayout_statLens->addWidget(statFocusIR, 3, 3, 1, 1);


        horizontalLayout_dashboard->addWidget(dashGroupLens);

        dashGroupIdentify = new QGroupBox(widgetBottomDashboard);
        dashGroupIdentify->setObjectName("dashGroupIdentify");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(dashGroupIdentify->sizePolicy().hasHeightForWidth());
        dashGroupIdentify->setSizePolicy(sizePolicy2);
        verticalLayout_identify = new QVBoxLayout(dashGroupIdentify);
        verticalLayout_identify->setObjectName("verticalLayout_identify");
        verticalLayout_identify->setContentsMargins(-1, 4, -1, 2);
        lblIdentifyCount = new QLabel(dashGroupIdentify);
        lblIdentifyCount->setObjectName("lblIdentifyCount");

        verticalLayout_identify->addWidget(lblIdentifyCount);

        tableIdentify = new QTableWidget(dashGroupIdentify);
        if (tableIdentify->columnCount() < 5)
            tableIdentify->setColumnCount(5);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        tableIdentify->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        tableIdentify->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        tableIdentify->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        tableIdentify->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        tableIdentify->setHorizontalHeaderItem(4, __qtablewidgetitem4);
        tableIdentify->setObjectName("tableIdentify");
        tableIdentify->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        tableIdentify->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        tableIdentify->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        tableIdentify->horizontalHeader()->setDefaultSectionSize(45);
        tableIdentify->horizontalHeader()->setStretchLastSection(true);
        tableIdentify->verticalHeader()->setVisible(false);

        verticalLayout_identify->addWidget(tableIdentify);


        horizontalLayout_dashboard->addWidget(dashGroupIdentify);

        dashGroupTrack = new QGroupBox(widgetBottomDashboard);
        dashGroupTrack->setObjectName("dashGroupTrack");
        sizePolicy1.setHeightForWidth(dashGroupTrack->sizePolicy().hasHeightForWidth());
        dashGroupTrack->setSizePolicy(sizePolicy1);
        gridLayout_track = new QGridLayout(dashGroupTrack);
        gridLayout_track->setSpacing(2);
        gridLayout_track->setObjectName("gridLayout_track");
        lblTrackStatus = new QLabel(dashGroupTrack);
        lblTrackStatus->setObjectName("lblTrackStatus");

        gridLayout_track->addWidget(lblTrackStatus, 0, 0, 1, 2);

        label19 = new QLabel(dashGroupTrack);
        label19->setObjectName("label19");

        gridLayout_track->addWidget(label19, 1, 0, 1, 1);

        trackPos = new QLineEdit(dashGroupTrack);
        trackPos->setObjectName("trackPos");
        trackPos->setMinimumSize(QSize(150, 0));
        trackPos->setReadOnly(true);

        gridLayout_track->addWidget(trackPos, 1, 1, 1, 1);

        label20 = new QLabel(dashGroupTrack);
        label20->setObjectName("label20");

        gridLayout_track->addWidget(label20, 2, 0, 1, 1);

        trackDistance = new QLineEdit(dashGroupTrack);
        trackDistance->setObjectName("trackDistance");
        trackDistance->setReadOnly(true);

        gridLayout_track->addWidget(trackDistance, 2, 1, 1, 1);

        label21 = new QLabel(dashGroupTrack);
        label21->setObjectName("label21");

        gridLayout_track->addWidget(label21, 3, 0, 1, 1);

        trackMissDistance = new QLineEdit(dashGroupTrack);
        trackMissDistance->setObjectName("trackMissDistance");
        trackMissDistance->setMinimumSize(QSize(150, 0));
        trackMissDistance->setReadOnly(true);

        gridLayout_track->addWidget(trackMissDistance, 3, 1, 1, 1);


        horizontalLayout_dashboard->addWidget(dashGroupTrack);

        dashGroupSysParams = new QGroupBox(widgetBottomDashboard);
        dashGroupSysParams->setObjectName("dashGroupSysParams");
        sizePolicy1.setHeightForWidth(dashGroupSysParams->sizePolicy().hasHeightForWidth());
        dashGroupSysParams->setSizePolicy(sizePolicy1);
        verticalLayout_sysParams = new QVBoxLayout(dashGroupSysParams);
        verticalLayout_sysParams->setSpacing(2);
        verticalLayout_sysParams->setObjectName("verticalLayout_sysParams");
        verticalLayout_sysParams->setContentsMargins(-1, 8, -1, 2);
        btnGetImageParams = new QPushButton(dashGroupSysParams);
        btnGetImageParams->setObjectName("btnGetImageParams");

        verticalLayout_sysParams->addWidget(btnGetImageParams);

        gridLayout_imgParamsInner = new QGridLayout();
        gridLayout_imgParamsInner->setSpacing(2);
        gridLayout_imgParamsInner->setObjectName("gridLayout_imgParamsInner");
        label22 = new QLabel(dashGroupSysParams);
        label22->setObjectName("label22");

        gridLayout_imgParamsInner->addWidget(label22, 0, 0, 1, 1);

        paramResolution = new QLineEdit(dashGroupSysParams);
        paramResolution->setObjectName("paramResolution");
        paramResolution->setReadOnly(true);

        gridLayout_imgParamsInner->addWidget(paramResolution, 0, 1, 1, 1);

        label23 = new QLabel(dashGroupSysParams);
        label23->setObjectName("label23");

        gridLayout_imgParamsInner->addWidget(label23, 0, 2, 1, 1);

        paramBitrate = new QLineEdit(dashGroupSysParams);
        paramBitrate->setObjectName("paramBitrate");
        paramBitrate->setReadOnly(true);

        gridLayout_imgParamsInner->addWidget(paramBitrate, 0, 3, 1, 1);

        label24 = new QLabel(dashGroupSysParams);
        label24->setObjectName("label24");

        gridLayout_imgParamsInner->addWidget(label24, 1, 0, 1, 1);

        paramCodec = new QLineEdit(dashGroupSysParams);
        paramCodec->setObjectName("paramCodec");
        paramCodec->setReadOnly(true);

        gridLayout_imgParamsInner->addWidget(paramCodec, 1, 1, 1, 1);

        label25 = new QLabel(dashGroupSysParams);
        label25->setObjectName("label25");

        gridLayout_imgParamsInner->addWidget(label25, 1, 2, 1, 1);

        paramWorkMode = new QLineEdit(dashGroupSysParams);
        paramWorkMode->setObjectName("paramWorkMode");
        paramWorkMode->setReadOnly(true);

        gridLayout_imgParamsInner->addWidget(paramWorkMode, 1, 3, 1, 1);

        label26 = new QLabel(dashGroupSysParams);
        label26->setObjectName("label26");

        gridLayout_imgParamsInner->addWidget(label26, 2, 0, 1, 1);

        paramPipShow = new QLineEdit(dashGroupSysParams);
        paramPipShow->setObjectName("paramPipShow");
        paramPipShow->setReadOnly(true);

        gridLayout_imgParamsInner->addWidget(paramPipShow, 2, 1, 1, 1);

        label27 = new QLabel(dashGroupSysParams);
        label27->setObjectName("label27");

        gridLayout_imgParamsInner->addWidget(label27, 2, 2, 1, 1);

        paramAlgoModel = new QLineEdit(dashGroupSysParams);
        paramAlgoModel->setObjectName("paramAlgoModel");
        paramAlgoModel->setReadOnly(true);

        gridLayout_imgParamsInner->addWidget(paramAlgoModel, 2, 3, 1, 1);

        label28 = new QLabel(dashGroupSysParams);
        label28->setObjectName("label28");

        gridLayout_imgParamsInner->addWidget(label28, 3, 0, 1, 1);

        paramMaxVisFL = new QLineEdit(dashGroupSysParams);
        paramMaxVisFL->setObjectName("paramMaxVisFL");
        paramMaxVisFL->setReadOnly(true);

        gridLayout_imgParamsInner->addWidget(paramMaxVisFL, 3, 1, 1, 1);

        label29 = new QLabel(dashGroupSysParams);
        label29->setObjectName("label29");

        gridLayout_imgParamsInner->addWidget(label29, 3, 2, 1, 1);

        paramMaxIRFL = new QLineEdit(dashGroupSysParams);
        paramMaxIRFL->setObjectName("paramMaxIRFL");
        paramMaxIRFL->setReadOnly(true);

        gridLayout_imgParamsInner->addWidget(paramMaxIRFL, 3, 3, 1, 1);


        verticalLayout_sysParams->addLayout(gridLayout_imgParamsInner);


        horizontalLayout_dashboard->addWidget(dashGroupSysParams);


        verticalLayout_pageMonitor->addWidget(widgetBottomDashboard);

        contentStack->addWidget(pageMonitor);
        pagePlayback = new QWidget();
        pagePlayback->setObjectName("pagePlayback");
        verticalLayout_pagePlayback = new QVBoxLayout(pagePlayback);
        verticalLayout_pagePlayback->setObjectName("verticalLayout_pagePlayback");
        labelPlaceholder1 = new QLabel(pagePlayback);
        labelPlaceholder1->setObjectName("labelPlaceholder1");
        labelPlaceholder1->setStyleSheet(QString::fromUtf8("font-size:20px;color:#888888;"));
        labelPlaceholder1->setAlignment(Qt::AlignmentFlag::AlignCenter);

        verticalLayout_pagePlayback->addWidget(labelPlaceholder1);

        contentStack->addWidget(pagePlayback);
        pageLog = new QWidget();
        pageLog->setObjectName("pageLog");
        verticalLayout_pageLog = new QVBoxLayout(pageLog);
        verticalLayout_pageLog->setObjectName("verticalLayout_pageLog");
        labelPlaceholder2 = new QLabel(pageLog);
        labelPlaceholder2->setObjectName("labelPlaceholder2");
        labelPlaceholder2->setStyleSheet(QString::fromUtf8("font-size:20px;color:#888888;"));
        labelPlaceholder2->setAlignment(Qt::AlignmentFlag::AlignCenter);

        verticalLayout_pageLog->addWidget(labelPlaceholder2);

        contentStack->addWidget(pageLog);
        pageSettings = new QWidget();
        pageSettings->setObjectName("pageSettings");
        verticalLayout_pageSettings = new QVBoxLayout(pageSettings);
        verticalLayout_pageSettings->setObjectName("verticalLayout_pageSettings");
        labelPlaceholder3 = new QLabel(pageSettings);
        labelPlaceholder3->setObjectName("labelPlaceholder3");
        labelPlaceholder3->setStyleSheet(QString::fromUtf8("font-size:20px;color:#888888;"));
        labelPlaceholder3->setAlignment(Qt::AlignmentFlag::AlignCenter);

        verticalLayout_pageSettings->addWidget(labelPlaceholder3);

        contentStack->addWidget(pageSettings);

        verticalLayout_main->addWidget(contentStack);

        MainWindow->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        contentStack->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "T-JSON \347\233\221\346\216\247\346\216\247\345\210\266\345\256\242\346\210\267\347\253\257", nullptr));
        labelAppIcon->setText(QString());
        labelAppTitle->setText(QCoreApplication::translate("MainWindow", "T-JSON", nullptr));
        btnNavMonitor->setText(QCoreApplication::translate("MainWindow", "\350\247\206\351\242\221\347\233\221\346\216\247", nullptr));
        btnNavPlayback->setText(QCoreApplication::translate("MainWindow", "\350\247\206\351\242\221\345\233\236\346\224\276", nullptr));
        btnNavLog->setText(QCoreApplication::translate("MainWindow", "\346\227\245\345\277\227\346\237\245\350\257\242", nullptr));
        btnNavSettings->setText(QCoreApplication::translate("MainWindow", "\347\263\273\347\273\237\350\256\276\347\275\256", nullptr));
#if QT_CONFIG(tooltip)
        btnMenu_Min->setToolTip(QCoreApplication::translate("MainWindow", "\346\234\200\345\260\217\345\214\226", nullptr));
#endif // QT_CONFIG(tooltip)
        btnMenu_Min->setText(QString());
#if QT_CONFIG(tooltip)
        btnMenu_Max->setToolTip(QCoreApplication::translate("MainWindow", "\346\234\200\345\244\247\345\214\226", nullptr));
#endif // QT_CONFIG(tooltip)
        btnMenu_Max->setText(QString());
#if QT_CONFIG(tooltip)
        btnMenu_Close->setToolTip(QCoreApplication::translate("MainWindow", "\345\205\263\351\227\255", nullptr));
#endif // QT_CONFIG(tooltip)
        btnMenu_Close->setText(QString());
        btnMapToggle->setText(QCoreApplication::translate("MainWindow", "\345\234\260\345\233\276", nullptr));
        groupAiAlgo->setTitle(QCoreApplication::translate("MainWindow", "AI \344\270\216\346\230\276\347\244\272", nullptr));
        lblAiMode->setText(QCoreApplication::translate("MainWindow", "AI\346\250\241\345\274\217:", nullptr));
        comboWorkMode->setItemText(0, QCoreApplication::translate("MainWindow", "\345\205\263\351\227\255AI", nullptr));
        comboWorkMode->setItemText(1, QCoreApplication::translate("MainWindow", "\347\233\256\346\240\207\350\257\206\345\210\253", nullptr));
        comboWorkMode->setItemText(2, QCoreApplication::translate("MainWindow", "\350\207\252\345\212\250\350\267\237\350\270\252", nullptr));
        comboWorkMode->setItemText(3, QCoreApplication::translate("MainWindow", "\347\202\271\351\200\211\350\267\237\350\270\252", nullptr));
        comboWorkMode->setItemText(4, QCoreApplication::translate("MainWindow", "\346\241\206\351\200\211\350\267\237\350\270\252", nullptr));

        lblAlgo->setText(QCoreApplication::translate("MainWindow", "\347\256\227\346\263\225:", nullptr));
        comboAlgoModel1->setItemText(0, QCoreApplication::translate("MainWindow", "0:\345\217\257\350\247\201\345\205\211\346\250\241\345\236\213", nullptr));
        comboAlgoModel1->setItemText(1, QCoreApplication::translate("MainWindow", "1:\347\272\242\345\244\226\346\250\241\345\236\213", nullptr));

        comboAlgoModel2->setItemText(0, QCoreApplication::translate("MainWindow", "2:\344\272\272\350\275\246\350\257\206\345\210\253", nullptr));
        comboAlgoModel2->setItemText(1, QCoreApplication::translate("MainWindow", "3:\350\210\271\350\257\206\345\210\253", nullptr));
        comboAlgoModel2->setItemText(2, QCoreApplication::translate("MainWindow", "4:\346\227\240\344\272\272\346\234\272\350\257\206\345\210\253", nullptr));
        comboAlgoModel2->setItemText(3, QCoreApplication::translate("MainWindow", "5:\351\243\236\346\234\272,\347\233\264\345\215\207\346\234\272\350\257\206\345\210\253", nullptr));
        comboAlgoModel2->setItemText(4, QCoreApplication::translate("MainWindow", "6:\351\270\237\350\257\206\345\210\253", nullptr));

        lblDisplayMode->setText(QCoreApplication::translate("MainWindow", "\346\230\276\347\244\272:", nullptr));
        comboDisplayMode->setItemText(0, QCoreApplication::translate("MainWindow", "0:\345\244\247\345\233\276\345\217\257\350\247\201\345\205\211,\345\260\217\345\233\276\347\272\242\345\244\226", nullptr));
        comboDisplayMode->setItemText(1, QCoreApplication::translate("MainWindow", "1:\347\272\242\345\244\226", nullptr));
        comboDisplayMode->setItemText(2, QCoreApplication::translate("MainWindow", "2:\345\217\257\350\247\201\345\205\211", nullptr));
        comboDisplayMode->setItemText(3, QCoreApplication::translate("MainWindow", "3:\350\236\215\345\220\210", nullptr));
        comboDisplayMode->setItemText(4, QCoreApplication::translate("MainWindow", "4:\345\244\247\345\233\276\347\272\242\345\244\226,\345\260\217\345\233\276\345\217\257\350\247\201\345\205\211", nullptr));

        groupPTZ->setTitle(QCoreApplication::translate("MainWindow", "\344\272\221\345\217\260\346\216\247\345\210\266 (PTZ)", nullptr));
        btnPtzTopLeft->setText(QCoreApplication::translate("MainWindow", "\342\206\226", nullptr));
        btnPtzUp->setText(QCoreApplication::translate("MainWindow", "\342\206\221", nullptr));
        btnPtzTopRight->setText(QCoreApplication::translate("MainWindow", "\342\206\227", nullptr));
        btnPtzLeft->setText(QCoreApplication::translate("MainWindow", "\342\206\220", nullptr));
        btnPtzReset->setText(QCoreApplication::translate("MainWindow", "\345\244\215\344\275\215", nullptr));
        btnPtzRight->setText(QCoreApplication::translate("MainWindow", "\342\206\222", nullptr));
        btnPtzBottomLeft->setText(QCoreApplication::translate("MainWindow", "\342\206\231", nullptr));
        btnPtzDown->setText(QCoreApplication::translate("MainWindow", "\342\206\223", nullptr));
        btnPtzBottomRight->setText(QCoreApplication::translate("MainWindow", "\342\206\230", nullptr));
        lblSpeed->setText(QCoreApplication::translate("MainWindow", "\351\200\237\345\272\246:", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "\346\226\271\344\275\215(\302\260):", nullptr));
        label1->setText(QCoreApplication::translate("MainWindow", "\344\277\257\344\273\260(\302\260):", nullptr));
        btnPtzMoveTo->setText(QCoreApplication::translate("MainWindow", "\344\270\213\345\217\221", nullptr));
        labelTargetLat->setText(QCoreApplication::translate("MainWindow", "\347\272\254\345\272\246:", nullptr));
        editTargetLat->setPlaceholderText(QCoreApplication::translate("MainWindow", "\302\260", nullptr));
        labelTargetLon->setText(QCoreApplication::translate("MainWindow", "\347\273\217\345\272\246:", nullptr));
        editTargetLon->setPlaceholderText(QCoreApplication::translate("MainWindow", "\302\260", nullptr));
        labelTargetAlt->setText(QCoreApplication::translate("MainWindow", "\351\253\230\345\272\246:", nullptr));
        editTargetAlt->setPlaceholderText(QCoreApplication::translate("MainWindow", "m", nullptr));
        btnPtzMoveToGps->setText(QCoreApplication::translate("MainWindow", "\347\233\256\346\240\207\345\274\225\345\257\274", nullptr));
        btnPanZeroCalib->setText(QCoreApplication::translate("MainWindow", "\346\260\264\345\271\263\351\233\266\347\202\271\346\240\207\345\256\232", nullptr));
        lblWiper->setText(QCoreApplication::translate("MainWindow", "\351\233\250\345\210\267\347\224\265\346\234\272:", nullptr));
        btnWiperStart->setText(QCoreApplication::translate("MainWindow", "\345\274\200\345\247\213", nullptr));
        btnWiperStop->setText(QCoreApplication::translate("MainWindow", "\345\201\234\346\255\242", nullptr));
        btnWiperLeft->setText(QCoreApplication::translate("MainWindow", "\345\267\246\350\275\254", nullptr));
        btnWiperRight->setText(QCoreApplication::translate("MainWindow", "\345\217\263\350\275\254", nullptr));
        btnWiperZeroCalib->setText(QCoreApplication::translate("MainWindow", "\351\233\266\347\202\271\346\240\241\345\207\206", nullptr));
        btnWiperMode->setText(QCoreApplication::translate("MainWindow", "\345\210\207\346\215\242\346\250\241\345\274\217", nullptr));
        btnWiperSilent->setText(QCoreApplication::translate("MainWindow", "\351\235\231\351\237\263\346\250\241\345\274\217", nullptr));
        lblWiperCurrent->setText(QCoreApplication::translate("MainWindow", "\347\224\265\346\234\272\347\224\265\346\265\201(mA):", nullptr));
        editWiperCurrent->setPlaceholderText(QCoreApplication::translate("MainWindow", "\350\276\223\345\205\245\347\224\265\346\265\2010-2000\357\274\214\345\233\236\350\275\246\350\256\276\347\275\256", nullptr));
        groupLens->setTitle(QCoreApplication::translate("MainWindow", "\351\225\234\345\244\264\346\216\247\345\210\266 (Lens)", nullptr));
        label2->setText(QCoreApplication::translate("MainWindow", "\345\217\230\345\200\215(Zoom):", nullptr));
        btnZoomOut->setText(QCoreApplication::translate("MainWindow", "\347\274\251\345\260\217 (-)", nullptr));
        btnZoomIn->setText(QCoreApplication::translate("MainWindow", "\346\224\276\345\244\247 (+)", nullptr));
        label3->setText(QCoreApplication::translate("MainWindow", "\345\217\230\347\204\246(Focus):", nullptr));
        btnFocusOut->setText(QCoreApplication::translate("MainWindow", "\350\277\221\347\204\246 (-)", nullptr));
        btnFocusIn->setText(QCoreApplication::translate("MainWindow", "\350\277\234\347\204\246 (+)", nullptr));
        lblZoomSpeed->setText(QCoreApplication::translate("MainWindow", "\345\217\230\345\200\215\351\200\237\345\272\246:", nullptr));
        groupPreset->setTitle(QCoreApplication::translate("MainWindow", "\351\242\204\347\275\256\344\275\215\346\216\247\345\210\266", nullptr));
        lblPreset->setText(QCoreApplication::translate("MainWindow", "\347\274\226\345\217\267:", nullptr));
        btnCallPreset->setText(QCoreApplication::translate("MainWindow", "\350\260\203\347\224\250", nullptr));
        btnSetPreset->setText(QCoreApplication::translate("MainWindow", "\350\256\276\347\275\256", nullptr));
        btnDelPreset->setText(QCoreApplication::translate("MainWindow", "\345\210\240\351\231\244", nullptr));
        groupLocationSet->setTitle(QCoreApplication::translate("MainWindow", "\350\256\276\345\244\207\347\273\217\347\272\254\345\272\246\351\205\215\347\275\256", nullptr));
        lblSetLat->setText(QCoreApplication::translate("MainWindow", "\347\272\254\345\272\246:", nullptr));
        editSetLat->setPlaceholderText(QCoreApplication::translate("MainWindow", "39.8365N", nullptr));
        lblSetLon->setText(QCoreApplication::translate("MainWindow", "\347\273\217\345\272\246:", nullptr));
        editSetLon->setPlaceholderText(QCoreApplication::translate("MainWindow", "116.2874E", nullptr));
        lblSetHeight->setText(QCoreApplication::translate("MainWindow", "\351\253\230\345\272\246(m):", nullptr));
        editSetHeight->setPlaceholderText(QCoreApplication::translate("MainWindow", "0", nullptr));
        btnSetLocation->setText(QCoreApplication::translate("MainWindow", "\344\270\213\345\217\221\345\217\202\346\225\260", nullptr));
        groupExtraSwitches->setTitle(QCoreApplication::translate("MainWindow", "\351\231\204\345\212\240\345\212\237\350\203\275\345\274\200\345\205\263", nullptr));
        checkDigitalZoom->setText(QCoreApplication::translate("MainWindow", "\346\225\260\345\255\227\345\217\230\345\200\215", nullptr));
        checkAutoZoom->setText(QCoreApplication::translate("MainWindow", "\350\207\252\345\212\250\345\217\230\345\200\215", nullptr));
        checkCaptureUpload->setText(QCoreApplication::translate("MainWindow", "\346\212\223\346\213\215\344\270\212\344\274\240", nullptr));
        checkPosReset->setText(QCoreApplication::translate("MainWindow", "\344\275\215\347\275\256\345\244\215\344\275\215", nullptr));
        dashGroupPTZ->setTitle(QCoreApplication::translate("MainWindow", "\344\272\221\345\217\260\347\212\266\346\200\201", nullptr));
        label4->setText(QCoreApplication::translate("MainWindow", "\346\260\264\345\271\263:", nullptr));
        label5->setText(QCoreApplication::translate("MainWindow", "\345\236\202\347\233\264:", nullptr));
        label6->setText(QCoreApplication::translate("MainWindow", "\350\267\235\347\246\273:", nullptr));
        label7->setText(QCoreApplication::translate("MainWindow", "\346\250\241\345\274\217:", nullptr));
        label8->setText(QCoreApplication::translate("MainWindow", "\347\272\254\345\272\246:", nullptr));
        label9->setText(QCoreApplication::translate("MainWindow", "\347\273\217\345\272\246:", nullptr));
        label10->setText(QCoreApplication::translate("MainWindow", "\351\253\230\345\272\246:", nullptr));
        dashGroupLens->setTitle(QCoreApplication::translate("MainWindow", "\351\225\234\345\244\264\347\212\266\346\200\201", nullptr));
        label11->setText(QCoreApplication::translate("MainWindow", "\345\217\257\350\247\201\345\200\215\347\216\207:", nullptr));
        label12->setText(QCoreApplication::translate("MainWindow", "\345\217\257\350\247\201\347\204\246\350\267\235:", nullptr));
        label13->setText(QCoreApplication::translate("MainWindow", "\345\217\257\350\247\201\350\247\206\345\234\272:", nullptr));
        label14->setText(QCoreApplication::translate("MainWindow", "\345\217\257\350\247\201\350\201\232\347\204\246:", nullptr));
        label15->setText(QCoreApplication::translate("MainWindow", "\347\272\242\345\244\226\345\200\215\347\216\207:", nullptr));
        label16->setText(QCoreApplication::translate("MainWindow", "\347\272\242\345\244\226\347\204\246\350\267\235:", nullptr));
        label17->setText(QCoreApplication::translate("MainWindow", "\347\272\242\345\244\226\350\247\206\345\234\272:", nullptr));
        label18->setText(QCoreApplication::translate("MainWindow", "\347\272\242\345\244\226\350\201\232\347\204\246:", nullptr));
        dashGroupIdentify->setTitle(QCoreApplication::translate("MainWindow", "\350\257\206\345\210\253\344\270\212\346\212\245", nullptr));
        lblIdentifyCount->setText(QCoreApplication::translate("MainWindow", "\347\233\256\346\240\207\346\200\273\346\225\260: 0", nullptr));
        QTableWidgetItem *___qtablewidgetitem = tableIdentify->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("MainWindow", "ID", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = tableIdentify->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("MainWindow", "\347\261\273\345\210\253", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = tableIdentify->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("MainWindow", "\350\267\235\347\246\273", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = tableIdentify->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("MainWindow", "\345\235\220\346\240\207(X,Y)", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = tableIdentify->horizontalHeaderItem(4);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("MainWindow", "\350\204\261\351\235\266(mrad)", nullptr));
        dashGroupTrack->setTitle(QCoreApplication::translate("MainWindow", "\350\267\237\350\270\252\344\270\212\346\212\245", nullptr));
        lblTrackStatus->setText(QCoreApplication::translate("MainWindow", "\347\212\266\346\200\201: \346\234\252\351\224\201\345\256\232", nullptr));
        label19->setText(QCoreApplication::translate("MainWindow", "\345\235\220\346\240\207:", nullptr));
        label20->setText(QCoreApplication::translate("MainWindow", "\350\267\235\347\246\273:", nullptr));
        label21->setText(QCoreApplication::translate("MainWindow", "\350\204\261\351\235\266:", nullptr));
        dashGroupSysParams->setTitle(QCoreApplication::translate("MainWindow", "\347\263\273\347\273\237\345\217\202\346\225\260", nullptr));
        btnGetImageParams->setText(QCoreApplication::translate("MainWindow", "\350\216\267\345\217\226\345\275\223\345\211\215\345\217\202\346\225\260", nullptr));
        label22->setText(QCoreApplication::translate("MainWindow", "\345\210\206\350\276\250:", nullptr));
        label23->setText(QCoreApplication::translate("MainWindow", "\347\240\201\347\216\207:", nullptr));
        label24->setText(QCoreApplication::translate("MainWindow", "\347\274\226\347\240\201:", nullptr));
        label25->setText(QCoreApplication::translate("MainWindow", "\346\250\241\345\274\217:", nullptr));
        label26->setText(QCoreApplication::translate("MainWindow", "PIP:", nullptr));
        label27->setText(QCoreApplication::translate("MainWindow", "\347\256\227\346\263\225:", nullptr));
        label28->setText(QCoreApplication::translate("MainWindow", "VMax:", nullptr));
        label29->setText(QCoreApplication::translate("MainWindow", "IMax:", nullptr));
        labelPlaceholder1->setText(QCoreApplication::translate("MainWindow", "\350\247\206\351\242\221\345\233\236\346\224\276 \342\200\224 \346\225\254\350\257\267\346\234\237\345\276\205", nullptr));
        labelPlaceholder2->setText(QCoreApplication::translate("MainWindow", "\346\227\245\345\277\227\346\237\245\350\257\242 \342\200\224 \346\225\254\350\257\267\346\234\237\345\276\205", nullptr));
        labelPlaceholder3->setText(QCoreApplication::translate("MainWindow", "\347\263\273\347\273\237\350\256\276\347\275\256 \342\200\224 \346\225\254\350\257\267\346\234\237\345\276\205", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
