# 架构债务治理与应用层重构设计

## 1. 背景

当前项目已经完成基础设施层拆分、设备上下文收口、Presenter/View 初步隔离和多设备电机信号修复，但两个主文件仍承担过多职责：

- `src/ui/main/MainPresenter.cpp` 同时处理 UI 命令、设备切换、设备状态缓存、AI 解析、地图数据、视频事件和电机控制。
- `src/ui/views/mainwindow.cpp` 同时处理 Qt 控件绑定、窗口行为、地图布局、视频布局、系统托盘、导航和对话框。

剩余架构债务集中在三个方面：超大文件、设备销毁与 RTSP 线程生命周期、多设备状态和当前设备隐式耦合。本设计允许重做内部 C++ 接口，但保持设备协议、配置数据格式、用户可见行为和必要的 `IMainView` 语义兼容。

## 2. 目标与非目标

### 2.1 目标

- 将 `MainPresenter.cpp` 和 `mainwindow.cpp` 拆分为职责清晰、可独立测试的组件。
- 让设备生命周期拥有明确状态机、幂等关闭和可观测的线程退出结果。
- 让所有设备数据按 `deviceId` 隔离，当前设备只表示当前控制和展示选择。
- 消除 Presenter 服务对 `MainPresenter*` 的反向依赖。
- 使设备事件可以安全地跨越切换、删除、重建流程，不污染新设备会话。
- 保持每个阶段可编译、可测试、可回滚，最终一次性完成目标架构。

### 2.2 非目标

- 不修改 T-JSON、RTSP、Pelco-D、VISCA、MODBUS-RTU 或 STM32-TCP 协议。
- 不重做地图前端、视频解码算法或设备配置文件格式。
- 不引入新的全局状态容器或通用事件框架替代现有 `EventBus`。
- 不为旧的内部 Presenter 方法保留兼容包装层。

## 3. 最终架构

```text
MainWindow
  └── MainPresenter
        ├── DeviceSessionService
        ├── DeviceStateService
        ├── DeviceControlService
        ├── DeviceMediaService
        └── DeviceMapService
              └── DeviceContext
                    ├── TJsonClient
                    ├── DeviceController
                    ├── RtspThread
                    └── PtzForwarder
```

### 3.1 MainPresenter

`MainPresenter` 是页面级适配器，只负责：

- 接收 `IMainView` 产生的用户命令。
- 从 `DeviceSessionService` 获取明确的当前选择。
- 调用业务服务并转发服务结果到 `IMainView`。
- 处理全局窗口状态和跨服务协调。

`MainPresenter` 不再持有设备状态缓存，不解析 AI 业务细节，不直接访问 `DeviceContext` 的底层组件，也不直接依赖具体 Widget 类型。

目标规模：`MainPresenter.cpp` 约 250～350 行，单个方法不超过 80 行。

### 3.2 DeviceSessionService

负责设备集合和会话选择：

- 添加、连接、断开、删除设备。
- 持有唯一的 `selectedDeviceId`。
- 发出 `deviceAdded`、`deviceRemoved`、`selectionChanged`、`deviceConnectionChanged` 等带设备标识的信号。
- 为每个设备维护 `sessionGeneration`，设备删除或重建时递增。
- 连接 `DeviceContext` 的业务信号，并在设备删除前解除连接。

该服务不向外暴露 `TJsonClient`、`DeviceController`、`RtspThread` 或 `PtzForwarder`。

### 3.3 DeviceStateService

按 `deviceId` 管理设备状态快照和 UI 所需派生数据：

- `DeviceSnapshot`：设备状态、AI 目标、镜头统计、图像参数和初始化标志。
- 接收 `DeviceContext` 的状态事件并更新对应快照。
- 处理 `ZoomInfo`、`ImageSetting`、`AIInfo` 的业务转换。
- 维护每台设备独立的轨迹抽稀状态、AI 超时状态和上次 ACK。
- 提供当前快照和设备快照查询，不依赖当前设备全局变量。

纯解析和地理算法继续放在 `core/`，服务只编排解析结果和业务状态。

### 3.4 DeviceControlService

统一承接设备控制用例：

- PTZ、镜头、预置位。
- 电机通道、雨刷电机、静音和电流设置。
- 工作模式、算法模型、显示模式、数字变倍、自动变倍、抓拍上传和位置复位。
- PTZ 转发器初始化与零点偏移。

所有方法显式接收 `DeviceId`。服务不得自行读取 `selectedDeviceId`，避免在异步调用中发生设备漂移。

### 3.5 DeviceMediaService

负责 RTSP 和视频交互：

- 按 `deviceId` 启停 RTSP。
- 按 `deviceId` 路由视频帧、打开结果和错误。
- 管理视频选择事件并将其转换为目标设备的点选/框选命令。
- 设备删除时清理对应视频槽位和媒体状态。

视频帧始终发送到 `VideoGridWidget` 对应的设备槽位，不使用当前设备作为路由依据。

### 3.6 DeviceMapService

负责地图展示数据：

- 设备位置、可见光/红外 FOV、AI 目标和轨迹。
- 从 `DeviceStateService` 获取指定设备快照。
- 维护每台设备的地图轨迹和目标数据。
- 当前地图面板默认展示 `selectedDeviceId`，切换时使用缓存快照立即刷新。

地图服务不直接调用 `MapWidget`，只通过窄 View 接口输出地图展示数据。

### 3.7 MainWindow

`MainWindow` 只负责 Qt 控件生命周期、Designer 控件绑定和 `IMainView` 实现。现有职责拆分为：

- `MainWindowNavigation`：导航按钮、页面切换和日志/设置入口。
- `MainWindowDialogService`：设置、命令日志和相关对话框生命周期。
- `MainWindowLayoutService`：地图迷你/全屏、PiP、视频布局和窗口尺寸调整。
- `MainWindowSystemService`：系统托盘、标题栏、最小化、最大化、关闭事件。

这些服务只能接收所需的窄接口和控件句柄，不持有 `MainPresenter` 的反向指针。`MainWindow` 不再公开 `getUi()`、地图控件或视频控件给 Presenter。

目标规模：`mainwindow.cpp` 约 300～450 行，窗口事件入口保持短小。

## 4. 生命周期设计

### 4.1 DeviceContext 状态机

```text
Created -> Active -> ShuttingDown -> Stopped
```

- `Created`：构造完成但尚未接入业务。
- `Active`：允许连接、视频、控制和状态更新。
- `ShuttingDown`：拒绝所有新业务命令，执行一次关闭流程。
- `Stopped`：资源已关闭，不允许重新激活；重连必须创建新的上下文。

`shutdown()` 幂等，并返回包含各资源关闭结果的 `ShutdownResult`。所有业务入口统一经过状态检查，`ShuttingDown` 和 `Stopped` 状态返回失败或忽略命令，并记录必要日志。

### 4.2 关闭顺序

1. 原子地将状态从 `Active` 变为 `ShuttingDown`。
2. 停止系统参数和 AI 清理定时器，停止 T-JSON 自动重连。
3. 解除 EventBus 和设备信号的业务转发，阻止新异步事件进入服务层。
4. 调用 `RtspThread::closeStream()`，等待线程退出并取得结果。
5. 停止 PTZ 转发器，关闭电机 TCP 和串口。
6. 断开 T-JSON TCP。
7. 清理待处理连接，状态改为 `Stopped`。

关闭顺序必须集中在 `DeviceContext::shutdown()`，外部不得分别关闭内部组件。

### 4.3 RTSP 安全规则

- `RtspThread::closeStream()` 返回明确的 `CloseResult`，区分正常退出和超时。
- 继续使用 FFmpeg interrupt callback，确保阻塞调用响应停止请求。
- 关闭超时时，记录设备 ID、线程状态和 URL，并将上下文标记为关闭故障；不得继续在同一个线程对象上重新打开。
- 只有确认线程退出后，`DeviceManager` 才允许销毁上下文。
- 析构函数只做兜底关闭，不承担正常业务关闭职责。

### 4.4 DeviceManager 所有权

`DeviceManager` 是 `DeviceContext` 的唯一生命周期所有者：

- `removeDevice()` 委托统一的 `destroyDevice()` 流程。
- 销毁前发出 `deviceAboutToBeRemoved(deviceId, generation)`。
- `shutdown()` 失败时保留结构化错误并阻止静默删除；由上层决定重试或终止流程。
- 删除、重建同一 `deviceId` 时保证新旧 generation 不相同。

## 5. 多设备数据流

```text
DeviceContext(deviceId)
  -> DeviceSessionService
  -> DeviceStateService / DeviceMediaService
  -> MainPresenter
  -> IMainView
```

所有事件至少携带：

- `deviceId`
- `sessionGeneration`
- 业务载荷

服务层转发事件前检查设备仍存在、generation 仍匹配且上下文未进入 `ShuttingDown`。过期事件直接丢弃并记录 debug 日志。

### 5.1 展示与控制的分离

- 视频宫格：展示所有已启用设备，按 `deviceId` 定位槽位。
- 仪表盘：展示 `selectedDeviceId` 的快照。
- 地图：默认展示 `selectedDeviceId`，但数据按设备保存，后续可扩展多设备叠加而不改状态层。
- PTZ/电机/参数控制：只作用于命令提交时明确解析出的 `deviceId`。

设备切换不会清空其他设备状态、视频或轨迹。当前设备删除时，选择服务选择下一个可用设备并触发完整快照刷新。

## 6. 接口和依赖规则

- 内部接口允许重新设计，不保留旧 Presenter 访问器和过渡包装函数。
- UI 层只能依赖 `IMainView`、应用服务接口和 `DeviceId`/快照类型。
- `Presenter*Service` 不依赖 `MainPresenter*`。
- `MainPresenter` 不 include 具体地图、视频和设备基础设施类。
- `MainWindow` 不把 `Ui::MainWindow*`、具体 Widget 或底层控制器暴露给 Presenter。
- `DeviceContext` 继续作为单设备聚合根，底层组件只能通过其业务 API 使用。
- `core/` 不依赖 UI、service 或 infrastructure。
- 所有 QObject 连接保存为明确的连接句柄，设备删除或切换时集中断开。

## 7. 迁移顺序

这是一个最终架构目标，但实现按以下可验证阶段进行：

1. 建立 `DeviceId`、`DeviceSnapshot`、`DeviceEvent`、`ShutdownResult` 和窄接口。
2. 将 Presenter 的设备状态缓存和 JSON/AI 转换迁移到 `DeviceStateService`。
3. 将设备切换、连接、删除和信号连接迁移到 `DeviceSessionService`。
4. 合并现有电机服务和设备命令入口为 `DeviceControlService`，移除对 Presenter 的反向依赖。
5. 将 RTSP、视频帧和框选链路迁移到 `DeviceMediaService`。
6. 将地图目标、FOV、位置和轨迹迁移到 `DeviceMapService`。
7. 实现 `DeviceContext` 关闭结果、RTSP 退出结果和 generation 过滤。
8. 拆分 `MainWindow` 的布局、系统、导航和对话框职责，收紧 `IMainView`。
9. 删除旧方法、旧缓存和具体 Widget 依赖，更新 `ARCHITECTURE.md` 与 `CHANGELOG.md`。

每一阶段都必须通过编译、现有单元测试和新增的服务测试；阶段之间不保留长期兼容层。

## 8. 测试矩阵

### 8.1 单元测试

- `DeviceStateService`：多设备状态隔离、快照恢复、设备删除清理、AI 超时。
- `DeviceSessionService`：设备选择、当前设备删除后的回退、generation 递增和过期事件丢弃。
- `DeviceContext`：状态转换、重复 shutdown、关闭中命令拒绝。
- `RtspThread`：重复打开、打开后立即关闭、重连中关闭、关闭超时结果。
- `DeviceMediaService`：帧按设备路由、设备切换不影响其他设备、框选命令目标正确。
- `DeviceMapService`：轨迹按设备隔离、切换后快照立即刷新、目标和 FOV 更新。
- `MainPresenter`：使用 fake View 验证命令只经过应用服务，不依赖具体 Widget。

### 8.2 集成验收

- 两台设备同时连接并持续出帧。
- 两台设备交替切换，仪表盘和地图数据不串台。
- 非当前设备断线时，当前设备仍可控制。
- 删除当前设备后自动选择下一设备，旧设备事件全部失效。
- 删除后重新添加相同 ID，旧 RTSP 帧、AI 状态和电机错误不能进入新会话。
- 关闭窗口时所有设备按顺序停止，RTSP 线程无未回收实例。

## 9. 完成标准

- `MainPresenter.cpp` 和 `mainwindow.cpp` 达到目标规模，且没有超过 80 行的业务方法。
- `grep` 检查不到 Presenter 对具体 Widget、底层 TCP、RTSP 或电机控制器的直接依赖。
- 所有设备事件带 `deviceId` 和 generation，当前设备不再是数据归属依据。
- `DeviceManager` 是唯一设备销毁入口，RTSP 线程退出结果可测试、可记录。
- 现有 7 个纯逻辑测试保持通过，并新增服务、生命周期和多设备测试。
- `ARCHITECTURE.md` 的超大文件、生命周期风险和多设备收口债务更新为已解决或明确剩余项。
