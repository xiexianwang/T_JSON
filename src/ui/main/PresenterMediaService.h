#ifndef PRESENTERMEDIASERVICE_H
#define PRESENTERMEDIASERVICE_H

#include <QObject>
#include <QString>

class IMainView;
class DeviceContext;

class PresenterMediaService : public QObject
{
    Q_OBJECT
public:
    explicit PresenterMediaService(IMainView* view, QObject* parent = nullptr);

    void connectStream(DeviceContext* device, const QString& url);
    void disconnectStream(DeviceContext* device);
    void startStream(DeviceContext* device, const QString& url);
    void closeStream(DeviceContext* device);
    bool isStreamRunning(const DeviceContext* device) const;

private:
    IMainView* m_view;
};

#endif // PRESENTERMEDIASERVICE_H
