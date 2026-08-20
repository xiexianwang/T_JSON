#ifndef MAINWINDOWNAVIGATION_H
#define MAINWINDOWNAVIGATION_H

#include <QObject>

class QButtonGroup;
class QToolButton;
class QPushButton;
class QDialog;
class CmdLogDialog;
class ConfigManager;
class MainPresenter;
class IMainView;

class MainWindowNavigation : public QObject
{
    Q_OBJECT
public:
    struct NavButtons {
        QToolButton* monitor = nullptr;
        QToolButton* playback = nullptr;
        QToolButton* log = nullptr;
        QToolButton* settings = nullptr;
    };

    explicit MainWindowNavigation(QObject* parent = nullptr);

    void setup(NavButtons buttons, QPushButton* btnMapToggle,
               ConfigManager* cfg, MainPresenter* presenter, IMainView* view);
    void setLogDialog(CmdLogDialog* dlg);

public slots:
    void onBtnNavMonitorClicked();
    void onBtnNavPlaybackClicked();
    void onBtnNavLogClicked();
    void onBtnNavSettingsClicked();

private:
    NavButtons m_btns;
    QPushButton* m_btnMapToggle = nullptr;
    ConfigManager* m_cfg = nullptr;
    MainPresenter* m_presenter = nullptr;
    IMainView* m_view = nullptr;
    CmdLogDialog* m_logDialog = nullptr;
};

#endif // MAINWINDOWNAVIGATION_H
