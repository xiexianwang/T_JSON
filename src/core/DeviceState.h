#ifndef DEVICESTATE_H
#define DEVICESTATE_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>

// 表示设备的运行时状态 (纯数据模型)
struct DeviceState {
    bool isConnected = false;
    
    // PTZ 参数
    double currentPan = 0.0;
    double currentTilt = 0.0;
    double currentVisZoom = 1.0;
    double currentIrZoom = 1.0;
    
    // 镜头统摄参数
    double visMaxFocal = 0.0;
    double irMaxFocal = 0.0;
    
    // 位置参数
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    
    // AI 及业务参数
    double lastAiDist = 0.0;
    bool lastAiDistEstimated = false;
    QDateTime lastAiInfoTime;
    
    // 当前工作模式 / 图像参数等
    int currentPipShow = 0;
    int workMode = 0;
    int algoModel1 = 0;
    int algoModel2 = 0;
};

#endif // DEVICESTATE_H
