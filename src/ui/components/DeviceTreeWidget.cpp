#include "DeviceTreeWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QInputDialog>
#include <QStandardPaths>
#include <QToolButton>
#include <QButtonGroup>
#include <QLabel>
#include <QLineEdit>
#include <QSaveFile>
#include <QSettings>
#include <QTimer>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QStyledItemDelegate>
#include <functional>

namespace {

constexpr int kMaxGroupDepth = 4;       // 分组最大嵌套层级
constexpr int kMaxDeviceCount = 64;     // 设备总数上限
constexpr int kDeviceStatusColumn = 1;  // “设备”状态列（TCP 连接）
constexpr int kVideoStatusColumn = 2;   // “视频流”状态列（RTSP）
const QString kLayoutKey = QStringLiteral("ui/gridLayoutMode");

int itemDepth(const QStandardItem *item)
{
    int depth = 0;
    for (const QStandardItem *p = item ? item->parent() : nullptr; p; p = p->parent())
        ++depth;
    return depth;
}

// 名称项同行指定列的状态项。顶层节点的 parent() 为 nullptr，
// 需经模型 invisibleRootItem 取同一行。
//   column 1 = 设备（TCP 连接）状态，column 2 = 视频流（RTSP）状态
QStandardItem *statusItemFor(QStandardItem *item, QStandardItemModel *model, int column)
{
    if (!item || !model) return nullptr;
    QStandardItem *parent = item->parent() ? item->parent() : model->invisibleRootItem();
    return parent->child(item->row(), column);
}

// 状态列（只读、不可拖拽）占位项
QStandardItem *makeStatusItem()
{
    auto *it = new QStandardItem(QString());
    it->setEditable(false);
    it->setDragEnabled(false);
    it->setDropEnabled(false);
    return it;
}

} // namespace

// 设备列代理：为设备行绘制“当前设备”左侧色条与淡背景，
// 并在名称前绘制编号徽标（#N），使树与视频格一一对应。
class DeviceItemDelegate : public QStyledItemDelegate
{
public:
    explicit DeviceItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const int col = index.column();
        const bool isNameCol = (col == 0);
        // 角色数据仅挂在名称列(第0列)，状态列读取其同行的名称列
        const QModelIndex nameIdx = index.siblingAtColumn(0);
        const int type = nameIdx.data(DeviceTreeWidget::RoleNodeType).toInt();
        const bool isDevice = (type == static_cast<int>(DeviceNodeType::Device));
        const bool isCurrent = nameIdx.data(DeviceTreeWidget::RoleCurrentDevice).toBool();
        const int number = nameIdx.data(DeviceTreeWidget::RoleDeviceNumber).toInt();

        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        // 当前设备：整行淡蓝背景（与强调色同色系）
        if (isCurrent) {
            painter->save();
            painter->fillRect(option.rect, QColor(0, 170, 255, 40));
            painter->restore();
        }

        // 名称列编号徽标：为设备行预留左侧空间并绘制 #N
        const int badgeW = (isDevice && isNameCol && number > 0) ? 28 : 0;
        if (badgeW > 0) {
            QStyleOptionViewItem textOpt(opt);
            textOpt.rect = opt.rect.adjusted(badgeW, 0, 0, 0);
            // 先绘制基础项（不含文本对齐调整的背景）
            QStyledItemDelegate::paint(painter, textOpt, index);

            // 徽标圆角底色
            const QRect r(opt.rect.left() + 3, opt.rect.top() + 2,
                          badgeW - 6, opt.rect.height() - 4);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(QColor(isCurrent ? "#00aaff" : "#3a3a3a"));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(r, 3, 3);
            QFont f = opt.font;
            f.setBold(true);
            f.setPointSize(qMax(7, f.pointSize() - 1));
            painter->setFont(f);
            painter->setPen(QColor(isCurrent ? "#ffffff" : "#cccccc"));
            painter->drawText(r, Qt::AlignCenter, QStringLiteral("%1").arg(number));
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, opt, index);
        }

        // 当前设备名称列：左侧强调色条（精致高亮）
        if (isCurrent && isNameCol) {
            painter->save();
            painter->fillRect(QRect(opt.rect.left(), opt.rect.top(), 3, opt.rect.height()),
                              QColor("#00aaff"));
            painter->restore();
        }
    }
};

// ============================================================================
// 内部 QTreeView 子类：拦截键盘，保证结构约束
// ============================================================================
class DeviceTreeView : public QTreeView
{
public:
    explicit DeviceTreeView(QWidget *parent = nullptr) : QTreeView(parent) {}

    // 键盘动作以回调暴露，避免在 .cpp 中引入 Q_OBJECT（免 moc 依赖）
    std::function<void(const QModelIndex &)> onActivate;
    std::function<void(const QModelIndex &)> onDelete;
    std::function<void(const QModelIndex &)> onRename;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (onActivate) onActivate(currentIndex());
            return;
        case Qt::Key_Delete:
            if (onDelete) onDelete(currentIndex());
            return;
        case Qt::Key_F2:
            if (onRename) onRename(currentIndex());
            return;
        default:
            break;
        }
        QTreeView::keyPressEvent(event);
    }

};

// ============================================================================
// 构造
// ============================================================================
DeviceTreeWidget::DeviceTreeWidget(QWidget *parent)
    : QWidget(parent)
    , m_treeView(nullptr)
    , m_model(new QStandardItemModel(this))
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto *view = new DeviceTreeView(this);
    m_treeView = view;

    m_model->setHorizontalHeaderLabels({QStringLiteral("设备资源"),
                                        QStringLiteral("设备"),
                                        QStringLiteral("视频流")});
    m_treeView->setModel(m_model);
    m_treeView->setItemDelegate(new DeviceItemDelegate(m_treeView));
    m_treeView->setHeaderHidden(false);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(16);
    // 双击仅用于连接设备；重命名改为 F2 / 右键菜单（避免与双击冲突）
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->header()->setStretchLastSection(false);
    m_treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeView->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_treeView->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_treeView->header()->resizeSection(1, 52);
    m_treeView->header()->resizeSection(2, 52);
    // 拖拽重组已移除：历史实现依赖 Qt InternalMove 的
    // “复制插入 + 删除源行”，在三列/可嵌套结构下会因行索引错位误删设备，
    // 且立即落盘导致数据丢失。设备归属改由右键菜单维护。
    m_treeView->setDragEnabled(false);
    m_treeView->setAcceptDrops(false);
    m_treeView->setDropIndicatorShown(false);
    m_treeView->setDragDropMode(QAbstractItemView::NoDragDrop);

    connect(view, &QTreeView::doubleClicked, this, &DeviceTreeWidget::onDoubleClicked);
    view->onActivate = [this](const QModelIndex &idx) { onDoubleClicked(idx); };
    view->onDelete = [this](const QModelIndex &idx) {
        if (idx.isValid()) removeItem(m_model->itemFromIndex(idx));
    };
    view->onRename = [this](const QModelIndex &idx) {
        if (idx.isValid()) m_treeView->edit(m_model->index(idx.row(), 0, idx.parent()));
    };
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &DeviceTreeWidget::onCustomContextMenu);
    connect(m_model, &QStandardItemModel::itemChanged,
            this, &DeviceTreeWidget::onItemChanged);
    // 刷新当前设备高亮（左色条/淡背景需重绘）
    connect(m_model, &QStandardItemModel::dataChanged, m_treeView, [this]() {
        m_treeView->viewport()->update();
    });

    lay->addWidget(m_treeView);

    // ---- 布局选择器 ----
    auto *bottomBar = new QHBoxLayout();
    bottomBar->setContentsMargins(4, 4, 4, 4);
    auto *labelLayout = new QLabel(QStringLiteral("布局:"));
    labelLayout->setStyleSheet("color: #aaaaaa; font-size: 11px;");
    bottomBar->addWidget(labelLayout);

    static const QString kLabels[] = { QStringLiteral("1"), QStringLiteral("4"),
                                       QStringLiteral("9"), QStringLiteral("16") };
    static const int kSizes[][2] = { {1,1}, {2,2}, {3,3}, {4,4} };
    auto *btnGroup = new QButtonGroup(this);
    btnGroup->setExclusive(true);
    for (int i = 0; i < 4; ++i) {
        auto *btn = new QToolButton(this);
        btn->setText(kLabels[i]);
        btn->setCheckable(true);
        btn->setFixedHeight(22);
        btn->setMinimumWidth(28);
        btn->setStyleSheet(
            "QToolButton { background: #3a3a3a; color: #cccccc; border: 1px solid #555555; "
            "border-radius: 3px; padding: 1px 4px; font-size: 11px; }"
            "QToolButton:checked { background: #00aaff; color: #ffffff; border-color: #00aaff; }"
        );
        btnGroup->addButton(btn, i);
        bottomBar->addWidget(btn);
        m_layoutBtns[i] = btn;
        connect(btn, &QToolButton::clicked, this, [this, i]() {
            persistLayoutSetting();
            emit layoutModeChanged(kSizes[i][0], kSizes[i][1]);
        });
    }
    bottomBar->addStretch(1);
    lay->addLayout(bottomBar);

    // ---- 载入持久化数据 ----
    loadFromDisk(defaultSavePath());
    connect(this, &DeviceTreeWidget::treeModified, this, &DeviceTreeWidget::scheduleSave);

    // ---- 恢复上次布局 ----
    QTimer::singleShot(0, this, [this]() { restoreLayoutSetting(); });
}

// ============================================================================
// 节点创建/查找
// ============================================================================
QStandardItem *DeviceTreeWidget::createItem(const QString &text, DeviceNodeType type) const
{
    auto *item = new QStandardItem(text);
    item->setEditable(true);
    item->setDragEnabled(false);
    item->setDropEnabled(false);
    item->setData(static_cast<int>(type), RoleNodeType);
    return item;
}

QStandardItem *DeviceTreeWidget::findItemById(const QString &id) const
{
    if (id.isEmpty() || !m_model) return nullptr;
    std::function<QStandardItem *(QStandardItem *)> walk =
        [&](QStandardItem *parent) -> QStandardItem * {
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem *item = parent->child(i);
            if (!item) continue;
            if (item->data(RoleEntryId).toString() == id) return item;
            if (item->hasChildren()) {
                if (QStandardItem *found = walk(item)) return found;
            }
        }
        return nullptr;
    };
    return walk(m_model->invisibleRootItem());
}

// ============================================================================
// 设备连接状态
// ============================================================================
void DeviceTreeWidget::setDeviceConnected(const QString &id, bool connected)
{
    QStandardItem *item = findItemById(id);
    if (!item) return;

    if (item->data(RoleConnected).toBool() == connected) return;

    // 连接态属运行时状态、不落盘：整体屏蔽 itemChanged 触发的写盘
    const bool wasLoading = m_loading;
    m_loading = true;
    item->setData(connected, RoleConnected);
    if (QStandardItem *status = statusItemFor(item, m_model, kDeviceStatusColumn)) {
        status->setText(connected ? QStringLiteral("已连接") : QStringLiteral("未连接"));
        status->setForeground(connected ? QColor(0x00, 0xcc, 0x66) : QColor(0x99, 0x99, 0x99));
    }
    if (!connected) {
        // 设备断开即视频流断开，同步复位视频流状态
        item->setData(false, RoleVideoConnected);
        if (QStandardItem *video = statusItemFor(item, m_model, kVideoStatusColumn)) {
            video->setText(QStringLiteral("未连接"));
            video->setForeground(QColor(0x99, 0x99, 0x99));
        }
    }
    // 粗体表示"已连接"；当前焦点设备额外保持粗体
    QFont f = item->font();
    f.setBold(connected || id == m_currentDeviceId);
    item->setFont(f);
    m_loading = wasLoading;
}

// 按树序重算设备编号（1..N），并写入 RoleDeviceNumber（不落盘）。
void DeviceTreeWidget::renumberDevices()
{
    int n = 0;
    std::function<void(QStandardItem *)> walk = [&](QStandardItem *parent) {
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem *item = parent->child(i);
            if (!item) continue;
            if (item->data(RoleNodeType).toInt() == static_cast<int>(DeviceNodeType::Device)) {
                ++n;
                item->setData(n, RoleDeviceNumber);
            } else {
                item->setData(0, RoleDeviceNumber);
            }
            if (item->hasChildren()) walk(item);
        }
    };
    const bool wasLoading = m_loading;
    m_loading = true;
    walk(m_model->invisibleRootItem());
    m_loading = wasLoading;
    if (m_treeView) m_treeView->viewport()->update();
}

// 当前焦点设备：整行强调（左色条 + 淡蓝背景，见 DeviceItemDelegate）。
void DeviceTreeWidget::setCurrentDevice(const QString &id)
{
    const bool wasLoading = m_loading;
    m_loading = true;

    // 清除旧标记
    if (!m_currentDeviceId.isEmpty() && m_currentDeviceId != id) {
        if (QStandardItem *old = findItemById(m_currentDeviceId)) {
            old->setData(false, RoleCurrentDevice);
            old->setData(QVariant(), Qt::ForegroundRole);
            QFont f = old->font();
            f.setBold(old->data(RoleConnected).toBool());
            old->setFont(f);
        }
    }

    m_currentDeviceId = id;

    if (!id.isEmpty()) {
        if (QStandardItem *item = findItemById(id)) {
            item->setData(true, RoleCurrentDevice);
            item->setForeground(QColor(0x00, 0xaa, 0xff));
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            // 确保当前设备可见并选中（视觉焦点跟随）
            const QModelIndex idx = item->index().siblingAtColumn(0);
            if (idx.isValid()) {
                m_treeView->setCurrentIndex(idx);
                m_treeView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
            }
        }
    }

    if (m_treeView) m_treeView->viewport()->update();
    m_loading = wasLoading;
}

void DeviceTreeWidget::setVideoConnected(const QString &id, bool connected)
{
    QStandardItem *item = findItemById(id);
    if (!item) return;

    if (item->data(RoleVideoConnected).toBool() == connected) return;

    const bool wasLoading = m_loading;
    m_loading = true;
    item->setData(connected, RoleVideoConnected);
    if (QStandardItem *video = statusItemFor(item, m_model, kVideoStatusColumn)) {
        video->setText(connected ? QStringLiteral("已连接") : QStringLiteral("未连接"));
        video->setForeground(connected ? QColor(0x00, 0xcc, 0x66) : QColor(0x99, 0x99, 0x99));
    }
    m_loading = wasLoading;
}

QList<QString> DeviceTreeWidget::allDeviceIds() const
{
    QList<QString> ids;
    std::function<void(QStandardItem *)> walk = [&](QStandardItem *parent) {
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem *item = parent->child(i);
            if (!item) continue;
            if (item->data(RoleNodeType).toInt() == static_cast<int>(DeviceNodeType::Device)) {
                const QString devId = item->data(RoleEntryId).toString();
                if (!devId.isEmpty()) ids.append(devId);
            }
            if (item->hasChildren()) walk(item);
        }
    };
    walk(m_model->invisibleRootItem());
    return ids;
}

int DeviceTreeWidget::deviceNumberForId(const QString &id) const
{
    QStandardItem *item = findItemById(id);
    if (!item) return 0;
    return item->data(RoleDeviceNumber).toInt();
}

// ============================================================================
// 条目查询/更新
// ============================================================================
DeviceEntry DeviceTreeWidget::entryForId(const QString &id) const
{
    QStandardItem *item = findItemById(id);
    if (!item) return DeviceEntry();

    DeviceEntry e;
    e.id = id;
    e.type = static_cast<DeviceNodeType>(item->data(RoleNodeType).toInt());
    e.name = item->text();
    e.ip = item->data(RoleIp).toString();
    e.rtspUrl = item->data(RoleRtspUrl).toString();
    const QVariantMap map = item->data(RoleConfig).toMap();
    if (!map.isEmpty()) e.config = DeviceConfig::fromJson(QJsonObject::fromVariantMap(map));
    return e;
}

void DeviceTreeWidget::setConfigForId(const QString &id, const DeviceConfig &cfg)
{
    QStandardItem *item = findItemById(id);
    if (!item) return;
    item->setData(cfg.toJson().toVariantMap(), RoleConfig);
    emit treeModified();
}

// ============================================================================
// 双击/激活
// ============================================================================
void DeviceTreeWidget::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    // 状态列（第 1/2 列）是独立空 item，统一归一到第 0 列的名称 item
    QStandardItem *item = m_model->itemFromIndex(index.siblingAtColumn(0));
    if (!item) return;
    if (item->data(RoleNodeType).toInt() != static_cast<int>(DeviceNodeType::Device)) return;

    const QString id = item->data(RoleEntryId).toString();
    if (id.isEmpty()) return;
    m_treeView->setCurrentIndex(index.siblingAtColumn(0));
    emit deviceActivated(id, entryForId(id));
}

// ============================================================================
// 右键菜单
// ============================================================================
void DeviceTreeWidget::onCustomContextMenu(const QPoint &pos)
{
    QModelIndex idx = m_treeView->indexAt(pos);
    QMenu menu;

    if (idx.isValid()) {
        // 状态列（第 1 列）是独立的空 item，不含节点数据；统一取第 0 列的名称 item
        idx = idx.siblingAtColumn(0);
        QStandardItem *item = m_model->itemFromIndex(idx);
        if (!item) return;

        const QString id = item->data(RoleEntryId).toString();
        const bool isDevice =
            (item->data(RoleNodeType).toInt() == static_cast<int>(DeviceNodeType::Device));

        if (isDevice) {
            // 右键先把选中项同步到该设备，保证视觉焦点跟随
            m_treeView->setCurrentIndex(idx);

            auto *activate = menu.addAction(QStringLiteral("连接/切换到该设备"));
            connect(activate, &QAction::triggered, this, [this, id]() {
                QStandardItem *it = findItemById(id);
                if (it) emit deviceActivated(id, entryForId(id));
            });

            menu.addSeparator();

            auto *actEditIp = menu.addAction(QStringLiteral("编辑 IP"));
            connect(actEditIp, &QAction::triggered, this, [this, id]() {
                QStandardItem *it = findItemById(id);
                if (it) editIp(it);
            });

            auto *actEditRtsp = menu.addAction(QStringLiteral("编辑 RTSP URL"));
            connect(actEditRtsp, &QAction::triggered, this, [this, id]() {
                QStandardItem *it = findItemById(id);
                if (it) editRtsp(it);
            });

            auto *devProps = menu.addAction(QStringLiteral("设备属性"));
            connect(devProps, &QAction::triggered, this, [this, id]() {
                emit devicePropertiesRequested(id);
            });

            menu.addSeparator();

            auto *toggleConn = menu.addAction(
                item->data(RoleConnected).toBool() ? QStringLiteral("断开")
                                                   : QStringLiteral("连接"));
            connect(toggleConn, &QAction::triggered, this, [this, id]() {
                QStandardItem *it = findItemById(id);
                if (it) emit deviceToggleConnect(id, entryForId(id));
            });
        } else {
            auto *addSub = menu.addAction(QStringLiteral("添加子分组"));
            connect(addSub, &QAction::triggered, this, [this, id]() {
                QStandardItem *it = findItemById(id);
                if (it) addGroup(it);
            });

            auto *addDev = menu.addAction(QStringLiteral("添加设备"));
            connect(addDev, &QAction::triggered, this, [this, id]() {
                QStandardItem *it = findItemById(id);
                if (it) addDevice(it);
            });
        }

        menu.addSeparator();

        auto *rename = menu.addAction(QStringLiteral("重命名 (F2)"));
        connect(rename, &QAction::triggered, this, [this, id]() {
            QStandardItem *it = findItemById(id);
            if (!it) return;
            const QModelIndex nameIdx = it->index().siblingAtColumn(0);
            m_treeView->setCurrentIndex(nameIdx);
            m_treeView->edit(nameIdx);
        });

        auto *remove = menu.addAction(QStringLiteral("删除 (Del)"));
        connect(remove, &QAction::triggered, this, [this, id]() {
            QStandardItem *it = findItemById(id);
            if (it) removeItem(it);
        });
    } else {
        auto *addRoot = menu.addAction(QStringLiteral("添加分组"));
        connect(addRoot, &QAction::triggered, this, [this]() {
            addGroup(nullptr);
        });
    }

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

// ============================================================================
// 交互动作
// ============================================================================
void DeviceTreeWidget::addGroup(QStandardItem *parentItem)
{
    if (parentItem && itemDepth(parentItem) >= kMaxGroupDepth) {
        QMessageBox::warning(this, QStringLiteral("层级超限"),
                             QStringLiteral("分组嵌套层级不能超过 %1 层。").arg(kMaxGroupDepth));
        return;
    }

    const DeviceEntry entry = DeviceEntry::makeGroup();
    auto *group = createItem(entry.name, DeviceNodeType::Group);
    group->setData(entry.id, RoleEntryId);

    if (parentItem && parentItem->data(RoleNodeType).toInt()
                          == static_cast<int>(DeviceNodeType::Group)) {
        parentItem->appendRow({group, makeStatusItem(), makeStatusItem()});
    } else {
        m_model->appendRow({group, makeStatusItem(), makeStatusItem()});
    }

    const QModelIndex nameIdx = group->index().siblingAtColumn(0);
    m_treeView->setCurrentIndex(nameIdx);
    m_treeView->expand(nameIdx.parent());
    m_treeView->edit(nameIdx);
    emit treeModified();
}

void DeviceTreeWidget::addDevice(QStandardItem *parentItem)
{
    if (parentItem && parentItem->data(RoleNodeType).toInt()
                          != static_cast<int>(DeviceNodeType::Group)) {
        return;
    }
    if (parentItem && itemDepth(parentItem) >= kMaxGroupDepth) {
        QMessageBox::warning(this, QStringLiteral("层级超限"),
                             QStringLiteral("分组嵌套层级不能超过 %1 层。").arg(kMaxGroupDepth));
        return;
    }

    // 设备数量上限
    int deviceCount = 0;
    std::function<void(QStandardItem *)> count = [&](QStandardItem *p) {
        for (int i = 0; i < p->rowCount(); ++i) {
            QStandardItem *c = p->child(i);
            if (!c) continue;
            if (c->data(RoleNodeType).toInt() == static_cast<int>(DeviceNodeType::Device))
                ++deviceCount;
            if (c->hasChildren()) count(c);
        }
    };
    count(m_model->invisibleRootItem());
    if (deviceCount >= kMaxDeviceCount) {
        QMessageBox::warning(this, QStringLiteral("数量超限"),
                             QStringLiteral("设备数量已达上限 %1。").arg(kMaxDeviceCount));
        return;
    }

    // IP：循环校验格式与唯一性
    QString ip;
    for (;;) {
        bool ok = false;
        ip = QInputDialog::getText(this, QStringLiteral("设备 IP"),
                                   QStringLiteral("IP 地址 / 主机名:"),
                                   QLineEdit::Normal, QStringLiteral("192.168.1."), &ok)
                 .trimmed();
        if (!ok) return;
        if (!DeviceEntry::isValidHost(ip)) {
            QMessageBox::warning(this, QStringLiteral("格式错误"),
                                 QStringLiteral("请输入合法的 IPv4 地址或主机名。"));
            continue;
        }
        bool duplicate = false;
        std::function<void(QStandardItem *)> check = [&](QStandardItem *p) {
            for (int i = 0; i < p->rowCount() && !duplicate; ++i) {
                QStandardItem *c = p->child(i);
                if (!c) continue;
                if (c->data(RoleNodeType).toInt() == static_cast<int>(DeviceNodeType::Device)
                    && c->data(RoleIp).toString() == ip) {
                    duplicate = true;
                    return;
                }
                if (c->hasChildren()) check(c);
            }
        };
        check(m_model->invisibleRootItem());
        if (duplicate) {
            QMessageBox::warning(this, QStringLiteral("重复设备"),
                                 QStringLiteral("已存在 IP 为 %1 的设备。").arg(ip));
            continue;
        }
        break;
    }

    bool nameOk = false;
    QString name = QInputDialog::getText(this, QStringLiteral("设备名称"),
                                         QStringLiteral("名称（可留空）:"),
                                         QLineEdit::Normal, ip, &nameOk)
                       .trimmed();
    if (!nameOk) return;

    const DeviceEntry entry = DeviceEntry::makeDevice(name, ip);
    auto *dev = createItem(entry.name, DeviceNodeType::Device);
    dev->setData(entry.id, RoleEntryId);
    dev->setData(entry.ip, RoleIp);
    dev->setData(entry.rtspUrl, RoleRtspUrl);
    dev->setData(entry.config.toJson().toVariantMap(), RoleConfig);
    dev->setData(false, RoleConnected);
    dev->setData(false, RoleVideoConnected);
    dev->setToolTip(QStringLiteral("%1\nRTSP: %2").arg(entry.ip, entry.rtspUrl));

    auto *deviceStatus = makeStatusItem();
    deviceStatus->setText(QStringLiteral("未连接"));
    deviceStatus->setForeground(QColor(0x99, 0x99, 0x99));
    auto *videoStatus = makeStatusItem();
    videoStatus->setText(QStringLiteral("未连接"));
    videoStatus->setForeground(QColor(0x99, 0x99, 0x99));

    if (parentItem) {
        parentItem->appendRow({dev, deviceStatus, videoStatus});
        m_treeView->expand(parentItem->index());
    } else {
        m_model->appendRow({dev, deviceStatus, videoStatus});
    }

    emit treeModified();
}

void DeviceTreeWidget::editIp(QStandardItem *item)
{
    const QString current = item->data(RoleIp).toString();
    QString ip;
    for (;;) {
        bool ok = false;
        ip = QInputDialog::getText(this, QStringLiteral("设备 IP"),
                                   QStringLiteral("IP 地址 / 主机名:"),
                                   QLineEdit::Normal, current, &ok)
                 .trimmed();
        if (!ok) return;
        if (!DeviceEntry::isValidHost(ip)) {
            QMessageBox::warning(this, QStringLiteral("格式错误"),
                                 QStringLiteral("请输入合法的 IPv4 地址或主机名。"));
            continue;
        }
        // 唯一性校验（排除自身）
        const QString selfId = item->data(RoleEntryId).toString();
        bool duplicate = false;
        std::function<void(QStandardItem *)> check = [&](QStandardItem *p) {
            for (int i = 0; i < p->rowCount() && !duplicate; ++i) {
                QStandardItem *c = p->child(i);
                if (!c) continue;
                if (c != item
                    && c->data(RoleNodeType).toInt() == static_cast<int>(DeviceNodeType::Device)
                    && c->data(RoleIp).toString() == ip) {
                    duplicate = true;
                    return;
                }
                if (c->hasChildren()) check(c);
            }
        };
        check(m_model->invisibleRootItem());
        Q_UNUSED(selfId);
        if (duplicate) {
            QMessageBox::warning(this, QStringLiteral("重复设备"),
                                 QStringLiteral("已存在 IP 为 %1 的设备。").arg(ip));
            continue;
        }
        break;
    }

    item->setData(ip, RoleIp);
    // RTSP 若仍为旧的默认模板，则同步更新为新 IP
    const QString rtsp = item->data(RoleRtspUrl).toString();
    if (rtsp.isEmpty() || rtsp.contains(current)) {
        item->setData(DeviceEntry::defaultRtspUrl(ip), RoleRtspUrl);
    }
    item->setToolTip(QStringLiteral("%1\nRTSP: %2")
                         .arg(ip, item->data(RoleRtspUrl).toString()));
    emit treeModified();
}

void DeviceTreeWidget::editRtsp(QStandardItem *item)
{
    const QString current = item->data(RoleRtspUrl).toString();
    bool ok = false;
    const QString url = QInputDialog::getText(this, QStringLiteral("RTSP URL"),
                                              QStringLiteral("RTSP 地址:"),
                                              QLineEdit::Normal, current, &ok)
                            .trimmed();
    if (!ok || url.isEmpty()) return;
    item->setData(DeviceEntry::normalizedRtspUrl(url, item->data(RoleIp).toString()),
                  RoleRtspUrl);
    emit treeModified();
}

void DeviceTreeWidget::removeItem(QStandardItem *item)
{
    if (!item) return;

    const bool isDevice =
        (item->data(RoleNodeType).toInt() == static_cast<int>(DeviceNodeType::Device));
    const QString id = item->data(RoleEntryId).toString();

    if (isDevice) {
        // 设备删除二次确认（正在连接时额外提示）
        const bool connected = item->data(RoleConnected).toBool();
        const QString extra = connected
                                  ? QStringLiteral("\n该设备当前处于连接状态，删除将断开连接。")
                                  : QString();
        const auto ret = QMessageBox::question(
            this, QStringLiteral("确认删除"),
            QStringLiteral("确定删除设备「%1」吗？%2").arg(item->text(), extra),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    } else if (item->hasChildren()) {
        const auto ret = QMessageBox::question(
            this, QStringLiteral("确认删除"),
            QStringLiteral("分组「%1」下有子节点，删除后其内所有设备将一并移除，确认？")
                .arg(item->text()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    // 先收集设备 id（删除行之前），再从模型移除，最后通知业务层
    QList<QString> deviceIds;
    std::function<void(QStandardItem *)> collect = [&](QStandardItem *p) {
        if (p->data(RoleNodeType).toInt() == static_cast<int>(DeviceNodeType::Device)) {
            const QString devId = p->data(RoleEntryId).toString();
            if (!devId.isEmpty()) deviceIds.append(devId);
        }
        for (int i = 0; i < p->rowCount(); ++i)
            collect(p->child(i));
    };
    if (isDevice) {
        if (!id.isEmpty()) deviceIds.append(id);
    } else {
        collect(item);
    }

    QStandardItem *parent = item->parent();
    if (parent) parent->removeRow(item->row());
    else m_model->removeRow(item->row());

    // 当前焦点设备被删除时清除标记（业务层随后会切换或置空）
    if (!m_currentDeviceId.isEmpty() && deviceIds.contains(m_currentDeviceId))
        m_currentDeviceId.clear();

    emit treeModified();
    for (const QString &devId : deviceIds)
        emit deviceRemoved(devId);

    Q_UNUSED(id);
}

// ============================================================================
// 条目变更
// ============================================================================
void DeviceTreeWidget::onItemChanged(QStandardItem *item)
{
    if (!item || m_loading) return;

    // 名称列（第 0 列）变更时做基本约束
    if (item->column() == 0) {
        const QString trimmed = item->text().trimmed();
        if (trimmed.isEmpty()) {
            m_loading = true;
            item->setText(QStringLiteral("未命名"));
            m_loading = false;
        } else if (trimmed != item->text()) {
            m_loading = true;
            item->setText(trimmed);
            m_loading = false;
        }
    }
    emit treeModified();
}

// ============================================================================
// 序列化
// ============================================================================
QJsonArray DeviceTreeWidget::collectNodes(QStandardItem *parent) const
{
    QJsonArray arr;
    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *item = parent->child(i);
        if (!item) continue;

        DeviceEntry e;
        e.id = item->data(RoleEntryId).toString();
        e.type = static_cast<DeviceNodeType>(item->data(RoleNodeType).toInt());
        e.name = item->text();
        if (e.isDevice()) {
            e.ip = item->data(RoleIp).toString();
            e.rtspUrl = item->data(RoleRtspUrl).toString();
            const QVariantMap map = item->data(RoleConfig).toMap();
            if (!map.isEmpty())
                e.config = DeviceConfig::fromJson(QJsonObject::fromVariantMap(map));
        }

        QJsonObject obj = e.toJson();
        if (item->hasChildren()) {
            const QJsonArray children = collectNodes(item);
            if (!children.isEmpty()) obj[QStringLiteral("children")] = children;
        }
        arr.append(obj);
    }
    return arr;
}

QJsonArray DeviceTreeWidget::saveNodesToJson() const
{
    if (!m_model) return QJsonArray();
    return collectNodes(m_model->invisibleRootItem());
}

QJsonObject DeviceTreeWidget::saveToJson() const
{
    QJsonObject root;
    root[QStringLiteral("version")] = kDeviceTreeSchemaVersion;
    root[QStringLiteral("nodes")] = saveNodesToJson();
    return root;
}

void DeviceTreeWidget::appendEntries(QStandardItem *parent, const QJsonArray &nodes)
{
    for (const auto &v : nodes) {
        if (!v.isObject()) continue;
        const QJsonObject obj = v.toObject();
        DeviceEntry e = DeviceEntry::fromJson(obj);

        auto *item = createItem(e.name, e.type);
        item->setData(e.id, RoleEntryId);

        auto *deviceStatus = makeStatusItem();
        auto *videoStatus = makeStatusItem();

        if (e.isDevice()) {
            item->setData(e.ip, RoleIp);
            item->setData(e.rtspUrl, RoleRtspUrl);
            item->setData(e.config.toJson().toVariantMap(), RoleConfig);
            item->setData(false, RoleConnected);
            item->setData(false, RoleVideoConnected);
            item->setToolTip(QStringLiteral("%1\nRTSP: %2").arg(e.ip, e.rtspUrl));
            deviceStatus->setText(QStringLiteral("未连接"));
            deviceStatus->setForeground(QColor(0x99, 0x99, 0x99));
            videoStatus->setText(QStringLiteral("未连接"));
            videoStatus->setForeground(QColor(0x99, 0x99, 0x99));
        }

        parent->appendRow({item, deviceStatus, videoStatus});

        if (obj.contains(QStringLiteral("children"))) {
            const QJsonArray children = obj.value(QStringLiteral("children")).toArray();
            if (!children.isEmpty()) appendEntries(item, children);
        }
    }
}

void DeviceTreeWidget::clear()
{
    m_loading = true;
    m_model->clear();
    m_model->setHorizontalHeaderLabels({QStringLiteral("设备资源"),
                                        QStringLiteral("设备"),
                                        QStringLiteral("视频流")});
    m_loading = false;
}

// ============================================================================
// 持久化
// ============================================================================
QString DeviceTreeWidget::defaultSavePath() const
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return appData + QStringLiteral("/device_tree.json");
}

void DeviceTreeWidget::saveToDisk(const QString &path) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "DeviceTree: 无法写入" << path << file.errorString();
        return;
    }
    file.write(QJsonDocument(saveToJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qWarning() << "DeviceTree: 提交失败" << path << file.errorString();
    }
}

void DeviceTreeWidget::loadFromDisk(const QString &path)
{
    QFile file(path);
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) return;

    const QByteArray raw = file.readAll();
    file.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);

    QJsonArray nodes;
    bool ok = true;

    if (err.error != QJsonParseError::NoError) {
        ok = false;
    } else if (doc.isObject()) {
        const QJsonObject root = doc.object();
        const int version = root.value(QStringLiteral("version")).toInt(1);
        if (version > kDeviceTreeSchemaVersion) {
            // 高版本文件：拒绝加载，避免误改损坏
            QMessageBox::warning(this, QStringLiteral("设备树版本不兼容"),
                                 QStringLiteral("设备树文件版本为 %1，高于当前支持的 %2，"
                                                "已跳过加载（原文件未修改）。")
                                     .arg(version)
                                     .arg(kDeviceTreeSchemaVersion));
            return;
        }
        nodes = root.value(QStringLiteral("nodes")).toArray();
    } else if (doc.isArray()) {
        nodes = doc.array(); // 旧格式兼容
    } else {
        ok = false;
    }

    if (!ok) {
        // 损坏文件：备份后以空树继续，避免静默丢数据
        const QString backup = path + QStringLiteral(".bad");
        QFile::remove(backup);
        QFile::rename(path, backup);
        QMessageBox::warning(this, QStringLiteral("设备树文件损坏"),
                             QStringLiteral("解析失败，原文件已备份为:\n%1").arg(backup));
        return;
    }

    clear();
    m_loading = true;
    appendEntries(m_model->invisibleRootItem(), nodes);
    m_treeView->expandAll();
    m_loading = false;

    // 旧格式或缺失 version：立即以新格式回写
    if (doc.isArray() || doc.object().value(QStringLiteral("version")).toInt() != kDeviceTreeSchemaVersion) {
        saveToDisk(path);
    }
}

void DeviceTreeWidget::scheduleSave()
{
    if (m_loading || m_dirty) return;
    m_dirty = true;
    QTimer::singleShot(0, this, [this]() {
        m_dirty = false;
        saveNow();
    });
}

void DeviceTreeWidget::saveNow()
{
    saveToDisk(defaultSavePath());
}

void DeviceTreeWidget::restoreLayoutSetting()
{
    QSettings settings;
    const int mode = settings.value(kLayoutKey, 1).toInt();
    if (mode < 0 || mode > 3) return;
    if (m_layoutBtns[mode]) m_layoutBtns[mode]->setChecked(true);

    static const int kSizes[][2] = { {1,1}, {2,2}, {3,3}, {4,4} };
    emit layoutModeChanged(kSizes[mode][0], kSizes[mode][1]);
}

void DeviceTreeWidget::persistLayoutSetting()
{
    for (int i = 0; i < 4; ++i) {
        if (m_layoutBtns[i] && m_layoutBtns[i]->isChecked()) {
            QSettings settings;
            settings.setValue(kLayoutKey, i);
            return;
        }
    }
}
