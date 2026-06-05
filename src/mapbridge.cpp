#include "mapbridge.h"
#include <QDebug>

MapBridge::MapBridge(QObject *parent) : QObject(parent) {}

void MapBridge::onMapInitialized()
{
    m_ready = true;
    emit mapReadyChanged();
    qDebug() << "Map initialized";
}

void MapBridge::onMapClick(double lat, double lon)
{
    emit mapClicked(lat, lon);
}

void MapBridge::onMapZoomChanged(int zoom)
{
    emit mapZoomChanged(zoom);
}

void MapBridge::onRequestEnlarge()
{
    emit requestEnlarge();
}
