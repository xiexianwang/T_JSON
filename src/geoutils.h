#ifndef GEOUTILS_H
#define GEOUTILS_H

#include <QString>
#include <QDateTime>
#include <QtMath>

namespace GeoUtils {

double parseCoord(const QString& s);

double haversineDistance(double lat1, double lon1, double lat2, double lon2);

double bearing(double lat1, double lon1, double lat2, double lon2);

bool shouldPlotTrackPoint(double newLat, double newLon,
                           double plotLat, double plotLon,
                           double plotHeading, const QDateTime& plotTime,
                           double* outBearing = nullptr);

QString missMradStr(double dx, double dy, double pixelSizeUm, double focalMm);

double estimateTargetDistance(int boxPixels, double focalMm, double pixelSizeUm, double refSize);

void pixelToGps(double pixelX, double pixelY, double distance,
                 double devLat, double devLon, double pan,
                 double pixelSize, double focal,
                 int halfW, int halfH,
                 double& outLat, double& outLon);

void pixelBboxToGps(double pixelX, double pixelY, double distance,
                     double tiltDeg, double devLat, double devLon, double pan,
                     double pixelSize, double focal,
                     int halfW, int halfH,
                     double& outLat, double& outLon);

} // namespace GeoUtils

#endif // GEOUTILS_H
