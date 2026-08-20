#ifndef MAINWINDOWLAYOUTSERVICE_H
#define MAINWINDOWLAYOUTSERVICE_H

#include <QObject>
#include <QPoint>
#include <QPair>
#include <functional>

class QDialog;
class QPushButton;
class QWidget;
class QEvent;
class QResizeEvent;
class MapWidget;
class VideoGridWidget;

class MainWindowLayoutService : public QObject
{
    Q_OBJECT
public:
    struct Setup {
        QWidget* display = nullptr;
        QWidget* mapContainer = nullptr;
        MapWidget* mapWidget = nullptr;
        QWidget* mapOverlay = nullptr;
        QDialog* pipDialog = nullptr;
        QWidget* pipTitle = nullptr;
        VideoGridWidget* videoGrid = nullptr;
        QPushButton* mapToggle = nullptr;
        std::function<void()> addVideoToDisplay;
        std::function<double(const QString&)> parseCoordinate;
        std::function<QPair<QString, QString>()> coordinates;
    };

    explicit MainWindowLayoutService(QObject* parent = nullptr);

    void setup(const Setup& setup);
    void toggleMap();
    void toggleMapMode();
    void updateMapLayout();
    bool handleEventFilter(QObject* obj, QEvent* event);

private:
    Setup m_setup;
    bool m_mapVisible = false;
    bool m_mapExpanded = false;
    QPoint m_miniMapPos{10, 10};
    bool m_dragging = false;
    QPoint m_dragStart;
    QPoint m_pipPos{10, 10};
    QPoint m_pipDragStart;
};

#endif // MAINWINDOWLAYOUTSERVICE_H
