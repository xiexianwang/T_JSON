/********************************************************************************
** Form generated from reading UI file 'mapwidget.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAPWIDGET_H
#define UI_MAPWIDGET_H

#include <QtCore/QVariant>
#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MapWidget
{
public:
    QVBoxLayout *verticalLayout;
    QWidget *toolbar;
    QHBoxLayout *toolbarLayout;
    QSpinBox *spinZoom;
    QCheckBox *chkDeviceInfo;
    QComboBox *comboDetailLevel;
    QPushButton *btnRefresh;
    QPushButton *btnMini;
    QSpacerItem *horizontalSpacer;
    QPushButton *btnClose;
    QWebEngineView *m_webView;

    void setupUi(QWidget *MapWidget)
    {
        if (MapWidget->objectName().isEmpty())
            MapWidget->setObjectName("MapWidget");
        MapWidget->resize(800, 600);
        verticalLayout = new QVBoxLayout(MapWidget);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        toolbar = new QWidget(MapWidget);
        toolbar->setObjectName("toolbar");
        toolbar->setMinimumSize(QSize(0, 40));
        toolbar->setMaximumSize(QSize(16777215, 40));
        toolbarLayout = new QHBoxLayout(toolbar);
        toolbarLayout->setObjectName("toolbarLayout");
        toolbarLayout->setContentsMargins(8, 2, 8, 2);
        spinZoom = new QSpinBox(toolbar);
        spinZoom->setObjectName("spinZoom");
        spinZoom->setMinimumSize(QSize(64, 0));
        spinZoom->setMaximumSize(QSize(64, 16777215));
        spinZoom->setMinimum(1);
        spinZoom->setMaximum(18);
        spinZoom->setValue(12);

        toolbarLayout->addWidget(spinZoom);

        chkDeviceInfo = new QCheckBox(toolbar);
        chkDeviceInfo->setObjectName("chkDeviceInfo");
        chkDeviceInfo->setChecked(true);

        toolbarLayout->addWidget(chkDeviceInfo);

        comboDetailLevel = new QComboBox(toolbar);
        comboDetailLevel->addItem(QString());
        comboDetailLevel->addItem(QString());
        comboDetailLevel->addItem(QString());
        comboDetailLevel->addItem(QString());
        comboDetailLevel->setObjectName("comboDetailLevel");
        comboDetailLevel->setMinimumSize(QSize(80, 0));
        comboDetailLevel->setMaximumSize(QSize(80, 16777215));

        toolbarLayout->addWidget(comboDetailLevel);

        btnRefresh = new QPushButton(toolbar);
        btnRefresh->setObjectName("btnRefresh");

        toolbarLayout->addWidget(btnRefresh);

        btnMini = new QPushButton(toolbar);
        btnMini->setObjectName("btnMini");
        btnMini->setMinimumSize(QSize(60, 24));
        btnMini->setMaximumSize(QSize(60, 24));

        toolbarLayout->addWidget(btnMini);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        toolbarLayout->addItem(horizontalSpacer);

        btnClose = new QPushButton(toolbar);
        btnClose->setObjectName("btnClose");
        btnClose->setMinimumSize(QSize(24, 24));
        btnClose->setMaximumSize(QSize(24, 24));

        toolbarLayout->addWidget(btnClose);


        verticalLayout->addWidget(toolbar);

        m_webView = new QWebEngineView(MapWidget);
        m_webView->setObjectName("m_webView");

        verticalLayout->addWidget(m_webView);

        verticalLayout->setStretch(1, 1);

        retranslateUi(MapWidget);

        comboDetailLevel->setCurrentIndex(1);


        QMetaObject::connectSlotsByName(MapWidget);
    } // setupUi

    void retranslateUi(QWidget *MapWidget)
    {
        chkDeviceInfo->setText(QCoreApplication::translate("MapWidget", "\350\256\276\345\244\207\344\277\241\346\201\257", nullptr));
        comboDetailLevel->setItemText(0, QCoreApplication::translate("MapWidget", "\345\215\253\346\230\237", nullptr));
        comboDetailLevel->setItemText(1, QCoreApplication::translate("MapWidget", "\345\215\253\346\230\237+\346\211\271\346\263\250", nullptr));
        comboDetailLevel->setItemText(2, QCoreApplication::translate("MapWidget", "\350\267\257\347\275\221", nullptr));
        comboDetailLevel->setItemText(3, QCoreApplication::translate("MapWidget", "\350\267\257\347\275\221+\346\211\271\346\263\250", nullptr));

        btnRefresh->setText(QCoreApplication::translate("MapWidget", "\345\210\267\346\226\260", nullptr));
        btnMini->setText(QCoreApplication::translate("MapWidget", "\345\260\217\345\234\260\345\233\276", nullptr));
        btnClose->setText(QCoreApplication::translate("MapWidget", "\342\234\225", nullptr));
        (void)MapWidget;
    } // retranslateUi

};

namespace Ui {
    class MapWidget: public Ui_MapWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAPWIDGET_H
