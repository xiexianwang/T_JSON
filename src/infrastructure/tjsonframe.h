// ============================================================
// 文件: tjsonframe.h
// 描述: T-JSON 协议帧基础定义。帧类型枚举与帧布局常量，供
//       TJsonFrameCodec（编解码）、TJsonProtocolParser（载荷解析）
//       与 TJsonClient（传输）共同引用。本文件无 Qt Object 依赖。
// ============================================================

#ifndef TJSONFRAME_H
#define TJSONFRAME_H

#include <QtGlobal>

// 帧类型枚举：定义 T-JSON 协议中所有指令帧的类型字节
// 每个值对应帧头的第三个字节，用于区分不同功能
enum class FrameType : quint8 {
    Status = 0x01,              // 状态上报帧（设备 -> 客户端）
    Control = 0x03,             // 通用控制帧
    ImageSnap = 0x04,           // 图像抓拍帧（特殊帧头 0xEB 0x92）
    QueryImageParams = 0x05,    // 查询图像参数
    SetAreaDot = 0x06,          // 设置区域/点位
    SetDisplayMode = 0x07,      // 设置显示模式（画中画等）
    SetAlgoModel = 0x08,        // 设置算法模型
    SetCaptureState = 0x09,     // 设置抓拍上传状态
    SetDigitalZoom = 0x0A,      // 设置数字变焦
    SetPosReset = 0x0B,         // 设置位置归零
    QueryTofu7Params = 0x0C,    // 查询 Tofu7 参数
    SetTofu7Params = 0x0D,      // 设置 Tofu7 参数
    QueryTofu7Ignore = 0x0E,    // 查询 Tofu7 忽略区域
    SetTofu7Ignore = 0x0F,      // 设置 Tofu7 忽略区域
    Heartbeat = 0x11,           // 心跳帧（双向保活）
    Ack = 0x12,                 // 确认应答帧（含状态码）
    SetLocation = 0x20          // 设置 GPS 经纬度位置
};

// T-JSON 帧布局常量
namespace TJsonFrame {

    // ---- 标准帧 (0xEC 0x91) ----
    static constexpr quint8 kHeaderB1 = 0xEC;               // 帧头字节 1
    static constexpr quint8 kHeaderB2 = 0x91;               // 帧头字节 2
    static constexpr int kHeaderSize = 7;                   // 2(帧头)+1(类型)+4(长度)
    static constexpr quint32 kMaxJsonLength = 10 * 1024 * 1024;  // JSON 帧最大 10 MB

    // ---- 图像抓拍帧 (0xEB 0x92) ----
    // 帧结构: [EB][92][04][jpegSize(4B)][left(2B)][top(2B)][width(2B)][height(2B)]
    //         [jpegData(NB)][checksum(1B)][FB][92]
    static constexpr quint8 kSnapHeaderB1 = 0xEB;           // 抓拍帧头字节 1
    static constexpr quint8 kSnapHeaderB2 = 0x92;           // 抓拍帧头字节 2
    static constexpr quint8 kSnapType = 0x04;               // 抓拍帧类型字节
    static constexpr int kSnapHeaderSize = 18;              // 固定帧头大小（不含 JPEG 数据）
    static constexpr quint32 kMaxJpegLength = 50 * 1024 * 1024;  // JPEG 帧最大 50 MB
    static constexpr int kSnapCoordOffset = 7;              // 画面区域坐标起点（left/top/width/height 各 2B）
    static constexpr int kSnapJpegOffset = 15;              // JPEG 数据起点

} // namespace TJsonFrame

#endif // TJSONFRAME_H
