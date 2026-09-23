#ifndef GEOCALCULATOR_H
#define GEOCALCULATOR_H

#include <QString>
#include <QtMath>
#include <QDateTime>
#include <QDateTime>

struct CameraIntrinsics {
    double pixelSizeUm;
    double focalLengthMm;
    int resX;
    int resY;
};

struct DevicePose {
    double lat;
    double lon;
    double panDeg;
};

class GeoCalculator
{
public:
    // 解析十进制坐标（可选 N/S/E/W 后缀，大小写不敏感）。
    // 带 ok 出参的重载：解析失败时返回 0 且 *ok=false，用于区分「真正的 0」与「非法输入」。
    static double parseCoord(const QString& s, bool* ok);
    static double parseCoord(const QString& s);
    static double haversineDistance(double lat1, double lon1, double lat2, double lon2);
    static double bearing(double lat1, double lon1, double lat2, double lon2);
    static bool shouldPlotTrackPoint(double newLat, double newLon, double plotLat, double plotLon, double plotHeading, const QDateTime& plotTime, double* outBearing = nullptr);

public:
    static QString missMradStr(double dx, double dy, double pixelSizeUm, double focalMm);
    
    static double estimateTargetDistance(int boxPixels, double focalMm, double pixelSizeUm, double refSize);
    
    static void pixelToGps(double pixelX, double pixelY, double distance, 
                           const CameraIntrinsics& cam, const DevicePose& pose,
                           double& outLat, double& outLon);
                           
    static void pixelBboxToGps(double pixelX, double pixelY, double distance, double tiltDeg,
                               const CameraIntrinsics& cam, const DevicePose& pose,
                               double& outLat, double& outLon);
};

#endif // GEOCALCULATOR_H
