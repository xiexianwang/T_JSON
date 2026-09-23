// ============================================================
// 文件: devicecommandservice.cpp
// 描述: 电机指令服务实现。
// ============================================================

#include "devicecommandservice.h"
#include "infrastructure/configmanager.h"
#include "core/DeviceConfig.h"
#include "infrastructure/modbustransport.h"
#include "infrastructure/stm32tcptransport.h"
#include <QTimer>
#include <QJsonDocument>

DeviceCommandService::DeviceCommandService(ConfigManager* cfg, DeviceConfig* devCfg, QObject *parent)
    : QObject(parent)
    , m_cfg(cfg)
    , m_devCfg(devCfg)
{
    m_modbus = new ModbusTransport(this);
    m_tcp = new Stm32TcpTransport(this);

    // MODBUS 串口接收：仅对「读保持寄存器应答」(功能码 0x03, 2 字节数据) 消费请求 FIFO，
    // 按请求顺序区分「模式查询(0x01B1)」与「电流读取(0x000D)」——两者应答结构相同。
    // 写指令回显(0x06) 等其它帧不消费队列，避免错位。
    connect(m_modbus, &ModbusTransport::dataReceived, this, [this](const QByteArray& data) {
        if (data.size() >= 5 && static_cast<quint8>(data.at(1)) == 0x03
            && static_cast<quint8>(data.at(2)) == 0x02
            && !m_modbusReadQueue.isEmpty()) {
            const ModbusReadKind kind = m_modbusReadQueue.dequeue();
            if (kind == ModbusReadKind::Mode) {
                emit motorModeResult(static_cast<quint8>(data.at(3)) == 0x01);
            } else {
                quint16 ma = 0;
                // MODBUS 无保持电流/延迟概念，hold/delay 置 -1 表示不适用
                if (ModbusTransport::parseReadRegisterResponse(data, &ma))
                    emit motorCurrentResult(static_cast<int>(ma), -1, -1);
            }
        }
        emit commandSent("MODBUS_RECV", data);
    });
    connect(m_modbus, &ModbusTransport::errorOccurred, this, &DeviceCommandService::motorSerialError);

    // STM32-TCP 接收：解析 status / error_code
    connect(m_tcp, &Stm32TcpTransport::dataReceived, this, [this](const QByteArray& data) {
        emit commandSent("STM32-TCP_RECV", data);

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
        QJsonObject obj = doc.object();

        // 电机上报电流：motor_ack {run_current, hold_current, iholddelay}
        if (obj.contains("run_current") || obj.contains("hold_current")) {
            int run   = obj["run_current"].toInt(-1);
            int hold  = obj["hold_current"].toInt(-1);
            int delay = obj["iholddelay"].toInt(-1);
            if (run >= 0 && hold >= 0 && delay >= 0)
                emit motorCurrentResult(run, hold, delay);
        }

        if (!obj.contains("status")) return;
        int status = obj["status"].toInt(-1);
        int errorCode = obj["error_code"].toInt(0);

        // status: 0=OK/空闲, 1=BUSY, 2=COMPLETE, 3=ERROR, 4=LIMIT_ZERO, 5=LIMIT_END, 6=INTERRUPTED
        switch (status) {
        case 3: // ERROR
            emit motorTcpError(tr("电机错误: %1").arg(errorCodeToString(errorCode)));
            break;
        case 4: // LIMIT_ZERO
            emit motorTcpError(tr("触发零点限位"));
            break;
        case 5: // LIMIT_END
            emit motorTcpError(tr("触发终点限位"));
            break;
        case 6: // INTERRUPTED
            emit commandSent("STM32-TCP", QByteArray("被新指令打断"));
            break;
        default:
            break;
        }
    });
    connect(m_tcp, &Stm32TcpTransport::errorOccurred, this, &DeviceCommandService::motorTcpError);
    // 已发送完整帧（含帧头）通知上层日志
    connect(m_tcp, &Stm32TcpTransport::frameSent, this, [this](const QByteArray& pkt) {
        emit commandSent("STM32-TCP-V4.0", pkt);
    });
}

QString DeviceCommandService::proto() const { return m_devCfg ? m_devCfg->motorProtocol : m_cfg->motorProtocol(); }
QString DeviceCommandService::channel() const { return m_devCfg ? m_devCfg->motorCommandChannel : m_cfg->motorCommandChannel(); }

void DeviceCommandService::setPelcoDSender(const std::function<void(const QByteArray&)>& sender)
{
    m_pelcoDSender = sender;
}

// ================= 电机串口管理 =================

bool DeviceCommandService::openMotorSerial(const QString& portName)
{
    return m_modbus->open(portName);
}

void DeviceCommandService::closeMotorSerial()
{
    m_modbus->close();
}

bool DeviceCommandService::isMotorSerialOpen() const
{
    return m_modbus->isOpen();
}

// ================= 电机 TCP 管理 =================

void DeviceCommandService::openMotorTcp()
{
    if (m_devCfg) {
        m_tcp->open(m_devCfg->motorTcpIp, m_devCfg->motorTcpPort);
    } else {
        m_tcp->open(m_cfg->motorTcpIp(), m_cfg->motorTcpPort());
    }
}

void DeviceCommandService::closeMotorTcp()
{
    m_tcp->close();
}

bool DeviceCommandService::isMotorTcpOpen() const
{
    return m_tcp->isOpen();
}

void DeviceCommandService::sendModbus(const QByteArray& pkt)
{
    if (channel() == "串口") {
        if (!m_modbus->isOpen()) {
            emit motorSerialError(tr("电机串口未打开"));
            return;
        }
        m_modbus->send(pkt);
        emit commandSent("MODBUS-RTU(SERIAL)", pkt);
    } else {
        // 默认走 Pelco-D 透传
        sendPelcoDWiper(pkt);
    }
}

void DeviceCommandService::sendPelcoDWiper(const QByteArray& pkt)
{
    if (m_pelcoDSender)
        m_pelcoDSender(pkt);
}

void DeviceCommandService::sendMotorTcpV4(const QJsonObject& json)
{
    // cmd_id 使用自增前序列号（与原行为保持一致）
    QJsonObject cmd = json;
    cmd["cmd_id"] = m_tcp->seq();
    m_tcp->send(cmd);
}

// ================= 雨刷电机控制 =================

void DeviceCommandService::motorStart()
{
    if (proto() == "MODBUS-RTU") {
        sendModbus(QByteArray::fromHex("01060037001039C8"));
    } else if (proto() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 5;
        sendMotorTcpV4(cmd);
    } else {
        sendPelcoDWiper(QByteArray::fromHex("FF01000900010B"));
    }
}

void DeviceCommandService::motorStop()
{
    if (proto() == "MODBUS-RTU") {
        sendModbus(QByteArray::fromHex("0106003800000807"));
        // 停止需连续下发两条指令，帧间隔 50ms
        QTimer::singleShot(50, this, [this]() {
            sendModbus(QByteArray::fromHex("01060037000439C7"));
        });
    } else if (proto() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 8;
        sendMotorTcpV4(cmd);
    } else {
        sendPelcoDWiper(QByteArray::fromHex("FF01000B00010D"));
    }
}

void DeviceCommandService::motorWiperStop()
{
    if (proto() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 8;
        sendMotorTcpV4(cmd);
        QTimer::singleShot(50, this, [this]() {
            QJsonObject cmd2;
            cmd2["action"] = 2;
            sendMotorTcpV4(cmd2);
        });
    } else {
        motorStop();
    }
}

void DeviceCommandService::motorJogLeft()
{
    if (proto() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 3;
        cmd["target_pos"] = 50000;
        cmd["speed"] = 8000;
        sendMotorTcpV4(cmd);
        return;
    }
    if (proto() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("01060037008039A4"));
}

void DeviceCommandService::motorJogRight()
{
    if (proto() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 4;
        cmd["target_pos"] = 50000;
        cmd["speed"] = 8000;
        sendMotorTcpV4(cmd);
        return;
    }
    if (proto() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("01060037004039F4"));
}

void DeviceCommandService::motorZeroCalib()
{
    if (proto() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("0106003A00016807"));
}

void DeviceCommandService::motorReturnZero()
{
    if (proto() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 2;
        sendMotorTcpV4(cmd);
        return;
    }
    if (proto() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("01060037000439C7"));
}

void DeviceCommandService::motorCheckMode()
{
    if (proto() != "MODBUS-RTU") return;
    m_modbusReadQueue.enqueue(ModbusReadKind::Mode);
    sendModbus(QByteArray::fromHex("010301B10001D5D1"));
}

void DeviceCommandService::motorReadCurrent()
{
    if (proto() != "MODBUS-RTU") return;
    m_modbusReadQueue.enqueue(ModbusReadKind::Current);
    // 读保持寄存器 0x000D（实际电流）
    sendModbus(QByteArray::fromHex("0103000D000115C9"));
}

void DeviceCommandService::motorToggleMode()
{
    if (proto() == "STM32-TCP-V4.0") {
        m_tcpIsAuto = !m_tcpIsAuto;
        QJsonObject cmd;
        cmd["action"] = m_tcpIsAuto ? 11 : 10;
        sendMotorTcpV4(cmd);
        // 通知界面更新模式结果
        emit motorModeResult(!m_tcpIsAuto);
        return;
    }
    if (proto() != "MODBUS-RTU") return;

    m_modbusIsAuto = !m_modbusIsAuto;
    if (m_modbusIsAuto) {
        // 切换到自动模式
        sendModbus(QByteArray::fromHex("011001B100030600140009000AD4A8"));
    } else {
        // 切换到手动模式
        sendModbus(QByteArray::fromHex("011001B1000306000000000000B4AE"));
        QTimer::singleShot(50, this, [this]() {
            sendModbus(QByteArray::fromHex("010600380001C9C7"));
        });
        QTimer::singleShot(100, this, [this]() {
            sendModbus(QByteArray::fromHex("01060037000439C7")); // 触发绝对位置模式启动(回零)
        });
    }
    emit motorModeResult(!m_modbusIsAuto); // isManual = !isAuto
}

void DeviceCommandService::motorToggleSilentMode()
{
    if (proto() == "STM32-TCP-V4.0") {
        m_tcpIsSilent = !m_tcpIsSilent;
        QJsonObject cmd;
        cmd["action"] = m_tcpIsSilent ? 6 : 7;
        sendMotorTcpV4(cmd);
        // 通知界面更新按钮文字
        emit motorSilentResult(m_tcpIsSilent);
    }
}

void DeviceCommandService::motorSetCurrent(int run, int hold, int delay)
{
    // STM32-TCP-V4.0: action=9 SetCurrent (run_current/hold_current/iholddelay)
    if (proto() == "STM32-TCP-V4.0") {
        run   = qBound(1, run, 31);
        hold  = qBound(1, hold, 31);
        delay = qBound(0, delay, 15);
        QJsonObject cmd;
        cmd["action"] = 9;
        cmd["run_current"] = run;
        cmd["hold_current"] = hold;
        cmd["iholddelay"] = delay;
        sendMotorTcpV4(cmd);
        return;
    }

    // MODBUS-RTU 保留原有逻辑（单一运行电流值作为 mA）
    int ma = run;
    if (ma < 0) ma = 0;
    if (ma > 2000) ma = 2000;

    QByteArray pkt;
    pkt.append((char)0x01);
    pkt.append((char)0x06);
    pkt.append((char)0x00);
    pkt.append((char)0x1E);
    pkt.append((char)((ma >> 8) & 0xFF));
    pkt.append((char)(ma & 0xFF));

    quint16 crc = ModbusTransport::crc16(pkt);
    pkt.append((char)(crc & 0xFF));
    pkt.append((char)((crc >> 8) & 0xFF));

    // 根据通道配置下发
    sendModbus(pkt);

    // 延时50ms后发送固化指令
    QTimer::singleShot(50, this, [this]() {
        QByteArray savePkt = QByteArray::fromHex("010600178000580E");
        sendModbus(savePkt);
    });
}

QString DeviceCommandService::errorCodeToString(int code)
{
    switch (code) {
    case 0: return QStringLiteral("无错误");
    case 1: return QStringLiteral("超时(TIMEOUT)");
    case 2: return QStringLiteral("堵转(STALL)");
    case 3: return QStringLiteral("过流(OVERCURRENT)");
    case 4: return QStringLiteral("限位(LIMIT)");
    case 5: return QStringLiteral("驱动器故障(DRIVER_FAULT)");
    default: return QStringLiteral("未知错误(%1)").arg(code);
    }
}
