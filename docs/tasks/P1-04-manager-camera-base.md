# P1-04 Manager Camera Base

Status: completed  
Depends on: P1-02, P1-03  
Blocks: P4 manager cameras

## Goal

实现管理器相机基类，使一个虚拟相机可以拥有多个子相机，按派生类规则选择 live 子相机，并在子相机之间混合。同时实现 `ExternalCamera`，让宿主引擎相机参与混合。

## Scope

- `VirtualCameraManagerBase`：
  - 子相机列表和缓存。
  - `DefaultTarget`（子相机未指定目标时继承）。
  - `DefaultBlend`、`CustomBlends`。
  - `LiveChild`、`ActiveBlend`、`IsBlending`。
  - `ChooseCurrentCamera()` 抽象。
  - `InternalUpdateCameraState()`：更新子相机、选择、混合、finalize。
  - `OnTransitionFromCamera()`。
  - `PreviousStateIsValid` 传播到子相机。
  - `OnTargetObjectWarped()` / `ForceCameraPosition()` 传播。
  - `InvalidateCameraCache()`。
- `ExternalCamera`：
  - 从 `ICameraOutput` 或 owner entity 读取当前相机状态。
  - 可设置 `LookAtTarget` 改善混合质量。
  - `BlendHint`。
  - 参与 Brain 的优先级选择。
- 嵌套管理器的层级：父管理器扩展如何应用到子相机。

## Non-goals

- 不实现 MixingCamera / ClearShot / Sequencer / StateDriven（P4）。
- 不实现 Editor 子相机列表 UI。
- 不实现 Timeline 集成。

## Deliverables

```text
include/cinerig/camera/
├── virtual_camera_manager_base.hpp
└── external_camera.hpp
src/camera/
├── virtual_camera_manager_base.cpp
└── external_camera.cpp
tests/scenario/
├── manager_camera_base_test.cpp
└── external_camera_test.cpp
```

## API Sketch

```cpp
namespace cinerig
{
class VirtualCameraManagerBase : public VirtualCamera, public ICinemachineMixer
{
public:
    struct DefaultTargetSettings
    {
        bool enabled = false;
        CameraTarget target{};
    };

    DefaultTargetSettings defaultTarget{};
    BlendDefinition defaultBlend{ BlendStyle::EaseInOut, 0.5f };
    BlendLookupTable customBlends{};

    const std::vector<VirtualCamera*>& childCameras() const;
    VirtualCamera* liveChild() const;
    bool isBlending() const;
    const Blend* activeBlend() const;

    void invalidateCameraCache();

    void internalUpdateCameraState(Vec3 worldUp, float deltaTime) override;
    void onTransitionFromCamera(
        const ICinemachineCamera* from, Vec3 worldUp, float deltaTime) override;
    void onTargetObjectWarped(EntityId target, Vec3 positionDelta) override;
    void forceCameraPosition(Vec3 pos, Quat rot) override;

protected:
    virtual VirtualCamera* chooseCurrentCamera(Vec3 worldUp, float deltaTime) = 0;
    virtual BlendDefinition lookupBlend(
        const ICinemachineCamera* outgoing,
        const ICinemachineCamera* incoming) const;

    void setLiveChild(VirtualCamera* camera, Vec3 worldUp, float deltaTime);
    void resetLiveChild();
    void finalizeCameraState(float deltaTime);
};

class ExternalCamera : public VirtualCamera
{
public:
    EntityId cameraEntity = 0;       // 可选：从实体读取变换
    ICameraOutput* output = nullptr; // 可选：从输出相机读取
    TargetRef lookAtTarget{};
    BlendHint blendHint = BlendHint::Nothing;

    void internalUpdateCameraState(Vec3 worldUp, float deltaTime) override;
};
}
```

## Behavior Details

### 子相机缓存

对齐 `Runtime/Core/CinemachineCameraManagerBase.cs:277-301`：

- 子相机是显式注册还是从 owner 层级发现？C++ 建议显式注册：
  - `addChild(VirtualCamera&)` / `removeChild(VirtualCamera&)`。
  - 同时保留 `invalidateCameraCache()` 供动态变化时重建。
- 原项目通过 `transform.childCount` 和 `GetComponentsInChildren` 发现；C++ 不依赖 Transform 层级，因此必须显式管理。
- 缓存无效时：
  - `previousStateIsValid = false`。
  - 清空 live child / blend。
- 子相机不能是管理器自身；循环嵌套要检测。

### 更新流程

对齐 `Runtime/Core/CinemachineCameraManagerBase.cs:183-216`：

1. 更新目标缓存。
2. 若 `previousStateIsValid == false` → `resetLiveChild()`。
3. `best = chooseCurrentCamera(worldUp, deltaTime)`。
4. 若 `best` 未 enabled → 启用并立即更新。
5. `setLiveChild(best, worldUp, deltaTime)`：
   - `blendManager.updateRootFrame(this, best, worldUp, deltaTime)`。
   - `computeCurrentBlend()`。
   - `processActiveCamera(...)`。
6. 处理 `m_TransitioningFrom`（来自 `OnTransitionFromCamera`）。
7. `finalizeCameraState(deltaTime)`：
   - `state = blendManager.state`。
   - `invokePostPipelineStage(Finalize, state, deltaTime)`。
8. `previousStateIsValid = true`。

### DefaultTarget

对齐 `Runtime/Core/CinemachineCameraManagerBase.cs:14-36, 141-175`：

- `defaultTarget.enabled == false` 时，`lookAt`/`follow` 返回空。
- enabled 时：
  - `follow` = `resolveFollow(defaultTarget.target.tracking)`。
  - `lookAt` = `customLookAt ? defaultTarget.target.lookAt : defaultTarget.target.tracking`。
- 子相机未指定 target 时，通过 `ResolveFollow`/`ResolveLookAt` 继承父管理器的默认目标。
- C++ 中 `TargetRef` 的继承需要 `CameraSystem` 解析；管理器只保存默认 target。

### PreviousStateIsValid

- 管理器重置时，必须传播到所有子相机。
- 子相机状态无效时，管理器的 blend 也应重置，避免从旧状态插值。

### 管理器之间的嵌套

- 管理器可以作为另一个管理器的子相机。
- 父管理器的扩展应应用到子相机管线（原项目通过 `InvokePostPipelineStageCallback` 向上递归）。
- 更新顺序由 `CameraSystem` 的嵌套层级保证：叶子先更新，管理器后更新。
- `IsLiveChild` 需要沿父链向上判断。

### ExternalCamera

对齐 `Runtime/Behaviours/CinemachineExternalCamera.cs`：

- `internalUpdateCameraState`：
  - `state = CameraState::makeDefault()`。
  - 从 `cameraEntity` 或 `output` 获取位置/朝向。
  - `referenceUp = rawOrientation * up`。
  - `lens` 从输出相机/实体读取。
  - 若 `lookAtTarget` 有效：
    - `referenceLookAt = target position`。
    - 将目标方向投影到相机 forward 上，避免偏离视线。
  - `blendHint = this.blendHint`。
  - 调用 Finalize stage 的扩展回调。
- ExternalCamera 不修改 owner 引擎相机；它只是把当前状态暴露给 Brain 参与混合。
- 可选：`Follow` 为空。

### Manager 的 Blend 查找

- 使用 `defaultBlend` + `customBlends`。
- `customBlends` 使用与 Brain 相同的匹配规则（P1-03）。
- 子相机之间的 blend 与 Brain 的 blend 独立：管理器内部先混合，Brain 再把它当作一个相机与外部相机混合。

## Tests

- 子相机缓存：add/remove/invalidate。
- 选择：派生测试类返回固定子相机；验证 live child 和 state。
- 混合：两个子相机切换，使用 defaultBlend；嵌套管理器混合。
- DefaultTarget 继承：子相机无 target 时使用管理器 target；customLookAt。
- PreviousStateIsValid 传播。
- Transition：`onTransitionFromCamera` 触发子相机激活事件。
- ForceCameraPosition / warp 传播到所有子相机。
- ExternalCamera：从 entity/output 读取状态、LookAt 投影、BlendHint。
- 循环嵌套检测。

## Acceptance Checklist

- [x] 子相机显式注册/注销安全。
- [x] 子相机缓存无效时重置混合。
- [x] 选择/混合/finalize 流程与基类一致。
- [x] DefaultTarget 继承正确。
- [x] PreviousStateIsValid 传播正确。
- [x] 嵌套管理器叶子优先更新。
- [x] ExternalCamera 可参与 Brain 混合。
- [x] 场景测试全绿。

## 参考原项目（Cinemachine）

> 参考管理器基类的子相机管理、选择流程和外部相机语义；用 C++ 重写，不复制 C# 代码。

### 必读源码

- `com.unity.cinemachine/Runtime/Core/CinemachineCameraManagerBase.cs:12-328`
  - DefaultTarget、DefaultBlend、CustomBlends、ChildCameras、PreviousStateIsValid、InternalUpdateCameraState、LookupBlend、ChooseCurrentCamera、SetLiveChild、FinalizeCameraState、缓存。
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineExternalCamera.cs`
  - 外部相机状态、LookAt 投影、BlendHint、Finalize 回调。
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineClearShot.cs:30-231`
  - 派生类如何选择子相机（P1 只参考接口，不实现）。
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineMixingCamera.cs:20-242`
  - 派生类如何混合子相机（P1 只参考接口）。
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs`
  - `ResolveFollow`、`ResolveLookAt`、父相机、扩展传播。

### 必读测试

- `com.unity.cinemachine/Tests/Runtime/ClearShotTests.cs`
- `com.unity.cinemachine/Tests/Runtime/StateDrivenCameraTests.cs`（P4）
- `com.unity.cinemachine/Tests/Runtime/CamerasBlendingTests.cs`
- `com.unity.cinemachine/Tests/Runtime/BrainTargetOverrideTests.cs`

### 需要核对的行为

- 子相机缓存的失效条件和 `PreviousStateIsValid` 传播。
- `SetLiveChild` 的调用顺序和 `m_TransitioningFrom` 的特殊处理。
- `FinalizeCameraState` 的扩展回调。
- ExternalCamera 的 `ReferenceLookAt` 投影公式。
- 管理器作为子相机时的 `IsLiveChild` 递归。
- DefaultTarget 的继承规则。

### 差异记录

- 原项目通过 Unity Transform 层级发现子相机；CineRig 显式注册子相机，避免依赖场景层级。
- 原项目管理器是 `MonoBehaviour`；CineRig 由 `CameraSystem` 管理生命周期。
- 原项目管理器可以自动激活未激活的子 GameObject；CineRig 子相机始终由系统持有，只切换 enabled 状态。
- ExternalCamera 的实体/输出优先级已确定为：`cameraEntity`（无效时回退 `ownerEntity`）提供变换，`output` 提供镜头参数。

## 实现记录

已实现：

- `VirtualCameraManagerBase` 支持显式 `addChild` / `removeChild`，拒绝自身、重复、已有其他父管理器以及循环嵌套；`CameraSystem::setCameraParent` 与显式子相机列表同步。
- 管理器内部使用 `BlendManager` 完成子相机选择、根帧更新、`computeCurrentBlend`、`processActiveCamera` 和 `finalizeCameraState`；更新后 `state_` 取混合状态并调用 Finalize 扩展链。
- `defaultTarget` 在启用时提供 follow/look-at 继承，`customLookAt` 使用显式 look-at；子相机通过父链解析目标。
- `PreviousStateIsValid` 失效会递归传播到嵌套管理器；缓存失效会清空 live child、根混合和子相机 live 标记。
- `onTransitionFromCamera` 设置 transitioning 状态，在无混合时向 live child 发送 manager-origin 激活事件，保留原项目的特殊处理。
- `forceCameraPosition` 和 `onTargetObjectWarped` 传播到所有子相机。
- `ExternalCamera` 优先从 `cameraEntity`（不存在时回退 `ownerEntity`）读取变换，从 `output` 读取镜头，读取失败时使用自身 lens；`lookAtTarget` 投影到相机 forward 线，状态为只读，不写回宿主实体，并调用 Finalize 扩展。
- 增加管理器/外部相机场景测试，覆盖子相机缓存、选择、默认/自定义混合、嵌套、目标继承、状态传播、过渡、force/warp、Brain 参与和销毁安全。

验证：Windows Debug 全量 **144/144** 通过（70 unit + 68 scenario + 6 golden）。

差异记录：外部相机接口 `ICameraOutput` 只暴露 lens/aspect，不提供宿主相机变换；因此实体变换优先使用 `IEntityWorld`，`output` 用作镜头来源。管理器子相机发现改为显式注册，不依赖 Transform 层级。

