# P2-01 Target Tracking and Full Follow

Status: completed  
Depends on: P0-02, P0-03, P1-02, P1-06  
Blocks: P2 OrbitalFollow, PositionComposer, rotation composer extensions

## Goal

实现原项目 `TargetTracking.cs` 的 `BindingMode`、`TrackerSettings` 和
`Tracker`，并用它替换 P1 的最小 WorldSpace Follow，使 Follow 支持完整的
目标坐标系绑定、旋转/位置阻尼、最小目标距离、warp 和 force-position 行为。

## Scope

- 新增 `core::BindingMode`：
  - `LockToTargetOnAssign`
  - `LockToTargetWithWorldUp`
  - `LockToTargetNoRoll`
  - `LockToTarget`
  - `WorldSpace`
  - `LazyFollow`
- 新增 `core::AngularDampingMode`：`Euler`、`Quaternion`。
- 新增 `core::TrackerSettings`：
  - 默认 `WorldSpace`，位置/旋转阻尼 `(1,1,1)`，四元数阻尼 `1`。
  - `validate()`、`effectivePositionDamping()`、`effectiveRotationDamping()`、
    `maxDampTime()`。
- 新增 `core::Tracker`：
  - `initStateInfo()`：目标变更或时间跳变时重置内部状态，并记录
    `LockToTargetOnAssign` 的目标旋转。
  - `getReferenceOrientation()`：按 binding mode 计算目标参考旋转。
  - `trackTarget()`：参考旋转阻尼、偏移阻尼、offset 变化补偿。
  - `getOffsetForMinimumTargetDistance()`：XZ 平面最小距离修正。
  - `onTargetObjectWarped()`、`onForceCameraPosition()`。
- `components::Follow`：
  - 新增 `TrackerSettings trackerSettings`，用 `Tracker` 驱动位置。
  - 保留 `followOffset`；新增 `effectiveOffset()`（LazyFollow 时将偏移限制在
    camera 后侧）。
  - 支持 `onTargetObjectWarped()`、`forceCameraPosition()`、完整
    `maxDampTime()`，并更新 `referenceUp`。
- 更新现有 Follow 调用点和测试到 `trackerSettings.positionDamping`。
- 更新 `01_follow` 示例：目标增加缓慢 yaw/roll，`B` 循环 binding mode，
  HUD 显示当前模式，便于手动验证。

## Non-goals

- 不在本任务实现 OrbitalFollow、PositionComposer、PanTilt、ThirdPerson。
- 不实现 Lookahead/PositionPredictor；留给后续 composer 任务。
- 不实现 TargetGroup 的创建/管理；Tracker 只消费 VirtualCamera 已解析的目标。
- 不实现 Unity 的物理、输入或 Timeline 集成。

## API Sketch

```cpp
namespace cinerig
{

enum class BindingMode { /* ... */ };
enum class AngularDampingMode { Euler, Quaternion };

struct TrackerSettings
{
    BindingMode bindingMode = BindingMode::WorldSpace;
    math::Vec3 positionDamping = math::Vec3::one;
    AngularDampingMode angularDampingMode = AngularDampingMode::Euler;
    math::Vec3 rotationDamping = math::Vec3::one;
    float quaternionDamping = 1.0f;

    static const TrackerSettings Default;
    void validate();
    math::Vec3 effectivePositionDamping() const;
    math::Vec3 effectiveRotationDamping() const;
    float maxDampTime() const;
};

struct TrackerTarget
{
    TargetRef ref;
    math::Vec3 position;
    math::Quat rotation;
    bool valid = false;
};

class Tracker
{
public:
    void initStateInfo(
        const TrackerTarget& target,
        float deltaTime,
        bool previousStateIsValid,
        const TrackerSettings& settings,
        math::Vec3 targetOffset,
        math::Vec3 worldUp,
        const CameraState& cameraState);

    math::Quat getReferenceOrientation(
        const TrackerTarget& target,
        BindingMode bindingMode,
        math::Vec3 targetOffset,
        math::Vec3 worldUp,
        const CameraState& cameraState) const;

    void trackTarget(
        const TrackerTarget& target,
        float deltaTime,
        bool previousStateIsValid,
        math::Vec3 worldUp,
        math::Vec3 desiredCameraOffset,
        const TrackerSettings& settings,
        math::Vec3 targetOffset,
        CameraState& cameraState,
        float followTargetAttachment,
        math::Vec3& outTargetPosition,
        math::Quat& outTargetOrient);

    math::Vec3 getOffsetForMinimumTargetDistance(
        float followTargetAttachment,
        math::Vec3 dampedTargetPos,
        math::Vec3 cameraOffset,
        math::Vec3 cameraForward,
        math::Vec3 worldUp,
        math::Vec3 actualTargetPos);

    void onTargetObjectWarped(math::Vec3 positionDelta);
    void onForceCameraPosition(
        const TrackerTarget& target,
        const TrackerSettings& settings,
        math::Vec3 targetOffset,
        const CameraState& oldState,
        const CameraState& newState);

    const math::Vec3& previousTargetPosition() const noexcept;
    const math::Quat& previousReferenceOrientation() const noexcept;
};

} // namespace cinerig
```

## Behavior Details

- `WorldSpace`：参考旋转恒为单位四元数，Follow 偏移固定在世界空间。
- `LockToTargetOnAssign`：目标引用变化或首次有效时记录目标旋转，后续保持该
  旋转作为参考，不跟随目标旋转变化。
- `LockToTargetWithWorldUp`：将目标 forward 投影到 world-up 平面后重新
  `lookRotation`，去除 pitch/roll。
- `LockToTargetNoRoll`：使用目标 forward 与 world-up 计算无 roll 参考。
- `LockToTarget`：直接使用目标旋转。
- `LazyFollow`：相机位置只沿目标局部 forward 的反方向跟随；参考旋转由目标
  位置加局部偏移指向相机，且 X 轴位置阻尼为 0。
- `deltaTime < 0` 或 `previousStateIsValid == false` 时全部 snap，不应用阻尼。
- 阻尼在相机偏移局部空间计算；offset 突变大于阈值时用
  `safeFromToRotation()` 保持相机相对目标的位置连续。
- 目标切换时保留上一帧相机位置，仅更新 on-assign 旋转和跟踪状态。
- `forceCameraPosition()` 只重建 Tracker 内部历史，不改变最终传给 Brain 的
  forced state。

## Tests

- `tests/unit/target_tracking_test.cpp`
  - WorldSpace 忽略目标旋转。
  - LockToTarget 随目标旋转旋转 offset。
  - LockToTargetWithWorldUp / NoRoll 去除不必要旋转。
  - LockToTargetOnAssign 在目标旋转变化后保持 assign 旋转。
  - LazyFollow 使用目标局部偏移且 X 位置阻尼为 0。
  - 零阻尼即时到达；正阻尼位于旧位置与目标之间；负 dt snap。
  - Euler/Quaternion 旋转阻尼。
  - 最小目标距离修正和 target warp。
  - settings 默认值、validate、effective damping、maxDampTime。
- `tests/unit/follow_binding_test.cpp`
  - Follow 通过 `Tracker` 在 WorldSpace/LockToTarget 下的期望位置。
  - LazyFollow 的 `effectiveOffset()` 约束。
  - warp 和 force-position 后不再跳变。
- 现有 Follow 场景/示例测试迁移到 `trackerSettings.positionDamping`。
- 全部核心和示例测试通过。

## Acceptance Checklist

- [x] 六种 BindingMode 均有实现和测试。
- [x] Tracker 状态在首帧、负 dt、目标切换时正确重置。
- [x] Follow 使用 Tracker 的参考旋转/位置阻尼和最小距离。
- [x] warp/force-position 不产生位置跳变。
- [x] 现有示例仍可构建、运行。
- [x] 核心和示例测试全绿。

## 参考原项目（Cinemachine）

### 必读源码

- `com.unity.cinemachine/Runtime/Core/TargetTracking.cs`
  - `BindingMode`、`AngularDampingMode`、`TrackerSettings`。
  - `Tracker.InitStateInfo()`、`GetReferenceOrientation()`、`TrackTarget()`。
  - `GetOffsetForMinimumTargetDistance()`、`OnTargetObjectWarped()`、
    `OnForceCameraPosition()`。
- `com.unity.cinemachine/Runtime/Components/CinemachineFollow.cs`
  - `EffectiveOffset`、`MutateCameraState()`、`ForceCameraPosition()`。
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs`
  - `DetachedFollowTargetDamp()`、`FollowTargetAttachment`。
- `com.unity.cinemachine/Runtime/Core/UnityVectorExtensions.cs`
  - `SafeFromToRotation()`、`ProjectOntoPlane()`。

### 必读测试

- `com.unity.cinemachine/Tests/Runtime/FollowForcePositionTests.cs`
- `com.unity.cinemachine/Tests/Runtime/FreelookForcePositionTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CameraPositionTests.cs`

### 差异记录

- C++ 用 `TrackerTarget` 显式传递目标 ref/位置/旋转，避免 core 反向依赖
  `VirtualCamera`。
- Unity 用零四元数作为 previous-reference 未初始化哨兵；C++ 使用显式
  `hasPreviousReferenceOrientation_` 标志。
- `followTargetAttachment` 由调用方传入；本任务未在 `VirtualCamera` 上新增
  detached-damp helper，Tracker 内部按原项目公式处理 attachment。
- 阻尼空间和最小距离按原项目公式实现，但坐标约定使用 C++ 右手系
  （`forward = -Z`）。
- `ForceCameraPosition` 在 C++ 中由 `Follow` 组件重放 Tracker 历史，最终
  forced state 仍由 `VirtualCamera` 写回。
- Follow 的测试/示例 API 从 `positionDamping` 迁移到
  `trackerSettings.positionDamping`；这是 P2 对齐原项目 `TrackerSettings` 的
  显式变更。
- `01_follow` 已增加 binding mode 可视化；默认仍为 `WorldSpace`，不改变
  P1 示例的默认行为。
- 本任务不包含 Lookahead、TargetGroup 管理、OrbitalFollow 或 composer 扩展。

## 实现记录

### 已实现

- 新增 `include/cinerig/core/tracker.hpp`、`src/core/tracker.cpp`：
  - `BindingMode` 六种模式、`AngularDampingMode`、`TrackerSettings`。
  - `TrackerTarget` 显式承载目标 ref/位置/旋转。
  - `Tracker::initStateInfo()`、`getReferenceOrientation()`、`trackTarget()`。
  - `getOffsetForMinimumTargetDistance()`、`onTargetObjectWarped()`、
    `onForceCameraPosition()`、`reset()`。
  - `TrackerSettings::validate()`、effective damping、max damp time。
- `include/cinerig/cinerig.hpp` 和 `CMakeLists.txt` 已注册 Tracker。
- `components::Follow` 已改为由 Tracker 驱动：
  - 新增 `trackerSettings`、`effectiveOffset()`。
  - 支持六种 binding mode、参考/位置阻尼、最小目标距离。
  - 实现 `onTargetObjectWarped()`、`forceCameraPosition()`、`maxDampTime()`。
  - `LazyFollow` 将 offset 的 X 清零、Z 限制为负值。
- 新增测试：
  - `tests/unit/target_tracking_test.cpp`：settings、六种模式、Euler/Quaternion
    旋转阻尼、位置阻尼、负 dt、最小距离、warp。
  - `tests/unit/follow_binding_test.cpp`：LockToTarget、LazyFollow offset、
    force-position、target warp。
  - 现有 Follow 单元/场景/示例测试迁移到 `trackerSettings.positionDamping`。
- `examples/01_follow` 已更新：
  - 目标沿 Lissajous 路径移动并缓慢 yaw/roll。
  - `B` 循环 `WorldSpace`、`LockToTarget`、`NoRoll`、`WithWorldUp`、
    `OnAssign`、`LazyFollow`。
  - HUD 显示 binding mode；切换时重置相机状态。
  - README 和 examples README 更新为 P2-01 行为。

### 验证

- 核心 Debug/Release：**176/176 passed**（95 unit + 75 scenario + 6 golden）。
- 示例 Debug/Release：**194/194 passed**；关闭 smoke 后为 **191/191**
  （包含 15 个 example 测试）。
- `cinerig_01_follow --frames 2 --hidden --no-vsync` 退出 0。
- 可见窗口确认：`B` 切换到 `Binding: LockToTarget`，旋转目标的 offset 随
  目标旋转，HUD、目标坐标轴和相机位置均更新；默认仍显示
  `Binding: WorldSpace`。
- Linux 未实机验证。
