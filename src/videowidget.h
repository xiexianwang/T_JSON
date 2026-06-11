#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QImage>
#include <QMutex>
#include <QPainter>

// VideoWidget：视频显示与选区自定义控件
// 负责渲染 RTSP 解码后的视频帧，同时支持鼠标拖拽选择区域，
// 并将选中的区域坐标映射回原始帧像素坐标后通过信号发出。
class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);

    // 设置当前要显示的帧图像（线程安全）
    void setFrame(const QImage &frame);

    // 清空当前显示的帧，恢复为"未连接"提示状态
    void clearFrame();

    // 返回当前选中的矩形区域（若未在选中状态则返回空矩形）
    QRect selectionRect() const;

    // ── 网格分屏支持 ──
    void setChannelLabel(const QString &text) { m_channelLabel = text; update(); }
    QString channelLabel() const { return m_channelLabel; }
    void setSelected(bool sel) { m_selected = sel; update(); }
    bool isSelected() const { return m_selected; }
    QImage frame() const { QMutexLocker lock(&m_frameMutex); return m_frame; }

signals:
    // 选区完成信号：返回选中区域中心坐标及宽高（原始帧像素单位）
    void selectionFinished(int centerX, int centerY, int width, int height);

protected:
    // 固定宽高比 16:9，支持高度自适应布局
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w * 9 / 16; }

    // 绘制事件：绘制视频帧及选区叠加层
    void paintEvent(QPaintEvent *event) override;

    // 鼠标事件：开始/拖动/完成区域选择
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QImage m_frame;              // 当前帧图像（原始尺寸）
    mutable QMutex m_frameMutex; // 保证帧数据的线程安全访问
    bool m_hasFrame = false;     // 是否已有有效帧数据

    // ── 网格分屏状态 ──
    QString m_channelLabel;
    bool m_selected = false;

    // 选区状态
    bool m_selecting = false;    // 是否正在拖拽选择中
    QPoint m_selStart;           // 选区起点（控件坐标）
    QPoint m_selEnd;             // 选区终点（控件坐标）

    // 显示几何信息
    QRect m_displayRect;         // 实际绘制帧图像的区域（居中缩放后）
    QSize m_frameSize;           // 原始帧的尺寸

    // 将选区归一化为正常矩形（起点≤终点），供内部计算使用
    QRect normalizedSelRect() const;
};

#endif // VIDEOWIDGET_H
