# P1-03 Brain and Blending

Status: completed  
Depends on: P1-01, P1-02, P0-04  
Blocks: P1-07

## Goal

实现 Brain：从优先级队列选择 live 相机，创建和推进混合，处理覆盖栈、激活/反激活事件，并把最终 `CameraState` 应用到 `ICameraOutput`。

## Scope

- `CameraBrain`：
  - 输出相机、ChannelMask、UpdateMethod、BlendUpdateMethod。
  - WorldUpOverride/WorldUp provider。
  - DefaultBlend、CustomBlends、`GetBlendOverride`、`GetCustomBlender`。
  - LensModeOverride。
  - 手动更新。
- `BlendManager`：
  - 当前根混合 `m_CurrentLiveCameras`。
  - `ActiveVirtualCamera`、`ActiveBlend`、`IsBlending`。
  - 激活/反激活事件、BlendCreated、BlendFinished、CameraUpdated。
- `CameraBlendStack`：
  - 覆盖栈帧（override frames）。
  - Snapshot（FreezeWhenBlendingOut / InheritPosition）。
  - NestedBlendSource / FrozenBlendSource / SnapshotBlendSource。
  - 混合反向取消（backing out of blend）。
- `BlendDefinition` 查找：
  - 精确 `from/to` 名称匹配。
  - `**ANY CAMERA**` 通配。
  - 默认 blend。
  - 全局 override 回调。
- `ICameraOutput` 应用：
  - NoPosition/NoOrientation/NoLens hint。
  - 输出相机 lens 继承与 LensModeOverride。
- 没有 live 相机时的 fallback 状态。

## Non-goals

- 不实现具体 Body/Aim 组件（P2）。
- 不实现 Manager 相机的选择逻辑（P1-04 只做基类）。
- 不实现 Timeline 资产；只实现通用 OverrideStack API。
- 不实现 Editor 调试文本。

## Deliverables

```text
include/cinerig/brain/
├── camera_brain.hpp
├── blend_manager.hpp
├── camera_blend_stack.hpp
├── blend_lookup.hpp
└── brain_events.hpp
src/brain/
├── camera_brain.cpp
├── blend_manager.cpp
├── camera_blend_stack.cpp
└── blend_lookup.cpp
tests/scenario/
├── brain_priority_test.cpp
├── brain_blend_test.cpp
├── brain_nested_blend_test.cpp
├── brain_override_stack_test.cpp
└── brain_events_test.cpp
```

## API Sketch

```cpp
namespace cinerig
{
struct BrainConfig
{
    OutputChannels channelMask = OutputChannels::Default;
    UpdateMethod updateMethod = UpdateMethod::Smart;
    enum class BlendUpdateMethod { Fixed, Late };
    BlendUpdateMethod blendUpdateMethod = BlendUpdateMethod::Late;
    BlendDefinition defaultBlend{ BlendStyle::EaseInOut, 2.0f };
    bool ignoreTimeScale = false;
};

struct LensModeOverrideSettings
{
    bool enabled = false;
    LensSettings::OverrideMode defaultMode = LensSettings::OverrideMode::Perspective;
};

struct ActivationEventParams
{
    ICinemachineMixer* origin = nullptr;
    ICinemachineCamera* outgoingCamera = nullptr;
    ICinemachineCamera* incomingCamera = nullptr;
    bool isCut = false;
    Vec3 worldUp{};
    float deltaTime = 0.0f;
};

class CameraBrain
{
public:
    CameraBrain(CameraSystem& system, ICameraOutput& output, const BrainConfig& config);

    void update(float deltaTime);
    void fixedUpdate(float fixedDeltaTime);
    void manualUpdate(float deltaTime, uint64_t frame);

    ICinemachineCamera* activeVirtualCamera() const;
    bool isBlending() const;
    const Blend* activeBlend() const;
    const CameraState& state() const;

    int setCameraOverride(
        int overrideId, int priority,
        ICinemachineCamera* camA, ICinemachineCamera* camB,
        float weightB, float deltaTime);
    void releaseCameraOverride(int overrideId);

    void resetState();
    bool isLiveChild(const ICinemachineCamera* cam) const;

    void applyToOutput();
};
}
```

## Behavior Details

### 选择 live 相机

对齐 `Runtime/Behaviours/CinemachineBrain.cs:684-694`：

- 遍历注册表的活跃相机（已按优先级/激活顺序排序）。
- 返回第一个 `outputChannel & channelMask != 0` 的相机。
- 没有匹配 → `nullptr`。

### 更新流程

对齐 `Runtime/Behaviours/CinemachineBrain.cs:580-638`：

1. 设置当前更新帧。
2. 计算有效 deltaTime：
   - 手动 override > 0 时使用 override。
   - Solo 相机存在时使用 unscaled deltaTime。
   - `ignoreTimeScale` 时使用 unscaled。
   - 否则使用 frame delta 或 fixed delta。
3. 调用 `CameraSystem` 更新 vcams：
   - Fixed / Late / Smart 过滤器。
   - Smart 模式先更新 `UpdateTracker`。
4. `blendManager.updateRootFrame(topCamera, worldUp, deltaTime)`。
5. `blendManager.computeCurrentBlend()`。
6. `blendManager.processActiveCamera(...)`。
7. `applyToOutput()`。

### BlendManager

对齐 `Runtime/Core/BlendManager.cs`：

- `m_CurrentLiveCameras` 是根混合；`CamB` 始终是当前相机。
- `ActiveVirtualCamera` 深度遍历嵌套 blend，返回最内层有效 `CamB`。
- `ActiveBlend` 仅在 `CamA != null && !IsComplete` 时非空。
- `IsBlending` = `ActiveBlend != null`。
- `IsLive(cam)` = 根混合 `Uses(cam)`。
- `ProcessActiveCamera`：
  1. 对上一帧 live 但本帧不 live 的相机发送 `CameraDeactivatedEvent`。
  2. 若 incoming != outgoing：
     - 若有混合，outgoing 用 `NestedBlendSource(ActiveBlend)` 包装并更新。
     - 构造 `ActivationEventParams`：`IsCut = !IsBlending`。
     - 调用 `incoming.OnCameraActivated`、`mixer.OnCameraActivated`、`CameraActivatedEvent`。
     - 再次 `incoming.updateCameraState`。
  3. 收集本帧 live 相机集合，供下一帧反激活事件使用。
  4. 记录 `m_PreviousActiveCamera`、`m_WasBlending`。
- 若 incoming == outgoing 且之前在混合、现在 `CamA == null` → `BlendFinishedEvent`。

### 创建混合

对齐 `Runtime/Core/CameraBlendStack.cs:232-369`：

- 根帧 `CamB` 是当前 live 相机；`CamA` 是 outgoing。
- 若 activeCamera 变化：
  - 查 `LookupBlend(outgoing, incoming)`。
  - 若 blend 曲线非空且时长 > epsilon → 创建混合。
  - 若正在反向取消同一混合：
    - 计算 `normalizedBlendPosition`。
    - 缩短新混合时长，跳过已完成部分。
  - 设置 `Source.CamA/CamB/BlendCurve/Duration/TimeInBlend/CustomBlender`。
- 若已有混合未完成：
  - `FreezeWhenBlendingOut` → 使用 snapshot。
  - incoming 带 `InheritPosition` 且已在 outgoing blend 中 → snapshot。
  - 避免嵌套过深：若 `CamA` 已是嵌套 blend，且其 `CamA` 也是嵌套，则把内层 `CamA` 转成 snapshot。
  - 非 snapshot → 把旧混合复制成 `NestedBlendSource`；反向取消时用 `FrozenBlendSource`。
- `BlendCreatedEvent` 在 duration > 0 时发出；事件处理器不能改变相机，回调后恢复 CamA/CamB。
- 每帧推进 `TimeInBlend`；deltaTime < 0 时直接完成；完成后清空混合。
- 嵌套 blend 递归推进。

### Snapshot

- `SnapshotBlendSource` 保存相机状态快照，`FreezeWhenBlendingOut` 清除。
- `GetSnapshotIfAppropriate(cam, weight)`：
  - 相机为空或不带 `FreezeWhenBlendingOut` → 清空 snapshot，返回原相机。
  - 否则，若 source 变化或 weight 回退 → 拍新 snapshot。
- 用于：
  - 从 Freeze 相机混合出去。
  - InheritPosition 相机已在 outgoing blend 中。
  - 嵌套过深时冻结内层。
- Snapshot 名称格式 `"*" + source.name`。

### Override Stack

对齐 `Runtime/Core/CameraBlendStack.cs:123-199, 379-501`：

- 帧 0 是游戏逻辑；帧 1..N 是覆盖。
- `setCameraOverride`：
  - `overrideId < 0` 时自动分配 ID。
  - 查找已有 ID；否则按 priority 插入。
  - 设置 `DeltaTimeOverride`、`Source.TimeInBlend = weightB`、CamA/CamB、CustomBlender。
  - 新帧默认 duration=1、linear curve。
- `releaseCameraOverride(id)`：删除对应帧。
- `processOverrideFrames`：
  - 从低到高处理活跃帧。
  - 空 CamA/CamB 时用前一活跃帧或 snapshot 补齐。
  - 输出最上层活跃帧的 blend。
- `getDeltaTimeOverride`：返回最上层活跃帧的 deltaTime，否则 -1。
- `resetState`：清空根帧混合和 CamA/CamB。

### Blend 查找

对齐 `Runtime/Core/CinemachineBlenderSettings.cs`：

- 精确匹配 `from == outgoing.name && to == incoming.name` → 返回。
- `**ANY CAMERA**` 通配：
  - `from` 为 any、`to` 匹配 → 记录 `anyToMe`。
  - `from` 匹配、`to` 为 any → 记录 `meToAny`。
  - 两者都是 any → 覆盖默认 blend。
- 优先级：精确 > anyToMe > meToAny > default。
- 最后调用全局 `GetBlendOverride`（如果存在）。

### 应用输出

对齐 `Runtime/Behaviours/CinemachineBrain.cs:699-795`：

- 有 solo 相机 → 用 solo state。
- 有 active camera → 用 blendManager state。
- 没有 active camera：
  - 使用输出相机当前 transform/lens。
  - 设置 `NoTransform | NoLens` hint，表示不主动修改。
- `applyToOutput`：
  - `finalPosition` / `finalOrientation`，除非 `NoPosition` / `NoOrientation`。
  - `NoLens` 时不写 lens。
  - `LensModeOverride.Enabled` 且 lens.modeOverride != None 时：
    - 设置 orthographic/physical 模式。
  - 否则若 `defaultMode != None` 使用默认模式。
  - 物理镜头时填充 sensor/gate/focal/focus/ISO/shutter/aperture/blades/curvature/barrel/anamorphism。
  - 最后发 `CameraUpdatedEvent`。

### Lens 继承

- 当 `lens.modeOverride == None`：
  - `orthographic`、`physicalCamera`、`aspect` 从 `ICameraOutput::outputLens()` / `aspect()` 继承。
  - 在 Brain 应用前或 vcam 更新时填充；实现记录中确定。
- P1 只实现 perspective/orthographic 继承；物理镜头字段完整传递。

## Tests

- 优先级选择：多个相机、channelMask 过滤、无相机 fallback。
- Cut：duration 0 时立即切换，`IsBlending == false`。
- EaseInOut 2s：t=0/1/2 的权重和状态。
- 混合中途切换：嵌套 blend、`Uses`、event 顺序。
- FreezeWhenBlendingOut：snapshot 状态不随源相机变化。
- InheritPosition：进入时位置与 outgoing 一致。
- 反向取消混合：时长和起点调整。
- Override stack：优先级、weightB、release、deltaTime override。
- Blend lookup：精确、通配、默认、全局 override。
- 事件：activated/deactivated/blendCreated/blendFinished/cameraUpdated 顺序。
- No active camera：输出保持原 transform/lens。
- NoLens/NoPosition/NoOrientation hint 的应用。

## Acceptance Checklist

- [x] 优先级和激活顺序与注册表一致。
- [x] Cut/EaseInOut/Linear/Custom 正确。
- [x] 嵌套混合和反向取消正确。
- [x] Snapshot 行为正确。
- [x] OverrideStack 可用。
- [x] Blend 查找优先级正确。
- [x] 事件顺序正确。
- [x] 输出相机 lens 继承和模式覆盖正确。
- [x] 场景测试全绿。

## 参考原项目（Cinemachine）

> 参考选择、混合、覆盖栈、事件和输出应用；用 C++ 重写，不复制 C# 代码。

### 必读源码

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineBrain.cs:29-797`
  - Brain 字段、更新流程、`TopCameraFromPriorityQueue`、`LookupBlend`、`ProcessActiveCamera`、`PushStateToUnityCamera`、`SetCameraOverride`、`IsLiveChild`。
- `com.unity.cinemachine/Runtime/Core/BlendManager.cs`
  - 当前混合、`ActiveVirtualCamera`、`ProcessActiveCamera`、事件。
- `com.unity.cinemachine/Runtime/Core/CameraBlendStack.cs`
  - 根帧、覆盖栈、snapshot、嵌套混合、反向取消。
- `com.unity.cinemachine/Runtime/Core/CinemachineBlend.cs`
  - Blend、BlendDefinition、NestedBlendSource。
- `com.unity.cinemachine/Runtime/Core/CinemachineBlenderSettings.cs`
  - 自定义 blend 查找、通配。
- `com.unity.cinemachine/Runtime/Core/CinemachineCore.cs:194-240`
  - 事件定义。
- `com.unity.cinemachine/Runtime/Core/LensSettings.cs`
  - ModeOverride、物理镜头字段。

### 必读测试

- `com.unity.cinemachine/Tests/Runtime/CamerasBlendingTests.cs`
- `com.unity.cinemachine/Tests/Editor/BlendManagerTests.cs`
- `com.unity.cinemachine/Tests/Runtime/BrainTargetOverrideTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CinemachineEventRegistrationTests.cs`
- `com.unity.cinemachine/Tests/Runtime/PriorityTests.cs`
- `com.unity.cinemachine/Tests/Runtime/ManualUpdateTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CheckPhysicalCameraProperties.cs`

### 需要核对的行为

- `ProcessActiveCamera` 中 outgoing/incoming 事件的精确顺序。
- `IsCut = !IsBlending` 的时机。
- 反向取消混合时 `normalizedBlendPosition` 和 duration 的计算。
- `SnapshotBlendSource` 的 weight 回退条件。
- 嵌套过深时的 snapshot 替换。
- `BlendCreatedEvent` 回调后恢复 CamA/CamB。
- `NoPosition` / `NoOrientation` / `NoLens` 对输出应用的影响。
- `LensModeOverride` 与 `LensSettings.ModeOverride` 的优先级。
- 物理镜头字段的 FOV->focal length 转换。

### 差异记录

- 原项目 Brain 是 `MonoBehaviour`，由 Unity 生命周期驱动；CineRig 由 `CameraSystem` 显式调用。
- 原项目使用静态事件 `CinemachineCore.*`；CineRig 使用 Brain/System 实例上的回调或事件总线，避免跨世界污染。
- 原项目 `SoloCamera` 是全局编辑器/调试功能；P1 可先不实现，P5 调试 HUD 再补。
- 原项目输出到 Unity Camera；CineRig 输出到 `ICameraOutput`，Adapter 负责物理镜头/投影。
- 原项目 `ControlledObject` 可重定向到其他 GameObject；CineRig 直接绑定 `ICameraOutput`，不实现重定向。
- 事件使用实例 `BrainEvents`，监听器通过 `CameraBrain::events()` 注册；没有跨 Brain 的全局事件总线。
- `CameraSystem::createBrain` 负责创建和持有 `CameraBrain`；`CameraSystem::lateUpdate/fixedUpdate/manualUpdate` 会按 Brain 的 `blendUpdateMethod` 驱动已注册 Brain，Brain 直接调用 `update()` 时只更新相机并处理自身。
- Brain 会在更新前把上一帧 active camera 标记为 live，并在每帧处理后同步 blend 中相机的 live 标记，使 Standby RoundRobin 不会跳过当前 live 相机。
- Snapshot/Frozen/Nested source 由 `CameraBlendStack` 持有；引用在 reset 前保持稳定，当前实现不逐帧回收临时 source。
- 无 active camera 时向 `ICameraOutput` 发送带 `NoTransform | NoLens` 的 fallback state，`hasCamera=false`，由 adapter 保留自身相机。
- `LensModeOverride` 已实现 perspective/orthographic 模式切换和输出 lens 的 orthographic/physical/aspect 继承；物理 FOV→focal length 转换仍由输出 adapter 负责。
- `ignoreTimeScale` 和 Solo 相机未实现；CineRig 当前没有 unscaled time 或全局 Solo 状态。`OnTransitionFromCamera` 继续按 P1-02 的单次更新实现。
- `CameraBlendStack` 的临时 source 容器使用 stack 所有权，避免 `Blend` 持有可变所有权；这是与原项目 C# 对象生命周期的 C++ 差异。

## 实现记录

- 新增 `include/cinerig/brain/`：
  - `brain_events.hpp`：实例事件类型和注册/派发接口。
  - `blend_lookup.hpp`：精确、`**ANY CAMERA**` 通配、默认和 override 回调。
  - `camera_blend_stack.hpp`：根帧、override frames、Snapshot/Frozen/Nested source。
  - `blend_manager.hpp`：active camera、active blend、事件处理和 live 同步。
  - `camera_brain.hpp`：Brain 选择、更新、输出应用、lens mode override。
- 新增 `src/brain/`：`blend_lookup.cpp`、`camera_blend_stack.cpp`、`blend_manager.cpp`、`camera_brain.cpp`。
- `ICinemachineCamera` 增加默认 `updateCameraState()` / `onCameraActivated()`；`VirtualCamera` 覆盖并接入原有 transition。
- `CameraSystem` 的 BrainEntry 改为持有 `CameraBrain`，新增 `brain(id)`、`updateCameras(...)`、Brain 自动驱动和 reset 同步。
- `CMakeLists.txt` 已加入 4 个 Brain 源文件；`cinerig.hpp` 已暴露 Brain 头文件。
- 新增测试：
  - `tests/scenario/brain_priority_test.cpp`：优先级、ChannelMask、无相机 fallback、active 相机持续更新、系统驱动、destroy 安全。
  - `tests/scenario/brain_blend_test.cpp`：Cut、EaseInOut、BlendHint、lens 继承和 LensModeOverride。
  - `tests/scenario/brain_nested_blend_test.cpp`：嵌套混合、Freeze snapshot、InheritPosition、反向取消。
  - `tests/scenario/brain_override_stack_test.cpp`：override weight/priority/release/deltaTime、Blend lookup 通配和 override。
  - `tests/scenario/brain_events_test.cpp`：activation/deactivation/blend created/finished/camera updated 顺序和事件防篡改。
  - `tests/support/brain_test_support.hpp`：Brain 测试 rig。
- Windows Debug/Release、GLM、自定义 test main 均 129/129 通过（70 unit + 53 scenario + 6 golden）。
- `CINERIG_BUILD_TESTS=OFF` 仅构建 core；安装后消费者构建/运行通过。
- 未在 Linux 实机执行。

