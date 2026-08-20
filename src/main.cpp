// ============================================================
// 文件: main.cpp
// 描述: T-JSON 客户端主入口。初始化 Qt 应用程序、配置 WebEngine
//       参数，然后启动主窗口进入事件循环。
// ============================================================

#include "ui/views/mainwindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QStyleFactory>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>

static LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep)
{
    const unsigned int code = ep->ExceptionRecord->ExceptionAddress ? ep->ExceptionRecord->ExceptionCode : 0;
    QMessageBox::critical(nullptr, "T-JSON Crash",
        QString("程序崩溃\n异常代码: 0x%1\n异常地址: 0x%2")
            .arg(code, 8, 16, QChar('0'))
            .arg(reinterpret_cast<quintptr>(ep->ExceptionRecord->ExceptionAddress), 16, 16, QChar('0')));
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

// 应用程序主入口点
int main(int argc, char *argv[])
{
    qputenv("QTWEBENGINE_REMOTE_DEBUGGING", "9999");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--ignore-gpu-blocklist --enable-webgl --num-raster-threads=4 --disable-frame-rate-limit --log-level=3");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication a(argc, argv);
    a.setOrganizationName("LSS");
    a.setApplicationName("LSS Video Manager");
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(crashHandler);
#endif
    MainWindow w;
    w.show();
    return QApplication::exec();
}
