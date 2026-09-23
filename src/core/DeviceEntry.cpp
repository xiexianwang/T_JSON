#include "DeviceEntry.h"

#include <QRegularExpression>
#include <QUuid>

QString DeviceEntry::newId(const QString& prefix)
{
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    uuid.remove('-');
    return prefix + uuid;
}

DeviceEntry DeviceEntry::makeGroup(const QString& name)
{
    DeviceEntry e;
    e.id = newId(QStringLiteral("grp_"));
    e.type = DeviceNodeType::Group;
    e.name = name;
    return e;
}

DeviceEntry DeviceEntry::makeDevice(const QString& name, const QString& ip, quint16 port)
{
    DeviceEntry e;
    e.id = newId(QStringLiteral("dev_"));
    e.type = DeviceNodeType::Device;
    e.name = name.isEmpty() ? ip : name;
    e.ip = ip;
    e.port = port;
    e.rtspUrl = defaultRtspUrl(ip);
    e.config = DeviceConfig();
    return e;
}

QString DeviceEntry::defaultRtspUrl(const QString& ip, const QString& user,
                                    const QString& password, quint16 rtspPort)
{
    return QStringLiteral("rtsp://%1:%2@%3:%4/live/1")
        .arg(user, password, ip, QString::number(rtspPort));
}

bool DeviceEntry::isValidHost(const QString& host)
{
    if (host.isEmpty() || host.size() > 255) return false;
    if (host.contains(QStringLiteral("://")) || host.contains(QLatin1Char('/'))
        || host.contains(QLatin1Char(' ')) || host.contains(QLatin1Char('\\'))) {
        return false;
    }

    // 纯数字与点组成的串只允许是完整合法的 IPv4（拦截 "192.168.1" 这类漏段输入）
    static const QRegularExpression numericLike(QStringLiteral("^[\\d.]+$"));
    if (numericLike.match(host).hasMatch()) {
        static const QRegularExpression ipv4(
            QStringLiteral("^(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$"));
        const QRegularExpressionMatch m = ipv4.match(host);
        if (!m.hasMatch()) return false;
        for (int i = 1; i <= 4; ++i) {
            if (m.captured(i).toInt() > 255) return false;
        }
        return true;
    }

    static const QRegularExpression hostname(
        QStringLiteral("^(?=.{1,253}$)([A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?)"
                       "(\\.[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?)*$"));
    return hostname.match(host).hasMatch();
}

QString DeviceEntry::normalizedRtspUrl(const QString& url, const QString& ip)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) return defaultRtspUrl(ip);
    if (trimmed.startsWith(QStringLiteral("rtsp://"), Qt::CaseInsensitive)) return trimmed;
    return QStringLiteral("rtsp://%1").arg(trimmed);
}

QJsonObject DeviceEntry::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("type")] = isDevice() ? QStringLiteral("device")
                                             : QStringLiteral("group");
    obj[QStringLiteral("name")] = name;

    if (isDevice()) {
        obj[QStringLiteral("ip")] = ip;
        obj[QStringLiteral("port")] = port;
        obj[QStringLiteral("rtspUrl")] = rtspUrl;
        if (!userName.isEmpty()) obj[QStringLiteral("userName")] = userName;
        if (!password.isEmpty()) obj[QStringLiteral("password")] = password;
        obj[QStringLiteral("config")] = config.toJson();
    }
    return obj;
}

DeviceEntry DeviceEntry::fromJson(const QJsonObject& obj)
{
    DeviceEntry e;
    e.id = obj.value(QStringLiteral("id")).toString();
    e.name = obj.value(QStringLiteral("name")).toString();

    const QString typeStr = obj.value(QStringLiteral("type")).toString();
    if (typeStr == QStringLiteral("device")) {
        e.type = DeviceNodeType::Device;
    } else if (typeStr == QStringLiteral("group")) {
        e.type = DeviceNodeType::Group;
    } else {
        e.type = (obj.contains(QStringLiteral("rtspUrl"))
                  || obj.contains(QStringLiteral("ip")))
                     ? DeviceNodeType::Device
                     : DeviceNodeType::Group;
    }

    e.ip = obj.value(QStringLiteral("ip")).toString();
    e.port = static_cast<quint16>(obj.value(QStringLiteral("port")).toInt(8089));
    e.rtspUrl = obj.value(QStringLiteral("rtspUrl")).toString();
    e.userName = obj.value(QStringLiteral("userName")).toString();
    e.password = obj.value(QStringLiteral("password")).toString();

    if (obj.value(QStringLiteral("config")).isObject()) {
        e.config = DeviceConfig::fromJson(obj.value(QStringLiteral("config")).toObject());
    } else if (obj.value(QStringLiteral("deviceConfig")).isObject()) {
        e.config = DeviceConfig::fromJson(obj.value(QStringLiteral("deviceConfig")).toObject());
    }

    if (e.id.isEmpty()) {
        e.id = newId(e.isDevice() ? QStringLiteral("dev_") : QStringLiteral("grp_"));
    }
    return e;
}
