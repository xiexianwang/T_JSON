#ifndef GEOCALCULATOR_H
#define GEOCALCULATOR_H

#include <QString>
#include <QtMath>

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
