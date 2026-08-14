#ifndef VIDEOGRIDWIDGET_H
#define VIDEOGRIDWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QMap>
#include <QString>
#include "ui/views/videowidget.h"

// ============================================================================
// VideoGridWidget - 多设备视频网格管理器
// ============================================================================
class VideoGridWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoGridWidget(QWidget* parent = nullptr);
    ~VideoGridWidget() override;

    // 绑定设备并分配一个 VideoWidget 实例
    VideoWidget* bindDevice(const QString& deviceId);
    
    // 解绑设备并移除对应的 VideoWidget
    void unbindDevice(const QString& deviceId);
    
    // 获取指定设备的 VideoWidget
    VideoWidget* getWidget(const QString& deviceId) const;

    // 动态调整宫格布局 (如 1x1, 2x2, 3x3)
    void setLayoutMode(int rows, int cols);

private:
    QGridLayout* m_layout;
    QMap<QString, VideoWidget*> m_widgets;
    int m_rows;
    int m_cols;

    void updateLayout();
};

#endif // VIDEOGRIDWIDGET_H
