# T-JSON 架构索引

本文件是项目的单一架构事实来源（Single Source of Truth），基于当前代码现状整理。
配合 `docs/开发规范.md`（编码/协议/UI 约束）、`docs/需求说明书.md`（需求）、`docs/CHANGELOG.md`（变更历史）使用。

---

## 1. 项目概览

- **产品**：江苏莱瑟斯监控设备 Windows 上位机控制客户端（输出名 `LSSVideoManager.exe`）
- **技术栈**：C++17 / Qt 6.11 Widgets / CMake / FFmpeg（RTSP 解码）/ Qt WebEngine + WebChannel（地图）
- **核心能力**：TCP 8089 信令与状态、RTSP 拉流解码、云台/镜头控制（Pelco-D / VISCA / MODBUS-RTU / STM32-TCP）、AI 识别/跟踪、天地图叠加（Leaflet + 天地图）、多设备并行管理

## 2. 目录结构索引

```
T-JSON-V1.0/
├── CMakeLists.txt            # 构建定义（Qt6 + FFmpeg 静态链接）
├── src/
│   ├── main.cpp              # 入口：WebEngine 参数 + MainWindow
│   ├── ui/                   # 表现层
│   │   ├── main/MainPresenter.*        # MVP Presenter：UI 逻辑与设备调度
│   │   ├── views/
│   │   │   ├── mainwindow.*            # 主窗口 View（"哑巴视图"，业务经 Presenter）
│   │   │   ├── videowidget.*           # 视频渲染 + 框选坐标映射
│   │   │   ├── mapwidget.* / mapbridge.*   # 地图 View + JS 桥
│   │   │   ├── settingsdialog.*        # 参数设置对话框
│   │   │   └── cmdlogdialog.*          # 十六进制指令日志
│   │   └── components/
│   │       ├── DeviceTreeWidget.*      # 多设备树
│   │       └── VideoGridWidget.*       # 多设备视频宫格
│   ├── core/                 # 领域层（无 UI / 无平台依赖）
│   │   ├── DeviceState.h     # 纯数据模型
│   │   ├── EventBus.*        # 全局事件总线（14 类跨模块事件）
│   │   ├── GeoCalculator.*   # 地理算法（pixelToGps / 距离 / 航向 / 抽稀）
│   │   └── JsonFrameParser.* # 状态帧 JSON 解析（ZoomInfo/AiInfo/ImageSetting）
│   ├── service/              # 业务上下文层
│   │   ├── DeviceContext.*   # 单设备聚合根（TCP+RTSP+PTZ+State）
│   │   └── DeviceManager.*   # 多设备生命周期管理（单例）
│   └── infrastructure/       # 基础设施层
│       ├── tjsonframe.h      # 协议帧常量与 FrameType 枚举（Codec/Parser/Client 共享）
│       ├── tjsonframecodec.* # 帧编解码器（粘包/半包/重同步/组包）
│       ├── tjsonprotocolparser.* # 载荷解析（JSON/ACK/抓拍）
│       ├── tjsonclient.*     # TCP 8089 协议客户端（Socket/心跳/重连/分发）
│       ├── devicecontroller.*# 设备指令控制器（ProtocolBuilder）
│       ├── rtspthread.*      # FFmpeg RTSP 拉流解码线程
│       ├── ptzforwarder.*    # Pelco-D 串口服务器转发 + 角度偏移
│       ├── configmanager.*   # QSettings 配置持久化
│       └── s3uploader.*      # S3 上传（⚠️ 未参与构建，见 §8）
├── ui/*.ui                   # Qt Designer 布局（AUTOUIC）
├── resources/                # QSS、图标、地图前端（Leaflet + 天地图 内联）
├── tests/track_sim.py        # 轨迹模拟脚本
├── docs/                     # 开发规范 / 需求说明书 / CHANGELOG / 算法参考 / 指令速查
```

## 3. 分层依赖

```
┌──────────────────────────── 表现层 ────────────────────────────┐
│  ui/views/mainwindow (View)  ◄────  ui/main/MainPresenter (Presenter)   │
│  ui/views (Video/Map/Settings/CmdLog)  ui/components (DeviceTree/VideoGrid)│
└──────────────────────────────┬──────────────────────────────────┘
                               │ EventBus（横向解耦枢纽）
┌──────────────────────────── 业务层 ────────────────────────────┐
│  service/DeviceManager ──► service/DeviceContext（单设备聚合根）│
└──────────────┬──────────────────────────────────────────────────┘
               │
┌──────────────▼───────────────────── 基础设施层 ─────────────────┐
│ infrastructure/ tjsonframecodec │ tjsonprotocolparser │         │
│ tjsonclient │ rtspthread │ devicecontroller │                  │
│ ptzforwarder │ configmanager                                    │
└──────────────┬──────────────────────────────────────────────────┘
               │
┌──────────────▼───────────── 核心领域层 (core/) ─────────────────┐
│ DeviceState │ EventBus │ GeoCalculator │ JsonFrameParser         │
└─────────────────────────────────────────────────────────────────┘
```

**强制约束**：依赖单向。`core/` 不依赖任何外层；UI 绝不直接调用 `TJsonClient` 等底层，一律经 Presenter 与 EventBus 中转。

## 4. 模块职责表

| 模块 | 文件 | 职责 | 关键依赖 | 构建 |
|---|---|---|---|---|
| 入口 | `main.cpp` | WebEngine 调试端口 9999、Chromium flags、启动 MainWindow | — | ✅ |
| 主窗口 View | `ui/views/mainwindow.*` | 布局、按钮、视频/地图/仪表盘展示；事件回调更新 UI | Presenter, VideoGrid, MapWidget | ✅ |
| Presenter | `ui/main/MainPresenter.*` | 所有按钮业务逻辑、设备指令下发、EventBus 订阅、多设备切换 | MainWindow, DeviceManager, DeviceController | ✅ |
| 事件总线 | `core/EventBus.*` | 14 类跨模块事件（连接/状态/PTZ/图像/RTSP/ACK/AI） | DeviceState | ✅ |
| 数据模型 | `core/DeviceState.h` | 设备运行时纯数据（PTZ/镜头/位置/AI/图像参数） | — | ✅ |
| 地理算法 | `core/GeoCalculator.*` | haversine、bearing、pixelToGps、目标测距、轨迹抽稀判定 | — | ✅ |
| 帧解析 | `core/JsonFrameParser.*` | ZoomInfoData / AiInfoData / ImageSettingData 提取 | — | ✅ |
| 设备上下文 | `service/DeviceContext.*` | 单设备聚合根：持有 TCP/RTSP/PTZ/State，管理定时器 | TJsonClient, RtspThread, DeviceController, PtzForwarder | ✅ |
| 设备管理器 | `service/DeviceManager.*` | 多设备增删查，全局单例 | DeviceContext | ✅ |
| TCP 客户端 | `infrastructure/tjsonclient.*` | Socket、连接/断开、心跳 10s、指数退避重连、事件分发 | QTcpSocket, TJsonFrameCodec, TJsonProtocolParser | ✅ |
| 帧编解码 | `infrastructure/tjsonframecodec.*` | 帧头识别、长度解析、粘包/半包、重同步、发送组包 | TJsonFrame, QByteArray | ✅ |
| 载荷解析 | `infrastructure/tjsonprotocolparser.*` | JSON 状态帧 / ACK / 抓拍帧解析（校验和与帧尾校验） | TJsonFrame, QJsonObject | ✅ |
| 指令控制器 | `infrastructure/devicecontroller.*` | ProtocolBuilder（Pelco-D/VISCA）、云台/镜头/预置位/雨刷、电机串口/TCP | TJsonClient, ConfigManager | ✅ |
| RTSP 线程 | `infrastructure/rtspthread.*` | FFmpeg 拉流解码、16:9 渲染、断线重连、32 字节对齐缓冲 | FFmpeg | ✅ |
| PTZ 转发 | `infrastructure/ptzforwarder.*` | Pelco-D 串口服务器双向转发、角度偏移、零点标定 | QTcpSocket/Server | ✅ |
| 配置 | `infrastructure/configmanager.*` | PTZ/镜头/相机/电机配置，QSettings 持久化，FOV 距离常量 | QSettings | ✅ |
| 地图 View | `ui/views/mapwidget.*` | WebEngine 天地图、FOV 扇形、目标/轨迹、脏标记批量刷新 | MapBridge, WebChannel | ✅ |
| 地图桥 | `ui/views/mapbridge.*` | C++ ↔ JS 双向桥接（初始化/点击/缩放） | QWebChannel | ✅ |
| 视频控件 | `ui/views/videowidget.*` | 帧渲染、16:9 锁定、框选区域坐标映射 | QPainter | ✅ |
| 设备树 | `ui/components/DeviceTreeWidget.*` | 多设备树增删改、JSON 持久化 | QTreeView | ✅ |
| 视频宫格 | `ui/components/VideoGridWidget.*` | 多设备 VideoWidget 宫格布局 | VideoWidget | ✅ |
| 设置对话框 | `ui/views/settingsdialog.*` | 串口/协议/相机参数编辑 | ConfigManager | ✅ |
| 指令日志 | `ui/views/cmdlogdialog.*` | 串口 HEX 收发日志窗口 | — | ✅ |
| S3 上传 | `infrastructure/s3uploader.*` | AWS S3 上传（`ENABLE_S3_UPLOAD` 宏 + AWS SDK） | aws-sdk-cpp | ❌ |

## 5. 关键数据流

### 5.1 状态上行（设备 → UI）

```
设备 → TCP 8089 → TJsonClient（粘包解析）
  → DeviceContext → EventBus.postDeviceStateUpdated / postJsonReceived 等
  → MainPresenter（订阅 sig* 信号）→ MainWindow 刷新仪表盘/地图/视频
```
### 5.2 控制下行（UI → 设备）

```
MainWindow 按钮 → MainPresenter.onXxx()
  → DeviceController（ProtocolBuilder 组包）
  → TJsonClient.sendSerialCmd / sendJsonCmd → TCP 8089 → 设备
```

### 5.3 轨迹抽稀

```
AIInfo(40ms) → GeoCalculator.shouldPlotTrackPoint（3m 死区 / 20m 强制 / 2.5s 心跳 / 15° 航向）
  → MapWidget.appendTrackPoint → 200ms 批量 runJS → 前端 FIFO(2000 点) → Leaflet polyline
```
详见 `docs/轨迹点抽稀算法.md`。

## 6. 协议速查

| 项 | 值 |
|---|---|
| 信令/状态端口 | TCP 8089（JSON + 图像抓拍） |
| 独立大图 | 48M-Tofu7 TCP 8091 |
| 视频流 | RTSP 554（由 RtspThread/FFmpeg 处理） |
| 标准帧头 | `0xEC 0x91` [帧类型 1B] [长度 4B 大端] [负载] |
| 抓拍帧头 | `0xEB 0x92 0x04` [位置 8B] [图像] [校验] `0xFB 0x92` |
| 心跳 | `0x11` 双向，10s 周期，4 字节无负载 |
| ACK | `0x12` 双向，负载 2B：`0x00 00`正常 / `0x00 01`不完整 / `0x00 02`内容错误 |

主要帧类型（`tjsonframe.h` `enum class FrameType`）：Status 0x01 / Control 0x03 / ImageSnap 0x04 / QueryImageParams 0x05 / SetAreaDot 0x06 / SetDisplayMode 0x07 / SetAlgoModel 0x08 / SetCaptureState 0x09 / SetDigitalZoom 0x0A / SetPosReset 0x0B / QueryTofu7 0x0C-0x0F / Heartbeat 0x11 / Ack 0x12 / SetLocation 0x20。

**电机协议**：Pelco-D 指令包见 `docs/指令.md`；VISCA 变倍/变焦；MODBUS-RTU（9600-8-N-1）；STM32-TCP-V4.0（`5A A5 02+长度+序号+JSON`）。通道与协议经 `ConfigManager` 配置，`DeviceController` 三选一分发。

## 7. 构建与运行

- **环境**：MSVC2022 x64 Debug，`vcvars64.bat` + CMake + jom（QtCreator 构建目录 `build/Desktop_Qt_6_11_1_MSVC2022_64bit_Debug`）
- **依赖**：Qt6（Core/Widgets/Network/Gui/WebEngineWidgets/WebChannel/SerialPort）+ FFmpeg（`thirdparty/ffmpeg` 静态 `.lib`，DLL 构建后拷贝）+ AWS SDK（仅 S3，当前未启用）
- **输出**：`LSSVideoManager.exe`
- **调试**：`http://localhost:9999`（WebEngine 远程调试地图页面）
- **辅助脚本**：`check_main.py` / `check_main2.py` / `check_dm.py` / `check_dm_cpp.py`（代码复查用）

## 8. 已知架构债（现状 vs 重构规则）

| 债务 | 位置 | 说明 |
|---|---|---|
| 超大文件 | `ui/views/mainwindow.cpp`(约 1400 行)、`ui/main/MainPresenter.cpp`(约 1290 行) | 违反"方法超 80 行拆分"规则 |
| 死代码 | `infrastructure/s3uploader.*` + `thirdparty/aws-sdk-cpp`(~1GB) | 未进 CMakeLists，`ENABLE_S3_UPLOAD` 无定义 |
| 生命周期风险 | `service/DeviceContext.*`、`infrastructure/rtspthread.*` | 设备销毁、RTSP 停止与后台线程退出需要持续验证 |
| 多设备收口 | `ui/main/MainPresenter.cpp`、`ui/views/mainwindow.cpp` | 当前设备切换与视频控件绑定仍需继续收敛，避免业务依赖默认设备 |
| DeviceController 过重 | `infrastructure/devicecontroller.*` | 仍同时承担 Pelco-D/VISCA/MODBUS-RTU/STM32-TCP 与串口/TCP 传输，待 5.2 拆分（`sendMotorTcpV4` 的 `waitForConnected(500)` 可能阻塞 UI 线程） |
| DeviceContext 暴露底层 | `service/DeviceContext.*` | 仍公开 `tcpClient()/motorController()/videoStream()/ptzForwarder()`，待 5.3 收口为业务 API |

> ✅ 已解决：`MainPresenter` 过渡期访问器 `motorController()/tcpClient()/videoStream()/ptzForwarder()` 已移出公有接口（降为私有）；`mainwindow.cpp` PTZ 方向/镜头按钮不再直连底层，全部经 Presenter 业务方法。View 已不再直取底层组件。
> ✅ 已解决：`TJsonClient` 职责过重 —— 帧编解码已拆为 `TJsonFrameCodec`，载荷解析已拆为 `TJsonProtocolParser`（阶段 5.1）。

## 9. 文档导航

| 文档 | 适用场景 |
|---|---|
| `docs/开发规范.md` | 编写代码前必读：编码/协议/UI 强制约束 |
| `docs/需求说明书.md` | 需求分析与模块说明 |
| `docs/CHANGELOG.md` | 变更历史、已知问题、下一步计划 |
| `docs/轨迹点抽稀算法.md` | 地图轨迹抽稀算法细节 |
| `docs/指令.md` | Pelco-D 云台/镜头/预置位指令速查 |
