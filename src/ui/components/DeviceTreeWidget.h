#ifndef DEVICETREEWIDGET_H
#define DEVICETREEWIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QMenu>
#include <QJsonArray>
#include "core/DeviceConfig.h"
#include "core/DeviceEntry.h"

class QToolButton;

// ============================================================================
// DeviceTreeWidget - 设备资源树
// 节点类型显式（分组/设备），设备以稳定 id 标识，支持增删改、连接状态展示
// 与带版本的 JSON 持久化（原子写入）。
// 设备归属通过右键菜单（添加子分组/添加设备/删除）维护，不提供拖拽重组。
// ============================================================================
class DeviceTreeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceTreeWidget(QWidget *parent = nullptr);

    // ---- 序列化 ----
    // 返回带 schema 版本的设备树对象 { "version": 2, "nodes": [...] }
    QJsonObject saveToJson() const;
    // 兼容旧调用：仅返回节点数组（不做持久化）
    QJsonArray saveNodesToJson() const;
    void clear();

    void saveToDisk(const QString &path) const;
    void loadFromDisk(const QString &path);

    // ---- 节点角色 ----
    static const int RoleEntryId   = Qt::UserRole + 1;  // 稳定 id
    static const int RoleNodeType  = Qt::UserRole + 2;  // DeviceNodeType
    static const int RoleIp        = Qt::UserRole + 3;
    static const int RoleRtspUrl   = Qt::UserRole + 4;
    static const int RoleConnected = Qt::UserRole + 5;      // 设备（TCP）连接
    static const int RoleConfig    = Qt::UserRole + 6;      // DeviceConfig JSON
    static const int RoleVideoConnected = Qt::UserRole + 7; // 视频流（RTSP）连接
    static const int RoleCurrentDevice  = Qt::UserRole + 8; // 当前焦点设备
    static const int RoleDeviceNumber   = Qt::UserRole + 9; // 设备显示编号（按树序 1..N）

    // ---- 设备状态 ----
    // 设备列（TCP 连接）；断开时联动复位视频流列
    void setDeviceConnected(const QString &id, bool connected);
    // 视频流列（RTSP 连接）
    void setVideoConnected(const QString &id, bool connected);
    // 当前焦点设备（仪表盘/电机/地图跟随）；空串表示无当前设备
    void setCurrentDevice(const QString &id);

    // 重算设备编号（按树序 1..N）并刷新显示；编号经 RoleDeviceNumber 暴露
    void renumberDevices();

    // ---- 节点查询/更新 ----
    DeviceEntry entryForId(const QString &id) const;
    void setConfigForId(const QString &id, const DeviceConfig &cfg);
    // 树内所有设备节点的稳定 id（按树序，仅设备节点）
    QList<QString> allDeviceIds() const;
    // 设备显示编号（按树序 1..N；非设备/未知返回 0）
    int deviceNumberForId(const QString &id) const;

signals:
    void deviceActivated(const QString &id, const DeviceEntry &entry);
    void deviceRemoved(const QString &id);
    void treeModified();
    void deviceToggleConnect(const QString &id, const DeviceEntry &entry);
    void layoutModeChanged(int rows, int cols);
    void devicePropertiesRequested(const QString &id);

private slots:
    void onCustomContextMenu(const QPoint &pos);
    void onItemChanged(QStandardItem *item);
    void onDoubleClicked(const QModelIndex &index);

private:
    QStandardItem *createItem(const QString &text, DeviceNodeType type) const;
    QStandardItem *findItemById(const QString &id) const;

    // 数据构建/装载
    void appendEntries(QStandardItem *parent, const QJsonArray &nodes);
    QJsonArray collectNodes(QStandardItem *parent) const;

    // 交互动作
    void addGroup(QStandardItem *parentItem);
    void addDevice(QStandardItem *parentItem);
    void removeItem(QStandardItem *item);
    void editIp(QStandardItem *item);
    void editRtsp(QStandardItem *item);

    // 持久化
    QString defaultSavePath() const;
    void scheduleSave();
    void saveNow();
    void restoreLayoutSetting();
    void persistLayoutSetting();

    QTreeView *m_treeView;
    QStandardItemModel *m_model;
    QToolButton *m_layoutBtns[4];

    bool m_loading = false;
    bool m_dirty = false;
    QString m_currentDeviceId;   // 当前焦点设备（运行时状态，不落盘）
};

#endif // DEVICETREEWIDGET_H
