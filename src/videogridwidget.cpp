#include "videogridwidget.h"
#include "videowidget.h"
#include <QGridLayout>
#include <QLabel>
#include <QtMath>

VideoGridWidget::VideoGridWidget(QWidget *parent)
    : QWidget(parent)
    , m_grid(new QGridLayout(this))
{
    m_grid->setSpacing(2);
    m_grid->setContentsMargins(0, 0, 0, 0);
    rebuildGrid();
}

int VideoGridWidget::cellsForMode(SplitMode mode) const
{
    switch (mode) {
    case Split1:  return 1;
    case Split4:  return 4;
    case Split9:  return 9;
    case Split16: return 16;
    default: return 1;
    }
}

void VideoGridWidget::setSplitMode(SplitMode mode)
{
    if (mode == m_splitMode) return;
    m_splitMode = mode;
    m_selectedCell = -1;

    m_channels.resize(cellsForMode(mode));
    rebuildGrid();
}

void VideoGridWidget::rebuildGrid()
{
    clearGrid();
    int n = cellsForMode(m_splitMode);
    int cols = qSqrt(n);
    int rows = n / cols;
    m_cells.reserve(n);
    m_channels.resize(n);

    for (int i = 0; i < n; ++i) {
        int r = i / cols;
        int c = i % cols;

        auto *vw = new VideoWidget(this);
        vw->setMinimumSize(160, 120);
        vw->setProperty("cellIndex", i);

        if (!m_channels[i].name.isEmpty()) {
            vw->setChannelLabel(m_channels[i].name);
        }

        connect(vw, &VideoWidget::selectionFinished, this, [this, i](int cx, int cy, int pw, int ph) {
            selectCell(i);
            emit cellSelectionFinished(i, cx, cy, pw, ph);
        });

        m_grid->addWidget(vw, r, c);
        m_cells.append(vw);
    }
}

void VideoGridWidget::clearGrid()
{
    for (auto *w : m_cells) {
        m_grid->removeWidget(w);
        w->deleteLater();
    }
    m_cells.clear();
}

void VideoGridWidget::clearAll()
{
    for (auto &ch : m_channels)
        ch = ChannelInfo();
    for (auto *w : m_cells)
        w->clearFrame();
    m_selectedCell = -1;
}

int VideoGridWidget::assignChannel(int cellIndex, const QString &channelName, const QString &rtspUrl)
{
    if (cellIndex < 0 || cellIndex >= m_cells.size())
        return -1;

    m_channels[cellIndex] = {channelName, rtspUrl};
    m_cells[cellIndex]->setChannelLabel(channelName);
    return cellIndex;
}

void VideoGridWidget::removeChannel(int cellIndex)
{
    if (cellIndex < 0 || cellIndex >= m_cells.size()) return;
    m_channels[cellIndex] = ChannelInfo();
    m_cells[cellIndex]->clearFrame();
    m_cells[cellIndex]->setChannelLabel(QString());
}

VideoWidget *VideoGridWidget::cellAt(int index) const
{
    if (index < 0 || index >= m_cells.size()) return nullptr;
    return m_cells[index];
}

void VideoGridWidget::selectCell(int index)
{
    if (index == m_selectedCell) return;
    if (m_selectedCell >= 0 && m_selectedCell < m_cells.size())
        m_cells[m_selectedCell]->setSelected(false);

    m_selectedCell = index;
    if (index >= 0 && index < m_cells.size())
        m_cells[index]->setSelected(true);

    emit cellSelected(index);
}

QImage VideoGridWidget::cellFrame(int index) const
{
    auto *vw = cellAt(index);
    if (!vw) return QImage();
    return vw->frame();
}
