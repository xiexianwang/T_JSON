# 架构债务治理实施计划

关联设计：`docs/superpowers/specs/2026-08-20-architecture-debt-refactor-design.md`

## 1. 实施约束

- 保持设备协议、配置格式和用户可见行为不变。
- 允许重做内部 C++ 接口，不保留旧 Presenter 兼容包装。
- 每个阶段必须可编译；完成后运行现有测试和新增测试。
- 不在同一阶段同时改变协议、FFmpeg 解码和 UI 展示语义。
- 每完成一个阶段检查 `MainPresenter.cpp`、`mainwindow.cpp` 的职责和依赖。

## 2. 阶段一：基础类型与接口边界

### 修改范围

- `src/core/`：新增 `DeviceId`、`DeviceEvent`、`DeviceSnapshot` 等纯数据类型。
- `src/service/DeviceContext.*`：新增 `ShutdownResult`、生命周期检查和统一业务命令守卫。
- `src/service/DeviceManager.*`：收口 `destroyDevice()` 和设备删除信号。
- `src/ui/main/IMainView.h`：按输入、状态、视频、地图拆分窄接口或接口分组。
- `CMakeLists.txt`：加入新增源文件。

### 实现要点

- `DeviceId` 使用现有设备 ID 的字符串语义，先不改变持久化格式。
- 所有设备事件携带 `deviceId` 和 `sessionGeneration`。
- `DeviceContext::shutdown()` 改为幂等并返回关闭结果。
- `ShuttingDown`/`Stopped` 状态拒绝所有连接、视频和控制命令。

### 验证

- 新增 `test_devicecontextlifecycle.cpp`：状态转换、重复关闭、关闭中命令拒绝。
- 现有 7 个测试全部通过。

## 3. 阶段二：设备会话服务

### 新增

- `src/ui/main/DeviceSessionService.h/.cpp`

### 修改

- `src/ui/main/PresenterDeviceService.*`：迁移或删除重复职责。
- `src/ui/main/MainPresenter.*`：只保留设备命令入口和服务信号适配。
- `src/service/DeviceManager.*`：由会话服务统一调用添加、选择、删除和销毁。

### 实现要点

- 唯一维护 `selectedDeviceId`。
- 设备删除、重建时递增 generation。
- 当前设备删除后按设备树顺序选择下一个可用设备。
- 切换和删除前集中断开设备信号连接。
- 服务不持有 `MainPresenter*`，Presenter 只连接服务信号。

### 验证

- 新增 `test_devicesessionservice.cpp`：切换、删除、回退、generation 过期事件。
- fake `DeviceManager`/fake View 验证设备操作不触碰具体 Widget。

## 4. 阶段三：状态服务

### 新增

- `src/ui/main/DeviceStateService.h/.cpp`

### 修改

- `src/core/JsonFrameParser.*`：仅补充缺失的纯解析结果，不加入 UI 逻辑。
- `src/ui/main/MainPresenter.*`：删除设备级状态字段和 AI/镜头统计实现。
- `src/ui/main/PresenterMapService.*`：改为消费 `DeviceSnapshot`。

### 实现要点

- 用 `QHash<DeviceId, DeviceSnapshot>` 保存设备快照。
- `ZoomInfo`、`ImageSetting`、`AIInfo` 更新只能作用于事件中的 device ID。
- 轨迹、AI 超时、镜头统计、工作模式初始化状态全部按设备隔离。
- 切换设备时立即发出缓存快照，避免等待设备下一帧。

### 验证

- 新增 `test_devicestateservice.cpp`：两设备状态隔离、切换恢复、删除清理、AI 超时。
- 保持地图和仪表盘现有显示结果一致。

## 5. 阶段四：设备控制服务

### 新增或重命名

- `src/ui/main/DeviceControlService.h/.cpp`

### 修改

- `PresenterMotorService.*`
- `MainPresenter.*`
- `DeviceContext.*`

### 实现要点

- 合并 PTZ、镜头、预置位、电机和设备参数命令。
- 每个方法显式接收 `DeviceId`。
- 服务只依赖 `DeviceManager`/`DeviceContext` 和窄接口，不依赖 Presenter。
- 删除 MainPresenter 中重复的当前设备获取和底层访问代码。

### 验证

- fake `DeviceContext` 验证每个命令的目标设备正确。
- 验证切换设备后新命令不会发送到旧设备。
- 保留现有电机串口、STM32-TCP 和 PTZ 协议测试。

## 6. 阶段五：媒体与地图服务

### 新增或重命名

- `src/ui/main/DeviceMediaService.h/.cpp`
- `src/ui/main/DeviceMapService.h/.cpp`

### 修改

- `PresenterMapService.*`
- `MainPresenter.*`
- `IMainView.h`
- `VideoGridWidget.*`
- `MapWidget.*`（仅调整输入边界，不重写地图前端）

### 实现要点

- 视频帧、RTSP 打开、错误和关闭结果都携带 device ID。
- 视频帧按 device ID 路由到宫格，不使用当前设备。
- 框选事件保留来源 device ID，禁止使用切换后的当前设备覆盖目标。
- 地图数据按设备缓存，当前地图面板切换时加载对应快照。
- 服务不 include 具体 Widget 类型。

### 验证

- 双设备同时出帧时画面不串台。
- 切换设备不会清空其他设备视频、状态或轨迹。
- 删除设备清理其视频槽位和地图轨迹。
- 新增媒体服务和地图服务 fake View 测试。

## 7. 阶段六：生命周期与 RTSP 收口

### 修改

- `src/infrastructure/rtspthread.*`
- `src/service/DeviceContext.*`
- `src/service/DeviceManager.*`
- `src/ui/main/DeviceSessionService.*`

### 实现要点

- `RtspThread::closeStream()` 返回正常退出/超时结果。
- 关闭超时后禁止同一线程对象再次打开。
- `DeviceContext` 关闭顺序固定：停止定时器、阻断事件、关闭 RTSP、关闭 PTZ/电机/TCP。
- `DeviceManager` 只有在线程退出确认后才销毁上下文。
- 所有 QObject 连接保存句柄，设备删除前统一解除。

### 验证

- RTSP 打开后立即关闭。
- RTSP 自动重连期间关闭。
- 重复关闭和窗口退出。
- 删除并重建同一设备 ID，旧事件不能进入新会话。
- 记录关闭超时的 device ID、URL 和线程状态。

## 8. 阶段七：MainWindow 拆分

### 新增或完善

- `src/ui/views/MainWindowLayoutService.*`
- `src/ui/views/MainWindowSystemService.*`
- `src/ui/views/MainWindowNavigation.*`
- `src/ui/views/MainWindowDialogService.*`

### 修改

- `src/ui/views/mainwindow.*`
- `ui/mainwindow.ui`

### 实现要点

- MainWindow 只保留 Qt 事件入口、控件绑定和 IMainView 实现。
- 地图布局、PiP、窗口拖动迁移到 LayoutService。
- 系统托盘、标题栏、窗口关闭迁移到 SystemService。
- 导航和对话框服务只依赖所需控件和窄接口。
- 删除 `getUi()`、公开地图/视频控件和业务对象成员。

### 验证

- 主窗口创建、关闭、最小化、最大化、托盘退出。
- 地图迷你/全屏和 PiP 行为不回归。
- 设置、日志、导航按钮行为不回归。
- `mainwindow.cpp` 控制在 300～450 行。

## 9. 阶段八：删除旧边界并更新文档

### 清理

- 删除旧 `PresenterDeviceService`、`PresenterMotorService`、`PresenterMapService` 中已迁移代码。
- 删除 MainPresenter 的设备级缓存、旧槽函数和具体 Widget 依赖。
- 删除 MainWindow 对外暴露的内部控件访问器。
- 清理 CMake 中失效源文件和 include。

### 文档

- 更新 `ARCHITECTURE.md` 目录、分层依赖、模块职责和架构债务表。
- 更新 `docs/CHANGELOG.md`，记录最终架构、测试和实机验收状态。

### 验证

- `grep` 检查 Presenter 不直接依赖 Widget、TJsonClient、RtspThread、DeviceController。
- `grep` 检查服务不依赖 `MainPresenter*`。
- 主程序构建通过。
- 所有 CTest 测试通过。
- 双设备实机验收通过。

## 10. 提交策略

建议按阶段提交，提交信息保持单一职责：

1. `refactor: 建立设备事件与生命周期接口`
2. `refactor: 提取设备会话与状态服务`
3. `refactor: 收口设备控制与媒体地图服务`
4. `fix: 完善设备销毁与 RTSP 线程关闭`
5. `refactor: 拆分 MainWindow 并收紧 UI 边界`
6. `docs: 更新最终架构与验收记录`

每个提交都应包含对应测试，不提交未编译验证的跨阶段半成品。
