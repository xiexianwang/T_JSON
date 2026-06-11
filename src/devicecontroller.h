// ============================================================
// 文件: devicecontroller.h
// 描述: 设备控制器模块。上层业务逻辑与底层通信协议之间的中间
//       层，封装云台(PTZ)、镜头(Lens)、算法模式、显示模式、
//       预置位等设备控制功能。内部使用 ProtocolBuilder 组包，
//       通过 TJsonClient 的串口透传通道发送指令。
// ============================================================

#ifndef DEVICECONTROLLER_H
#define DEVICECONTROLLER_H

#include <QObject>
#include <QByteArray>
#include "tjsonclient.h"
#include "configmanager.h"

// 协议构建器：纯静态工具类，用于组装各类底层串口通信协议的数据包
// 当前支持：Pelco-D（云台控制/红外镜头）、VISCA（可见光镜头变倍/变焦）、IRAY（红外镜头）
class ProtocolBuilder {
public:
    // IRAY 红外协议动作枚举
    enum IrayAction : quint8 {
        IrayStepPos   = 0x00,  // 单步+
        IrayStepNeg   = 0x01,  // 单步-
        IrayContNeg   = 0x02,  // 连续-
        IrayContPos   = 0x03,  // 连续+
        IrayStop      = 0x04,  // 连续停止
        IrayAutoFocus = 0x05,  // 自动聚焦（仅聚焦电机）
    };
    // Pelco-D 协议组包
    // 格式: [0xFF][地址][Cmd1][Cmd2][Data1][Data2][Checksum]
    // Checksum = (地址 + Cmd1 + Cmd2 + Data1 + Data2) % 256
    static QByteArray buildPelcoD(quint8 address, quint8 cmd1, quint8 cmd2, quint8 data1, quint8 data2) {
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

    // VISCA 变倍控制（Zoom Tele/Wide），速度范围 0-7
    static QByteArray buildViscaZoom(quint8 addr, bool tele, quint8 speed) {
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

    // VISCA 变焦控制（Focus Far/Near），速度范围 0-7
    static QByteArray buildViscaFocus(quint8 addr, bool far, quint8 speed) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0x80 | addr));
        pkt.append(static_cast<char>(0x01));
        pkt.append(static_cast<char>(0x04));
        pkt.append(static_cast<char>(0x08));            // 子命令: 变焦 (Focus)
        // bit5: 0=Near(近焦), 1=Far(远焦); 低 3 位为速度
        pkt.append(static_cast<char>((far ? 0x20 : 0x30) | (speed & 0x07)));
        pkt.append(static_cast<char>(0xFF));
        return pkt;
    }

    // VISCA 变倍/变焦停止命令
    static QByteArray buildViscaStop(quint8 addr, bool zoom) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0x80 | addr));
        pkt.append(static_cast<char>(0x01));
        pkt.append(static_cast<char>(0x04));
        pkt.append(static_cast<char>(zoom ? 0x07 : 0x08));  // 0x07=变倍停止, 0x08=变焦停止
        pkt.append(static_cast<char>(0x00));                // 停止速度参数为 0
        pkt.append(static_cast<char>(0xFF));
        return pkt;
    }

    // IRAY 红外协议组包（聚焦/变倍电机控制）
    // 格式: [0xAA][0x06][0x10][motor][0x01][action][0x00][checksum][0xEB][0xAA]
    // checksum = sum(0xAA + 0x06 + 0x10 + motor + 0x01 + action + 0x00) & 0xFF
    static QByteArray buildIray(quint8 motor, quint8 action) {
        QByteArray pkt;
        pkt.append(static_cast<char>(0xAA));
        pkt.append(static_cast<char>(0x06));
        pkt.append(static_cast<char>(0x10));
        pkt.append(static_cast<char>(motor));      // 0x00=聚焦, 0x01=变倍
        pkt.append(static_cast<char>(0x01));
        pkt.append(static_cast<char>(action));     // 动作
        pkt.append(static_cast<char>(0x00));
        quint8 sum = 0;
        for (int i = 0; i < pkt.size(); ++i)
            sum += static_cast<quint8>(pkt[i]);
        pkt.append(static_cast<char>(sum));        // checksum
        pkt.append(static_cast<char>(0xEB));
        pkt.append(static_cast<char>(0xAA));
        return pkt;
    }
};

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

// 设备控制器类
// 协调 TJsonClient（网络通信）和 ConfigManager（配置参数），
// 封装上层业务逻辑为对设备的各种控制操作
class DeviceController : public QObject
{
    Q_OBJECT
public:
    explicit DeviceController(TJsonClient* client, ConfigManager* cfg, QObject *parent = nullptr);

    // ================= 基础控制 =================
    void setWorkMode(int mode);             // 设置工作模式
    void queryImageParams();                // 查询当前图像参数

    // ================= 算法与显示控制 =================
    void setAlgoModel(int model);           // 设置 AI 算法模型
    void setDisplayMode(int mode);          // 设置显示模式（画中画等）
    void setLocation(const QString& lat, const QString& lon);  // 设置 GPS 位置

    // ================= 云台控制 (Pelco-D) =================
    void ptzMove(PtzDir dir);               // 云台向指定方向运动
    void ptzStop();                         // 云台停止运动

    // ================= 框选跟踪 =================
    void setBoxTrack(int centerX, int centerY, int width, int height);  // 设置跟踪框

    // ================= 预置位控制 (Pelco-D) =================
    void setPreset(int preset);             // 设置预置位
    void callPreset(int preset);            // 调用预置位
    void delPreset(int preset);             // 删除预置位

    // ================= 附加功能开关 =================
    void setDigitalZoom(bool enable);       // 数字变焦开关
    void setAutoZoom(bool enable);          // 自动变倍开关
    void setCaptureUpload(bool enable);     // 抓拍上传开关
    void posReset(bool enable);             // 位置归零

    // ================= 镜头控制 (VISCA / Pelco-D) =================
    // target: 0=可见光(VISCA), 1=红外(Pelco-D)
    void lensZoomIn(int target);            // 变倍放大
    void lensZoomOut(int target);           // 变倍缩小
    void lensFocusIn(int target);           // 变焦拉近
    void lensFocusOut(int target);          // 变焦拉远
    void lensStop();                        // 停止镜头运动

    // ================= 串口透传通用网关 =================
    void sendTransparentData(const QString& serialType, const QByteArray& data);  // 通用透传

signals:
    void commandSent(const QString& serialType, const QByteArray& data);  // 指令已发送通知

private:
    TJsonClient* m_client;          // 网络客户端（非拥有指针）
    ConfigManager* m_cfg;           // 配置管理器（非拥有指针）
    int m_lastLensTarget = 0;       // 最近一次镜头操作的目标（0=可见光, 1=红外）
    bool m_lastLensIsZoom = true;   // 最近一次镜头操作是否为变倍（true=变倍, false=变焦）
};

#endif // DEVICECONTROLLER_H