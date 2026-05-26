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
    m_frameSize = frame.size();
    m_hasFrame = true;
    if (isVisible())
        update();
}

void VideoWidget::clearFrame()
{
    QMutexLocker lock(&m_frameMutex);
    m_frame = QImage();
    m_hasFrame = false;
    m_selecting = false;
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
        if (sel.width() > 4 && sel.height() > 4 &&
            !m_frameSize.isEmpty() && m_displayRect.width() > 0 && m_displayRect.height() > 0) {

            // Map directly from widget coords → original frame pixel coords
            int cx = int((sel.center().x() - m_displayRect.left()) * m_frameSize.width()  / (double)m_displayRect.width());
            int cy = int((sel.center().y() - m_displayRect.top() ) * m_frameSize.height() / (double)m_displayRect.height());
            int pw = int(sel.width()  * m_frameSize.width()  / (double)m_displayRect.width());
            int ph = int(sel.height() * m_frameSize.height() / (double)m_displayRect.height());

            cx = qBound(0, cx, m_frameSize.width()  - 1);
            cy = qBound(0, cy, m_frameSize.height() - 1);
            pw = qMax(1, qMin(pw, m_frameSize.width()));
            ph = qMax(1, qMin(ph, m_frameSize.height()));

            emit selectionFinished(cx, cy, pw, ph);
        }

        update();
    }
}
