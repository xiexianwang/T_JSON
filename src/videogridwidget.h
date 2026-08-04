#ifndef VIDEOGRIDWIDGET_H
#define VIDEOGRIDWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QVector>
#include <QImage>

class VideoWidget;

class VideoGridWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoGridWidget(QWidget *parent = nullptr);

    enum SplitMode { Split1 = 0, Split4, Split9, Split16, SplitCount };

    SplitMode splitMode() const { return m_splitMode; }
    void setSplitMode(SplitMode mode);

    int assignChannel(int cellIndex, const QString &channelName, const QString &rtspUrl);
    void removeChannel(int cellIndex);
    void clearAll();

    VideoWidget *cellAt(int index) const;
    int cellCount() const { return m_cells.size(); }

    int selectedCell() const { return m_selectedCell; }
    void selectCell(int index);

    QImage cellFrame(int index) const;

signals:
    void cellSelected(int cellIndex);
    void channelRemoved(int cellIndex);
    void cellSelectionFinished(int cellIndex, int cx, int cy, int pw, int ph);

private:
    void rebuildGrid();
    void clearGrid();
    int cellsForMode(SplitMode mode) const;

    QGridLayout *m_grid;
    QVector<VideoWidget *> m_cells;
    SplitMode m_splitMode = Split1;
    int m_selectedCell = -1;

    struct ChannelInfo {
        QString name;
        QString rtspUrl;
    };
    QVector<ChannelInfo> m_channels;
};

#endif
