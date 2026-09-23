#ifndef DEVICEENTRY_H
#define DEVICEENTRY_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>

#include "DeviceConfig.h"

// ============================================================================
// DeviceEntry - 设备树的纯数据节点模型（core 层，无 UI 依赖）
//
// 设计要点：
//   1. 节点类型显式化（Group / Device），不再靠 "RTSP 非空" 隐式推断；
//   2. id 为稳定唯一标识（UUID），IP 仅是可编辑的连接参数；
//   3. 名称独立存储，不拼接 IP、不按空格截断；
//   4. JSON 序列化带 schema 版本，支持从旧格式无损迁移。
// ============================================================================
enum class DeviceNodeType {
    Group,
    Device
};

struct DeviceEntry {
    // 稳定唯一标识：设备 "dev_<uuid>"，分组 "grp_<uuid>"
    QString id;
    DeviceNodeType type = DeviceNodeType::Group;
    // 显示名称（完整保留，不做任何截断）
    QString name;

    // ---- 连接参数（仅设备节点使用） ----
    QString ip;
    quint16 port = 8089;
    QString rtspUrl;
    QString userName;
    QString password;

    // ---- 设备业务配置 ----
    DeviceConfig config;

    bool isDevice() const { return type == DeviceNodeType::Device; }
    bool isGroup() const { return type == DeviceNodeType::Group; }

    // ---- 构造辅助 ----
    static QString newId(const QString& prefix);
    static DeviceEntry makeGroup(const QString& name = QStringLiteral("新分组"));
    static DeviceEntry makeDevice(const QString& name, const QString& ip,
                                  quint16 port = 8089);
    static QString defaultRtspUrl(const QString& ip,
                                  const QString& user = QStringLiteral("admin"),
                                  const QString& password = QStringLiteral("admin"),
                                  quint16 rtspPort = 554);

    // ---- 校验 ----
    // 接受 IPv4 字面量或合法主机名（不含空格、协议前缀、路径）
    static bool isValidHost(const QString& host);
    static QString normalizedRtspUrl(const QString& url, const QString& ip);

    // ---- 序列化 ----
    QJsonObject toJson() const;
    static DeviceEntry fromJson(const QJsonObject& obj);
};

// 设备树文件 schema 版本：1 = 旧数组格式（无 id/type），2 = 当前对象格式
constexpr int kDeviceTreeSchemaVersion = 2;

#endif // DEVICEENTRY_H
