// ============================================================
// 文件: pelcodprotocol.h
// 描述: Pelco-D 协议组包器（纯静态，无状态）。负责将云台(PTZ)、
//       预置位、红外镜头控制指令封装为 Pelco-D 协议帧。
//       协议帧格式: [0xFF][地址][Cmd1][Cmd2][Data1][Data2][Checksum]
//       Checksum = (地址 + Cmd1 + Cmd2 + Data1 + Data2) % 256
// ============================================================

#ifndef PELCODPROTOCOL_H
#define PELCODPROTOCOL_H

#include <QByteArray>
#include <QtGlobal>

// PTZ 方向枚举：对应 Pelco-D 协议中 Cmd2 字节的位定义
// 组合位可实现对角线方向（如 UpLeft = Up | Left）
enum class PtzDir : quint8 {
    Up = 0x08,              // 上
    Down = 0x10,            // 下
    Left = 0x04,            // 左
    Right = 0x02,           // 右
    UpLeft = 0x0C,          // 左上
    UpRight = 0x0A,         // 右上
    DownLeft = 0x14,        // 左下
    DownRight = 0x12        // 右下
};

// Pelco-D 协议组包器
class PelcoDProtocol {
public:
    // 基础组包
    static QByteArray build(quint8 address, quint8 cmd1, quint8 cmd2, quint8 data1, quint8 data2) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0xFF));                                    // Pelco-D 起始字节
        pkt.append(static_cast<char>(address));                                 // 设备地址
        pkt.append(static_cast<char>(cmd1));                                    // 命令字节 1
        pkt.append(static_cast<char>(cmd2));                                    // 命令字节 2
        pkt.append(static_cast<char>(data1));                                   // 数据字节 1（如 Pan 速度）
        pkt.append(static_cast<char>(data2));                                   // 数据字节 2（如 Tilt 速度）
        quint8 checksum = (address + cmd1 + cmd2 + data1 + data2) % 256;        // 累加和校验
        pkt.append(static_cast<char>(checksum));
        return pkt;
    }

    // 云台方向运动
    static QByteArray buildMove(quint8 address, PtzDir dir, quint8 panSpeed, quint8 tiltSpeed) {
        return build(address, 0x00, static_cast<quint8>(dir), panSpeed, tiltSpeed);
    }

    // 云台停止运动
    static QByteArray buildStop(quint8 address) {
        return build(address, 0x00, 0x00, 0x00, 0x00);
    }

    // 云台转到绝对 Pan 角（panVal = 角度 * 100）
    static QByteArray buildPanTo(quint8 address, int panVal) {
        return build(address, 0x00, 0x4B, (panVal >> 8) & 0xFF, panVal & 0xFF);
    }

    // 云台转到绝对 Tilt 角（tiltVal = 角度 * 100，负角 +36000）
    static QByteArray buildTiltTo(quint8 address, int tiltVal) {
        return build(address, 0x00, 0x4D, (tiltVal >> 8) & 0xFF, tiltVal & 0xFF);
    }

    // 云台水平零点标定
    static QByteArray buildSetZero(quint8 address) {
        return build(address, 0x00, 0x49, 0x00, 0x00);
    }

    // 设置预置位
    static QByteArray buildSetPreset(quint8 address, quint8 preset) {
        return build(address, 0x00, 0x03, preset, 0x00);
    }

    // 调用预置位
    static QByteArray buildCallPreset(quint8 address, quint8 preset) {
        return build(address, 0x00, 0x07, preset, 0x00);
    }

    // 删除预置位
    static QByteArray buildClearPreset(quint8 address, quint8 preset) {
        return build(address, 0x00, 0x05, preset, 0x00);
    }

    // 红外镜头变倍放大
    static QByteArray buildZoomIn(quint8 address, quint8 speed) {
        return build(address, 0x00, 0x20, 0x00, speed);
    }

    // 红外镜头变倍缩小
    static QByteArray buildZoomOut(quint8 address, quint8 speed) {
        return build(address, 0x00, 0x40, 0x00, speed);
    }

    // 红外镜头变焦拉近
    static QByteArray buildFocusIn(quint8 address) {
        return build(address, 0x01, 0x00, 0x00, 0x00);
    }

    // 红外镜头变焦拉远
    static QByteArray buildFocusOut(quint8 address) {
        return build(address, 0x00, 0x80, 0x00, 0x00);
    }

    // 红外镜头停止（zoom=true 停止变倍，false 停止变焦）
    static QByteArray buildLensStop(quint8 address, bool zoom) {
        return zoom ? build(address, 0x00, 0x60, 0x00, 0x00)
                    : build(address, 0x01, 0x80, 0x00, 0x00);
    }

    // 雨刷电机开（K1 开）
    static QByteArray buildWiperOn(quint8 address) {
        return build(address, 0x00, 0x09, 0x00, 0x01);
    }

    // 雨刷电机关（K1 关）
    static QByteArray buildWiperOff(quint8 address) {
        return build(address, 0x00, 0x0B, 0x00, 0x01);
    }
};

#endif // PELCODPROTOCOL_H
