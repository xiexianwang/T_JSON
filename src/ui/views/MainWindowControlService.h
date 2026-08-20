#ifndef MAINWINDOWCONTROLSERVICE_H
#define MAINWINDOWCONTROLSERVICE_H

#include <QObject>
#include <functional>

class QSlider;
class QSpinBox;
class QPushButton;
class QCheckBox;
class QLineEdit;
class QLabel;
class QStatusBar;
class ConfigManager;
class MainPresenter;

class MainWindowControlService : public QObject
{
    Q_OBJECT
public:
    struct ControlWidgets {
        QPushButton* ptzUp = nullptr;
        QPushButton* ptzDown = nullptr;
        QPushButton* ptzLeft = nullptr;
        QPushButton* ptzRight = nullptr;
        QPushButton* ptzTopLeft = nullptr;
        QPushButton* ptzTopRight = nullptr;
        QPushButton* ptzBottomLeft = nullptr;
        QPushButton* ptzBottomRight = nullptr;
        QSlider* sliderSpeed = nullptr;
        QSpinBox* spinSpeed = nullptr;

        QPushButton* zoomIn = nullptr;
        QPushButton* zoomOut = nullptr;
        QPushButton* focusIn = nullptr;
        QPushButton* focusOut = nullptr;
        QSlider* sliderZoomSpeed = nullptr;
        QSpinBox* spinZoomSpeed = nullptr;

        QPushButton* callPreset = nullptr;
        QPushButton* setPreset = nullptr;
        QPushButton* delPreset = nullptr;
        QPushButton* ptzReset = nullptr;

        QCheckBox* checkDigitalZoom = nullptr;
        QCheckBox* checkAutoZoom = nullptr;
        QCheckBox* checkCaptureUpload = nullptr;
        QCheckBox* checkPosReset = nullptr;

        QPushButton* btnWiperStart = nullptr;
        QPushButton* btnWiperStop = nullptr;
        QPushButton* btnWiperLeft = nullptr;
        QPushButton* btnWiperRight = nullptr;
        QPushButton* btnWiperZeroCalib = nullptr;
        QPushButton* btnWiperMode = nullptr;
        QPushButton* btnWiperSilent = nullptr;
        QLineEdit* editWiperCurrent = nullptr;
        QLineEdit* statWiperStatus = nullptr;
        QStatusBar* statusbar = nullptr;
    };

    explicit MainWindowControlService(QObject* parent = nullptr);

    void setup(const ControlWidgets& widgets, ConfigManager* cfg,
               MainPresenter* presenter,
               std::function<bool()> requireConnected,
               std::function<bool()> requireMotorReady);

    void updateMotorButtons();

private:
    void refreshStyle(QWidget* w);

    ControlWidgets m_w;
    ConfigManager* m_cfg = nullptr;
    MainPresenter* m_presenter = nullptr;
    std::function<bool()> m_requireConnected;
    std::function<bool()> m_requireMotorReady;
};

#endif // MAINWINDOWCONTROLSERVICE_H
