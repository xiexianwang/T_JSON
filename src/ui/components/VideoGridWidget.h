#ifndef VIDEOGRIDWIDGET_H
#define VIDEOGRIDWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QMap>
#include <QVector>
#include <QSet>
#include <QString>
#include "ui/views/videowidget.h"

// ============================================================================
// VideoGridWidget - 多设备视频网格管理器（固定槽位模型）
// ============================================================================
// 网格填满整个显示区，等分单元格；单个视频格内部按原始比例居中
// （letterbox，黑边仅落在格内），因此外部不产生多余黑区。
// 布局切换时创建对应数量的空槽位（1/4/9/16），
// 设备连接时自动分配到第一个空槽，设备断开时释放槽位（widget保留，显示"空闲"）。
class VideoGridWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoGridWidget(QWidget* parent = nullptr);
    ~VideoGridWidget() override;

    // 分配设备到第一个空槽位（已绑定则直接返回现有widget）
    VideoWidget* bindDevice(const QString& deviceId);

    // 释放设备绑定的槽位（widget保留，清帧清状态，显示"空闲"）
    void unbindDevice(const QString&deviceId);

    // 获取指定设备的 VideoWidget（未绑定返回nullptr）
    VideoWidget* getWidget(const QString& deviceId) const;

    // 获取所有已绑定设备的 ID 列表
    QList<QString> boundDeviceIds() const { return m_deviceSlotMap.keys(); }

    // 设置宫格布局（如 1x1, 2x2, 3x3, 4x4），创建对应数量的空槽位
    void setLayoutMode(int rows, int cols);

    // 设置某设备视频格的角标（编号 + 设备名）
    void setDeviceLabel(const QString& deviceId, int number, const QString& name);

    // 高亮当前活动设备对应的视频格（空串 = 取消高亮）
    void setActiveDevice(const QString& deviceId);

    // 切换单格放大：传入已放大的同一设备则还原。
    // 放大期间仅显示该格并铺满显示区，其余格仍在拉流（仅隐藏）。
    void toggleFocus(const QString& deviceId);
    void clearFocus();
    bool hasFocus() const { return m_focusSlot >= 0; }

signals:
    // 设备视频画面框选完成（携带 deviceId）
    void selectionFinished(const QString& deviceId, int centerX, int centerY, int width, int height);

    // 单击某设备视频格（用于切换当前设备）
    void deviceClicked(const QString& deviceId);

    // 双击某设备视频格（用于放大/还原）
    void deviceDoubleClicked(const QString& deviceId);

private:
    QGridLayout* m_layout;
    QVector<VideoWidget*> m_slots;        // 固定槽位数组
    QMap<QString, int> m_deviceSlotMap;   // deviceId → 槽位索引
    QMap<QString, int> m_deviceNumberMap;  // deviceId → 设备编号
    QMap<QString, QString> m_deviceNameMap; // deviceId → 设备名
    QString m_activeDeviceId;             // 当前活动设备
    int m_focusSlot = -1;                 // 放大的槽位索引（-1 = 网格态）
    int m_rows;
    int m_cols;

    void updateLayout();
    void appendSlot();
};

#endif // VIDEOGRIDWIDGET_H
