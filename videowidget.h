#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QImage>
#include <QMutex>
#include <QPainter>

class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);

    void setFrame(const QImage &frame);
    void clearFrame();
    QRect selectionRect() const;

signals:
    void selectionFinished(int centerX, int centerY, int width, int height);

protected:
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w * 9 / 16; }
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QImage m_frame;
    mutable QMutex m_frameMutex;
    bool m_hasFrame = false;

    // Selection state
    bool m_selecting = false;
    QPoint m_selStart;
    QPoint m_selEnd;

    // Computed display geometry
    QRect m_displayRect;
    QSize m_frameSize;

    QRect normalizedSelRect() const;
};

#endif // VIDEOWIDGET_H
