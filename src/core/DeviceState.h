#ifndef DEVICESTATE_H
#define DEVICESTATE_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QVector>

struct AiTargetItem {
    QString id;
    int cls = 0;          // Class：目标类型 (0xA0-0xA4)
    int state = 0;        // State：跟踪状态 (0xB1 跟踪正常 / 0xB2 跟踪丢失)
    double distance = 0;
    bool hasPoints = false;
    int left = 0, top = 0, right = 0, bottom = 0;
    double angleHor = 0;  // Angle.Hor（度）
    double angleVer = 0;  // Angle.Ver（度）
    bool hasAngle = false;
};

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
    QString latitudeRaw;    // 设备上报的原始坐标字符串（用于 UI 原样显示）
    QString longitudeRaw;
    
    // AI 及业务参数
    double lastAiDist = 0.0;
    bool lastAiDistEstimated = false;
    QDateTime lastAiInfoTime;
    
    // 当前工作模式 / 图像参数等
    int currentPipShow = 0;
    int workMode = 0;
    int algoModel1 = 0;
    int algoModel2 = 0;

    // --- ZoomInfo 原始字段 ---
    int camShowMode = 0;
    double laserRange = 0;
    bool hasZoomInfo = false;

    // --- ImageSetting 字段 ---
    int imgSize = 0;
    int bitrate = 0;
    int codec = 0;
    int model = 0;
    QString maxVisFL;
    QString maxIRFL;
    int resX = 2688;
    int resY = 1520;
    bool hasImageSetting = false;

    // --- AIInfo 目标列表 ---
    int aiWorkMode = 0;
    int aiObjectCount = 0;
    QVector<AiTargetItem> aiTargets;

    // --- 工具方法 ---
    static constexpr int kPipShowValues[] = {0, 1, 2, 3, 16};
    static constexpr int kPipShowCount = 5;
    static int pipShowToComboIndex(int pipShow) {
        for (int i = 0; i < kPipShowCount; i++)
            if (kPipShowValues[i] == pipShow) return i;
        if (pipShow >= 0 && pipShow < kPipShowCount) return pipShow;
        return 0;
    }
};

#endif // DEVICESTATE_H
