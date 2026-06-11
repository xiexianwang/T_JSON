#include "devicetreewidget.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QInputDialog>

DeviceTreeWidget::DeviceTreeWidget(QWidget *parent)
    : QWidget(parent)
    , m_treeView(new QTreeView(this))
    , m_model(new QStandardItemModel(this))
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_model->setHorizontalHeaderLabels({QStringLiteral("设备资源")});
    m_treeView->setModel(m_model);
    m_treeView->setHeaderHidden(false);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(16);
    m_treeView->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->setDragEnabled(true);
    m_treeView->setDragDropMode(QAbstractItemView::InternalMove);

    connect(m_treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        auto *item = m_model->itemFromIndex(index);
        if (!item) return;
        QString rtsp = item->data(RoleRtspUrl).toString();
        QString ip = item->data(RoleIp).toString();
        if (!rtsp.isEmpty())
            emit channelDoubleClicked(item->text(), ip, rtsp);
    });

    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &DeviceTreeWidget::onCustomContextMenu);

    connect(m_model, &QStandardItemModel::itemChanged,
            this, &DeviceTreeWidget::onItemChanged);

    lay->addWidget(m_treeView);
}

QStandardItem *DeviceTreeWidget::createItem(const QString &text) const
{
    auto *item = new QStandardItem(text);
    item->setEditable(true);
    item->setDragEnabled(true);
    item->setDropEnabled(true);
    return item;
}

static bool isDevice(const QStandardItem *item)
{
    return !item->data(DeviceTreeWidget::RoleRtspUrl).toString().isEmpty();
}

void DeviceTreeWidget::setDeviceConnected(const QString &ip, bool connected)
{
    std::function<void(QStandardItem*)> walk = [&](QStandardItem *parent) {
        for (int i = 0; i < parent->rowCount(); ++i) {
            auto *item = parent->child(i);
            if (!item) continue;
            if (item->data(RoleIp).toString() == ip) {
                item->setData(connected, RoleConnected);
                return;
            }
            if (item->hasChildren())
                walk(item);
        }
    };
    walk(m_model->invisibleRootItem());
}

void DeviceTreeWidget::onCustomContextMenu(const QPoint &pos)
{
    QModelIndex idx = m_treeView->indexAt(pos);
    QMenu menu;

    if (idx.isValid()) {
        auto *item = m_model->itemFromIndex(idx);

        if (isDevice(item)) {
            // ── 设备节点 ──
            auto *editIp = menu.addAction(QStringLiteral("编辑 IP"));
            connect(editIp, &QAction::triggered, this, [this, item]() {
                QString cur = item->data(RoleIp).toString();
                bool ok;
                QString ip = QInputDialog::getText(this,
                    QStringLiteral("设备 IP"), QStringLiteral("IP 地址:"),
                    QLineEdit::Normal, cur, &ok);
                if (ok && !ip.isEmpty()) {
                    item->setData(ip, RoleIp);
                    // 同步更新节点显示名
                    QString baseName = item->text().section(' ', 0, 0);
                    item->setText(QString("%1 [%2]").arg(baseName, ip));
                    emit treeModified();
                }
            });

            auto *editRtsp = menu.addAction(QStringLiteral("编辑 RTSP URL"));
            connect(editRtsp, &QAction::triggered, this, [this, item]() {
                QString cur = item->data(RoleRtspUrl).toString();
                bool ok;
                QString url = QInputDialog::getText(this,
                    QStringLiteral("RTSP URL"), QStringLiteral("RTSP 地址:"),
                    QLineEdit::Normal, cur, &ok);
                if (ok && !url.isEmpty()) {
                    item->setData(url, RoleRtspUrl);
                    emit treeModified();
                }
            });

            menu.addSeparator();

            auto *toggleConn = menu.addAction(
                item->data(RoleConnected).toBool()
                    ? QStringLiteral("断开") : QStringLiteral("连接"));
            connect(toggleConn, &QAction::triggered, this, [this, item]() {
                emit deviceToggleConnect(item->data(RoleIp).toString());
            });

            menu.addSeparator();

            auto *rename = menu.addAction(QStringLiteral("重命名"));
            connect(rename, &QAction::triggered, this, [this, idx]() {
                m_treeView->edit(idx);
            });

            auto *remove = menu.addAction(QStringLiteral("删除"));
            connect(remove, &QAction::triggered, this, [this, item]() {
                QStandardItem *parent = item->parent();
                if (parent) parent->removeRow(item->row());
                else m_model->removeRow(item->row());
                emit treeModified();
            });

        } else {
            // ── 分组节点 ──
            auto *addSub = menu.addAction(QStringLiteral("添加子分组"));
            connect(addSub, &QAction::triggered, this, [this, item]() {
                auto *child = createItem(QStringLiteral("新分组"));
                item->appendRow(child);
                m_treeView->setCurrentIndex(child->index());
                m_treeView->edit(child->index());
                emit treeModified();
            });

            auto *addDev = menu.addAction(QStringLiteral("添加设备"));
            connect(addDev, &QAction::triggered, this, [this, item]() {
                bool ok;
                QString ip = QInputDialog::getText(this,
                    QStringLiteral("设备 IP"), QStringLiteral("IP 地址:"),
                    QLineEdit::Normal, "192.168.1.", &ok);
                if (!ok || ip.isEmpty()) return;

                bool okName;
                QString name = QInputDialog::getText(this,
                    QStringLiteral("设备名称"), QStringLiteral("名称:"),
                    QLineEdit::Normal, ip, &okName);
                if (!okName || name.isEmpty()) name = ip;

                auto *dev = createItem(QString("%1 [%2]").arg(name, ip));
                dev->setData(ip, RoleIp);
                dev->setData(QString("rtsp://admin:admin@%1:554/live/1").arg(ip), RoleRtspUrl);
                item->appendRow(dev);
                emit deviceAdded(ip, name);
                emit treeModified();
            });

            menu.addSeparator();

            auto *rename = menu.addAction(QStringLiteral("重命名"));
            connect(rename, &QAction::triggered, this, [this, idx]() {
                m_treeView->edit(idx);
            });

            auto *remove = menu.addAction(QStringLiteral("删除"));
            connect(remove, &QAction::triggered, this, [this, item]() {
                if (item->hasChildren()) {
                    auto ret = QMessageBox::question(this,
                        QStringLiteral("确认删除"),
                        QStringLiteral("该分组包含子节点，确定删除？"),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                    if (ret != QMessageBox::Yes) return;
                }
                QStandardItem *parent = item->parent();
                if (parent) parent->removeRow(item->row());
                else m_model->removeRow(item->row());
                emit treeModified();
            });
        }

    } else {
        // ── 空白区：添加根分组 ──
        auto *addRoot = menu.addAction(QStringLiteral("添加分组"));
        connect(addRoot, &QAction::triggered, this, [this]() {
            auto *root = createItem(QStringLiteral("新分组"));
            m_model->appendRow(root);
            m_treeView->setCurrentIndex(root->index());
            m_treeView->edit(root->index());
            emit treeModified();
        });
    }

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

void DeviceTreeWidget::onItemChanged(QStandardItem *item)
{
    emit treeModified();
}

void DeviceTreeWidget::loadFromJson(const QJsonArray &arr)
{
    clear();
    for (const auto &v : arr)
        appendChildItems(m_model->invisibleRootItem(), v.toObject());
    m_treeView->expandAll();
}

void DeviceTreeWidget::appendChildItems(QStandardItem *parent, const QJsonObject &obj)
{
    QString name = obj["name"].toString();
    QString ip = obj["ip"].toString();
    QString rtsp = obj["rtspUrl"].toString();

    auto *item = createItem(name);
    if (!ip.isEmpty()) {
        item->setData(ip, RoleIp);
        item->setText(QString("%1 [%2]").arg(name, ip));
    }
    if (!rtsp.isEmpty())
        item->setData(rtsp, RoleRtspUrl);

    parent->appendRow(item);

    const QJsonArray children = obj["children"].toArray();
    for (const auto &c : children)
        appendChildItems(item, c.toObject());
}

QJsonArray DeviceTreeWidget::saveToJson() const
{
    QJsonArray arr;
    for (int i = 0; i < m_model->rowCount(); ++i)
        childrenToJson(m_model->item(i), arr);
    return arr;
}

void DeviceTreeWidget::childrenToJson(QStandardItem *item, QJsonArray &arr) const
{
    QJsonObject obj;
    obj["name"] = item->text().section(' ', 0, 0);

    QString ip = item->data(RoleIp).toString();
    if (!ip.isEmpty()) obj["ip"] = ip;

    QString rtsp = item->data(RoleRtspUrl).toString();
    if (!rtsp.isEmpty()) obj["rtspUrl"] = rtsp;

    if (item->hasChildren()) {
        QJsonArray children;
        for (int i = 0; i < item->rowCount(); ++i)
            childrenToJson(item->child(i), children);
        obj["children"] = children;
    }

    arr.append(obj);
}

void DeviceTreeWidget::clear()
{
    m_model->clear();
    m_model->setHorizontalHeaderLabels({QStringLiteral("设备资源")});
}

void DeviceTreeWidget::saveToDisk(const QString &path) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(saveToJson()).toJson());
        file.close();
    }
}

void DeviceTreeWidget::loadFromDisk(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isArray())
        loadFromJson(doc.array());
}
