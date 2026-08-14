// ============================================================
// 文件: devicecontroller.h
// 描述: 设备控制器。上层业务逻辑与底层通信协议之间的中间层，
//       负责协议选择与业务编排：
//       - JSON 设备指令经 TJsonClient 下发（工作模式/算法/显示等）
//       - PTZ/镜头/预置位经 PelcoDProtocol/ViscaProtocol 组包后
//         通过串口透传通道发送
//       - 电机指令委托给 DeviceCommandService（MODBUS/STM32/Pelco-D）
// ============================================================

#ifndef DEVICECONTROLLER_H
#define DEVICECONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QJsonObject>
#include "infrastructure/tjsonclient.h"
#include "infrastructure/configmanager.h"
#include "infrastructure/pelcodprotocol.h"
#include "infrastructure/viscaprotocol.h"

class DeviceCommandService;

// 设备控制器类
// 协调 TJsonClient（网络通信）、ConfigManager（配置参数）与
// DeviceCommandService（电机指令），封装上层业务逻辑为对设备的
// 各种控制操作。自身不再直接持有串口/TCP 底层对象。
class DeviceController : public QObject
{
    Q_OBJECT
public:
    // PipShow 映射表：combo 索引 → 设备实际值
    static constexpr int kPipShowValues[] = {0, 1, 2, 3, 16};
    static constexpr int kPipShowCount = 5;
    static int pipShowToComboIndex(int pipShow) {
        for (int i = 0; i < kPipShowCount; i++)
            if (kPipShowValues[i] == pipShow) return i;
        // 设备回传值为 combo 索引（模式 4 设 16 回 4）
        if (pipShow >= 0 && pipShow < kPipShowCount) return pipShow;
        return 0;
    }

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
    void ptzMoveTo(double pan, double tilt);// 云台转动到绝对角度
    void ptzSetZero();                      // 云台水平零点标定

    // ================= 框选跟踪 =================
    void setBoxTrack(int centerX, int centerY, int width, int height);  // 设置跟踪框
    void setPointTrack(int centerX, int centerY);                       // 点选跟踪

    // ================= 预置位控制 (Pelco-D) =================
    void setPreset(int preset);             // 设置预置位
    void callPreset(int preset);            // 调用预置位
    void delPreset(int preset);             // 删除预置位

    // ================= 附加功能开关 =================
    void setDigitalZoom(bool enable);       // 数字变焦开关
    void setAutoZoom(bool enable);          // 自动变焦开关
    void setCaptureUpload(bool enable);     // 抓拍上传开关
    void posReset(bool enable);             // 位置归零
    void setWiper(bool enable);             // 雨刷开关

    // ================= 雨刷电机控制（委托 DeviceCommandService） =================
    void motorStart();                      // 启动
    void motorStop();                       // 停止
    void motorJogLeft();                    // 左转(JOG-)
    void motorJogRight();                   // 右转(JOG+)
    void motorZeroCalib();                  // 零点校准
    void motorReturnZero();                 // 回到绝对位置零点
    void motorCheckMode();                  // 查询当前模式（手动/自动）
    void motorToggleMode();                 // 切换模式（手动↔自动）
    void motorToggleSilentMode();           // 切换静音/狂暴模式
    void motorSetCurrent(int ma);           // 设置电机电流并固化

    // ================= 镜头控制 (VISCA / Pelco-D) =================
    // target: 0=可见光(VISCA), 1=红外(Pelco-D)
    void lensZoomIn(int target);            // 变倍放大
    void lensZoomOut(int target);           // 变倍缩小
    void lensFocusIn(int target);           // 变焦拉近
    void lensFocusOut(int target);          // 变焦拉远
    void lensStop();                        // 停止镜头运动

    // ================= 串口透传通用网关 =================
    void sendTransparentData(const QString& serialType, const QByteArray& data);  // 通用透传

    // ================= 电机通道管理（委托 DeviceCommandService） =================
    bool openMotorSerial(const QString& portName);
    void closeMotorSerial();
    bool isMotorSerialOpen() const;
    void openMotorTcp();
    void closeMotorTcp();
    bool isMotorTcpOpen() const;

signals:
    void commandSent(const QString& serialType, const QByteArray& data);  // 指令已发送通知
    void motorModeResult(bool isManual);  // 电机模式查询结果: true=手动, false=自动
    void motorSilentResult(bool isSilent); // 静音模式切换结果: true=静音, false=狂暴
    void motorSerialError(const QString& msg);

private:
    TJsonClient* m_client;          // 网络客户端（非拥有指针）
    ConfigManager* m_cfg;           // 配置管理器（非拥有指针）
    DeviceCommandService* m_motorService;   // 电机指令服务（子对象）

    int m_lastLensTarget = 0;       // 最近一次镜头操作的目标（0=可见光, 1=红外）
    bool m_lastLensIsZoom = true;   // 最近一次镜头操作是否为变倍（true=变倍, false=变焦）
};

#endif // DEVICECONTROLLER_H
