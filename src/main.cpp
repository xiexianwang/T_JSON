#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--ignore-gpu-blocklist --enable-webgl --num-raster-threads=4 --disable-frame-rate-limit --log-level=3");
    QApplication a(argc, argv);
    a.setOrganizationName("TJSONClient");
    a.setApplicationName("T-JSON");
    MainWindow w;
    w.show();
    return QApplication::exec();
}
