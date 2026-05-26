#include "mapdialog.h"
#include <QVBoxLayout>
#include <QScreen>
#include <QGuiApplication>

MapDialog::MapDialog(QWidget *parent)
    : QDialog(parent)
    , m_map(new MapWidget(this))
{
    setWindowTitle(QStringLiteral("设备地图"));
    resize(900, 600);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_map);

    // Center on parent window or screen
    if (parent) {
        QWidget *p = parent->window();
        QPoint c = p->geometry().center();
        move(c.x() - width() / 2, c.y() - height() / 2);
    }
}
