#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("TJSONClient");
    a.setApplicationName("T-JSON");
    MainWindow w;
    w.show();
    return QApplication::exec();
}
