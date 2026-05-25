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
    QRect selectionRect() const;

signals:
    void selectionFinished(const QRectF &normRect);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QImage m_frame;
    QMutex m_frameMutex;
    bool m_hasFrame = false;

    // Selection state
    bool m_selecting = false;
    QPoint m_selStart;
    QPoint m_selEnd;

    // Computed display geometry
    QRect m_displayRect;

    QRect normalizedSelRect() const;
};

#endif // VIDEOWIDGET_H
