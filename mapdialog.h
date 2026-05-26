#ifndef MAPDIALOG_H
#define MAPDIALOG_H

#include <QDialog>
#include "mapwidget.h"

class MapDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MapDialog(QWidget *parent = nullptr);

    MapWidget* mapWidget() const { return m_map; }

private:
    MapWidget *m_map;
};

#endif
