
double GeoCalculator::parseCoord(const QString& s) {
    QString t = s.trimmed().toUpper();
    char suf = 0;
    if (!t.isEmpty()) {
        QChar c = t.at(t.size() - 1);
        if (c == 'N' || c == 'S' || c == 'E' || c == 'W') {
            suf = c.toLatin1(); t.chop(1);
        }
    }
    bool ok = false;
    double v = t.toDouble(&ok);
    if (!ok) return 0.0;
    return (suf == 'S' || suf == 'W') ? -v : v;
}

double GeoCalculator::haversineDistance(double lat1, double lon1, double lat2, double lon2) {
    double R = 6371000.0;
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = qSin(dLat / 2) * qSin(dLat / 2)
             + qCos(lat1 * M_PI / 180.0) * qCos(lat2 * M_PI / 180.0)
             * qSin(dLon / 2) * qSin(dLon / 2);
    double c = 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
    return R * c;
}

double GeoCalculator::bearing(double lat1, double lon1, double lat2, double lon2) {
    double lat1R = qDegreesToRadians(lat1);
    double lat2R = qDegreesToRadians(lat2);
    double lon1R = qDegreesToRadians(lon1);
    double lon2R = qDegreesToRadians(lon2);
    double dLon = lon2R - lon1R;
    double y = qSin(dLon) * qCos(lat2R);
    double x = qCos(lat1R) * qSin(lat2R) - qSin(lat1R) * qCos(lat2R) * qCos(dLon);
    double deg = qRadiansToDegrees(qAtan2(y, x));
    return deg < 0 ? deg + 360.0 : deg;
}

static constexpr double TRK_MIN_DIST_M = 3.0;
static constexpr double TRK_MAX_DIST_M = 20.0;
static constexpr double TRK_HEADING_DIFF_DEG = 15.0;
static constexpr int TRK_HEARTBEAT_MS = 2500;

bool GeoCalculator::shouldPlotTrackPoint(double newLat, double newLon,
                                  double plotLat, double plotLon,
                                  double plotHeading, const QDateTime& plotTime,
                                  double* outBearing)
{
    if (plotHeading < 0) return true; // 首次绘制 / 目标切换

    double dist = haversineDistance(plotLat, plotLon, newLat, newLon);

    if (dist < TRK_MIN_DIST_M) return false;

    if (dist > TRK_MAX_DIST_M) {
        if (outBearing) *outBearing = bearing(plotLat, plotLon, newLat, newLon);
        return true;
    }

    double head = bearing(plotLat, plotLon, newLat, newLon);
    double diff = qAbs(head - plotHeading);
    if (diff > 180.0) diff = 360.0 - diff;
    if (diff >= TRK_HEADING_DIFF_DEG) {
        if (outBearing) *outBearing = head;
        return true;
    }

    if (plotTime.isValid()) {
        qint64 elapsed = plotTime.msecsTo(QDateTime::currentDateTime());
        if (elapsed >= TRK_HEARTBEAT_MS) {
            if (outBearing) *outBearing = head;
            return true;
        }
    }

    return false;
}
