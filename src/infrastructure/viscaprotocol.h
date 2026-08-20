// ============================================================
// 文件: viscaprotocol.h
// 描述: VISCA 协议组包器（纯静态，无状态）。负责将可见光镜头
//       变倍/变焦/停止控制指令封装为 VISCA 协议帧。
// ============================================================

#ifndef VISCAPROTOCOL_H
#define VISCAPROTOCOL_H

#include <QByteArray>
#include <QtGlobal>

// VISCA 协议组包器
class ViscaProtocol {
public:
    // VISCA 变倍控制（Zoom Tele/Wide），速度范围 0-7
    static QByteArray buildZoom(quint8 addr, bool tele, quint8 speed) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0x80 | addr));     // 地址码（高位为 1 表示广播）
        pkt.append(static_cast<char>(0x01));            // VISCA 命令分类: 相机控制
        pkt.append(static_cast<char>(0x04));            // 命令: 变倍/变焦
        pkt.append(static_cast<char>(0x07));            // 子命令: 变倍 (Zoom)
        // bit5: 0=Wide(广角), 1=Tele(望远); 低 3 位为速度
        pkt.append(static_cast<char>((tele ? 0x20 : 0x30) | (speed & 0x07)));
        pkt.append(static_cast<char>(0xFF));            // 终止字节
        return pkt;
    }

    // VISCA 变焦控制（Focus Far/Near），固定指令
    static QByteArray buildFocus(quint8 addr, bool focusFar) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0x80 | addr));
        pkt.append(static_cast<char>(0x01));
        pkt.append(static_cast<char>(0x04));
        pkt.append(static_cast<char>(0x08));            // 子命令: 变焦 (Focus)
        pkt.append(static_cast<char>(focusFar ? 0x02 : 0x03));
        pkt.append(static_cast<char>(0xFF));
        return pkt;
    }

    // VISCA 变倍/变焦停止命令
    static QByteArray buildStop(quint8 addr, bool zoom) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0x80 | addr));
        pkt.append(static_cast<char>(0x01));
        pkt.append(static_cast<char>(0x04));
        pkt.append(static_cast<char>(zoom ? 0x07 : 0x08));  // 0x07=变倍停止, 0x08=变焦停止
        pkt.append(static_cast<char>(0x00));                // 停止速度参数为 0
        pkt.append(static_cast<char>(0xFF));
        return pkt;
    }
};

#endif // VISCAPROTOCOL_H
