# P1-01 CameraSystem

Status: completed  
Depends on: P0-03, P0-05  
Blocks: P1-02, P1-03

## Goal

实现 CineRig 的运行时中枢：句柄管理、相机注册表、更新顺序、更新时钟、Standby 策略和确定性手动更新。

## Scope

- `CameraSystem` 实例，拥有相机、Brain、TargetGroup、ImpulseManager、Spline 注册。
- `CameraId` / `BrainId` / `GroupId` 句柄和 slot map。
- `VirtualCameraRegistry`：
  - 活跃相机按优先级排序。
  - 所有相机按嵌套层级排序，叶子优先更新。
- `UpdateManager`：
  - `Fixed` / `Late` / `Smart` / `Manual` 更新模式。
  - 每个 vcam 的 `lastUpdateFrame` / `lastUpdateFixedFrame`。
  - 多帧 deltaTime 追赶。
  - Standby：Never / Always / RoundRobin。
- `UpdateTracker`：根据目标变换变化推断 Fixed/Late 偏好。
- 公共 API：
  - `createCamera<T>()`
  - `destroyCamera(CameraId)`
  - `createBrain(...)`
  - `fixedUpdate(dt)`
  - `lateUpdate(dt)`
  - `manualUpdate(dt)`
  - `resetCameraState()`
- 生命周期和禁用行为。

## Non-goals

- 不实现组件管线（P1-02）。
- 不实现 Brain 混合（P1-03）。
- 不实现管理器相机（P1-04）。
- 不实现多线程更新。
- 不实现编辑器/序列化。

## Deliverables

```text
include/cinerig/runtime/
├── camera_system.hpp
├── virtual_camera_registry.hpp
├── update_manager.hpp
└── update_tracker.hpp
src/runtime/
├── camera_system.cpp
├── virtual_camera_registry.cpp
├── update_manager.cpp
└── update_tracker.cpp
tests/scenario/
├── camera_system_priority_test.cpp
├── camera_system_update_clock_test.cpp
├── camera_system_standby_test.cpp
└── camera_system_lifecycle_test.cpp
```

## API Sketch

```cpp
namespace cinerig
{
enum class UpdateMethod { Fixed, Late, Smart, Manual };

struct CameraSystemConfig
{
    UpdateMethod updateMethod = UpdateMethod::Smart;
    Vec3 worldUp{0.0f, 1.0f, 0.0f};
};

class CameraSystem
{
public:
    explicit CameraSystem(IEntityWorld& world,
                          IPhysicsWorld* physics = nullptr,
                          ISplineProvider* splines = nullptr);
    ~CameraSystem();

    CameraId createCamera(std::unique_ptr<VirtualCamera> camera);
    template <typename T, typename... Args>
    CameraId createCamera(Args&&... args);
    void destroyCamera(CameraId id);
    VirtualCamera* camera(CameraId id);
    const VirtualCamera* camera(CameraId id) const;

    BrainId createBrain(ICameraOutput& output, const BrainConfig& config);
    void destroyBrain(BrainId id);

    GroupId createTargetGroup(std::unique_ptr<TargetGroup> group);
    void destroyTargetGroup(GroupId id);

    void fixedUpdate(float fixedDeltaTime);
    void lateUpdate(float deltaTime);
    void manualUpdate(float deltaTime);

    void setWorldUp(Vec3 up);
    Vec3 worldUp() const;
    void resetCameraState();

    IEntityWorld& entityWorld();
    IPhysicsWorld& physicsWorld();       // 无物理时返回 Null
    ISplineProvider* splineProvider();   // 可能为 null
};
}
```

## Behavior Details

### 注册表

对齐 `Runtime/Core/VirtualCameraRegistry.cs`：

- `m_ActiveCameras`：所有 enabled 相机。
- 排序规则：
  - 优先级高者在前。
  - 优先级相同，`ActivationId` 大者在前（最近激活的优先）。
- `ActivationId` 在每次 enable/add 时递增。
- `m_AllCameras` 按嵌套层级分组：
  - 层级 0 为根相机。
  - 子相机层级 = 父相机层级 + 1。
  - 更新时从最高层级向 0 遍历，保证叶子先更新。
- 禁用/销毁时从活跃列表和层级列表移除。

### 更新顺序

对齐 `Runtime/Core/CameraUpdateManager.cs:84-144`：

1. 根据当前 UpdateFilter 决定 Fixed/Late/Smart。
2. Fixed 帧计数：同一固定帧只递增一次。
3. RoundRobin 索引：每个渲染帧只推进一次。
4. 从最高嵌套层级向下更新：
   - 跳过 `OutputChannel` 与 `channelMask` 不匹配的相机。
   - 若相机 live 或 `StandbyUpdate == Always`，更新。
   - 若是 RoundRobin 选中的相机且 enabled，更新。
5. 通过 `UpdateVirtualCamera` 保证同一帧不重复更新。

### 单相机更新

对齐 `Runtime/Core/CameraUpdateManager.cs:151-200`：

- Smart 模式：
  - 读取目标的 `lastChangeClock`（或 UpdateTracker 推断）。
  - 与当前 update clock 不符时跳过。
- 每个相机保存：
  - `lastUpdateFrame`
  - `lastUpdateFixedFrame`
  - `lastUpdateMode`
- 首次更新时故意设置 `+2` 让 frameDelta 为负，避免错误追赶。
- `frameDelta == 0 && lastUpdateMode == updateClock` → 已更新，跳过。
- 非单元测试模式且 `frameDelta > 0` → `deltaTime *= frameDelta`。
- 调用 `vcam.internalUpdateCameraState(worldUp, deltaTime)`。
- 更新状态记录。

### UpdateTracker

对齐 `Runtime/Core/UpdateTracker.cs`：

- 目标按 `EntityId` 跟踪。
- 每个目标有 30 帧窗口，统计 Fixed/Late 移动次数。
- 判定规则：
  - 默认 Late。
  - 若 Fixed 移动次数 > 3 且 Late 移动次数 < Fixed/3 → Fixed。
- 窗口结束时更新 `preferredUpdate`。
- C++ 适配：
  - 优先使用 `IEntityWorld::lastChangeClock(entity)`。
  - 若适配器不提供，`CameraSystem` 可在 Fixed/Late 更新前后采样目标变换，作为 fallback。
  - 不要依赖全局静态状态；`UpdateTracker` 由 `CameraSystem` 持有。

### Standby

- `Never`：只在 live 时更新。
- `Always`：每帧更新。
- `RoundRobin`：每个渲染帧推进一个索引，轮流更新非 live 相机。
- RoundRobin 索引跨帧保持，Reset 时归零。

### 生命周期

- `createCamera` 返回稳定句柄；对象存放在 slot map 中。
- `destroyCamera` 先从注册表移除，再销毁对象；之后访问返回 nullptr。
- `CameraSystem` 析构时按依赖顺序清理：
  1. Brains
  2. Cameras
  3. TargetGroups
  4. ImpulseManager
- `resetCameraState()`：
  - 所有相机 `previousStateIsValid = false`。
  - 所有 Brain 重置混合状态。

### 确定性

- 所有时间来自 `IEntityWorld`，不使用 `std::chrono`。
- 测试通过 `manualUpdate(dt)` 驱动，帧号由 `IEntityWorld::frame()` 提供。
- 不允许在更新路径使用全局随机；噪声/Impulse 使用显式种子。

## Tests

- 优先级：高优先级相机会被 Brain 选中；同优先级时最近激活者优先。
- 激活顺序：disable/enable 后 ActivationId 变化。
- 嵌套层级：子相机先于父相机更新。
- 更新时钟：
  - Fixed 模式只在该帧更新。
  - Late 模式只在该帧更新。
  - Smart 模式根据目标变化选择。
  - 多帧 deltaTime 追赶。
- Standby：
  - Never 不更新非 live 相机。
  - Always 每帧更新。
  - RoundRobin 轮流更新。
- 生命周期：destroy 后句柄无效；重复 destroy 安全。
- ChannelMask：不匹配的相机不更新。
- 手动更新：固定 dt 下状态可重复。

## Acceptance Checklist

- [x] 活跃相机排序与 `VirtualCameraRegistry` 一致。
- [x] 嵌套相机叶子优先更新。
- [x] 同一帧不重复更新。
- [x] Fixed/Late/Smart/Manual 行为正确。
- [x] Standby 三种模式正确。
- [x] 多帧追赶不产生负 dt。
- [x] 句柄销毁安全。
- [x] 场景测试全绿。

## 参考原项目（Cinemachine）

> 参考注册表排序、更新顺序、时钟和 Standby 语义；用 C++ 重写，不复制 C# 代码。

### 必读源码

- `com.unity.cinemachine/Runtime/Core/CinemachineCore.cs:21-403`
  - 全局服务、事件、时间覆盖、相机注册访问。
- `com.unity.cinemachine/Runtime/Core/CameraUpdateManager.cs:9-239`
  - `UpdateFilter`、`UpdateAllActiveVirtualCameras`、`UpdateVirtualCamera`、帧计数、RoundRobin。
- `com.unity.cinemachine/Runtime/Core/VirtualCameraRegistry.cs`
  - 活跃相机排序、嵌套层级、enable/disable/destroy。
- `com.unity.cinemachine/Runtime/Core/UpdateTracker.cs`
  - 30 帧窗口、Fixed/Late 偏好推断。
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs:28-839`
  - enable/disable、优先级、StandbyUpdate、PreviousStateIsValid、OnEnable/OnDisable。
- `com.unity.cinemachine/Runtime/Core/TargetPositionCache.cs`
  - 目标位置缓存；P1 可以只保留接口占位。
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineBrain.cs:200-400`
  - Brain 如何触发 vcam 更新和手动更新。

### 必读测试

- `com.unity.cinemachine/Tests/Runtime/PriorityTests.cs`
- `com.unity.cinemachine/Tests/Runtime/PriorityTestsImplicitCastingOperators.cs`
- `com.unity.cinemachine/Tests/Runtime/ManualUpdateTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CinemachineBrainSetupTest.cs`
- `com.unity.cinemachine/Tests/Runtime/UpdateCounterForTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CinemachineEventRegistrationTests.cs`
- `com.unity.cinemachine/Tests/Editor/BlendManagerTests.cs`

### 需要核对的行为

- 活跃相机排序使用 `Priority.Value` 和 `ActivationId` 的精确比较。
- `CameraUpdateManager` 的 Fixed 帧计数和 `s_LastFixedUpdateContext` 语义。
- RoundRobin 索引推进时机（每个渲染帧一次）。
- `frameDelta` 和 `deltaTime *= frameDelta` 的条件。
- `UnitTestMode` 对追赶的影响。
- `StandbyUpdateMode.RoundRobin` 与嵌套层级的交互。
- `TargetPositionCache` 在运行时与 Editor 下的不同行为。

### 差异记录

- 原项目使用静态 `CinemachineCore` / `CameraUpdateManager`；CineRig 使用实例 `CameraSystem`，便于多世界和测试。
- 原项目使用 `Transform` 指针跟踪目标；CineRig 使用 `EntityId` + `IEntityWorld`。
- 原项目 `UpdateTracker` 用全局静态字典；CineRig 由 `CameraSystem` 持有，避免跨世界污染。
- `TargetPositionCache` 的 Editor 录制/回放属于编辑器功能，P1 不实现；只保留运行时直接读取路径。
- P1-01 先引入最小 `VirtualCamera` 基类（`include/cinerig/camera/virtual_camera.hpp`），提供注册表、优先级、Standby、live 标记和 `internalUpdateCameraState()`；P1-02 在同一类型上追加组件管线、目标解析和 transform 回写，不替换注册表句柄。
- 句柄使用单调递增的 64 位 ID + `unordered_map` 句柄表；销毁后 ID 不复用，重复销毁安全。没有实现带 generation 的 slot map，因为当前 API 的 `CameraId` 已经是不透明句柄，稳定性和测试行为不依赖复用。
- `VirtualCameraRegistry` 在 enable/disable/parent 变化时重建 nesting levels；活跃列表按 `priority.effectiveValue()` 降序、`activationId` 降序惰性排序。`activationId` 从 0 开始递增，最近 enable 的相机优先。
- `CameraSystem::fixedUpdate()` / `lateUpdate()` 是显式时钟入口；`manualUpdate()` 在配置为 `Fixed` 时用 Fixed，否则用 Late。Smart 过滤只在 `UpdateMethod::Smart` 时启用。测试可用 `manualUpdate()` + `FakeEntityWorld` 固定帧和 dt。
- `UpdateManager` 的 `frameDelta` 从 `IEntityWorld::frame(clock)` 计算；首次状态故意设置 `+2`，`frameDelta > 0` 时按原始项目逻辑乘以 dt；`setUnitTestMode(true)` 可关闭追赶。Fixed 帧只计一次，Late 帧按更新入口去重。
- Smart 优先使用 `UpdateTracker` 的 30 帧窗口推断；若 `IEntityWorld::lastChangeClock(entity) == Fixed`，立即覆盖为 Fixed。没有目标的相机在 Smart 下按 Late 处理；目标解析只支持 Entity，Camera/Group 解析留给 P1-02/P3。
- RoundRobin 每个 Late frame 推进一次，沿用原项目的 level/sub-index 选择方式；`Always` 和 live 相机不受 RoundRobin 影响。live 由 `VirtualCamera::setLive()` 标记，P1-03 Brain 接入后会成为真实混合状态。
- `createBrain()` 目前只保存输出引用和 `BrainConfig`，不执行混合；P1-03 实现 `CameraBrain` 时会接管该入口。`createTargetGroup()` 已注册 `TargetGroup` 句柄，目标解析留给后续任务。
- `CameraSystem` 按 Brains → Cameras → TargetGroups 顺序释放；没有独立 `ImpulseManager` 类型，P3 再加入并调整清理顺序。
- `resetCameraState()` 清除所有相机 `previousStateIsValid`、更新状态、RoundRobin 索引和 UpdateTracker 数据；Brain 混合状态在 P1-03 接入后重置。
- 未在更新路径使用 `std::chrono`、全局静态状态或随机数；帧、时间、worldUp 都来自 `IEntityWorld` / 显式参数。

## 实现记录

- 新增 `include/cinerig/camera/virtual_camera.hpp`、`src/camera/virtual_camera.cpp`：最小运行时相机基类、优先级、输出通道、Standby、live 标记、父相机、稳定 CameraId、activationId。
- 新增 `include/cinerig/runtime/camera_system.hpp`、`src/runtime/camera_system.cpp`：`CameraSystemConfig`、`BrainConfig`、Camera/Brain/Group 句柄表、生命周期、更新入口、实体/物理/样条服务、Smart/tracker 协调。
- 新增 `include/cinerig/runtime/virtual_camera_registry.hpp`、`src/runtime/virtual_camera_registry.cpp`：活跃列表惰性排序、activation 序列、按 nesting level 分组。
- 新增 `include/cinerig/runtime/update_manager.hpp`、`src/runtime/update_manager.cpp`：Fixed/Late/Smart/Manual 时钟、每相机帧状态、多帧追赶、RoundRobin、channel mask、Standby、unit test mode。
- 新增 `include/cinerig/runtime/update_tracker.hpp`、`src/runtime/update_tracker.cpp`：按 EntityId 的 30 帧 Fixed/Late 窗口、目标位置采样、默认 Late。
- `include/cinerig/cinerig.hpp` 已暴露新增 camera/runtime 头文件；`CMakeLists.txt` 已加入 5 个新的 core 源文件。
- 新增测试：
  - `camera_system_priority_test.cpp`：优先级、同优先级 activation、disable/enable、嵌套叶子优先。
  - `camera_system_update_clock_test.cpp`：Fixed/Late 单帧去重、Smart 偏好、多帧追赶、负 dt、Manual 确定性。
  - `camera_system_standby_test.cpp`：Never/Always/RoundRobin、channel mask。
  - `camera_system_lifecycle_test.cpp`：句柄销毁/重复销毁、brain/group 生命周期、parent/cycle、reset、服务适配器。
- Windows Debug：95/95 通过（70 unit + 19 scenario + 6 golden）。
- Windows Release：95/95 通过。
- GLM 配置：95/95 通过。
- `CINERIG_USE_CUSTOM_TEST_MAIN=ON`：95/95 通过。
- `CINERIG_BUILD_TESTS=OFF`：仅构建 core，不配置测试。
- 安装后的 `find_package(CineRig CONFIG)` 消费者构建和运行通过。
- 未在 Linux 实机执行。
