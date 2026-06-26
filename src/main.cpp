// ============================================================
// 文件: main.cpp
// 描述: T-JSON 客户端主入口。初始化 Qt 应用程序、配置 WebEngine
//       参数，然后启动主窗口进入事件循环。
// ============================================================

#include "mainwindow.h"

#include <QApplication>
#include <QStyleFactory>
#include <QtGlobal>

// 应用程序主入口点
int main(int argc, char *argv[])
{
    // 配置 Chromium / WebEngine 标志：忽略 GPU 黑名单以启用 WebGL、
    // 设置光栅线程数、移除帧率限制、降低日志级别
    // 使用 Chrome 访问 http://localhost:9999 调试地图页面
    qputenv("QTWEBENGINE_REMOTE_DEBUGGING", "9999");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--ignore-gpu-blocklist --enable-webgl --num-raster-threads=4 --disable-frame-rate-limit --log-level=3");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication a(argc, argv);                    // 创建 QApplication 实例
    a.setOrganizationName("LSS");                  // 设置组织名（用于 QSettings 路径）
    a.setApplicationName("LSS Video Manager");     // 设置应用名
    // a.setStyle(QStyleFactory::create("Fusion"));   // 注释掉 Fusion，让 QSS 完全接管控件绘制
    MainWindow w;                                  // 创建主窗口
    w.show();                                      // 显示主窗口
    return QApplication::exec();                   // 进入 Qt 事件循环
}
