#include "configmanager.h"

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    load();
}

void ConfigManager::load()
{
    QSettings settings("Tofu", "T-JSON_Settings");
    m_ptz.load(settings);
    m_lens.load(settings);
    m_cam.load(settings);
}

void ConfigManager::reload()
{
    load();
}

void ConfigManager::save()
{
    QSettings settings("Tofu", "T-JSON_Settings");
    m_ptz.save(settings);
    m_lens.save(settings);
    emit ptzConfigChanged();
    emit lensConfigChanged();
}
