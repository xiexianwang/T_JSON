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

    // ================= 雨刷电机控制 (STM32-TCP-V4.0 action) =================
    void motorStart();                  // action=5: Continuous Swiping
    void motorStop();                   // action=8: Stop Only
    void motorWiperStop();              // action=8 + action=2(50ms): 雨刷关闭(停止+回零)
    void motorJogLeft();                // action=3: Left(target_pos, speed)
    void motorJogRight();               // action=4: Right(target_pos, speed)
    void motorZeroCalib();              // 零点校准(MODBUS-RTU 专用)
    void motorReturnZero();             // action=2: Home(回零)
    void motorCheckMode();              // 查询当前模式(MODBUS-RTU 专用)
    void motorToggleMode();             // action=10: Switch to Manual / action=11: Switch to Auto
    void motorToggleSilentMode();       // action=6: Silent Mode ON / action=7: Silent Mode OFF
    void motorSetCurrent(int ma);       // 设置电机电流并固化(MODBUS-RTU 专用)

signals:
    void commandSent(const QString& serialType, const QByteArray& data);
    void motorModeResult(bool isManual);    // true=手动, false=自动
    void motorSilentResult(bool isSilent);  // true=静音, false=狂暴
    void motorSerialError(const QString& msg);
    void motorTcpError(const QString& msg);

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
    static QString errorCodeToString(int code);
};

#endif // DEVICECOMMANDSERVICE_H
