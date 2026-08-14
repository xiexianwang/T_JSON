# T-JSON 变更日志（CHANGELOG）

> 本文件整合原 `docs/项目日志.md`（开发流水）、`docs/V1.1-beta-版本说明.md`（版本发布）与 `项目重构规则.md` 中的"最近优化记录"，按日期倒序排列。
> 最新记录在顶部。未解决事项集中记录于 §「遗留问题」。

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
