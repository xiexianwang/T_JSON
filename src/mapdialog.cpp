#include "mapdialog.h"
#include "ui_mapdialog.h"
#include "mapwidget.h"
#include <QScreen>
#include <QGuiApplication>

MapDialog::MapDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MapDialog)
{
    ui->setupUi(this);

    // Apply dark style to toolbar
    ui->toolbar->setStyleSheet(QStringLiteral(
        "background:#252526;border-bottom:1px solid #333;"
    ));
    ui->verticalLayout->setStretch(0, 0);
    ui->verticalLayout->setStretch(1, 1);

    // Zoom spinbox
    connect(ui->spinZoom, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        ui->m_map->setZoom(val);
    });

    // Center on parent window or screen
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

MapWidget* MapDialog::mapWidget() const
{
    return ui->m_map;
}

void MapDialog::on_comboMapType_currentIndexChanged(int index)
{
    ui->m_map->setMapType(index);
}
