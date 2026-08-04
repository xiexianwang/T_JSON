import re

# 1. Update mainwindow.h
with open('src/mainwindow.h', 'r', encoding='utf-8') as f:
    mh = f.read()

# Replace individual member variables with DeviceState pointer
replacements_h = [
    ('double m_currentVisZoom = 1.0;', ''),
    ('double m_currentIrZoom = 1.0;', ''),
    ('int m_currentPipShow = 0;', ''),
    ('int m_currentResX = 2688;', ''),
    ('int m_currentResY = 1520;', ''),
    ('int m_previousWorkMode = 0;', ''),
    ('int m_previousAlgoModel = 0;', ''),
    ('int m_previousDisplayMode = 0;', ''),
    ('bool m_updatingFromDevice = false;', ''),
]

for old, new in replacements_h:
    mh = mh.replace(old, new)

# Add #include "devicestate.h" and DeviceState *m_devState = nullptr;
if '#include "devicestate.h"' not in mh:
    mh = mh.replace('#include "tjsonclient.h"', '#include "tjsonclient.h"\n#include "devicestate.h"')
if 'DeviceState *m_devState = nullptr;' not in mh:
    mh = mh.replace('ConfigManager *m_cfg;', 'ConfigManager *m_cfg;\n    DeviceState *m_devState = nullptr;')

with open('src/mainwindow.h', 'w', encoding='utf-8') as f:
    f.write(mh)

# 2. Update mainwindow.cpp
with open('src/mainwindow.cpp', 'r', encoding='utf-8') as f:
    mc = f.read()

replacements_c = [
    ('m_currentVisZoom', 'm_devState->visZoom'),
    ('m_currentIrZoom', 'm_devState->irZoom'),
    ('m_currentPipShow', 'm_devState->pipShow'),
    ('m_currentResX', 'm_devState->resX'),
    ('m_currentResY', 'm_devState->resY'),
    ('m_previousWorkMode', 'm_devState->previousWorkMode'),
    ('m_previousAlgoModel', 'm_devState->previousAlgoModel'),
    ('m_previousDisplayMode', 'm_devState->previousDisplayMode'),
    ('m_updatingFromDevice', 'm_devState->updatingFromDevice'),
]

for old, new in replacements_c:
    mc = mc.replace(old, new)

# Initialize m_devState in constructor
init_target = 'm_cfg = ConfigManager::instance();'
init_insert = 'm_devState = new DeviceState();'
if init_insert not in mc:
    mc = mc.replace(init_target, init_target + '\n    ' + init_insert)

# delete m_devState in destructor
del_target = 'delete ui;'
del_insert = 'delete m_devState;'
if del_insert not in mc:
    mc = mc.replace(del_target, del_insert + '\n    ' + del_target)

with open('src/mainwindow.cpp', 'w', encoding='utf-8') as f:
    f.write(mc)

print("DeviceState refactored")