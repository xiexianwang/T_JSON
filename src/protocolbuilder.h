#ifndef PROTOCOLBUILDER_H
#define PROTOCOLBUILDER_H

#include <QByteArray>
#include <QtGlobal>

class ProtocolBuilder {
public:
    enum IrayAction : quint8 {
        IrayStepPos   = 0x00,
        IrayStepNeg   = 0x01,
        IrayContNeg   = 0x02,
        IrayContPos   = 0x03,
        IrayStop      = 0x04,
        IrayAutoFocus = 0x05,
    };

    static QByteArray buildPelcoD(quint8 address, quint8 cmd1, quint8 cmd2, quint8 data1, quint8 data2);
    static QByteArray buildViscaZoom(quint8 addr, bool tele, quint8 speed);
    static QByteArray buildViscaFocus(quint8 addr, bool far, quint8 speed);
    static QByteArray buildViscaStop(quint8 addr, bool zoom);
    static QByteArray buildIray(quint8 motor, quint8 action);

private:
    ProtocolBuilder() = delete;
};

enum class PtzDir : quint8 {
    Up = 0x08,
    Down = 0x10,
    Left = 0x04,
    Right = 0x02,
    UpLeft = 0x0C,
    UpRight = 0x0A,
    DownLeft = 0x14,
    DownRight = 0x12
};

#endif // PROTOCOLBUILDER_H
