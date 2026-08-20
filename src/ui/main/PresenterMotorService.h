#ifndef PRESENTERMOTORSERVICE_H
#define PRESENTERMOTORSERVICE_H

#include <QObject>
#include <QString>

class IMainView;
class ConfigManager;
class DeviceContext;

class PresenterMotorService : public QObject
{
    Q_OBJECT
public:
    explicit PresenterMotorService(IMainView* view, ConfigManager* cfg, QObject *parent = nullptr);

    void ptzMove(const QString& deviceId, int direction);
    void ptzStop(const QString& deviceId);

    void lensMove(const QString& deviceId, int op);
    void lensStop(const QString& deviceId);

    void initMotorChannel(const QString& deviceId);
    void applyMotorChannel(const QString& deviceId);
    
    void initPtzForwarder(const QString& deviceId);

    void onWiperStart(const QString& deviceId);
    void onWiperStop(const QString& deviceId);
    void onWiperJogLeft(const QString& deviceId);
    void onWiperJogRight(const QString& deviceId);
    void onWiperJogStop(const QString& deviceId);
    void onWiperZeroCalib(const QString& deviceId);
    void onWiperMode(const QString& deviceId);
    void onWiperSilent(const QString& deviceId);
    void onWiperCurrentSet(const QString& deviceId, int ma);
    void checkMotorMode(const QString& deviceId);

    void callPreset(const QString& deviceId, int preset);
    void setPreset(const QString& deviceId, int preset);
    void delPreset(const QString& deviceId, int preset);

private:
    DeviceContext* getCtx(const QString& deviceId) const;

    IMainView* m_view;
    ConfigManager* m_cfg;
};

#endif // PRESENTERMOTORSERVICE_H
