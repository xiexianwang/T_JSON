#ifndef JSONFRAMEPARSER_H
#define JSONFRAMEPARSER_H

#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVector>

struct ZoomInfoData {
    double visZoom = 1.0;
    double irZoom = 1.0;
    int camShowMode = 0;
    QString latitude;
    QString longitude;
    double height = 0;
    double laserRange = 0;
    double pan = 0;
    double tilt = 0;

    static ZoomInfoData parse(const QJsonObject& doc);
};

struct ImageSettingData {
    int imgSize = 0;
    int bitrate = 0;
    int codec = 0;
    int workMode = 0;
    int pipShow = 0;
    int model = 0;
    QString maxVisFL;
    QString maxIRFL;

    static ImageSettingData parse(const QJsonObject& doc);
};

struct AiTargetData {
    QString id;
    int cls = 0;          // Class：目标类型 (0xA0-0xA4)
    int state = 0;        // State：跟踪状态 (0xB1 跟踪正常 / 0xB2 跟踪丢失)
    double distance = 0;
    bool hasPoints = false;
    int left = 0, top = 0, right = 0, bottom = 0;
    double angleHor = 0;  // Angle.Hor：设备上报水平角度（度）
    double angleVer = 0;  // Angle.Ver：设备上报垂直角度（度）
    bool hasAngle = false;
};

struct AiInfoData {
    int workMode = 0;
    int objectCount = 0;
    QVector<AiTargetData> targets;

    static AiInfoData parse(const QJsonObject& doc);
};

#endif // JSONFRAMEPARSER_H
