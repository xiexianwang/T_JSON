#ifndef MAPDIALOG_H
#define MAPDIALOG_H

#include <QDialog>

class MapWidget;

namespace Ui {
class MapDialog;
}

class MapDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MapDialog(QWidget *parent = nullptr);
    ~MapDialog();

    MapWidget* mapWidget() const;

private slots:
    void on_comboMapType_currentIndexChanged(int index);

private:
    Ui::MapDialog *ui;
};

#endif
