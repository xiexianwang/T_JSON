#include "videowidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(320, 240);
}

void VideoWidget::setFrame(const QImage &frame)
{
    QMutexLocker lock(&m_frameMutex);
    m_frame = frame;
    m_hasFrame = true;
    if (isVisible())
        update();
}

QRect VideoWidget::selectionRect() const
{
    QMutexLocker lock(&m_frameMutex);
    if (!m_selecting)
        return QRect();
    return QRect(m_selStart, m_selEnd).normalized();
}

void VideoWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), Qt::black);

    QMutexLocker lock(&m_frameMutex);
    if (!m_hasFrame || m_frame.isNull()) {
        p.setPen(QColor("#666666"));
        p.setFont(QFont("Microsoft YaHei", 16));
        p.drawText(rect(), Qt::AlignCenter, "RTSP 未连接");
        return;
    }

    // Scale maintaining aspect ratio
    QImage scaled = m_frame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int x = (width()  - scaled.width())  / 2;
    int y = (height() - scaled.height()) / 2;
    m_displayRect = QRect(x, y, scaled.width(), scaled.height());
    p.drawImage(m_displayRect, scaled);

    // Draw selection box
    if (m_selecting) {
        QRect sel = QRect(m_selStart, m_selEnd).normalized();
        p.setPen(QPen(QColor("#00ff00"), 2));
        p.drawRect(sel);

        // Semi-transparent overlay
        p.fillRect(sel.adjusted(0, 0, -1, -1), QColor(0, 255, 0, 40));
    }
}

void VideoWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_hasFrame) {
        m_selStart = event->pos();
        m_selEnd = event->pos();
        m_selecting = true;
        update();
    }
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting) {
        QPoint pos = event->pos();
        // Clamp to widget bounds
        m_selEnd = QPoint(qBound(0, pos.x(), width() - 1),
                          qBound(0, pos.y(), height() - 1));
        update();
    }
}

void VideoWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        QRect sel = QRect(m_selStart, m_selEnd).normalized();

        // Only emit if selection has meaningful size
        if (sel.width() > 4 && sel.height() > 4) {
            // Normalize to displayRect coords [0, 1]
            QRectF norm;
            norm.setLeft  ((sel.left()   - m_displayRect.left()) / (double)m_displayRect.width());
            norm.setTop   ((sel.top()    - m_displayRect.top())  / (double)m_displayRect.height());
            norm.setRight ((sel.right()  - m_displayRect.left()) / (double)m_displayRect.width());
            norm.setBottom((sel.bottom() - m_displayRect.top())  / (double)m_displayRect.height());

            // Clamp to [0, 1]
            norm = norm.intersected(QRectF(0, 0, 1, 1));
            emit selectionFinished(norm);
        }

        update();
    }
}
