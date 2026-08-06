#include "VideoGridWidget.h"
#include <QDebug>

VideoGridWidget::VideoGridWidget(QWidget* parent)
    : QWidget(parent)
    , m_rows(1)
    , m_cols(1)
{
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);
    setLayout(m_layout);
}

VideoGridWidget::~VideoGridWidget()
{
    // 子 Widget 会随 parent 自动销毁
}

VideoWidget* VideoGridWidget::bindDevice(const QString& deviceId)
{
    if (m_widgets.contains(deviceId)) {
        return m_widgets.value(deviceId);
    }

    VideoWidget* vw = new VideoWidget(this);
    m_widgets.insert(deviceId, vw);
    
    // 如果数量超过当前宫格容量，自动扩展 (如 1x1 -> 2x2)
    int count = m_widgets.size();
    if (count > m_rows * m_cols) {
        if (m_rows == 1) { m_rows = 2; m_cols = 2; }
        else if (m_rows == 2) { m_rows = 3; m_cols = 3; }
        // 更多可按需扩展
    }
    
    updateLayout();
    return vw;
}

void VideoGridWidget::unbindDevice(const QString& deviceId)
{
    if (m_widgets.contains(deviceId)) {
        VideoWidget* vw = m_widgets.take(deviceId);
        m_layout->removeWidget(vw);
        vw->deleteLater();
        updateLayout();
    }
}

VideoWidget* VideoGridWidget::getWidget(const QString& deviceId) const
{
    return m_widgets.value(deviceId, nullptr);
}

void VideoGridWidget::setLayoutMode(int rows, int cols)
{
    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;
    m_rows = rows;
    m_cols = cols;
    updateLayout();
}

void VideoGridWidget::updateLayout()
{
    // 清空现有布局项 (不删除 widget)
    for (int i = m_layout->count() - 1; i >= 0; --i) {
        QLayoutItem* item = m_layout->takeAt(i);
        if (item) delete item;
    }

    // 重新排列
    int r = 0, c = 0;
    for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it) {
        m_layout->addWidget(it.value(), r, c);
        c++;
        if (c >= m_cols) {
            c = 0;
            r++;
        }
    }
}
