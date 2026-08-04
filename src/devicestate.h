#ifndef DEVICESTATE_H
#define DEVICESTATE_H

struct DeviceState {
    double visZoom = 1.0;
    double irZoom = 1.0;
    int pipShow = 0;
    int resX = 2688;
    int resY = 1520;
    int previousWorkMode = 0;
    int previousAlgoModel = 0;
    int previousDisplayMode = 0;
    bool updatingFromDevice = false;
};

#endif // DEVICESTATE_H
