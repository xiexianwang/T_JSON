# T-JSON Protocol Implementation Coding Rules

## 1. 技术栈要求 (Tech Stack)
- **编程语言:** C++17 或更高版本
- **核心框架:** Qt 6.x (Core, Network模块为主)
- **构建系统:** CMake

## 2. 协议规范说明 (Protocol Specifications)
- **通信架构:** 客户端 (Client/Computer) 连接到 服务端 (AI Camera/Server)。
- **通信端口:** 默认使用 TCP 端口 `8089`进行信令和状态通信。
- **字节序:** 默认遵循网络字节序（即 **大端模式 Big-Endian**，例如 `0xEC 0x91` 标准帧的 `4Byte` 长度字段）。**特例注意：** 仅在 48M-Tofu7 图像分包等特定帧头中明确规定了使用 **小端模式 (Little-Endian)**。开发时必须根据数据帧类型分别处理。
- **标准帧结构 (7字节包头):**
  - `Byte 1`: `0xEC` (标识码 1)
  - `Byte 2`: `0x91` (标识码 2)
  - `Byte 3`: `Frame Type` (帧类型，如0x01, 0x03, 0x11等)
  - `Byte 4-7`: `Payload Length N` (帧内容长度，4 Bytes，uint32)
  - `Byte 8...`: `Payload` (JSON 数据或纯二进制数据)

## 3. Qt 开发规范 (Qt-Specific Guidelines)
- **网络通信:** 必须使用 `QTcpSocket`。严禁使用阻塞模式，所有网络操作需采用异步信号槽机制 (Signals and Slots) 处理 (`readyRead`, `connected`, `disconnected`, `errorOccurred`)。
- **TCP 粘包/半包处理:**
  - 由于 TCP 是流式协议，不能假设每次 `readyRead` 都会读取到一个完整的协议帧。
  - 必须维护一个接收缓冲区 (如 `QByteArray buffer`)。
  - 解析逻辑：首先检查缓冲区是否满 7 字节。如果满足，解析出长度 `N`。接着检查缓冲区是否满足 `7 + N` 字节。只有完整时才能提取包并处理，同时从缓冲区移除该帧数据。
- **JSON 处理:** 使用 `QJsonDocument`, `QJsonObject`, `QJsonArray` 进行 JSON 格式的解析和生成。
- **内存与性能:** 图片接收等涉及大量二进制数据的操作，应避免不必要的深拷贝。

## 4. 保活与重连 (Heartbeat and Keep-Alive)
- **心跳机制 (`0x11`):** 客户端必须周期性向服务端发送心跳（建议每 5 秒一次）。如果 15 秒内未收到心跳回复，服务端会断开连接；客户端如果 15 秒没收到回复，也需主动判定断线并重连。
- **ACK 机制 (`0x12`):** 双向通信中，需要响应 ACK 确认（心跳除外，心跳回心跳包）。

## 5. 代码结构设计 (Code Structure)
- **模块分离:** 将网络通信层 (`TJsonClient`) 与 UI 层严格分离。通过信号将接收到的解析后数据传递给 UI。
- **枚举定义:** 使用 `enum class` 强类型枚举定义帧类型和工作模式，例如：
  ```cpp
  enum class FrameType : quint8 {
      Status = 0x01,       // 状态帧 (S->C)
      Control = 0x03,      // 控制指令 (C->S)
      Heartbeat = 0x11,    // 心跳 (双向)
      Ack = 0x12           // ACK (双向)
  };
  ```
