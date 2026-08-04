#include "GeoCalculator.h"

QString GeoCalculator::missMradStr(double dx, double dy, double pixelSizeUm, double focalMm)
{
    if (focalMm < 0.1) return QString();
    double dxMrad = dx * pixelSizeUm / focalMm;
    double dyMrad = dy * pixelSizeUm / focalMm;
    return QString("H: %1  V: %2 mrad").arg(dxMrad, 0, 'f', 2).arg(dyMrad, 0, 'f', 2);
}

double GeoCalculator::estimateTargetDistance(int boxPixels, double focalMm, double pixelSizeUm, double refSize)
{
    if (boxPixels <= 0 || focalMm < 0.1 || pixelSizeUm <= 0 || refSize <= 0)
        return 0.0;
    return qBound(1.0, refSize * focalMm * 1000.0 / (boxPixels * pixelSizeUm), 10000.0);
}

void GeoCalculator::pixelToGps(double pixelX, double pixelY, double distance, 
                               const CameraIntrinsics& cam, const DevicePose& pose,
                               double& outLat, double& outLon)
{
    int halfW = cam.resX / 2;
    int halfH = cam.resY / 2;
    (void)pixelY; // 简单转换不涉及 Y 轴对 GPS 的影响
    (void)halfH;

    if (pose.lat == 0 && pose.lon == 0) { outLat = 0; outLon = 0; return; }
    if (cam.focalLengthMm < 0.1) { outLat = 0; outLon = 0; return; }

    double dxAngle = (pixelX - halfW) * cam.pixelSizeUm / (cam.focalLengthMm * 1000.0);
    double bearing = pose.panDeg * M_PI / 180.0 + dxAngle;
    double range = distance > 0 ? distance : 100.0; 

    double R = 6371000.0;                          
    double lat1 = pose.lat * M_PI / 180.0;         
    double lon1 = pose.lon * M_PI / 180.0;         
    double d = range / R;                          

    double lat2 = qAsin(qSin(lat1) * qCos(d) + qCos(lat1) * qSin(d) * qCos(bearing));
    double lon2 = lon1 + qAtan2(qSin(bearing) * qSin(d) * qCos(lat1), qCos(d) - qSin(lat1) * qSin(lat2));

    outLat = lat2 * 180.0 / M_PI;
    outLon = lon2 * 180.0 / M_PI;
}

void GeoCalculator::pixelBboxToGps(double pixelX, double pixelY, double distance, double tiltDeg,
                                   const CameraIntrinsics& cam, const DevicePose& pose,
                                   double& outLat, double& outLon)
{
    int halfW = cam.resX / 2;
    int halfH = cam.resY / 2;

    if (pose.lat == 0 && pose.lon == 0) { outLat = 0; outLon = 0; return; }
    if (cam.focalLengthMm < 0.1) { outLat = 0; outLon = 0; return; }

    double dxAngle = (pixelX - halfW) * cam.pixelSizeUm / (cam.focalLengthMm * 1000.0);
    double dyAngle = (pixelY - halfH) * cam.pixelSizeUm / (cam.focalLengthMm * 1000.0);

    double tiltRad = tiltDeg * M_PI / 180.0;
    double rangeAdj = distance;
    if (tiltRad > 0.01) {
        double H = distance * qSin(tiltRad);          
        double effTilt = tiltRad + dyAngle;           
        if (effTilt > 0.005 && effTilt < M_PI - 0.005)
            rangeAdj = H / qSin(effTilt);             
    }

    double bearing = pose.panDeg * M_PI / 180.0 + dxAngle;
    double range = rangeAdj > 0 ? rangeAdj : 100.0;

    double R = 6371000.0;
    double lat1 = pose.lat * M_PI / 180.0;
    double lon1 = pose.lon * M_PI / 180.0;
    double d = range / R;

    double lat2 = qAsin(qSin(lat1) * qCos(d) + qCos(lat1) * qSin(d) * qCos(bearing));
    double lon2 = lon1 + qAtan2(qSin(bearing) * qSin(d) * qCos(lat1), qCos(d) - qSin(lat1) * qSin(lat2));

    outLat = lat2 * 180.0 / M_PI;
    outLon = lon2 * 180.0 / M_PI;
}
