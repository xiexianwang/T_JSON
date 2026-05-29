// ========================================================================
// mapdialog.h — 地图弹窗对话框头文件
// 功能：将 MapWidget 封装在独立 QDialog 窗口中，提供工具栏
//       （缩放控制、底图切换等），适合在全屏或大窗口中查看地图。
// ========================================================================

#ifndef MAPDIALOG_H
#define MAPDIALOG_H

#include <QDialog>

class MapWidget;

namespace Ui {
class MapDialog;
}

// ========================================================================
// MapDialog — 地图弹窗对话框
// 包含 MapWidget 和顶部工具栏（缩放 SpinBox、底图切换 ComboBox），
// 用于弹出式查看设备位置、FOV、轨迹和目标信息。
// ========================================================================
class MapDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MapDialog(QWidget *parent = nullptr);
    ~MapDialog();

    // 获取内部 MapWidget 指针，供外部直接操作地图（设置位置、FOV 等）
    MapWidget* mapWidget() const;

private slots:
    // 底图类型切换 ComboBox 响应
    void on_comboMapType_currentIndexChanged(int index);

private:
    Ui::MapDialog *ui;
};

#endif
