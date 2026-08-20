# T-JSON 变更日志（CHANGELOG）

> 本文件整合原 `docs/项目日志.md`（开发流水）、`docs/V1.1-beta-版本说明.md`（版本发布）与 `项目重构规则.md` 中的"最近优化记录"，按日期倒序排列。
> 最新记录在顶部。未解决事项集中记录于 §「遗留问题」。

---

## 2026-08-14 · 阶段 8：清理死代码（删除 S3Uploader + AWS SDK）

### 解决的问题 / 实现功能

- **删除 `s3uploader.h` / `s3uploader.cpp`**：原文件受 `ENABLE_S3_UPLOAD` 宏保护，未定义时为空桩，不参与构建，不链接 AWS SDK。已删除。
- **删除 `thirdparty/aws-sdk-cpp`**：约 1GB 的 AWS SDK 源码与构建系统，从未纳入 CMakeLists 构建。已删除。
- **更新 ARCHITECTURE.md**：移除目录树 `s3uploader.*` 条目、§4 模块职责表 S3 行、§7 依赖中 AWS SDK 引用、§8 债务表死代码行。
- **构建验证**：MSVC2022 x64 Debug 构建通过，7 个测试全部通过。

### 阶段 8 验收核对

| 验收项 | 结论 |
|---|---|
| `s3uploader.h` / `s3uploader.cpp` 已删除 | ✅ |
| `thirdparty/aws-sdk-cpp` 已删除 | ✅ 节省约 1GB 磁盘 |
| src/ 中无残留引用 | ✅ grep 确认 |
| 主程序构建通过 | ✅ 产物 `LSSVideoManager.exe` |
| 7 个测试全部通过 | ✅ 7/7 Passed |
| ARCHITECTURE.md 已同步更新 | ✅ |

### 核心改动

| 文件 | 改动内容 |
|---|---|
| `src/infrastructure/s3uploader.h` | 删除 |
| `src/infrastructure/s3uploader.cpp` | 删除 |
| `thirdparty/aws-sdk-cpp/` | 整目录删除 |
| `ARCHITECTURE.md` | 移除 S3 相关条目与债务记录 |

### 遗留问题

- **超大文件拆分**：`mainwindow.cpp`(约 1530行)、`MainPresenter.cpp`(约 1347行) 待后续轮次。
- **实机验收待办**：双设备并行 PTZ/镜头/框选/电机信号切换需实机确认。

---

### 解决的问题 / 实现功能

- **移除 Presenter 对具体 Widget 类型的直接依赖**：`MainPresenter.cpp` 原直接 `#include "ui/views/mapwidget.h"` / `"ui/views/videowidget.h"` / `"ui/components/VideoGridWidget.h"`，并直接调用 `VideoWidget::setFrame/clearFrame/setSelectionEnabled` 和 `MapWidget` 的 8 个方法；现已全部改为通过 `IMainView` 抽象接口调用。
- **扩展 `IMainView` 接口**：新增 11 个纯虚方法——`setVideoFrame/clearVideoFrame/setVideoSelectionEnabled`（视频网格）、`mapClearAllTracks/mapUpdateTargetMarkers/mapClearFov/mapAppendTrackPoint/mapSetDevicePosition/mapSetVisFov/mapSetIrFov/mapSetDeviceInfo`（地图操作）。移除旧的 `videoWidget()` / `mapWidget()` 返回具体类型的接口。
- **MainWindow 实现新接口**：在 `mainwindow.cpp` 中实现 11 个新方法，内部委托给 `m_videoGrid` / `m_mapWidget`。
- **构建验证**：MSVC2022 x64 Debug 构建通过，7 个测试全部通过。

### 阶段 7 验收核对

| 验收项 | 结论 |
|---|---|
| `MainPresenter.cpp` 不再 include `mapwidget.h` / `videowidget.h` / `VideoGridWidget.h` | ✅ 3 个 include 已移除 |
| Presenter 不直接调用 `VideoWidget::setFrame/clearFrame/setSelectionEnabled` | ✅ 改经 `IMainView` 接口 |
| Presenter 不直接调用 `MapWidget` 的任何方法（18 处） | ✅ 改经 `IMainView` 接口 |
| `IMainView` 不再暴露 `videoWidget()` / `mapWidget()` 具体类型 | ✅ 已移除 |
| 主程序构建通过 | ✅ 产物 `LSSVideoManager.exe` |
| 7 个测试全部通过 | ✅ 7/7 Passed |

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `src/ui/main/IMainView.h` | 新增 11 个纯虚方法（视频 3 个 + 地图 8 个）；移除 `videoWidget()` / `mapWidget()`；移除 `VideoWidget` / `MapWidget` 前置声明 |
| `src/ui/main/MainPresenter.cpp` | 移除 3 个 Widget include；18 处 `map->` 调用改为 `m_view->mapXxx()`；3 处 `videoWidget()` 调用改为 `m_view->setVideoFrame/clearVideoFrame/setVideoSelectionEnabled` |
| `src/ui/views/mainwindow.h` | 新增 11 个 override 方法声明；移除旧 `videoWidget()` / `mapWidget()` 声明 |
| `src/ui/views/mainwindow.cpp` | 新增 11 个方法实现（委托给 `m_videoGrid` / `m_mapWidget`）；移除旧实现 |

### 遗留问题

- **死代码清理**：`s3uploader.*` + `thirdparty/aws-sdk-cpp` 待确认是否保留。
- **超大文件拆分**：`mainwindow.cpp`(约 1530行)、`MainPresenter.cpp`(约 1347行) 待后续轮次。
- **实机验收待办**：双设备并行 PTZ/镜头/框选/电机信号切换需实机确认。

---

### 解决的问题 / 实现功能

- **新增 `test_modbustransport.cpp`**：覆盖 `ModbusTransport::crc16` 静态方法（MODBUS-RTU 标准多项式 0xA001），含已知向量、单字节、空数据三个用例；验证构造后 `isOpen()=false`。需链接 `Qt::SerialPort`。
- **新增 `test_stm32tcptransport.cpp`**：覆盖 `Stm32TcpTransport` 初始状态（`seq()=0`、`isOpen()=false`）与未连接时 `send()` 返回 `false`。需链接 `Qt::Network`。
- **修复多设备电机信号绑定**：`MainPresenter` 构造函数中 `motorModeResult/motorSerialError/motorSilentResult/commandSent` 原固定绑定默认设备；现改为 `connectDeviceSignals()/disconnectDeviceSignals()` 管理，`onDeviceDoubleClicked` 切换设备时自动断开旧连接、绑定新设备。
- **构建验证**：MSVC2022 x64 Debug 构建通过，7 个测试全部通过。

### 阶段 6 第二轮验收核对

| 验收项 | 结论 |
|---|---|
| 7 个测试全部通过 | ✅ 7/7 Passed |
| ModbusTransport CRC16 纯逻辑测试 | ✅ 标准向量 + 单字节 + 空数据 |
| Stm32TcpTransport 初始状态 / 未连接 send | ✅ |
| 多设备切换后电机信号跟随当前设备 | ✅ 动态断开/重连 |
| 主程序构建不回归 | ✅ 产物 `LSSVideoManager.exe` |

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `tests/CMakeLists.txt` | 新增 2 个测试目标 + `Qt::SerialPort` / `Qt::Network` 链接 |
| `tests/test_modbustransport.cpp` | 新增：CRC16 单元测试 |
| `tests/test_stm32tcptransport.cpp` | 新增：STM32-TCP 初始状态测试 |
| `src/ui/main/MainPresenter.h` | 新增 `connectDeviceSignals/disconnectDeviceSignals` + 4 个 `QMetaObject::Connection` 成员 |
| `src/ui/main/MainPresenter.cpp` | 构造函数改用 `connectDeviceSignals`；新增信号管理方法实现；`onDeviceDoubleClicked` 切换时重连信号 |

### 遗留问题

- **RtspThread 线程等待**：已确认 `closeStream()` 正确调用 `wait(3000)`，无需修复（债务已清除）。
- **Presenter/View 接口收口**：Presenter 仍直接操作 `VideoWidget`/`VideoGridWidget`/`MapWidget` 具体类型，待后续收口。
- **死代码清理**：`s3uploader.*` + `thirdparty/aws-sdk-cpp` 待确认是否保留。
- **超大文件拆分**：`mainwindow.cpp`(1514行)、`MainPresenter.cpp`(约 1340行) 待后续轮次。
- **实机验收待办**：双设备并行 PTZ/镜头/框选/电机信号切换需实机确认。

---

### 解决的问题 / 实现功能

- **建立 CTest 测试框架**：根 `CMakeLists.txt` 增加 `include(CTest)` + `enable_testing()` + `add_subdirectory(tests)`，`-DBUILD_TESTING=OFF` 可完全跳过测试；`find_package(Qt6)` 增加 `Test` 组件。
- **新增 `tests/CMakeLists.txt`**：公共函数 `tjson_add_test()` 定义 5 个独立 Qt Test 可执行文件（`QTEST_GUILESS_MAIN`，无 GUI），各自仅链接最小纯逻辑源文件 + `Qt::Core` `Qt::Test`，不链接 WebEngine/FFmpeg，构建轻量、可离线运行。
- **新增 5 个单元测试文件**（`tests/`）：
  - `test_tjsonframecodec.cpp`：标准帧/心跳帧组包逐字节校验、单帧切分、粘包两帧、半包补齐、非法超长长度丢弃并重同步、未知垃圾数据重同步、抓拍帧切分。
  - `test_jsonframeparser.cpp`：JSON 状态帧解析（合法/非法/非对象）、ACK 状态码（0/1/2/非法载荷）、抓拍帧解析（合法坐标与 JPEG、checksum 不匹配失败、帧尾缺失失败、短帧失败）。
  - `test_protocolbuilder.cpp`：Pelco-D 7 字节帧结构与 checksum、`PtzDir` 八方向位组合、停止、绝对角度（Pan/Tilt 高位在前）、预置位三连、红外镜头变倍/变焦/停止、雨刷开合；VISCA 变倍（Tele/Wide 速度位）、变焦、停止逐字节校验。
  - `test_devicestate.cpp`：`DeviceState` 默认值、ZoomInfo/ImageSetting 字段赋值、AI 目标列表增删清空。
  - `test_geocalc.cpp`：`parseCoord`（含 N/S/E/W 后缀与大小写）、haversine 已知坐标距离、bearing 0°/90°/180°、视觉测距、`pixelToGps` 中心像素与无位姿、轨迹抽稀判定（首点/死区/强制/航向变化）。
- **测试验证**：5 个测试目标全部通过（`100% tests passed`），主程序 `LSSVideoManager.exe` Debug 构建通过。

### 阶段 6 第一轮验收核对

| 验收项 | 结论 |
|---|---|
| CTest 集成（`enable_testing()` + `add_test()`） | ✅ 根 CMakeLists 启用，`ctest` 可运行 |
| 测试目标独立，不依赖 WebEngine/FFmpeg | ✅ 纯逻辑源文件 + Qt::Core/Test |
| 帧编解码/载荷解析/协议组包/状态/地理算法覆盖 | ✅ 5 个测试文件 |
| 粘包半包/非法帧头/非法长度/ACK/抓拍帧边界/Pelco-D checksum 优先覆盖 | ✅ |
| 测试全部通过 | ✅ 5/5 Passed |
| 主程序构建不回归 | ✅ 产物 `LSSVideoManager.exe` |

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `CMakeLists.txt` | 增加 `Test` 组件、`include(CTest)`、`enable_testing()`、`add_subdirectory(tests)` |
| `tests/CMakeLists.txt` | 新增：`tjson_add_test()` 辅助 + 5 个测试目标 |
| `tests/test_tjsonframecodec.cpp` | 新增：帧编解码器单元测试 |
| `tests/test_jsonframeparser.cpp` | 新增：载荷解析器单元测试 |
| `tests/test_protocolbuilder.cpp` | 新增：Pelco-D/VISCA 组包单元测试 |
| `tests/test_devicestate.cpp` | 新增：DeviceState 状态模型测试 |
| `tests/test_geocalc.cpp` | 新增：地理算法测试 |

### 遗留问题

- **阶段 6 后续补充**：`test_devicecontext.cpp`（聚合根生命周期，需链接 RtspThread+FFmpeg）、`test_modbustransport.cpp`（MODBUS 帧 + CRC16，可纯逻辑）、`test_stm32tcptransport.cpp`（STM32 帧组包，可纯逻辑）留待后续轮次。
- **RTSP/多设备切换测试**：依赖真实设备或模拟目标，与实机验收一并推进。
- **实机验收待办**：与阶段 5 各轮一致，需实机确认三通道电机指令、PTZ/镜头、视频流、框选跟踪、PTZ 转发。
- **遗留功能 TODO 仍有效**：Pelco-D 焦聚控制（`0x02` 指令）、框选坐标真实逆映射。

### 下一步计划

1. 提交本次阶段 6 改动（建议消息 `test: 建立 Qt Test 单元测试体系`）。
2. 补充 `test_devicecontext` / `test_modbustransport` / `test_stm32tcptransport`。
3. 实机联调验收（三通道电机指令、PTZ/镜头、视频、框选跟踪、PTZ 转发），测试用例作为回归基线复用。

---

### 解决的问题 / 实现功能

- **DeviceContext 收口为聚合根**：移除 `tcpClient()/motorController()/videoStream()/ptzForwarder()` 四个公开访问器，底层组件（TJsonClient / DeviceController / RtspThread / PtzForwarder）不再向业务层泄漏，仅由 DeviceContext 内部持有。
- **新增完整业务 API**：连接/断开（`connectDevice` / `disconnectDevice` / `disconnectNetwork` / `isConnected`）、视频（`startVideo` / `stopVideo` / `isVideoRunning`）、PTZ/镜头、图像参数/工作模式/算法/显示、附加开关、框选跟踪、预置位、电机通道、雨刷电机、PTZ 转发服务等全部封装为设备业务方法。
- **信号收口**：`DeviceController` 的 `commandSent / motorModeResult / motorSilentResult / motorSerialError` 经 DeviceContext 同名信号转发，View 经 Presenter 订阅，不直连底层。
- **MainPresenter 彻底解耦底层类型**：移除私有访问器 `motorController()/tcpClient()/videoStream()/ptzForwarder()` 及 `DeviceController/TJsonClient/RtspThread/PtzForwarder` 前置声明；新增 `currentDevice()` 业务入口，全部底层调用改为 `DeviceContext` 业务 API；`DeviceController::pipShowToComboIndex` 静态映射改经 `DeviceContext::pipShowToComboIndex`。
- **行为等价**：PTZ/镜头/电机/预置位/附加开关/框选跟踪/视频/PTZ 转发指令目标与下发链路保持不变；`disconnectDevice` 保留原 `stopConnection` 的完整停止语义（定时器 + PTZ 转发 + 电机 + RTSP + TCP）。
- **构建验证**：MSVC2022 x64 Debug 构建通过，产物 `LSSVideoManager.exe`。

### 阶段 5.3 验收核对（代码层面）

| 验收项 | 结论 |
|---|---|
| `DeviceContext` 不再公开 `tcpClient()/motorController()/videoStream()/ptzForwarder()` | ✅ 4 个访问器已移除，仅保留业务 API 与 `state()` |
| Presenter 不再接触底层组件类型 | ✅ `MainPresenter` 无 `DeviceController/TJsonClient/RtspThread/PtzForwarder` 引用 |
| 只公开设备业务 API 和状态 | ✅ `connectDevice/disconnectDevice/startVideo/stopVideo/ptzMove/lensMove/setWorkMode/setAlgoModel/state()` 等 |
| 底层信号经 DeviceContext 收口 | ✅ 4 个电机/指令日志信号转发，View 经 Presenter 订阅 |
| 外部调用全部迁移（MainPresenter/DeviceManager） | ✅ `startConnection/stopConnection` 已改为 `connectDevice/disconnectDevice` |
| Debug 构建通过 | ✅ 产物 `LSSVideoManager.exe` |

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `src/service/DeviceContext.h` | 移除 4 个公开访问器；新增完整业务 API 声明 + 4 个转发信号 + 静态 `pipShowToComboIndex` |
| `src/service/DeviceContext.cpp` | 实现业务 API 转发；构造函数连接 DeviceController 信号到 DeviceContext 信号 |
| `src/ui/main/MainPresenter.h` | 移除私有访问器声明与底层类型前置声明；新增 `currentDevice()` |
| `src/ui/main/MainPresenter.cpp` | 全部底层调用改为 `currentDevice()->xxx` 业务 API；静态映射改经 DeviceContext |
| `src/service/DeviceManager.cpp` | `stopConnection()` → `disconnectDevice()` |

### 遗留问题

- **实机验收待办**：电机 MODBUS/STM32/Pelco-D 三通道、PTZ/镜头、视频流、框选跟踪、PTZ 转发需实机确认（与 5.2 一致）。
- **阶段 6 测试落地**：`tests/` CTest 集成（帧编解码 / 载荷解析 / 协议组包 / DeviceState / DeviceContext）待阶段 6 建立。
- **`sendMotorTcpV4` 的 `waitForConnected(500)` 同步等待保留**：随 `Stm32TcpTransport` 迁入，行为等价保留，联调时评估异步化。
- **遗留功能 TODO 仍有效**：Pelco-D 焦聚控制（`0x02` 指令）、框选坐标真实逆映射。

### 下一步计划

1. 提交本次阶段 5.3 改动（建议消息 `refactor: 封装 DeviceContext 业务 API`）。
2. 实机联调验收（三通道电机指令、PTZ/镜头、视频、框选跟踪、PTZ 转发）。
3. 进入阶段 6：测试与构建体系（Qt Test + CTest，测试帧编解码/载荷解析/协议组包/DeviceState）。

---

### 解决的问题 / 实现功能

- **新增 `PelcoDProtocol`**（`src/infrastructure/pelcodprotocol.h`）：纯静态 Pelco-D 组包器，含 `PtzDir` 方向枚举与语义化组包方法（`buildMove` / `buildStop` / `buildPanTo` / `buildTiltTo` / `buildSetZero` / 预置位三连 / 红外镜头 / 雨刷开合），替代原 `ProtocolBuilder`。
- **新增 `ViscaProtocol`**（`src/infrastructure/viscaprotocol.h`）：纯静态 VISCA 组包器（`buildZoom` / `buildFocus` / `buildStop`）。
- **新增 `ModbusTransport`**（`src/infrastructure/modbustransport.*`）：QSerialPort 封装，固定 9600-8-N-1，负责串口开闭/收发与 CRC16 计算，仅做字节级收发。
- **新增 `Stm32TcpTransport`**（`src/infrastructure/stm32tcptransport.*`）：QTcpSocket 封装，负责 STM32-TCP-V4.0 帧头（`5A A5 02` + 长度 + 序号 + CRC）组包、命令序列号管理与连接生命周期。
- **新增 `DeviceCommandService`**（`src/infrastructure/devicecommandservice.*`）：电机指令编排。按配置在 MODBUS-RTU / STM32-TCP-V4.0 / Pelco-D 透传间选择协议，封装雨刷电机启停/点动/校准/模式切换/电流设置；Pelco-D 透传经注入回调由 `DeviceController` 接 TJsonClient。
- **DeviceController 收窄为"协议选择 + 业务编排"**：移除 `ProtocolBuilder`、`QSerialPort` / `QTcpSocket` 直连、CRC16 及电机指令实现；PTZ/镜头/预置位改用 `PelcoDProtocol` / `ViscaProtocol` 组包，电机全部委托 `DeviceCommandService`，并通过信号转发保持对外 `signals` 不变。
- **行为等价验证**：Pelco-D/VISCA 组包字节与电机指令（MODBUS 固定帧、STM32 JSON + 帧头、cmd_id 自增前取值、Pelco-D 透传）逐字节比对原实现一致；构建通过。

### 阶段 5.2 验收核对（代码层面）

| 验收项 | 结论 |
|---|---|
| `DeviceController` 不再直连串口/TCP 与 CRC16 | ✅ 已拆至 ModbusTransport / Stm32TcpTransport |
| 协议组包不再散落于控制器内 | ✅ PelcoDProtocol / ViscaProtocol 独立成类 |
| 电机指令编排独立成服务 | ✅ DeviceCommandService（协议选择 + 状态记录） |
| 对外 API 与信号保持兼容（Presenter 零改动） | ✅ 方法签名与 4 个信号原样保留，仅内部转发 |
| 字节级行为等价 | ✅ 组包/指令逐字节核对一致 |
| Debug 构建通过 | ✅ 产物 `LSSVideoManager.exe` |

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `src/infrastructure/pelcodprotocol.h` | 新增：Pelco-D 语义化组包器 + `PtzDir` 枚举 |
| `src/infrastructure/viscaprotocol.h` | 新增：VISCA 组包器 |
| `src/infrastructure/modbustransport.*` | 新增：MODBUS-RTU 串口传输层 + CRC16 |
| `src/infrastructure/stm32tcptransport.*` | 新增：STM32-TCP-V4.0 传输层 + 帧组包 + seq |
| `src/infrastructure/devicecommandservice.*` | 新增：电机指令编排服务 |
| `src/infrastructure/devicecontroller.h/.cpp` | 移除 ProtocolBuilder / 串口 / TCP / 电机实现，改为委托 |
| `CMakeLists.txt` | 加入 7 个新文件 |

### 遗留问题

- **`sendMotorTcpV4` 的 `waitForConnected(500)` 同步等待仍保留**：已随实现迁入 `Stm32TcpTransport`，行为保持原样；后续若需彻底消除 UI 阻塞，可改为异步重连（阶段 6 联调时评估）。
- **阶段 6 测试落地**：`tests/test_protocolbuilder.cpp`（Pelco-D checksum / VISCA 组包）、`test_modbustransport.cpp` 等 CTest 集成待阶段 6 建立。
- **实机验收待办**：电机 MODBUS/STM32/Pelco-D 三通道指令需实机确认。
- **5.3 未动**：DeviceContext 业务 API 收口（`motorController()` 等访问器移除）留待阶段 5.3。
- **遗留功能 TODO 仍有效**：Pelco-D 焦聚控制（`0x02` 指令）、框选坐标真实逆映射。

### 下一步计划

1. 提交本次阶段 5.2 改动（建议消息 `refactor: 拆分 DeviceController 协议与传输`）。
2. 实机联调验收（电机三通道指令、PTZ/镜头）。
3. 进入阶段 5.3：封装 DeviceContext 业务 API（移除 `tcpClient()/motorController()/videoStream()/ptzForwarder()` 公开访问器）。

---

## 2026-08-14 · 阶段 5.1：拆分 TJsonClient 协议编解码（TJsonFrameCodec / TJsonProtocolParser）

### 解决的问题 / 实现功能

- **新增 `TJsonFrameCodec`**（`src/infrastructure/tjsonframecodec.*`）：纯 C++ 帧编解码器（无 Qt 信号依赖），负责帧头识别、长度解析、粘包/半包缓冲、异常数据重同步（`0xEC91` / `0xEB92`），并提供发送帧组包（`buildStandardFrame` / `buildHeartbeatFrame`）。
- **新增 `TJsonProtocolParser`**（`src/infrastructure/tjsonprotocolparser.*`）：纯 C++ 载荷解析器，负责 JSON 状态帧、ACK 应答帧（2 字节大端状态码）与图像抓拍帧（校验和 + 帧尾校验）解析。
- **新增 `TJsonFrame` 协议常量头**（`src/infrastructure/tjsonframe.h`）：`FrameType` 枚举与帧布局常量自 `tjsonclient.h` 迁出，供 Codec/Parser/Client 共享；`tjsonclient.h` 保留 include，外部 `FrameType` 引用不受影响。
- **TJsonClient 瘦身**：移除 `processBuffer` / `parseJsonFrame` / `parseImageSnapFrame`，接收链路改为 `codec.feed → nextFrame → dispatchFrame`（委托 Parser 解析）；发送链路改用 `TJsonFrameCodec::buildStandardFrame/buildHeartbeatFrame`；`TJsonClient` 仅保留 Socket、连接/断开、心跳、指数退避重连与事件分发。
- **行为等价验证**：独立临时测试程序验证标准帧组包/切帧、心跳帧、粘包/半包（逐字节）、重同步、抓拍帧（校验和/帧尾/坐标）、ACK/JSON 解析全部通过。
- **构建验证**：MSVC2022 x64 Debug 构建通过，产物 `LSSVideoManager.exe`。

### 阶段 5.1 验收核对（代码层面）

| 验收项 | 结论 |
|---|---|
| `TJsonClient` 不再包含帧切分/载荷解析逻辑 | ✅ 已拆至 Codec / Parser |
| Codec / Parser 无 Qt 信号依赖，可单元测试 | ✅ 纯 C++ 类，独立测试程序验证通过 |
| 外部 `FrameType` 引用不受影响 | ✅ 枚举迁至 `tjsonframe.h`，`tjsonclient.h` 保留 include |
| 粘包/半包/重同步/非法长度行为保持原样 | ✅ 逐字节半包、异常长度丢弃、重同步对齐均验证通过 |
| Debug 构建通过 | ✅ 产物 `LSSVideoManager.exe` |

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `src/infrastructure/tjsonframe.h` | 新增：`FrameType` 枚举 + 帧布局常量 |
| `src/infrastructure/tjsonframecodec.h/.cpp` | 新增：帧编解码器（feed/nextFrame/组包/重同步） |
| `src/infrastructure/tjsonprotocolparser.h/.cpp` | 新增：载荷解析器（JSON/ACK/抓拍） |
| `src/infrastructure/tjsonclient.h/.cpp` | 移除帧切分/载荷解析，改用 Codec + Parser |
| `CMakeLists.txt` | 加入 5 个新文件 |

### 遗留问题

- **阶段 6 测试落地**：`tests/test_tjsonframecodec.cpp`、`test_jsonframeparser.cpp` 与 CTest 集成待阶段 6 建立。
- **实机验收待办**：TCP 8089 状态/抓拍/ACK 链路需实机确认。
- **5.2 / 5.3 未动**：DeviceController 传输协议拆分、DeviceContext 业务 API 收口留待后续阶段。
- **遗留功能 TODO 仍有效**：Pelco-D 焦聚控制（`0x02` 指令）、框选坐标真实逆映射。

### 下一步计划

1. 提交本次阶段 5.1 改动（建议消息 `refactor: 拆分 TJsonClient 协议编解码`）。
2. 实机联调验收（状态帧/抓拍帧/ACK）。
3. 进入阶段 5.2：拆分 DeviceController（DeviceCommandService + PelcoD/VISCA 协议 + Modbus/STM32 传输）。

---

## 2026-08-14 · 阶段 4：收敛 Presenter/View 边界（IMainView 窄接口）

### 解决的问题 / 实现功能

- **新增 `IMainView` 窄接口**（`src/ui/main/IMainView.h`）：Presenter 与 View 的解耦边界，覆盖状态栏/按钮/输入读取/设备状态/AI 表格/跟踪/图像参数/下拉框/复选框/视频网格/地图/校验/回调等全部交互。
- **MainWindow 实现 `IMainView`**：`MainWindow` 继承 `IMainView`，Presenter 不再直接操作 `Ui::MainWindow` 控件。
- **Presenter 彻底解耦**：`MainPresenter.cpp` 移除 `ui_mainwindow.h` / `mainwindow.h` include，100+ 处 `getUi()->xxx` 全部改为接口方法调用，不再调用 MainWindow 具体控件方法。
- **业务状态字段下沉 Presenter**：`m_currentVisZoom/currentIrZoom/currentTilt/currentPipShow`、`m_currentResX/Y`、`m_previous*`、初始化标志（`m_workModeInitialized` 等）、`m_track`（TrackState 结构体）、`m_lastAiDist` 缓存、`m_lastAiInfoTime`、`m_deviceHeight`、`m_updatingFromDevice`、`m_currentAlgoModel` 全部迁移至 `MainPresenter` 私有区。
- **计算逻辑下沉**：`calcVisualDistance()`、`updateLensStats()`、`currentAlgoModel()` 从 `MainWindow` 迁至 `MainPresenter`，View 仅剩纯展示。
- **弹窗归属**：Presenter 通过 `IMainView::asWidget()` 获取父窗口，`QMessageBox` 弹窗保留归属。
- **构建验证**：MSVC2022 x64 Debug 构建通过，产物 `LSSVideoManager.exe`。

### 阶段 4 验收核对（代码层面）

| 验收项 | 结论 |
|---|---|
| `MainPresenter.cpp` 不再 include `ui_mainwindow.h` | ✅ include 列表已无 ui_mainwindow.h / mainwindow.h |
| Presenter 不再调用 `ui->xxx` | ✅ 100+ 处 `getUi()->` 全部替换为接口方法 |
| Presenter 不再调用 MainWindow 具体控件方法 | ✅ 经 `IMainView` 接口交互，`refreshStyle` 等内部实现 |
| MainWindow 只保留布局、信号连接和展示逻辑 | ✅ 计算/状态字段下沉，View 收敛为哑巴视图 |

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `src/ui/main/IMainView.h` | 新增窄 View 接口（约 45 个纯虚方法） |
| `src/ui/main/MainPresenter.h` | `m_view` 改为 `IMainView*`；迁移全部业务状态字段；新增 `calcVisualDistance/updateLensStats` |
| `src/ui/main/MainPresenter.cpp` | 全部 UI 交互改为接口调用；计算逻辑下沉 |
| `src/ui/views/mainwindow.h/.cpp` | 继承并实现 `IMainView`；移除业务字段与方法 |

### 遗留问题

- **实机验收待办**：双设备并行时状态仪表盘、地图位置、AI 表格/轨迹数据一致性需实机确认。
- **AIInfo 深度解析保留**：`updateAiInfoFromJson` 仍解析 AI 帧 Object 字典，待后续下沉至 `DeviceState::aiTargets`。
- **View 内少量业务残留**：`MainWindow::onDeviceConnected` 的首次自动开 RTSP（`m_rtspEverOpened`）逻辑保留在 View，后续可进一步下沉至 Presenter。
- **遗留功能 TODO 仍有效**：Pelco-D 焦聚控制（`0x02` 指令）、框选坐标真实逆映射。

### 下一步计划

1. 提交本次阶段 4 改动（建议消息 `refactor: 收敛 Presenter/View 边界`）。
2. 实机双设备联调验收。
3. 进入阶段 5：拆分基础设施（`TJsonClient` 协议编解码 / `DeviceController` 传输协议 / `DeviceContext` 业务 API 收口）。

---

## 2026-08-14 · 阶段 3：收敛状态数据流（Presenter 消费 DeviceState）

### 解决的问题 / 实现功能

- **Presenter 改为订阅结构化事件**：`MainPresenter` 移除 `sigJsonReceived` 订阅，改为 `sigDeviceStateUpdated`（ZoomInfo/ImageSetting）与 `sigDeviceAiInfoUpdated`（AIInfo）。
- **消除 JSON 重复解析**：原 `updateStatusFromJson` 在 Presenter 中对 ZoomInfo/ImageSetting/AIInfo 二次解析；现拆为 `updateStatusFromState`（从 `DeviceState` 直接读取字段更新 UI）与 `updateAiInfoFromJson`（从 AI 专用事件读取）。
- **DeviceState 扩展**：新增 `latitudeRaw`/`longitudeRaw` 原始坐标字符串，UI 原样显示；`DeviceContext` 改用 `GeoCalculator::parseCoord` 解析坐标（支持 N/S/E/W 后缀，替代 `toDouble`）。
- **AI 超时同步状态模型**：`DeviceContext` 的 AI 清理定时器超时后同步清空 `aiObjectCount`/`aiTargets`。
- **移除无效广播**：`DeviceContext` 不再转发原始 `jsonReceived`（`postJsonReceived` 无订阅者），`sigJsonReceived` 从 UI 业务中彻底移除。
- **构建验证**：MSVC2022 x64 Debug 构建通过，产物 `LSSVideoManager.exe`。

### 阶段 3 验收核对（代码层面）

| 验收项 | 结论 |
|---|---|
| 同一状态帧不再被 Presenter 重复解析 | ✅ ZoomInfo/ImageSetting 消费 DeviceState；AIInfo 消费 AI 专用事件 |
| UI 只根据结构化状态更新 | ✅ Presenter 不再直接 parse 原始 JSON（AIInfo 深度 Object 数据除外） |
| AI 超时会同步更新状态模型 | ✅ DeviceContext 超时清空 aiTargets/aiObjectCount |
| 状态变化可用单元测试验证 | ⏳ 测试体系未建立（阶段 6），待引入 |

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `src/core/DeviceState.h` | 新增 `latitudeRaw`/`longitudeRaw` 原始坐标字符串 |
| `src/service/DeviceContext.cpp` | `parseCoord` 解析坐标；AI 超时清空状态模型；移除 `postJsonReceived` 转发 |
| `src/ui/main/MainPresenter.h` | 新增 `onDeviceStateUpdated/onDeviceAiInfoUpdated/updateStatusFromState/updateAiInfoFromJson`，移除 `onJsonReceived/updateStatusFromJson` |
| `src/ui/main/MainPresenter.cpp` | 订阅结构化事件；拆分状态与 AI 分支；`updateMapDevicePosition` 改从 DeviceState 读取 |

### 遗留问题

- **实机验收待办**：双设备并行时状态仪表盘、地图位置、AI 表格/轨迹数据与旧链路一致性需实机确认。
- **AIInfo 深度解析保留**：`updateAiInfoFromJson` 仍解析 AI 帧 Object 字典（含视觉测距/轨迹所需原始字段），待后续可下沉至 `DeviceState::aiTargets` 深化。
- **多设备电机信号**：电机结果信号仅在默认设备上转发。
- **遗留功能 TODO 仍有效**：Pelco-D 焦聚控制（`0x02` 指令）、框选坐标真实逆映射。

### 下一步计划

1. 提交本次阶段 3 改动（建议消息 `refactor: Presenter 消费 DeviceState`）。
2. 实机双设备联调验收（状态仪表盘/地图/AI 数据一致性）。
3. 进入阶段 4：收敛 Presenter/View 边界（Presenter 不再直接操作 `Ui::MainWindow`）。

---

## 2026-08-14 · 阶段 2 收口：框选跟踪链路修复 + 多设备验收

### 解决的问题 / 实现功能

- **修复多设备框选缺陷**：原 `mainwindow.cpp` 仅在构造函数对**初始设备**的 `VideoWidget` 连接 `selectionFinished`，`onDeviceDoubleClicked()` 新建的视频窗未连接信号，导致切换设备后框选/点选跟踪失效。
- **框选信号携带 deviceId**：`VideoGridWidget` 新增透传信号 `selectionFinished(deviceId, cx, cy, pw, ph)`，在 `bindDevice()` 统一连接每个 `VideoWidget` 并带上设备 ID；`MainWindow` 只连接一次网格信号，任意设备视频窗均可框选。
- **指令目标按设备定位**：`MainPresenter::onVideoSelection` 签名增加 `deviceId`，经 `DeviceManager::getDevice(deviceId)->motorController()` 下发指令，不再依赖 `m_currentDeviceId`；并校验目标设备 TCP 连接状态（不再使用基于当前设备的 `requireConnected()`，避免多设备误判）。
- **构建验证**：MSVC2022 x64 Debug 构建通过，产物 `LSSVideoManager.exe`。

### 阶段 2 验收核对（代码层面）

| 验收项 | 结论 |
|---|---|
| 业务不再依赖 `"default_device"` | ✅ 仅剩 `MainPresenter.cpp:19` 初始化保留 |
| 切换设备后 PTZ/镜头指令目标正确 | ✅ `motorController()` 基于 `m_currentDeviceId` |
| 当前设备断开不误清其他设备视频 | ✅ Presenter 按 `deviceId == m_currentDeviceId` 过滤 |
| 框选/点选跟踪发往正确设备 | ✅ 本次修复：按 `deviceId` 定位控制器 |
| 任意设备视频窗均可框选 | ✅ 本次修复：`VideoGridWidget` 统一透传 |
| 双设备并行实机联调（PTZ/镜头/框选/断开互不影响） | ⏳ 待实机验证 |

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `src/ui/components/VideoGridWidget.h/.cpp` | 新增 `selectionFinished(deviceId,...)` 透传信号，`bindDevice()` 统一连接 |
| `src/ui/views/mainwindow.h/.cpp` | 连接网格透传信号；`onVideoSelection` 携带 deviceId |
| `src/ui/main/MainPresenter.h/.cpp` | `onVideoSelection` 按 deviceId 定位设备控制器并校验连接 |

### 遗留问题

- **实机验收待办**：双设备并行时 PTZ/镜头指令目标、框选跟踪目标、断开互不影响需实机联调确认。
- **多设备电机信号**：电机结果信号仅在默认设备上转发，多设备切换后仍沿用默认设备信号。
- **遗留功能 TODO 仍有效**：Pelco-D 焦聚控制（`0x02` 指令）、框选坐标按当前分辨率/黑边的真实逆映射。

### 下一步计划

1. 提交本次阶段 2 修复（建议消息 `fix: 修复多设备框选跟踪链路`）。
2. 实机双设备联调验收（PTZ/镜头/框选/断开互不影响）。
3. 进入阶段 3：收敛状态数据流（Presenter 消费 `DeviceState`，减少 `sigJsonReceived` 重复解析）。

---

## 2026-08-14 · MVP 收尾：PTZ/镜头按钮迁移 + 移除过渡期访问器

### 解决的问题 / 实现功能

- **PTZ 八方向按钮迁入 Presenter**：`mainwindow.cpp` 不再直连 `motorController()`，改为 `m_presenter->ptzMove(dir)` / `ptzStop()`。
- **镜头控制迁入 Presenter**：新增 `lensMove(op)` / `lensStop()`，可见光/红外目标由 Presenter 按显示模式自动判定，View 不再传 target。
- **PTZ 转发启动收口**：新增 `initPtzForwarder()`，启动（构造函数延迟单发）与设置页协议变更后重启统一由 Presenter 处理。
- **底层信号改经 Presenter 转发**：`motorModeResult` / `motorSerialError` / `motorSilentResult` / `commandSent` 经新增信号 `motorModeChanged` / `motorSerialErrorOccurred` / `motorSilentChanged` / `commandSentToLog` 转发给 View，View 不再连接 DeviceController。
- **新增状态查询业务方法**：`isMotorSerialOpen()` / `isMotorTcpOpen()` / `isVideoStreamRunning()` / `closeVideoStream()`，取代 View 直取底层做判空/状态检查。
- **移除过渡期访问器**：`motorController()/tcpClient()/videoStream()/ptzForwarder()` 移出 MainPresenter 公有接口（降为私有内部助手），View 不再直取底层组件，仅经业务方法交互。
- **构建验证**：MSVC2022 x64 Debug 构建通过，产物 `LSSVideoManager.exe`。

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `src/ui/main/MainPresenter.h` | 新增 `lensMove/lensStop/initPtzForwarder/isMotorSerialOpen/isMotorTcpOpen/isVideoStreamRunning/closeVideoStream` 及 4 个转发信号；过渡期访问器移入私有区 |
| `src/ui/main/MainPresenter.cpp` | 构造函数转发默认设备电机/指令日志信号；新增上述方法实现 |
| `src/ui/views/mainwindow.cpp` | PTZ 方向/镜头按钮、PTZ 转发启动/重启、电机信号连接、视频流关闭/状态检查全部改为调用 Presenter 业务方法 |

### 遗留问题

- **已提交**：本次按钮迁移与过渡期访问器改动已提交，提交号为 `8d200cc`。
- **生命周期优化进行中**：开始收敛 DeviceContext、DeviceManager 与 RtspThread 的停止和销毁流程。
- **多设备电机信号**：电机结果信号仅在默认设备上转发，多设备切换后仍沿用默认设备信号（与迁移前行为一致）。
- **遗留功能 TODO 仍有效**：Pelco-D 焦聚控制（`0x02` 指令）、框选坐标按当前分辨率/黑边的真实逆映射。

### 下一步计划

1. 完成设备生命周期、RTSP 线程和多设备当前上下文收敛。
2. 实机联调验证：PTZ 八方向、镜头变倍/调焦、电机模式/静音状态显示、指令日志、雨刷、PTZ 转发服务。
3. 解决遗留功能 TODO：Pelco-D 焦聚控制、框选坐标真实逆映射。

---

## 2026-08-14 · MVP 改造收尾 + 雨刷电机逻辑梳理

### 解决的问题 / 实现功能

- **雨刷电机控制逻辑调研**：控制链路为 `UI 按钮 → DeviceController` 按配置协议三选一分发（**Pelco-D** 透传 / **MODBUS-RTU** 串口 9600-8-N-1 / **STM32-TCP-V4.0** 封包 `5A A5 02+长度+序号+JSON`），通道可选本地串口或 Pelco-D 透传。
- **重构进度盘点**：五步走中第 1-3、5 步已落地，第 4 步（MVP 改造）进行中。
- **修复被临时脚本清空的约 25 处设备调用**：此前 `mod.py` 机械删除了 `mainwindow.cpp` 中雨刷、预设、附加开关、框选跟踪、工作/算法/显示模式、电机通道初始化的全部调用，本次全部迁入 `MainPresenter`（21 个新方法），恢复设备指令下发链路。
- **ACK 处理迁移**：`m_lastAckFrameType` 状态迁入 Presenter，`MainWindow::onAckReceived` 由 `MainPresenter::showAck()` 取代（经 EventBus 回调）。
- **临时脚本清理**：删除 9 个已固化脚本，保留 4 个 `check_*.py` 供复查。
- **构建验证**：MSVC2022 x64 Debug 构建通过，产物 `LSSVideoManager.exe`。

### 核心改动文件

| 文件 | 改动内容 |
|---|---|
| `src/ui/main/MainPresenter.h` | 新增 21 个方法声明；`#include "tjsonclient.h"`；新增 `m_lastAckFrameType` |
| `src/ui/main/MainPresenter.cpp` | 新增约 340 行实现（电机通道/雨刷/预置位/开关/跟踪/模式切换/`showAck`） |
| `src/mainwindow.h` | `requireMotorReady` 提升 public；删除 `onAckReceived`/`sendAlgoModel`/`m_lastAckFrameType` |
| `src/mainwindow.cpp` | 构造函数改调 `initMotorChannel()`；雨刷/预设/开关 lambda 改调 `m_presenter->onXxx()`；槽改为转发 Presenter |
| 根目录 | 删除 9 个 `append_*.py`/`mod.py`/`fix_*.py`，保留 4 个 `check_*.py` |

### 遗留问题

- **改动未提交**：`mainwindow.*`、`MainPresenter.*`、`EventBus.*`、`DeviceContext.cpp`、`ui/mainwindow.ui` 未提交（`ui/mainwindow.ui` 有 73 行改动，来源需人工确认）。
- **过渡期接口未消除**：`MainPresenter.h` 仍暴露 `motorController()/tcpClient()/videoStream()/ptzForwarder()`。
- **PTZ 方向/镜头按钮未迁移**：`mainwindow.cpp:242-286` 仍在 View lambda 直接调 `motorController()`。
- **功能未经实机验证**：雨刷、预设、开关、跟踪、模式切换仅编译通过。
- **遗留 TODO 仍有效**：Pelco-D 焦聚控制对接（`0x02` 指令）、框选坐标真实逆映射。

### 下一步计划

1. 提交当前改动（MVP 迁移、脚本清理分组提交；提交前审查 `ui/mainwindow.ui`）。
2. 实机联调验证：雨刷启停/点动/校准/模式/静音/电流、预置位增删调、4 个附加开关、框选/点选跟踪、工作/算法/显示模式切换、设置页切换协议后电机通道重开。
3. 彻底化 MVP 收尾：PTZ 方向、镜头按钮迁入 Presenter；移除过渡期访问器，仅提供业务方法。
4. 解决遗留功能 TODO：Pelco-D 焦聚控制（`0x02`）、框选坐标按当前分辨率/黑边的真实逆映射。

---

## 2026-06-03 · V1.2 需求说明书

- 需求说明书更新至 V1.2，地图方案使用 Leaflet + 天地图（见下），增加点选跟踪、雨刷电机等需求。

---

## 2026-05-26 · 识别/跟踪上报修复 + 地图功能

- **ACK 帧处理**：修复 `0x12` ACK 帧被误当作 JSON 帧解析，添加独立分支发射 `ackReceived(quint8)` 信号。
- **trackId 修正**：跟踪上报中 `trackId` 曾赋状态文本（"锁定中"/"丢失"），改为使用上报 JSON 中 `Object` 内的真实 ID。
- **trackAngle 计算**：JSON 缺 Angle 字段时按脱靶量 `atan2(dy, dx)` 计算跟踪角度。
- **跟踪模式扩展**：从仅支持 `workMode==2` 扩展为 `workMode>=2 && workMode<=4`。
- **空对象判断**：将 `count>0` 守卫改为检查 `Object` 非空。
- **图片抓拍**：`onImageSnapped` 实现，JPEG 保存至 `snapshots/`，文件名含时间戳。
- **地图组件**：实现 `MapWidget` / `MapBridge` / `MapDialog`（QWebEngineView + Leaflet）。
- **地图瓦片源切换到天地图**：OSM 在境内不可用（超时白屏），迁移至天地图（`tianditu.gov.cn`）；采用 WGS-84 坐标直接叠加（CGCS2000 无需纠偏）。
- **其他**：可见光默认分辨率从 1920×1080 改为 2688×1520（2K）。

---

## 2026-05-24 · 底层通信框架 + 业务解耦 + 环境修复

### 1. 底层通信框架（`TJsonClient`）

- **TCP 异步链路**：基于 `QTcpSocket` 非阻塞收发（默认连接 `192.168.1.200:8089`）。
- **心跳保活**：`QTimer` 每 10s 发送 `0x11` 心跳帧。
- **粘包/半包处理**：大端序包长读取 + 缓冲区截断，支持 `0x01` 状态帧与 `0x04` 抓拍帧拆解。
- **防死锁防御**：JSON 负载限 10MB、JPEG 限 50MB，错位/超大包长主动丢弃残损帧头重新寻址，防 OOM。
- **工业级静默重连**：指数退避（2s→4s→8s...封顶 60s），无限期重连，界面底部状态栏反馈重连倒数。

### 2. 业务逻辑分离（`DeviceController` & `ProtocolBuilder`）

- 剥离强耦合 `sendPtzCommand`，提炼透传网关 `sendTransparentData(serialType, data)`。
- 创建 `ProtocolBuilder` 封装 Pelco-D（自动 Checksum）等基础协议，符合开闭原则，为 VISCA、Pelco-P 预留空间。

### 3. 弹窗与参数配置（`SettingsDialog`）

- 纯 `.ui` 实现"通信与协议配置"与"光学与相机参数"两大模块。
- `QSettings("Tofu", "T-JSON_Settings")` 持久化云台地址、通道协议、像元尺寸、焦距等，启动自动回显。

### 4. UI 与工程环境修复

- QSS 主题抽离为独立 `style.qss`，经 `resources.qrc` 打包。
- 修复 GBK/UTF-8 转换截断引起的 `mainwindow.ui` XML 标签损坏（`?/string>`），解决 `uic` 崩溃与 Stack corrupted。
- `CMakeLists.txt` 注入 `/utf-8`，源文件转为带 BOM 的 UTF-8，消除 MSVC 乱码（Mojibake）。

---

## 近期架构优化记录（重构五步走）

1. **UTF-8 with BOM 标准化（C4828 警告修复）**：脚本将 `src` 下所有 `.cpp`/`.h` 转为 UTF-8 with BOM，消除 MSVC C4828 及字符串未闭合（C3688）、宏定义报错。
2. **底层网络纯粹化与 UI 彻底解耦（Phase 4 & 5）**：`TJsonClient` 改为 Qt 标准信号；`DeviceContext` 接管 `TJsonClient`/`RtspThread` 与 EventBus 闭环绑定；`MainWindow` 不再直接调用 `tcpClient()->isConnected()` 等，全部收口至 `MainPresenter`。
3. **安全规范与智能指针**：EventBus 设备状态载荷升级为 `std::shared_ptr<DeviceState>` + `std::make_shared`，防止异步分发生命周期悬空指针。
4. **清理魔术数字**：TCP 端口（`8089`）、地图 FOV 距离（`4000`/`2000`）提取至 `ConfigManager`。
5. **多线程安全与堆损坏修复**：
   - FFmpeg SIMD 内存溢出：`RtspThread` 的 `m_rgbBuf` 加 32 字节对齐 + 1024 安全填充，解决 CRT HEAP CORRUPTION。
   - 退出野线程：`MainWindow::~MainWindow()` 调用 `DeviceManager::instance()->removeAllDevices()` 安全退出所有 TCP/FFmpeg 线程。
   - 双击切换卡顿：修复 `RtspThread::openStream` 的 `m_stop` 竞争导致 2s `wait()` 卡死，通过安全 `closeStream` + `wait(3000)` 释放旧线程。

---

## 版本发布记录

### V1.1-beta · 云台角度数据通道与零点标定重构

**主要变更**：

1. **云台角度数据源切换**：`statPanAngle`/`statTiltAngle` 显示数据源从串口服务器（Pelco-D）改为 8089 端口（JSON 协议），与设备内部真实角度一致。
2. **零点标定重构**：点击标定后，软件偏移量（offset）仅作用于转发给相机的 Pelco-D 数据，使视频 OSD 显示偏移后角度；标定后立即推送一次 0° 位置帧，OSD 即时更新。
3. **串口数据转发修正**：修复 `PtzForwarder` 原始帧覆盖偏移帧的 Bug，确保转发给相机（26 端口）数据含正确偏移。
4. **经纬度/角度引导**：`ptzMoveTo` 已集成偏移处理，输入偏移后坐标，内部自动转物理角度发送。
5. **高度显示**：设备高度为 0 时，UI 输入框自动清空。

**注意事项**：

- 安装包名：`LSS-Video-Manager-V1.1-beta-Setup.exe`
- 覆盖安装前需先退出已运行的 `LSSVideoManager.exe`
