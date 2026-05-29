#include "videowidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

// 构造函数：初始化视频显示控件
// 设置展开策略、最小尺寸，不启用鼠标追踪（仅在按下时接收鼠标事件）
VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(320, 240);
}

// 设置当前要显示的帧图像
// 在互斥锁保护下更新帧数据及尺寸，若控件可见则触发重绘
void VideoWidget::setFrame(const QImage &frame)
{
    QMutexLocker lock(&m_frameMutex);
    m_frame = frame;
    m_frameSize = frame.size();
    m_hasFrame = true;
    if (isVisible())
        update();
}

// 清空帧图像，恢复为初始状态
// 同时取消正在进行的选区操作，避免悬空状态
void VideoWidget::clearFrame()
{
    QMutexLocker lock(&m_frameMutex);
    m_frame = QImage();
    m_hasFrame = false;
    m_selecting = false;
    update();
}

// 获取当前选区矩形（控件坐标空间）
// 若不在选中状态则返回空矩形，供外部读取当前选区信息
QRect VideoWidget::selectionRect() const
{
    QMutexLocker lock(&m_frameMutex);
    if (!m_selecting)
        return QRect();
    return QRect(m_selStart, m_selEnd).normalized();
}

// 绘制事件：负责渲染视频帧及选区叠加层
// 无帧时显示"RTSP 未连接"占位文本；有帧时按等比缩放居中绘制，
// 并叠加绿色半透明选区矩形（正在选择时）
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

    // 按等比缩放图像以适应控件尺寸，居中绘制
    QImage scaled = m_frame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int x = (width()  - scaled.width())  / 2;
    int y = (height() - scaled.height()) / 2;
    m_displayRect = QRect(x, y, scaled.width(), scaled.height());
    p.drawImage(m_displayRect, scaled);

    // 绘制选区框：绿色边框 + 半透明绿色填充
    if (m_selecting) {
        QRect sel = QRect(m_selStart, m_selEnd).normalized();
        p.setPen(QPen(QColor("#00ff00"), 2));
        p.drawRect(sel);

        // 半透明叠加层，帮助用户看清选区范围
        p.fillRect(sel.adjusted(0, 0, -1, -1), QColor(0, 255, 0, 40));
    }
}

// 鼠标按下事件：在有帧且左键按下时，开始记录选区起点
void VideoWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_hasFrame) {
        m_selStart = event->pos();
        m_selEnd = event->pos();
        m_selecting = true;
        update();
    }
}

// 鼠标移动事件：正在选区时，实时更新终点并限制在控件边界内
void VideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting) {
        QPoint pos = event->pos();
        // 将鼠标位置限制在控件边界内，防止选区超出绘制区域
        m_selEnd = QPoint(qBound(0, pos.x(), width() - 1),
                          qBound(0, pos.y(), height() - 1));
        update();
    }
}

// 鼠标释放事件：完成选区操作
// 将控件坐标系的选区映射回原始帧像素坐标，要求选区具有有意义的尺寸，
// 然后发射 selectionFinished 信号供上层使用
void VideoWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        QRect sel = QRect(m_selStart, m_selEnd).normalized();

        // 仅当选区宽高大于 4 像素且有显示区域可映射时才处理
        if (sel.width() > 4 && sel.height() > 4 &&
            !m_frameSize.isEmpty() && m_displayRect.width() > 0 && m_displayRect.height() > 0) {

            // 将控件坐标映射到原始帧像素坐标：
            // 先减去显示区域的偏移，再按缩放比例换算回原始帧尺寸
            int cx = int((sel.center().x() - m_displayRect.left()) * m_frameSize.width()  / (double)m_displayRect.width());
            int cy = int((sel.center().y() - m_displayRect.top() ) * m_frameSize.height() / (double)m_displayRect.height());
            int pw = int(sel.width()  * m_frameSize.width()  / (double)m_displayRect.width());
            int ph = int(sel.height() * m_frameSize.height() / (double)m_displayRect.height());

            // 限制坐标和尺寸在有效范围内，防止越界
            cx = qBound(0, cx, m_frameSize.width()  - 1);
            cy = qBound(0, cy, m_frameSize.height() - 1);
            pw = qMax(1, qMin(pw, m_frameSize.width()));
            ph = qMax(1, qMin(ph, m_frameSize.height()));

            emit selectionFinished(cx, cy, pw, ph);
        }

        update();
    }
}
