#include "videowidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QEnterEvent>
#include <QApplication>

// 强调色：与设备树“当前设备”高亮同色（保持整体 UI 语言一致）
static const char* const kAccent = "#00aaff";

// 构造函数：初始化视频显示控件
// 设置展开策略、最小尺寸，不启用鼠标追踪（仅在按下时接收鼠标事件）
VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
{
    // 启用鼠标跟踪：为了悬停时展开设备名角标
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(60, 34);
    m_statusText = "空闲";
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
    m_statusText.clear();
    update();
}

void VideoWidget::setStatusText(const QString& text)
{
    m_statusText = text;
    if (isVisible()) update();
}

void VideoWidget::clearStatusText()
{
    m_statusText.clear();
    if (isVisible()) update();
}

void VideoWidget::setDeviceInfo(int number, const QString& deviceName)
{
    m_deviceNumber = number;
    m_deviceName = deviceName;
    if (isVisible()) update();
}

void VideoWidget::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    if (isVisible()) update();
}

// 悬停进入：展开设备名角标
void VideoWidget::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    if (!m_hovered) {
        m_hovered = true;
        update();
    }
}

// 悬停离开：收起设备名角标
void VideoWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (m_hovered) {
        m_hovered = false;
        update();
    }
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
// 无帧时显示占位文本（空闲/重连中）；有帧时按等比缩放居中绘制，
// 并叠加绿色半透明选区矩形（正在选择时）。边框最后绘制以始终可见。
void VideoWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    // 边框绘制助手：四条实心边带，厚度恒定（不受控件宽高奇偶影响），
    // 最后绘制以覆盖满幅画面，避免图片盖住边框。活动设备 2px 强调色，其余 1px 蓝灰。
    const auto drawBorder = [this, &p]() {
        const QColor borderColor = m_active ? QColor(kAccent) : QColor("#4488AA");
        const int t = m_active ? 2 : 1;
        p.fillRect(QRect(0, 0, width(), t), borderColor);
        p.fillRect(QRect(0, height() - t, width(), t), borderColor);
        p.fillRect(QRect(0, 0, t, height()), borderColor);
        p.fillRect(QRect(width() - t, 0, t, height()), borderColor);
    };

    QMutexLocker lock(&m_frameMutex);
    if (!m_hasFrame || m_frame.isNull()) {
        p.setPen(QColor("#666666"));
        p.setFont(QFont("Microsoft YaHei", 16));
        p.drawText(rect(), Qt::AlignCenter, m_statusText.isEmpty()
            ? QStringLiteral("空闲") : m_statusText);
        drawBorder();
        paintDeviceBadge(p);
        return;
    }

    // 按等比缩放图像以适应控件尺寸，居中绘制
    QImage scaled = m_frame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int x = (width()  - scaled.width())  / 2;
    int y = (height() - scaled.height()) / 2;
    m_displayRect = QRect(x, y, scaled.width(), scaled.height());
    p.drawImage(m_displayRect, scaled);

    // 有帧时，状态文本叠加在底部
    if (!m_statusText.isEmpty()) {
        QFont f("Microsoft YaHei", 10);
        p.setFont(f);
        QFontMetrics fm(f);
        QRect textRect(0, height() - fm.height() - 8, width(), fm.height() + 8);
        p.fillRect(textRect, QColor(0, 0, 0, 160));
        p.setPen(QColor("#00ff00"));
        p.drawText(textRect, Qt::AlignCenter, m_statusText);
    }

    // 绘制选区框：绿色边框 + 半透明绿色填充
    if (m_selecting) {
        QRect sel = QRect(m_selStart, m_selEnd).normalized();
        p.setPen(QPen(QColor("#00ff00"), 2));
        p.drawRect(sel);
        p.fillRect(sel.adjusted(0, 0, -1, -1), QColor(0, 255, 0, 40));
    }

    drawBorder();
    paintDeviceBadge(p);
}

// 绘制左上角设备角标：圆角 pill。
// 常驻显示编号；悬停时向右展开显示设备名。
void VideoWidget::paintDeviceBadge(QPainter& p)
{
    if (m_deviceNumber <= 0) return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool show = m_hovered && !m_deviceName.isEmpty();
    const QString text = show
        ? QStringLiteral("#%1  %2").arg(m_deviceNumber).arg(m_deviceName)
        : QStringLiteral("#%1").arg(m_deviceNumber);

    // 小号角标：紧凑字体与内边距，避免小格子（16 分屏）里过于抢眼
    QFont f(QStringLiteral("Microsoft YaHei"), 7);
    f.setBold(true);
    p.setFont(f);
    QFontMetrics fm(f);
    const int padH = 4;
    const int padV = 1;
    const int w = fm.horizontalAdvance(text) + padH * 2;
    const int h = fm.height() + padV * 2;
    const QRect pill(4, 4, w, h);

    // 背景：深色半透明；活动设备用强调色字 + 强调边
    p.setBrush(QColor(0, 0, 0, 165));
    p.setPen(QPen(QColor(m_active ? kAccent : "#8899AA"), 1));
    p.drawRoundedRect(pill, h / 2.0, h / 2.0);

    p.setPen(QColor(m_active ? kAccent : "#E8E8E8"));
    p.drawText(pill, Qt::AlignCenter, text);
    p.restore();
}

// 双击：发 doubleClicked()，由上层决定放大/还原
void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    // 双击不应触发框选
    m_pressed = false;
    m_movedBeyondThreshold = false;
    m_selecting = false;
    update();
    emit doubleClicked();
}

// 鼠标按下：记录起点，暂不区分单击/拖动（由移动阈值判定）
void VideoWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    m_pressed = true;
    m_movedBeyondThreshold = false;
    m_pressPos = event->pos();
    // 仅在允许框选且有帧时预置选区起点（真正开始由移动触发）
    if (m_hasFrame && m_selectionEnabled) {
        m_selStart = event->pos();
        m_selEnd = event->pos();
    }
}

// 鼠标移动：超过拖动阈值后才视为拖动（框选），
// 否则保持为“单击候选”。阈值用系统默认拖动距离。
void VideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed) return;
    if (!m_movedBeyondThreshold) {
        const int dist = (event->pos() - m_pressPos).manhattanLength();
        if (dist <= QApplication::startDragDistance())
            return;
        m_movedBeyondThreshold = true;
        // 仅在允许框选且有帧时才进入框选
        if (m_hasFrame && m_selectionEnabled)
            m_selecting = true;
    }
    if (m_selecting) {
        const QPoint pos = event->pos();
        m_selEnd = QPoint(qBound(0, pos.x(), width() - 1),
                          qBound(0, pos.y(), height() - 1));
    }
    update();
}

// 鼠标释放：未超过拖动阈值 → 单击（发 clicked 信号）；
// 否则完成框选映射并发 selectionFinished。
void VideoWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    const bool wasPressed = m_pressed;
    const bool wasClick = !m_movedBeyondThreshold;
    m_pressed = false;

    if (m_selecting) {
        m_selecting = false;
        QRect sel = QRect(m_selStart, m_selEnd).normalized();

        // 仅当选区宽高大于 4 像素且有显示区域可映射时才处理
        if (sel.width() > 4 && sel.height() > 4 &&
            !m_frameSize.isEmpty() && m_displayRect.width() > 0 && m_displayRect.height() > 0) {

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
    } else if (wasPressed && wasClick) {
        // 未拖动的纯单击：切换当前设备（不受框选模式限制）
        emit clicked();
    }

    m_movedBeyondThreshold = false;
    update();
}
