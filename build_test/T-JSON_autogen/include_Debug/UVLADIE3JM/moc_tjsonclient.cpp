/****************************************************************************
** Meta object code from reading C++ file 'tjsonclient.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/tjsonclient.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'tjsonclient.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN11TJsonClientE_t {};
} // unnamed namespace

template <> constexpr inline auto TJsonClient::qt_create_metaobjectdata<qt_meta_tag_ZN11TJsonClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TJsonClient",
        "deviceConnected",
        "",
        "deviceDisconnected",
        "errorOccurred",
        "errorMsg",
        "reconnecting",
        "attempt",
        "maxRetries",
        "reconnectFailed",
        "jsonReceived",
        "QJsonObject",
        "doc",
        "ackReceived",
        "statusCode",
        "imageSnapped",
        "jpegData",
        "QRect",
        "location",
        "connectToDevice",
        "ip",
        "port",
        "disconnectDevice",
        "sendJsonCmd",
        "cmd",
        "FrameType",
        "type",
        "sendBinaryCmd",
        "payload",
        "sendSerialCmd",
        "serialType",
        "data",
        "onReadyRead",
        "sendHeartbeat",
        "onSocketConnected",
        "onSocketDisconnected",
        "onSocketError",
        "QAbstractSocket::SocketError",
        "socketError",
        "attemptReconnect"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'deviceConnected'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'deviceDisconnected'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'reconnecting'
        QtMocHelpers::SignalData<void(int, int)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::Int, 8 },
        }}),
        // Signal 'reconnectFailed'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'jsonReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 12 },
        }}),
        // Signal 'ackReceived'
        QtMocHelpers::SignalData<void(quint8)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UChar, 14 },
        }}),
        // Signal 'imageSnapped'
        QtMocHelpers::SignalData<void(const QByteArray &, const QRect &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 16 }, { 0x80000000 | 17, 18 },
        }}),
        // Slot 'connectToDevice'
        QtMocHelpers::SlotData<void(const QString &, quint16)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 20 }, { QMetaType::UShort, 21 },
        }}),
        // Slot 'disconnectDevice'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'sendJsonCmd'
        QtMocHelpers::SlotData<void(const QJsonObject &, FrameType)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 24 }, { 0x80000000 | 25, 26 },
        }}),
        // Slot 'sendJsonCmd'
        QtMocHelpers::SlotData<void(const QJsonObject &)>(23, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { 0x80000000 | 11, 24 },
        }}),
        // Slot 'sendBinaryCmd'
        QtMocHelpers::SlotData<void(FrameType, const QByteArray &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 25, 26 }, { QMetaType::QByteArray, 28 },
        }}),
        // Slot 'sendBinaryCmd'
        QtMocHelpers::SlotData<void(FrameType)>(27, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { 0x80000000 | 25, 26 },
        }}),
        // Slot 'sendSerialCmd'
        QtMocHelpers::SlotData<void(const QString &, const QByteArray &)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 30 }, { QMetaType::QByteArray, 31 },
        }}),
        // Slot 'onReadyRead'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'sendHeartbeat'
        QtMocHelpers::SlotData<void()>(33, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSocketConnected'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSocketDisconnected'
        QtMocHelpers::SlotData<void()>(35, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSocketError'
        QtMocHelpers::SlotData<void(QAbstractSocket::SocketError)>(36, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 37, 38 },
        }}),
        // Slot 'attemptReconnect'
        QtMocHelpers::SlotData<void()>(39, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TJsonClient, qt_meta_tag_ZN11TJsonClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject TJsonClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11TJsonClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11TJsonClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11TJsonClientE_t>.metaTypes,
    nullptr
} };

void TJsonClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TJsonClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->deviceConnected(); break;
        case 1: _t->deviceDisconnected(); break;
        case 2: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->reconnecting((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 4: _t->reconnectFailed(); break;
        case 5: _t->jsonReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 6: _t->ackReceived((*reinterpret_cast<std::add_pointer_t<quint8>>(_a[1]))); break;
        case 7: _t->imageSnapped((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QRect>>(_a[2]))); break;
        case 8: _t->connectToDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint16>>(_a[2]))); break;
        case 9: _t->disconnectDevice(); break;
        case 10: _t->sendJsonCmd((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<FrameType>>(_a[2]))); break;
        case 11: _t->sendJsonCmd((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 12: _t->sendBinaryCmd((*reinterpret_cast<std::add_pointer_t<FrameType>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[2]))); break;
        case 13: _t->sendBinaryCmd((*reinterpret_cast<std::add_pointer_t<FrameType>>(_a[1]))); break;
        case 14: _t->sendSerialCmd((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[2]))); break;
        case 15: _t->onReadyRead(); break;
        case 16: _t->sendHeartbeat(); break;
        case 17: _t->onSocketConnected(); break;
        case 18: _t->onSocketDisconnected(); break;
        case 19: _t->onSocketError((*reinterpret_cast<std::add_pointer_t<QAbstractSocket::SocketError>>(_a[1]))); break;
        case 20: _t->attemptReconnect(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 19:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TJsonClient::*)()>(_a, &TJsonClient::deviceConnected, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TJsonClient::*)()>(_a, &TJsonClient::deviceDisconnected, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TJsonClient::*)(const QString & )>(_a, &TJsonClient::errorOccurred, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TJsonClient::*)(int , int )>(_a, &TJsonClient::reconnecting, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (TJsonClient::*)()>(_a, &TJsonClient::reconnectFailed, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (TJsonClient::*)(const QJsonObject & )>(_a, &TJsonClient::jsonReceived, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (TJsonClient::*)(quint8 )>(_a, &TJsonClient::ackReceived, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (TJsonClient::*)(const QByteArray & , const QRect & )>(_a, &TJsonClient::imageSnapped, 7))
            return;
    }
}

const QMetaObject *TJsonClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TJsonClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11TJsonClientE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int TJsonClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 21)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 21;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 21)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 21;
    }
    return _id;
}

// SIGNAL 0
void TJsonClient::deviceConnected()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void TJsonClient::deviceDisconnected()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void TJsonClient::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void TJsonClient::reconnecting(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void TJsonClient::reconnectFailed()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void TJsonClient::jsonReceived(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void TJsonClient::ackReceived(quint8 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void TJsonClient::imageSnapped(const QByteArray & _t1, const QRect & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2);
}
QT_WARNING_POP
