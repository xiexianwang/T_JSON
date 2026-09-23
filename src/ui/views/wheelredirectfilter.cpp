#include "wheelredirectfilter.h"

#include <QScrollArea>
#include <QWheelEvent>
#include <QCoreApplication>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSlider>
#include <QAbstractItemView>

WheelRedirectFilter::WheelRedirectFilter(QScrollArea* scrollArea, QObject* parent)
    : QObject(parent)
    , m_scrollArea(scrollArea)
{
}

bool WheelRedirectFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel && m_scrollArea) {
        auto* we = static_cast<QWheelEvent*>(event);
        // 转发给滚动区 viewport，由 QScrollArea 原生处理滚轮：
        // 保留系统滚动行数/惯性设置，同时避免输入控件改值。
        QWheelEvent forwarded(we->position(), we->globalPosition(),
                              we->pixelDelta(), we->angleDelta(),
                              we->buttons(), we->modifiers(),
                              we->phase(), we->inverted());
        QCoreApplication::sendEvent(m_scrollArea->viewport(), &forwarded);
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void WheelRedirectFilter::install(QScrollArea* scrollArea, QObject* parent)
{
    if (!scrollArea)
        return;

    // 跳过下拉列表弹出视图及其内部项，避免展开下拉列表时滚轮被劫持。
    auto inItemView = [](QWidget* w) {
        for (QWidget* p = w; p; p = p->parentWidget()) {
            if (qobject_cast<QAbstractItemView*>(p))
                return true;
        }
        return false;
    };

    auto* filter = new WheelRedirectFilter(scrollArea, parent);
    const auto children = scrollArea->findChildren<QWidget*>();
    for (QWidget* w : children) {
        if (inItemView(w))
            continue;
        if (qobject_cast<QAbstractSpinBox*>(w)
            || qobject_cast<QSlider*>(w)
            || qobject_cast<QComboBox*>(w)
            || qobject_cast<QLineEdit*>(w)) {
            w->installEventFilter(filter);
        }
    }
}
