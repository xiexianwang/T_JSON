// ============================================================
// 文件: devicecommandservice.h
// 描述: 电机指令服务。根据配置选择传输协议（MODBUS-RTU 串口 /
//       STM32-TCP-V4.0 / Pelco-D 透传），编排雨刷电机的启动、
//       停止、点动、校准、模式切换等业务指令。底层字节收发委托
//       给 ModbusTransport / Stm32TcpTransport，Pelco-D 指令经
//       注入的透传回调发出。
// ============================================================

#ifndef DEVICECOMMANDSERVICE_H
#define DEVICECOMMANDSERVICE_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QJsonObject>
#include <functional>

class ConfigManager;
class ModbusTransport;
class Stm32TcpTransport;

class DeviceCommandService : public QObject
{
    Q_OBJECT
public:
    explicit DeviceCommandService(ConfigManager* cfg, QObject *parent = nullptr);

    // 注入 Pelco-D 透传发送回调（由 DeviceController 接 TJsonClient 串口透传）
    void setPelcoDSender(const std::function<void(const QByteArray&)>& sender);

    // ================= 电机串口管理 (MODBUS-RTU) =================
    bool openMotorSerial(const QString& portName);
    void closeMotorSerial();
    bool isMotorSerialOpen() const;

    // ================= 电机 TCP 管理 (STM32-TCP-V4.0) =================
    void openMotorTcp();
    void closeMotorTcp();
    bool isMotorTcpOpen() const;

    // ================= 雨刷电机控制 =================
    void motorStart();                  // 启动
    void motorStop();                   // 停止
    void motorJogLeft();                // 左转(JOG-)
    void motorJogRight();               // 右转(JOG+)
    void motorZeroCalib();              // 零点校准
    void motorReturnZero();             // 回到绝对位置零点
    void motorCheckMode();              // 查询当前模式（手动/自动）
    void motorToggleMode();             // 切换模式（手动↔自动）
    void motorToggleSilentMode();       // 切换静音/狂暴模式
    void motorSetCurrent(int ma);       // 设置电机电流并固化

signals:
    void commandSent(const QString& serialType, const QByteArray& data);
    void motorModeResult(bool isManual);    // true=手动, false=自动
    void motorSilentResult(bool isSilent);  // true=静音, false=狂暴
    void motorSerialError(const QString& msg);

private:
    ConfigManager* m_cfg;
    ModbusTransport* m_modbus = nullptr;
    Stm32TcpTransport* m_tcp = nullptr;
    std::function<void(const QByteArray&)> m_pelcoDSender;

    bool m_modbusIsAuto = false;        // Modbus 模式记录
    bool m_tcpIsAuto = false;           // 记录当前是否是自动模式，用于 ToggleMode
    bool m_tcpIsSilent = false;         // 记录当前是否是静音模式

    void sendModbus(const QByteArray& pkt);
    void sendPelcoDWiper(const QByteArray& pkt);
    void sendMotorTcpV4(const QJsonObject& json);
};

#endif // DEVICECOMMANDSERVICE_H
