// ========================================================================
// mapdialog.cpp — 地图弹窗对话框实现
// 搭建工具栏与地图控件的布局，处理缩放/底图切换等交互。
// ========================================================================

#include "mapdialog.h"
#include "ui_mapdialog.h"
#include "mapwidget.h"
#include <QScreen>
#include <QGuiApplication>

// ========================================================================
// 构造函数
// 1. 设置 UI 布局，对工具栏应用暗色样式
// 2. 连接缩放 SpinBox → 地图缩放
// 3. 将对话框居中于父窗口（或屏幕）中央
// ========================================================================
MapDialog::MapDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MapDialog)
{
    ui->setupUi(this);

    // 对顶部工具栏应用暗色背景样式，与地图暗色主题保持一致
    ui->toolbar->setStyleSheet(QStringLiteral(
        "background:#252526;border-bottom:1px solid #333;"
        "color:#ccc;"
    ));
    // layout 拉伸：工具栏（第 0 行）不拉伸，地图（第 1 行）拉伸
    ui->verticalLayout->setStretch(0, 0);
    ui->verticalLayout->setStretch(1, 1);

    // 缩放 SpinBox 值变化 → 调用 MapWidget 设置缩放级别
    connect(ui->spinZoom, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int val) {
        ui->m_map->setZoom(val);
    });

    // 地图缩放变化（滚轮/手势）→ 同步 SpinBox 的值
    connect(ui->m_map, &MapWidget::mapZoomChanged,
            this, [this](int zoom) {
        QSignalBlocker _(ui->spinZoom);
        ui->spinZoom->setValue(zoom);
    });

    // 设备信息 OSD 开关
    connect(ui->chkDeviceInfo, &QCheckBox::toggled,
            this, [this](bool checked) {
        ui->m_map->setDeviceInfoVisible(checked);
    });

    // 将对话框居中显示在父窗口（或屏幕）的中央位置
    if (parent) {
        QWidget *p = parent->window();
        QPoint c = p->geometry().center();
        move(c.x() - width() / 2, c.y() - height() / 2);
    }
}

MapDialog::~MapDialog()
{
    delete ui;
}

// 返回内部 MapWidget 指针，供外部设置设备位置、FOV 等
MapWidget* MapDialog::mapWidget() const
{
    return ui->m_map;
}

// ComboBox 当前项变化 → 切换底图类型（卫星/街道）
void MapDialog::on_comboMapType_currentIndexChanged(int index)
{
    ui->m_map->setMapType(index);
}
