# P1-02 Virtual Camera Pipeline

Status: completed  
Depends on: P1-01  
Blocks: P1-03, P1-04, P1-06

## Goal

实现虚拟相机基类、组件管线和扩展回调，使 Body/Aim/Noise/Finalize 组件能按正确顺序修改 `CameraState`。

## Scope

- `VirtualCamera`：
  - 优先级、输出通道、Standby、BlendHint。
  - `CameraTarget`、`LensSettings`。
  - `previousStateIsValid`、`followTargetAttachment`、`lookAtTargetAttachment`。
  - `state()`、`internalUpdateCameraState()`。
  - `forceCameraPosition()`、`onTargetObjectWarped()`。
  - owner `EntityId` 和可选 transform 回写。
  - 父相机/嵌套关系。
- `CameraComponent`：
  - `Stage`：Body / Aim / Noise / Finalize。
  - `isValid()`、`prePipelineMutate()`、`mutateCameraState()`、`bodyAppliesAfterAim()`。
  - `onTransitionFromCamera()`、`onTargetObjectWarped()`、`forceCameraPosition()`、`maxDampTime()`。
- `CameraExtension`：
  - `prePipelineMutateCameraStateCallback()`。
  - `postPipelineStageCallback()`。
  - 连接/断开 vcam。
- `Pipeline`：
  - 每个 Stage 一个组件。
  - pre extensions → pre components → Body → Aim → (post-aim Body) → Noise → Finalize。
  - 每个阶段后调用 extensions。
- 目标解析：
  - `TargetRef` → entity transform / vcam state / group。
  - `customLookAt` 处理。
  - 目标不存在时的安全回退。
- `ICinemachineMixer` 接口（`isLiveChild`、`parentCamera`），追加到 `core/camera_interface.hpp`，供 Brain/Manager 使用；P1-04 实现。
- `pullStateFromVirtualCamera()`：
  - 从 owner transform 读取 raw position/orientation。
  - 从 lens/output 继承字段。
  - `referenceLookAt` 从 LookAt target 读取。
- transform 回写：如果 `IEntityWorld::trySetTransform` 支持，写回 raw position/orientation；否则只更新内部状态。

## Non-goals

- 不实现具体 Body/Aim 组件（P2）。
- 不实现 Manager 相机（P1-04）。
- 不实现 Brain 混合（P1-03）。
- 不实现 Editor 管线缓存。

## Deliverables

```text
include/cinerig/core/
└── camera_interface.hpp   # 追加 ICinemachineMixer
include/cinerig/camera/
├── virtual_camera.hpp
├── camera_component.hpp
├── camera_extension.hpp
└── pipeline.hpp
src/camera/
├── virtual_camera.cpp
├── camera_component.cpp
└── pipeline.cpp
tests/scenario/
├── pipeline_order_test.cpp
├── pipeline_extension_test.cpp
└── virtual_camera_target_test.cpp
```

## API Sketch

```cpp
namespace cinerig
{
enum class Stage { Body, Aim, Noise, Finalize };

class ICinemachineMixer : public ICinemachineCamera
{
public:
    virtual bool isLiveChild(
        const ICinemachineCamera* child, bool dominantChildOnly = false) const = 0;
};

class CameraComponent
{
public:
    virtual ~CameraComponent() = default;

    // 由 VirtualCamera::setBody/setAim/... 注入 owner。
    VirtualCamera& virtualCamera() const;

    // 目标解析辅助：无效时返回 nullopt。
    std::optional<Vec3> followTargetPosition() const;
    std::optional<Quat> followTargetRotation() const;
    std::optional<Vec3> lookAtTargetPosition() const;
    std::optional<Quat> lookAtTargetRotation() const;

    virtual Stage stage() const = 0;
    virtual bool isValid() const = 0;
    virtual bool bodyAppliesAfterAim() const { return false; }

    virtual void prePipelineMutate(CameraState& state, float deltaTime) {}
    virtual void mutateCameraState(CameraState& state, float deltaTime) = 0;

    virtual bool onTransitionFromCamera(
        const ICinemachineCamera* from, Vec3 worldUp, float deltaTime)
    { return false; }

    virtual void onTargetObjectWarped(EntityId target, Vec3 positionDelta) {}
    virtual void forceCameraPosition(Vec3 pos, Quat rot) {}
    virtual float maxDampTime() const { return 0.0f; }
};

class CameraExtension
{
public:
    virtual ~CameraExtension() = default;

    virtual void prePipelineMutateCameraStateCallback(
        VirtualCamera& vcam, CameraState& state, float deltaTime) {}
    virtual void postPipelineStageCallback(
        VirtualCamera& vcam, Stage stage, CameraState& state, float deltaTime) {}
    virtual void onTargetObjectWarped(EntityId target, Vec3 positionDelta) {}
    virtual bool onTransitionFromCamera(
        const ICinemachineCamera* from, Vec3 worldUp, float deltaTime)
    { return false; }
    virtual float maxDampTime() const { return 0.0f; }
};

class VirtualCamera : public ICinemachineCamera
{
public:
    CameraTarget targets{};
    LensSettings lens{};
    PrioritySettings priority{};
    OutputChannels outputChannel = OutputChannels::Default;
    StandbyUpdateMode standbyUpdate = StandbyUpdateMode::RoundRobin;
    BlendHint blendHint = BlendHint::Nothing;

    bool previousStateIsValid = false;
    float followTargetAttachment = 1.0f;
    float lookAtTargetAttachment = 1.0f;

    const CameraState& state() const override;
    bool isValid() const override;
    std::string_view name() const override;

    virtual void internalUpdateCameraState(Vec3 worldUp, float deltaTime);
    virtual void forceCameraPosition(Vec3 pos, Quat rot);
    virtual void onTargetObjectWarped(EntityId target, Vec3 positionDelta);

    template <typename T, typename... Args>
    T& setBody(Args&&... args);
    template <typename T, typename... Args>
    T& setAim(Args&&... args);
    template <typename T, typename... Args>
    T& setNoise(Args&&... args);
    template <typename T, typename... Args>
    T& setFinalize(Args&&... args);

    void addExtension(std::unique_ptr<CameraExtension> extension);
    void removeExtension(CameraExtension* extension);

protected:
    virtual void updateTargetCache();
    virtual void invokePrePipelineMutate(CameraState& state, float deltaTime);
    virtual void invokePostPipelineStage(Stage stage, CameraState& state, float deltaTime);
    virtual CameraState pullStateFromVirtualCamera(Vec3 worldUp);
    void pushStateToOwner(const CameraState& state);
};
}
```

## Behavior Details

### 管线顺序

对齐 `Runtime/Behaviours/CinemachineCamera.cs:236-276`：

1. `invokePrePipelineMutate`（extensions 先）。
2. 所有组件的 `prePipelineMutate`。
3. 按 Stage 顺序遍历：
   - 若组件是 Body 且 `bodyAppliesAfterAim()` → 暂存，跳到 Aim 后执行。
   - 否则执行 `mutateCameraState`。
   - 执行 `invokePostPipelineStage(stage)`。
   - 若当前是 Aim 且存在 post-aim Body：
     - 执行该 Body 的 `mutateCameraState`。
     - 再执行一次 Body stage 的 post callback。
4. 每个 Stage 只允许一个组件；重复添加时保留第一个或覆盖策略需在实现记录中确定。

### VirtualCamera 更新

对齐 `Runtime/Behaviours/CinemachineCamera.cs:208-234`：

1. 更新目标缓存。
2. `followTargetAttachment = 1`、`lookAtTargetAttachment = 1`。
3. `deltaTime < 0` → `previousStateIsValid = false`。
4. `state = pullStateFromVirtualCamera(worldUp)`。
5. 若 LookAt 目标存在：
   - 若 LookAt 目标是 vcam → 使用其 `state.finalPosition()`。
   - 否则使用 `IEntityWorld::transform(entity).position`。
   - 设置 `referenceLookAt`。
6. `state.blendHint = blendHint`。
7. 执行组件管线。
8. `pushStateToOwner(state)`。
9. `previousStateIsValid = true`。

### pullStateFromVirtualCamera

- 从 owner entity 读取 position/orientation/up。
- `rawPosition`、`rawOrientation`、`referenceUp`。
- `lens` 从 vcam 自身的 `LensSettings` 开始；`modeOverride == None` 时由 Brain/输出相机在 P1-03 处理继承。
- `shotQuality = 1`、corrections = 默认。
- 如果 owner 不存在，使用 identity/zero 并保持安全。

### 目标解析

- `TargetRef::Kind::Entity` → `IEntityWorld::exists` + `transform`。
- `TargetRef::Kind::Camera` → `CameraSystem` 查找 vcam，读取 `state.finalPosition()/finalOrientation()`。
- `TargetRef::Kind::Group` → `TargetGroup` 计算位置/朝向（P2/P3）。
- 目标无效时：
  - Follow 组件 `isValid() == false`，不修改位置。
  - Aim 组件 `isValid() == false`，不修改朝向。
- `customLookAt == false` 时 LookAt 使用 tracking target。

### 扩展

对齐 `Runtime/Core/CinemachineVirtualCameraBase.cs:272-391`：

- 扩展按添加顺序执行。
- pre pipeline 回调：所有 extensions 先执行，再执行组件 pre。
- post stage 回调：每个 Stage 后执行所有 extensions。
- 父相机（Manager）也会收到子相机的 pre/post 回调；P1-04 实现嵌套传播。
- 扩展可以返回 `forceUpdate`，触发 vcam 重新更新。
- 删除/禁用扩展时安全跳过。

### forceCameraPosition / warp

- `forceCameraPosition(pos, rot)`：
  - 设置 `state.rawPosition` / `state.rawOrientation`。
  - 通知所有组件。
  - 若 owner 可写，写回 transform。
- `onTargetObjectWarped(target, delta)`：
  - 若 target 是 Follow 或 LookAt，移动相机状态中对应位置。
  - 通知所有组件和扩展。
  - P1 只做基础传播；具体组件在 P2 实现。

## Tests

- 管线顺序：记录每个组件的调用顺序，验证 Body/Aim/Noise/Finalize。
- post-aim Body：`PositionComposer` 风格的组件在 Aim 之后执行，且 Body post callback 执行两次。
- Extension 顺序：pre 在组件前，post 在每个 stage 后。
- 父相机扩展传播（P1-04 后）。
- 目标解析：entity、camera、group（P2）、customLookAt。
- 无效目标：组件不修改状态。
- transform 回写：可写/不可写 owner。
- forceCameraPosition / warp 状态更新。
- previousStateIsValid 在 deltaTime < 0 时重置。

## Acceptance Checklist

- [x] 管线顺序与 `CinemachineCamera.InvokeComponentPipeline` 一致。
- [x] 每个 Stage 一个组件。
- [x] post-aim Body 正确执行。
- [x] Extension pre/post callback 正确。
- [x] 目标无效时安全。
- [x] transform 回写可选。
- [x] force/warp 基础行为正确。
- [x] 场景测试全绿。

## 参考原项目（Cinemachine）

> 参考管线顺序、扩展回调和状态同步；用 C++ 重写，不复制 C# 代码。

### 必读源码

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineCamera.cs:78-316`
  - `Target`、`Lens`、`BlendHint`、`State`、`LookAt/Follow`、`OnTargetObjectWarped`、`ForceCameraPosition`、`InternalUpdateCameraState`、`InvokeComponentPipeline`、`UpdatePipelineCache`。
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs:28-839`
  - 生命周期、优先级、Standby、扩展列表、pre/post 回调、父相机、target 解析、transform 同步。
- `com.unity.cinemachine/Runtime/Core/CinemachineComponentBase.cs:9-218`
  - 组件基类、Follow/LookAt target 访问、Stage、bodyAppliesAfterAim。
- `com.unity.cinemachine/Runtime/Core/CinemachineExtension.cs:12-189`
  - 扩展连接、pre/post callback、target warped。
- `com.unity.cinemachine/Runtime/Core/CameraState.cs`
  - 状态字段和默认值。
- `com.unity.cinemachine/Runtime/Core/CameraTarget.cs`
  - tracking/lookAt/customLookAt。

### 必读测试

- `com.unity.cinemachine/Tests/Runtime/CameraPositionTests.cs`
- `com.unity.cinemachine/Tests/Editor/CmCameraProceduralBehaviourCacheEditorTests.cs`
- `com.unity.cinemachine/Tests/Runtime/FollowForcePositionTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CinemachineBrainSetupTest.cs`
- `com.unity.cinemachine/Tests/Runtime/ManualUpdateTests.cs`

### 需要核对的行为

- `InvokeComponentPipeline` 中 postAimBody 的处理和 Body post callback 的调用次数。
- pre pipeline 回调的顺序：extensions 先，组件后。
- 父相机扩展传播。
- `PullStateFromVirtualCamera` 的位置/朝向/up/lens 来源。
- `InternalUpdateCameraState` 中 transform 回写的时机。
- `OnTransitionFromCamera` 的 `forceUpdate` 语义（原项目会调用两次 `InternalUpdateCameraState`）。
- `PreviousStateIsValid` 在 `deltaTime < 0` 时的重置。

### 差异记录

- 原项目组件是 `MonoBehaviour`，通过 `GetComponents` 发现；CineRig 组件由 `VirtualCamera` 显式添加/持有。
- 原项目通过 Unity transform 读写；CineRig 通过 `IEntityWorld` 和可选 `trySetTransform`。
- 原项目扩展挂在 GameObject 上并自动连接；CineRig 扩展显式注册到 vcam。
- `OnTransitionFromCamera` 的“调用两次”是原项目遗留行为；C++ 先按一次实现，记录是否需要复刻。
- C++ 组件与扩展均显式注册，`setBody`/`setAim`/`setNoise`/`setFinalize` 注入 owner；同一 Stage 再次设置时替换旧组件，而不是像 Unity 的 `GetComponents` 那样保留第一个。
- `CameraExtension` 增加 `enabled` 状态和 `virtualCamera()` 连接信息；`addExtension` 注入 owner，`removeExtension`/析构断开。禁用扩展在 pre/post/transition/force/warp 中跳过。
- 父相机扩展在 `VirtualCamera` 层先于 P1-04 Manager 实现：子相机调用 `pre/post` 时，按 child → parent 顺序把子相机指针传给父相机的扩展。
- `TargetGroup` 在 P1-02 采用加权平均位置和最高权重目标的朝向作为临时解析；P2 由完整 TargetGroup 替换。
- `onTargetObjectWarped` 对 Follow 目标移动 raw position 并回写 owner；对 LookAt 目标移动 `referenceLookAt`；同时传播给组件、扩展和父相机扩展。
- `forceCameraPosition` 先通知组件/扩展，再设置 raw position/orientation 并回写 owner；owner 不可写时只保留内部状态。
- `OnTransitionFromCamera` 的 `forceUpdate` 结果目前不触发原项目的二次 `InternalUpdateCameraState`，只执行一次；差异记录保留待 P1-03/P1-04 验证。
- `pullStateFromVirtualCamera` 的 `LensSettings::OverrideMode::None` 继承仍留给 P1-03 Brain/输出相机处理。

## 实现记录

- 新增 `include/cinerig/camera/camera_component.hpp`、`camera_extension.hpp`、`pipeline.hpp`：
  - `Stage` 四阶段、组件目标辅助、组件生命周期回调。
  - 扩展 owner 连接、启用状态、pre/post stage 回调。
  - Pipeline 每 Stage 一个 `unique_ptr`，替换策略和 pre 阶段遍历。
- 新增 `src/camera/camera_component.cpp`、`src/camera/pipeline.cpp`。
- 扩展 `src/camera/virtual_camera.cpp` 与 `include/cinerig/camera/virtual_camera.hpp`：
  - `internalUpdateCameraState` 更新目标缓存、重置 attachment、pull owner 状态、写入 `referenceLookAt`/`blendHint`、执行管线、回写 owner。
  - `forceCameraPosition`、`onTargetObjectWarped`、`onTransitionFromCamera`、`maxDampTime`。
  - Entity/Camera/Group 目标解析与父相机回退、自定义 LookAt、无效目标安全返回。
  - `ICinemachineMixer* parentCamera()` 动态查询。
- `include/cinerig/core/camera_interface.hpp` 追加 `ICinemachineMixer`。
- `CameraSystem::updateTargetFor` 改为使用 `effectiveFollowTarget()` / `effectiveLookAtTarget()`，使 Smart 更新识别父相机回退目标。
- `include/cinerig/cinerig.hpp` 暴露新头文件；`CMakeLists.txt` 加入 `camera_component.cpp`、`pipeline.cpp`。
- 新增测试：
  - `tests/scenario/pipeline_order_test.cpp`：Body/Aim/Noise/Finalize 顺序、post-aim Body、Body post callback 两次、重复 Stage 替换。
  - `tests/scenario/pipeline_extension_test.cpp`：extension 顺序/禁用、父相机回调、连接/断开、transition、damping。
  - `tests/scenario/virtual_camera_target_test.cpp`：owner pull、Entity/Camera/Group/customLookAt、无效目标、父目标回退、transform 回写、force/warp、负 dt。
- 新增 `tests/support/pipeline_test_components.hpp` 测试替身。
- Windows Debug/Release、GLM、自定义 test main 均 109/109 通过（70 unit + 33 scenario + 6 golden）。
- `CINERIG_BUILD_TESTS=OFF` 仍只构建 core；安装后消费者构建/运行通过。
- 未在 Linux 实机执行。
