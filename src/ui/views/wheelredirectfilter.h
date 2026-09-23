#ifndef WHEELREDIRECTFILTER_H
#define WHEELREDIRECTFILTER_H

#include <QObject>

class QScrollArea;

// 把滚动区内输入控件（Spin/Edit/Combo/Slider）的滚轮事件重定向到滚动区垂直滚动条：
// 鼠标位于任意子控件上时，滚轮只滚动面板，不改变控件数值。
class WheelRedirectFilter : public QObject
{
    Q_OBJECT
public:
    explicit WheelRedirectFilter(QScrollArea* scrollArea, QObject* parent = nullptr);

    // 为该滚动区内的所有输入控件安装过滤器；过滤器以 parent 为父对象管理生命周期。
    static void install(QScrollArea* scrollArea, QObject* parent);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QScrollArea* m_scrollArea;
};

#endif // WHEELREDIRECTFILTER_H
