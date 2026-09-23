#include "jsonframeparser.h"

ZoomInfoData ZoomInfoData::parse(const QJsonObject& doc)
{
    ZoomInfoData d;
    d.visZoom = doc.value("ZoomInfo").toDouble(1.0);
    d.irZoom = doc.value("ZoomInfoIR").toDouble(1.0);
    d.camShowMode = doc.value("CamShowMode").toInt();
    d.latitude = doc.value("Latitude").toString();
    d.longitude = doc.value("Longitude").toString();
    d.height = doc.value("Height").toDouble(0);
    d.laserRange = doc.value("LaserRange").toDouble(0);
    d.pan = doc.value("PTZInfoH").toDouble(0);
    d.tilt = doc.value("PTZInfoV").toDouble(0);
    return d;
}

ImageSettingData ImageSettingData::parse(const QJsonObject& doc)
{
    ImageSettingData d;
    d.imgSize = doc.value("ImageSize").toInt();
    d.bitrate = doc.value("ImageBit").toInt();
    d.codec = doc.value("ImageCode").toInt();
    d.workMode = doc.value("WorkMode").toInt();
    d.pipShow = doc.value("PipShow").toInt();
    d.model = doc.value("Model").toInt();
    d.maxVisFL = doc.value("MaxVisFL").toString();
    d.maxIRFL = doc.value("MaxIRFL").toString();
    return d;
}

AiInfoData AiInfoData::parse(const QJsonObject& doc)
{
    AiInfoData d;
    d.workMode = doc.value("WorkMode").toInt();
    d.objectCount = doc.value("ObjectCount").toInt();

    if (doc.contains("Object") && doc.value("Object").isObject()) {
        QJsonObject objMap = doc.value("Object").toObject();
        for (auto it = objMap.begin(); it != objMap.end(); ++it) {
            AiTargetData t;
            t.id = it.key();
            QJsonObject obj = it.value().toObject();
            t.cls = obj.value("Class").toInt();
            t.state = obj.value("State").toInt();
            t.distance = obj.value("Distance").toDouble(0);
            if (obj.contains("Points")) {
                QJsonObject pts = obj.value("Points").toObject();
                t.left = pts.value("Left").toInt();
                t.top = pts.value("Top").toInt();
                t.right = pts.value("Right").toInt();
                t.bottom = pts.value("Bottom").toInt();
                t.hasPoints = true;
            }
            if (obj.contains("Angle") && obj.value("Angle").isObject()) {
                const QJsonObject ang = obj.value("Angle").toObject();
                t.angleHor = ang.value("Hor").toDouble(0);
                t.angleVer = ang.value("Ver").toDouble(0);
                t.hasAngle = true;
            }
            d.targets.append(t);
        }
    }

    return d;
}
