#include "VideoGridWidget.h"
#include <QDebug>

namespace {
constexpr int kSpacing = 0; // 瓷砖间距（0 = 无缝贴合，靠 VideoWidget 自绘边框分隔）
} // namespace

VideoGridWidget::VideoGridWidget(QWidget* parent)
    : QWidget(parent)
    , m_rows(1)
    , m_cols(1)
{
    // 网格直接铺满整个控件；单元格由 QGridLayout 等分
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(kSpacing);

    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, Qt::black);
        setPalette(pal);
    }
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 预创建1个空槽位（默认1x1布局）
    appendSlot();
    updateLayout();
}

VideoGridWidget::~VideoGridWidget()
{
    // 子 Widget 随 parent 自动销毁
}

void VideoGridWidget::appendSlot()
{
    const int index = m_slots.size();
    VideoWidget* vw = new VideoWidget(this);
    connect(vw, &VideoWidget::selectionFinished, this,
        [this, index](int cx, int cy, int pw, int ph) {
            // 通过槽位索引反查 deviceId
            for (auto it = m_deviceSlotMap.constBegin();
                 it != m_deviceSlotMap.constEnd(); ++it) {
                if (it.value() == index) {
                    emit selectionFinished(it.key(), cx, cy, pw, ph);
                    return;
                }
            }
        });
    connect(vw, &VideoWidget::clicked, this, [this, index]() {
        // 通过槽位索引反查 deviceId
        for (auto it = m_deviceSlotMap.constBegin();
             it != m_deviceSlotMap.constEnd(); ++it) {
            if (it.value() == index) {
                emit deviceClicked(it.key());
                return;
            }
        }
    });
    connect(vw, &VideoWidget::doubleClicked, this, [this, index]() {
        // 通过槽位索引反查 deviceId
        for (auto it = m_deviceSlotMap.constBegin();
             it != m_deviceSlotMap.constEnd(); ++it) {
            if (it.value() == index) {
                emit deviceDoubleClicked(it.key());
                return;
            }
        }
    });
    vw->setStatusText("空闲");
    vw->setToolTip(QStringLiteral("单击切换设备；双击放大"));
    m_slots.append(vw);
}

VideoWidget* VideoGridWidget::bindDevice(const QString& deviceId)
{
    if (deviceId.isEmpty())
        return nullptr;

    // 已绑定 → 直接返回现有 widget
    if (m_deviceSlotMap.contains(deviceId))
        return m_slots[m_deviceSlotMap[deviceId]];

    // 查找第一个空槽位
    QSet<int> occupied;
    for (auto it = m_deviceSlotMap.constBegin(); it != m_deviceSlotMap.constEnd(); ++it)
        occupied.insert(it.value());

    for (int i = 0; i < m_slots.size(); ++i) {
        if (!occupied.contains(i)) {
            m_deviceSlotMap[deviceId] = i;
            m_slots[i]->clearFrame();
            m_slots[i]->clearStatusText();
            // 回放已记录的设备标识（名称/编号可能先于首帧到达）
            m_slots[i]->setDeviceInfo(m_deviceNumberMap.value(deviceId, 0),
                                      m_deviceNameMap.value(deviceId));
            m_slots[i]->setActive(deviceId == m_activeDeviceId);
            return m_slots[i];
        }
    }

    // 槽位已满
    return nullptr;
}

void VideoGridWidget::unbindDevice(const QString& deviceId)
{
    if (!m_deviceSlotMap.contains(deviceId)) return;

    int slotIndex = m_deviceSlotMap.take(deviceId);
    if (slotIndex >= 0 && slotIndex < m_slots.size()) {
        m_slots[slotIndex]->clearFrame();
        m_slots[slotIndex]->setStatusText("空闲");
    }
    // 被放大的设备断开：自动还原网格态
    if (m_focusSlot == slotIndex) {
        m_focusSlot = -1;
        updateLayout();
    }
}

VideoWidget* VideoGridWidget::getWidget(const QString& deviceId) const
{
    if (!m_deviceSlotMap.contains(deviceId)) return nullptr;
    int idx = m_deviceSlotMap[deviceId];
    return (idx >= 0 && idx < m_slots.size()) ? m_slots[idx] : nullptr;
}

void VideoGridWidget::setDeviceLabel(const QString& deviceId, int number, const QString& name)
{
    if (deviceId.isEmpty()) return;
    m_deviceNumberMap[deviceId] = number;
    m_deviceNameMap[deviceId] = name;
    if (VideoWidget* vw = getWidget(deviceId))
        vw->setDeviceInfo(number, name);
}

void VideoGridWidget::setActiveDevice(const QString& deviceId)
{
    if (m_activeDeviceId == deviceId) return;
    const QString old = m_activeDeviceId;
    m_activeDeviceId = deviceId;
    if (!old.isEmpty()) {
        if (VideoWidget* vw = getWidget(old)) vw->setActive(false);
    }
    if (!deviceId.isEmpty()) {
        if (VideoWidget* vw = getWidget(deviceId)) vw->setActive(true);
    }
}

void VideoGridWidget::toggleFocus(const QString& deviceId)
{
    const int slot = m_deviceSlotMap.value(deviceId, -1);
    if (slot < 0) return;

    m_focusSlot = (m_focusSlot == slot) ? -1 : slot;   // 同一格再次双击则还原
    updateLayout();
}

void VideoGridWidget::clearFocus()
{
    if (m_focusSlot < 0) return;
    m_focusSlot = -1;
    updateLayout();
}

void VideoGridWidget::setLayoutMode(int rows, int cols)
{
    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;
    m_focusSlot = -1;   // 切布局时退出放大态
    int newCapacity = rows * cols;

    // 1. 释放超出新容量的设备绑定
    for (int i = newCapacity; i < m_slots.size(); ++i) {
        QString devId = m_deviceSlotMap.key(i);
        if (!devId.isEmpty()) {
            m_deviceSlotMap.remove(devId);
            m_slots[i]->clearFrame();
            m_slots[i]->setStatusText("空闲");
        }
    }

    // 2. 调整槽位数组大小
    if (newCapacity > m_slots.size()) {
        // 追加新的空槽位
        while (m_slots.size() < newCapacity) appendSlot();
    } else if (newCapacity < m_slots.size()) {
        // 移除多余槽位
        for (int i = m_slots.size() - 1; i >= newCapacity; --i) {
            VideoWidget* vw = m_slots.takeLast();
            m_layout->removeWidget(vw);
            vw->hide();
            delete vw;
        }
    }

    m_rows = rows;
    m_cols = cols;
    updateLayout();
}

void VideoGridWidget::updateLayout()
{
    // 清空现有布局项（不删除 widget）
    for (int i = m_layout->count() - 1; i >= 0; --i) {
        QLayoutItem* item = m_layout->takeAt(i);
        delete item;
    }

    // 重置并设置均匀行列拉伸比
    for (int r = 0; r < m_layout->rowCount(); ++r)
        m_layout->setRowStretch(r, 0);
    for (int c = 0; c < m_layout->columnCount(); ++c)
        m_layout->setColumnStretch(c, 0);

    for (int r = 0; r < m_rows; ++r)
        m_layout->setRowStretch(r, 1);
    for (int c = 0; c < m_cols; ++c)
        m_layout->setColumnStretch(c, 1);

    // 刷新悬停提示：放大态提示可还原，网格态提示可放大
    for (int i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i]) continue;
        m_slots[i]->setToolTip(i == m_focusSlot
            ? QStringLiteral("双击或 Esc 还原")
            : QStringLiteral("单击切换设备；双击放大"));
    }

    // 放大态：仅显示目标槽位并铺满整个网格，其余槽位隐藏（仍在接收帧）
    const int capacity = m_rows * m_cols;
    if (m_focusSlot >= 0 && m_focusSlot < m_slots.size()) {
        for (int i = 0; i < m_slots.size(); ++i)
            m_slots[i]->hide();
        m_slots[m_focusSlot]->show();
        m_layout->addWidget(m_slots[m_focusSlot], 0, 0, m_rows, m_cols);
        return;
    }

    // 排列所有槽位到网格
    for (int i = 0; i < capacity && i < m_slots.size(); ++i) {
        m_slots[i]->show();
        m_layout->addWidget(m_slots[i], i / m_cols, i % m_cols);
    }
}
