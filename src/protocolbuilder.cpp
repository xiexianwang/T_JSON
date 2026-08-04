#include "protocolbuilder.h"

QByteArray ProtocolBuilder::buildPelcoD(quint8 address, quint8 cmd1, quint8 cmd2, quint8 data1, quint8 data2)
{
    QByteArray pkt;
    pkt.append(static_cast<char>(0xFF));
    pkt.append(static_cast<char>(address));
    pkt.append(static_cast<char>(cmd1));
    pkt.append(static_cast<char>(cmd2));
    pkt.append(static_cast<char>(data1));
    pkt.append(static_cast<char>(data2));
    quint8 checksum = (address + cmd1 + cmd2 + data1 + data2) % 256;
    pkt.append(static_cast<char>(checksum));
    return pkt;
}

QByteArray ProtocolBuilder::buildViscaZoom(quint8 addr, bool tele, quint8 speed)
{
    QByteArray pkt;
    pkt.append(static_cast<char>(0x80 | addr));
    pkt.append(static_cast<char>(0x01));
    pkt.append(static_cast<char>(0x04));
    pkt.append(static_cast<char>(0x07));
    pkt.append(static_cast<char>((tele ? 0x20 : 0x30) | (speed & 0x07)));
    pkt.append(static_cast<char>(0xFF));
    return pkt;
}

QByteArray ProtocolBuilder::buildViscaFocus(quint8 addr, bool far, quint8 speed)
{
    QByteArray pkt;
    pkt.append(static_cast<char>(0x80 | addr));
    pkt.append(static_cast<char>(0x01));
    pkt.append(static_cast<char>(0x04));
    pkt.append(static_cast<char>(0x08));
    pkt.append(static_cast<char>((far ? 0x20 : 0x30) | (speed & 0x07)));
    pkt.append(static_cast<char>(0xFF));
    return pkt;
}

QByteArray ProtocolBuilder::buildViscaStop(quint8 addr, bool zoom)
{
    QByteArray pkt;
    pkt.append(static_cast<char>(0x80 | addr));
    pkt.append(static_cast<char>(0x01));
    pkt.append(static_cast<char>(0x04));
    pkt.append(static_cast<char>(zoom ? 0x07 : 0x08));
    pkt.append(static_cast<char>(0x00));
    pkt.append(static_cast<char>(0xFF));
    return pkt;
}

QByteArray ProtocolBuilder::buildIray(quint8 motor, quint8 action)
{
    QByteArray pkt;
    pkt.append(static_cast<char>(0xAA));
    pkt.append(static_cast<char>(0x06));
    pkt.append(static_cast<char>(0x10));
    pkt.append(static_cast<char>(motor));
    pkt.append(static_cast<char>(0x01));
    pkt.append(static_cast<char>(action));
    pkt.append(static_cast<char>(0x00));
    quint8 sum = 0;
    for (int i = 0; i < pkt.size(); ++i)
        sum += static_cast<quint8>(pkt[i]);
    pkt.append(static_cast<char>(sum));
    pkt.append(static_cast<char>(0xEB));
    pkt.append(static_cast<char>(0xAA));
    return pkt;
}
