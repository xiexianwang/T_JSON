#ifndef DEVICETREEWIDGET_H
#define DEVICETREEWIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QMenu>
#include <QJsonArray>

class DeviceTreeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceTreeWidget(QWidget *parent = nullptr);

    void loadFromJson(const QJsonArray &arr);
    QJsonArray saveToJson() const;
    void clear();

    void saveToDisk(const QString &path) const;
    void loadFromDisk(const QString &path);

    void setDeviceConnected(const QString &ip, bool connected);

    static const int RoleRtspUrl = Qt::UserRole + 1;
    static const int RoleIp      = Qt::UserRole + 2;
    static const int RoleConnected = Qt::UserRole + 3;

signals:
    void channelDoubleClicked(const QString &name, const QString &ip, const QString &rtspUrl);
    void deviceAdded(const QString &ip, const QString &name);
    void treeModified();
    void deviceToggleConnect(const QString &ip);

private slots:
    void onCustomContextMenu(const QPoint &pos);
    void onItemChanged(QStandardItem *item);

private:
    QStandardItem *createItem(const QString &text) const;
    void appendChildItems(QStandardItem *parent, const QJsonObject &obj);
    void childrenToJson(QStandardItem *item, QJsonArray &arr) const;

    QTreeView *m_treeView;
    QStandardItemModel *m_model;
public:
    QStandardItemModel *model() const { return m_model; }
};

#endif
