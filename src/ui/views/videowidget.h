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

    // 启用/禁用鼠标框选（非点选/框选跟踪模式时禁用）
    void setSelectionEnabled(bool enabled) { m_selectionEnabled = enabled; }

    // 设置状态叠加文本（RTSP/连接状态等），清空帧时自动重置
    void setStatusText(const QString& text);
    void clearStatusText();

    // 设备标识：左上角常驻编号；悬停时展开显示设备名
    void setDeviceInfo(int number, const QString& deviceName);

    // 是否为当前活动设备（活动格子用强调色边框）
    void setActive(bool active);

signals:
    // 选区完成信号：返回选中区域中心坐标及宽高（原始帧像素单位）
    void selectionFinished(int centerX, int centerY, int width, int height);

    // 单击（未拖动）——用于切换当前设备
    void clicked();

    // 双击——用于放大/还原该视频格
    void doubleClicked();

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
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    // 悬停：用于展开/收起设备名角标
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QImage m_frame;              // 当前帧图像（原始尺寸）
    mutable QMutex m_frameMutex; // 保证帧数据的线程安全访问
    bool m_hasFrame = false;     // 是否已有有效帧数据

    // 选区状态
    bool m_selecting = false;    // 是否正在拖拽选择中
    QPoint m_selStart;           // 选区起点（控件坐标）
    QPoint m_selEnd;             // 选区终点（控件坐标）
    bool m_selectionEnabled = true; // 是否允许鼠标框选
    QString m_statusText;            // 状态叠加文本

    // 设备标识与活动态
    int m_deviceNumber = 0;      // 设备编号（0 = 未编号）
    QString m_deviceName;        // 设备名（悬停时显示）
    bool m_active = false;       // 是否当前活动设备
    bool m_hovered = false;      // 鼠标是否悬停

    // 单击/拖动判定
    bool m_pressed = false;              // 左键是否按下
    bool m_movedBeyondThreshold = false; // 是否已超过拖动阈值
    QPoint m_pressPos;                   // 按下位置（控件坐标）

    // 绘制左上角设备角标（编号常驻，悬停展开名称）
    void paintDeviceBadge(QPainter& p);

    // 显示几何信息
    QRect m_displayRect;         // 实际绘制帧图像的区域（居中缩放后）
    QSize m_frameSize;           // 原始帧的尺寸

    // 将选区归一化为正常矩形（起点≤终点），供内部计算使用
    QRect normalizedSelRect() const;
};

#endif // VIDEOWIDGET_H
