#include "MainWindowLayoutService.h"
#include "ui/views/mapwidget.h"
#include "ui/components/VideoGridWidget.h"
#include <QDialog>
#include <QEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegion>

MainWindowLayoutService::MainWindowLayoutService(QObject* parent)
    : QObject(parent)
{
}

void MainWindowLayoutService::setup(const Setup& setup)
{
    m_setup = setup;
}

void MainWindowLayoutService::toggleMap()
{
    m_mapVisible = !m_mapVisible;
    m_setup.mapToggle->setChecked(m_mapVisible);
    if (m_mapVisible) {
        m_mapExpanded = false;
        updateMapLayout();
    } else {
        m_setup.pipDialog->hide();
        m_setup.addVideoToDisplay();
        m_setup.mapContainer->setVisible(false);
    }
}

void MainWindowLayoutService::toggleMapMode()
{
    m_mapExpanded = !m_mapExpanded;
    updateMapLayout();
}

void MainWindowLayoutService::updateMapLayout()
{
    const QSize ps = m_setup.display->size();
    if (ps.isEmpty()) return;

    if (!m_mapVisible) {
        m_setup.addVideoToDisplay();
        m_setup.mapContainer->setVisible(false);
        m_setup.pipDialog->hide();
        return;
    }

    m_setup.mapContainer->setVisible(true);
    if (m_mapExpanded) {
        m_setup.mapContainer->setGeometry(0, 0, ps.width(), ps.height());
        m_setup.mapContainer->setAttribute(Qt::WA_TranslucentBackground, false);
        m_setup.mapContainer->clearMask();
        m_setup.mapWidget->setGeometry(0, 0, ps.width(), ps.height());
        m_setup.mapWidget->setCircularClip(false);
        m_setup.mapOverlay->setVisible(false);

        m_setup.videoGrid->setParent(m_setup.pipDialog);
        m_setup.pipDialog->layout()->addWidget(m_setup.videoGrid);
        m_pipPos = QPoint(8, ps.height() - 240 - 8);
        m_setup.pipDialog->move(m_pipPos);
        m_setup.pipDialog->show();
        m_setup.videoGrid->setVisible(true);
    } else {
        m_setup.addVideoToDisplay();
        m_setup.pipDialog->hide();
        m_setup.mapContainer->setGeometry(m_miniMapPos.x(), m_miniMapPos.y(), 280, 280);
        m_setup.mapContainer->setAttribute(Qt::WA_TranslucentBackground, true);
        m_setup.mapContainer->setMask(QRegion(0, 0, 280, 280, QRegion::Ellipse));
        m_setup.mapWidget->setGeometry(0, 0, 280, 280);
        const auto coords = m_setup.coordinates();
        const double lat = m_setup.parseCoordinate(coords.first);
        const double lon = m_setup.parseCoordinate(coords.second);
        if (lat != 0 || lon != 0)
            m_setup.mapWidget->setCircularClip(true, lat, lon, 12);
        else
            m_setup.mapWidget->setCircularClip(true);
        m_setup.mapOverlay->setGeometry(0, 0, 280, 280);
        m_setup.mapOverlay->setVisible(true);
    }
    m_setup.mapContainer->raise();
}

bool MainWindowLayoutService::handleEventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_setup.mapOverlay) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragStart = me->pos();
                m_dragging = false;
            }
            return true;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                const QPoint delta = me->pos() - m_dragStart;
                if (delta.manhattanLength() > 5) {
                    m_dragging = true;
                    m_miniMapPos = m_setup.mapContainer->pos() + delta;
                    m_setup.mapContainer->move(m_miniMapPos);
                }
            }
            return true;
        }
        case QEvent::MouseButtonRelease:
            m_dragging = false;
            return true;
        case QEvent::MouseButtonDblClick:
            toggleMapMode();
            return true;
        default:
            break;
        }
    }

    if (obj == m_setup.pipTitle) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_pipDragStart = me->globalPosition().toPoint();
                m_setup.pipTitle->setCursor(Qt::ClosedHandCursor);
            }
            return true;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                m_pipPos = m_setup.pipDialog->pos() +
                    (me->globalPosition().toPoint() - m_pipDragStart);
                m_setup.pipDialog->move(m_pipPos);
                m_pipDragStart = me->globalPosition().toPoint();
            }
            return true;
        }
        case QEvent::MouseButtonRelease:
            m_setup.pipTitle->setCursor(Qt::OpenHandCursor);
            return true;
        case QEvent::MouseButtonDblClick:
            toggleMapMode();
            return true;
        default:
            break;
        }
    }
    return false;
}
