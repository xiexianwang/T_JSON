// ============================================================
// 文件: devicecommandservice.cpp
// 描述: 电机指令服务实现。
// ============================================================

#include "devicecommandservice.h"
#include "infrastructure/configmanager.h"
#include "infrastructure/modbustransport.h"
#include "infrastructure/stm32tcptransport.h"
#include <QTimer>
#include <QJsonDocument>

DeviceCommandService::DeviceCommandService(ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_cfg(cfg)
{
    m_modbus = new ModbusTransport(this);
    m_tcp = new Stm32TcpTransport(this);

    // MODBUS 串口接收：识别模式查询应答 (data[1]==0x03)
    connect(m_modbus, &ModbusTransport::dataReceived, this, [this](const QByteArray& data) {
        if (data.size() >= 5 && static_cast<quint8>(data.at(1)) == 0x03) {
            bool isManual = (static_cast<quint8>(data.at(3)) == 0x01);
            emit motorModeResult(isManual);
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
    m_tcp->open(m_cfg->motorTcpIp(), m_cfg->motorTcpPort());
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
    if (m_cfg->motorCommandChannel() == "串口") {
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
    if (m_cfg->motorProtocol() == "MODBUS-RTU") {
        sendModbus(QByteArray::fromHex("01060037001039C8"));
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 5;
        sendMotorTcpV4(cmd);
    } else {
        sendPelcoDWiper(QByteArray::fromHex("FF01000900010B"));
    }
}

void DeviceCommandService::motorStop()
{
    if (m_cfg->motorProtocol() == "MODBUS-RTU") {
        sendModbus(QByteArray::fromHex("0106003800000807"));
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 8;
        sendMotorTcpV4(cmd);
    } else {
        sendPelcoDWiper(QByteArray::fromHex("FF01000B00010D"));
    }
}

void DeviceCommandService::motorWiperStop()
{
    if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
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
    if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 3;
        cmd["target_pos"] = 50000;
        cmd["speed"] = 8000;
        sendMotorTcpV4(cmd);
        return;
    }
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("01060037008039A4"));
}

void DeviceCommandService::motorJogRight()
{
    if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 4;
        cmd["target_pos"] = 50000;
        cmd["speed"] = 8000;
        sendMotorTcpV4(cmd);
        return;
    }
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("01060037004039F4"));
}

void DeviceCommandService::motorZeroCalib()
{
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("0106003A00016807"));
}

void DeviceCommandService::motorReturnZero()
{
    if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["action"] = 2;
        sendMotorTcpV4(cmd);
        return;
    }
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("01060037000439C7"));
}

void DeviceCommandService::motorCheckMode()
{
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("010301B10001D5D1"));
}

void DeviceCommandService::motorToggleMode()
{
    if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        m_tcpIsAuto = !m_tcpIsAuto;
        QJsonObject cmd;
        cmd["action"] = m_tcpIsAuto ? 11 : 10;
        sendMotorTcpV4(cmd);
        // 通知界面更新模式结果
        emit motorModeResult(!m_tcpIsAuto);
        return;
    }
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;

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
    if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        m_tcpIsSilent = !m_tcpIsSilent;
        QJsonObject cmd;
        cmd["action"] = m_tcpIsSilent ? 6 : 7;
        sendMotorTcpV4(cmd);
        // 通知界面更新按钮文字
        emit motorSilentResult(m_tcpIsSilent);
    }
}

void DeviceCommandService::motorSetCurrent(int ma)
{
    if (ma < 0) ma = 0;
    if (ma > 2000) ma = 2000;

    // 构造设置电流指令
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
