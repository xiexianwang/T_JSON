#ifndef MAPBRIDGE_H
#define MAPBRIDGE_H

#include <QObject>

class MapBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool mapReady READ mapReady NOTIFY mapReadyChanged)
public:
    explicit MapBridge(QObject *parent = nullptr);

    bool mapReady() const { return m_ready; }

public slots:
    void onMapInitialized();
    void onMapClick(double lat, double lon);
    void onMapZoomChanged(int zoom);

signals:
    void mapReadyChanged();
    void mapClicked(double lat, double lon);
    void mapZoomChanged(int zoom);

private:
    bool m_ready = false;
};

#endif
