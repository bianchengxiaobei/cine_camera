# CineRig — Engine-Agnostic Camera Rig Runtime

Status: P0–P5 feature work complete on Windows; release readiness in progress  
Version: 0.1.0  
Target: C++20  
Reference implementation: Unity Cinemachine 3.1.x (`com.unity.cinemachine`)

---

## 1. 项目目标

用 C++ 重实现 Cinemachine 的**运行时相机行为**，并保持与渲染引擎解耦。任何引擎只要实现一组适配接口，就可以使用 CineRig 的虚拟相机、混合、跟随、构图、噪声、Impulse、碰撞约束、Spline 和管理器相机。

目标不是逐行翻译 C#，而是：

- 行为等价：默认值、更新顺序、混合语义、阻尼、构图规则、边界条件与原项目一致。
- 结构可维护：C++ 原生所有权、句柄、无每帧堆分配、可测试。
- 引擎无关：核心库零渲染/物理/窗口依赖。
- 可验证：每个阶段都有单元测试、场景测试和验收标准。

## 2. 已确定决策

| 项目 | 决策 |
| --- | --- |
| 项目名 | `CineRig` |
| C++ 标准 | C++20 |
| 构建 | CMake + CMakePresets |
| 测试 | Catch2 v3 + CTest |
| 平台 | Windows + Linux 优先，macOS 后续 |
| 坐标约定 | 右手系，+Y up，-Z forward |
| 数学 | 内置 `cinerig::math`，默认 native 后端；GLM 选项保留但尚未实现 |
| 引擎接口 | 纯虚接口 + 值类型 + `uint64_t` 句柄 |
| Confiner2D | 允许使用可选依赖 Clipper2（BSL-1.0） |
| C API | P5 已评估：0.1 不提供稳定 C ABI，继续使用 C++20 API；结论与重启条件见 [docs/c-api.md](docs/c-api.md) |
| 原项目代码 | 每个任务必须参考；只参考行为与 API 语义，不复制 C# 代码和 Unity 资源 |

## 3. 参考策略

每个任务开始前必须完成以下步骤：

1. 阅读任务文档 `## 参考原项目（Cinemachine）` 中列出的源码、测试和文档。
2. 用原项目源码确认默认值、更新顺序、边界条件；用原项目测试确认期望行为。
3. 如果原项目文档与源码/测试不一致，以源码和测试为准，并在任务文档中记录差异。
4. C++ 实现使用自己的命名、所有权和数学类型，不逐行翻译 C#。
5. 不复制 Unity Editor 工具、Prefab、场景、贴图、动画曲线资产等资源。
6. 原项目使用 Unity Companion License；CineRig 作为独立引擎无关实现，应按行为规格重写，并在发布前做一次许可证复核。

## 4. 总架构

```text
┌─────────────────────────────────────────────┐
│                  Engine / Game              │
│  IEntityWorld  IPhysicsWorld  IInputSource  │
│  ICameraOutput  ISplineProvider  ...        │
└──────────────────────┬──────────────────────┘
                       │ adapters
┌──────────────────────▼──────────────────────┐
│                CineRig Runtime              │
│                                             │
│  CameraSystem                               │
│   ├─ VirtualCameraRegistry / UpdateManager  │
│   ├─ CameraBrain × N                        │
│   ├─ ImpulseManager                         │
│   └─ TargetGroupRegistry                    │
│                                             │
│  VirtualCamera                              │
│   ├─ CameraState                            │
│   ├─ Body / Aim / Noise / Finalize 组件     │
│   ├─ Extensions                             │
│   └─ Target / Lens / Priority / Channel     │
│                                             │
│  ManagerCamera                              │
│   ├─ MixingCamera / ClearShot               │
│   ├─ SequencerCamera / StateDrivenCamera    │
│   └─ 子相机选择与嵌套混合                   │
│                                             │
│  BlendManager / CameraBlendStack            │
│  CameraState::Lerp / BlendDefinition        │
└─────────────────────────────────────────────┘
```

### 分层

- `cinerig::math`：值类型和纯函数，无引擎依赖。
- `cinerig::core`：状态、镜头、混合、优先级、更新时钟。
- `cinerig::engine`：引擎适配接口和 Null 实现。
- `cinerig::camera`：虚拟相机、组件管线、扩展、管理器基类。
- `cinerig::brain`：输出相机选择、混合、覆盖栈、事件。
- `cinerig::components`：Follow、OrbitalFollow、Composer、ThirdPerson、Noise、SplineDolly 等。
- `cinerig::targets`、`cinerig::impulse`、`cinerig::managers`：对应原项目模块。
- `examples/`：SDL3 + OpenGL 示例，不属于核心库。
- `tests/`：单元、场景、golden、集成测试。

## 5. 目录规划

```text
CineRig/
├── plan.md
├── README.md
├── CMakeLists.txt
├── CMakePresets.json
├── .gitignore
├── cmake/
│   └── CineRigConfig.cmake.in
├── include/cinerig/
│   ├── cinerig.hpp
│   ├── math/
│   ├── core/
│   ├── engine/
│   ├── camera/
│   ├── components/
│   ├── targets/
│   ├── impulse/
│   ├── managers/
│   └── adapters/
├── src/
├── tests/
│   ├── unit/
│   ├── scenario/
│   └── golden/
├── examples/
│   ├── common/
│   ├── 01_follow/
│   └── 03_blend/
└── docs/
    ├── architecture.md
    ├── coordinate-convention.md
    ├── feature-matrix.md
    ├── parity.md
    ├── performance.md
    ├── c-api.md
    ├── licenses.md
    ├── release-readiness.md
    └── tasks/                  # P0-01 … P5-07, indexed by tasks/README.md
```

P2–P5 的任务文档在进入对应阶段前创建，避免过早写空文档；当前均已创建。发布前检查项见 [docs/release-readiness.md](docs/release-readiness.md)。

## 6. 公共 API 草图

```cpp
#include <cinerig/cinerig.hpp>

using namespace cinerig;

class GameWorld final : public IEntityWorld { /* ... */ };
class GameCamera final : public ICameraOutput { /* ... */ };

int main()
{
    GameWorld world;
    GameCamera output;

    CameraSystem system(world);
    BrainId brain = system.createBrain(output, {
        .channelMask = OutputChannels::Default,
        .defaultBlend = { BlendStyle::EaseInOut, 2.0f },
    });

    CameraId camera = system.createCamera<VirtualCamera>();
    auto& vcam = system.camera(camera);
    vcam.targets.tracking = TargetRef::entity(playerEntity);
    vcam.lens.fieldOfView = 40.0f;

    auto& follow = vcam.setBody<Follow>();
    follow.followOffset = { 0.0f, 2.0f, -6.0f };
    follow.tracker.positionDamping = { 0.5f, 0.5f, 0.5f };

    auto& aim = vcam.setAim<RotationComposer>();
    aim.composition.screenPosition = { 0.0f, 0.1f };

    while (running)
    {
        world.pollEvents();
        system.fixedUpdate(world.fixedDeltaTime());
        system.lateUpdate(world.deltaTime());
        render();
    }
}
```

API 设计约束：

- 所有相机、Brain、TargetGroup 用 `uint64_t` 句柄管理，避免引擎对象销毁后的悬空引用。
- `CameraSystem` 是实例，不使用全局单例；每个世界一个实例。
- `IEntityWorld` 至少提供变换、时间、帧号、视口；物理、Spline、输入、后处理为可选接口。
- `ICameraOutput` 接收最终 `AppliedCamera`，由适配器设置引擎相机的位置、旋转、FOV、裁剪面、正交/物理镜头、Dutch。
- 公共配置使用度（degrees），核心数学使用弧度（radians）；边界处显式转换。

## 7. 功能对齐概览

| 分类 | 原项目 | CineRig 对应 | 阶段 |
| --- | --- | --- | --- |
| 基础状态 | `CameraState`、`LensSettings`、`ScreenComposerSettings`、`LookaheadSettings` | 同名结构 | P0 |
| 核心运行时 | `CinemachineCore`、`CameraUpdateManager`、`VirtualCameraRegistry`、`UpdateTracker` | `CameraSystem`、`UpdateManager`、`Registry`、`UpdateTracker` | P1 |
| 虚拟相机 | `CinemachineVirtualCameraBase`、`CinemachineCamera`、`CinemachineComponentBase`、`CinemachineExtension` | `VirtualCamera`、`CameraComponent`、`CameraExtension`、`Pipeline` | P1 |
| Brain/混合 | `CinemachineBrain`、`BlendManager`、`CameraBlendStack`、`CinemachineBlendDefinition` | `CameraBrain`、`BlendManager`、`OverrideStack`、`BlendDefinition` | P1 |
| 外部相机 | `CinemachineExternalCamera` | `ExternalCamera` | P1 |
| 3D Body | `CinemachineFollow`、`CinemachineOrbitalFollow`、`CinemachinePositionComposer`、`CinemachineThirdPersonFollow`、`CinemachineHardLockToTarget` | 同名类 | P2（P1 先做最小 Follow） |
| 3D Aim | `CinemachineRotationComposer`、`CinemachinePanTilt`、`CinemachineThirdPersonAim`、`CinemachineHardLookAt`、`CinemachineRotateWithFollowTarget`、`CinemachineRecomposer` | 同名类 | P2（P1 先做最小 RotationComposer） |
| 辅助 | `CinemachineCameraOffset`、`CinemachineFollowZoom`、`CinemachineFreeLookModifier` | 同名类 | P2/P3 |
| 输入 | `InputAxis`、`InputAxisController`、`IInputAxisReader` | `InputAxis`、`InputAxisController`、`IInputSource` | P2 |
| 目标组 | `CinemachineTargetGroup`、`CinemachineGroupFraming` | `TargetGroup`、`GroupFraming` | P2/P3 |
| 噪声 | `NoiseSettings`、`CinemachineBasicMultiChannelPerlin`、`GaussianFilter` | `NoiseProfile`、`PerlinNoise`、`GaussianFilter` | P3 |
| Impulse | `CinemachineImpulseManager`、`CinemachineImpulseDefinition`、`CinemachineImpulseSource`、`CinemachineImpulseListener` | 同名类 | P3 |
| 碰撞/约束 | `CinemachineDeoccluder`、`CinemachineDecollider`、`CinemachineConfiner3D` | 同名类 + `IPhysicsWorld` | P3 |
| 2D | `CinemachineConfiner2D`、`ConfinerOven` | `Confiner2D` + 多边形缓存 | P4 |
| Spline | `CinemachineSplineDolly`、`CinemachineSplineCart`、`CinemachineSplineRoll`、`CinemachineSplineDollyLookAtTargets` | `BezierSpline` + 同名组件 | P4 |
| 管理器 | `CinemachineMixingCamera`、`CinemachineClearShot`、`CinemachineSequencerCamera`、`CinemachineStateDrivenCamera` | 同名类 | P4 |
| 后期/覆盖层 | `CinemachineStoryboard`、`CinemachinePixelPerfect`、PostProcessing | `StoryboardExtension`、`PixelPerfectExtension`、`IPostProcessor` | P5 |
| Timeline | `ICameraOverrideStack` + Timeline Track | `OverrideStack` API + 适配示例 | P5 |
| Editor | Inspector、Handles、SaveDuringPlay、Presets、CM2 升级 | 不实现 | 排除 |

## 8. 阶段路线图

### P0 — 基础层

目标：建立可编译、可测试、零引擎依赖的基础库。

- 工程骨架、CMake、Catch2、CTest。
- 数学库、基础状态、混合定义、引擎接口。
- 不实现相机系统。

验收：

- `cmake --preset windows` / `linux` 成功。
- `ctest` 全绿。
- `libcinerig` 不链接 SDL/OpenGL。
- 公共头文件可独立包含。

任务文档：

- [P0-01 工程骨架](docs/tasks/P0-01-project-skeleton.md)
- [P0-02 数学库](docs/tasks/P0-02-math.md)
- [P0-03 核心类型](docs/tasks/P0-03-core-types.md)
- [P0-04 CameraState 与混合](docs/tasks/P0-04-camera-state-blend.md)
- [P0-05 引擎接口](docs/tasks/P0-05-engine-interfaces.md)
- [P0-06 测试框架](docs/tasks/P0-06-test-harness.md)

### P1 — 核心运行时 + Brain + 首批示例

目标：虚拟相机、组件管线、Brain、混合、SDL3/OpenGL 示例闭环。

- CameraSystem、Registry、UpdateManager、Standby、SmartUpdate。
- VirtualCamera、CameraComponent、Extension、管线。
- CameraBrain、BlendManager、OverrideStack、事件。
- ManagerCameraBase、ExternalCamera。
- 最小 Follow + RotationComposer（WorldSpace 跟随 + 基础构图，P2 扩展完整功能）。
- SDL3 + OpenGL 公共层。
- 示例 `01_follow`、`03_blend`。

验收：

- 相机按优先级和激活顺序选择。
- Cut/EaseInOut/Linear/Custom 混合正确。
- 嵌套混合、InheritPosition、FreezeWhenBlendingOut 正确。
- 多 Brain + ChannelMask 分流。
- Fixed/Late/Smart/Manual 更新模式正确。
- 两个示例可运行。

任务文档：

- [P1-01 CameraSystem](docs/tasks/P1-01-camera-system.md)
- [P1-02 虚拟相机与管线](docs/tasks/P1-02-virtual-camera-pipeline.md)
- [P1-03 Brain 与混合](docs/tasks/P1-03-brain-blending.md)
- [P1-04 管理器相机基类](docs/tasks/P1-04-manager-camera-base.md)
- [P1-05 SDL3/OpenGL 示例框架](docs/tasks/P1-05-sdl-example-framework.md)
- [P1-06 最小 Follow/RotationComposer 与示例](docs/tasks/P1-06-follow-example.md)
- [P1-07 Blend 示例](docs/tasks/P1-07-blend-example.md)

### P2 — 3D 程序化相机

目标：覆盖主要 3D 跟随、环绕、构图、第三人称相机。

- Tracker、Damper、PositionPredictor、Lookahead。
- Follow、OrbitalFollow（Sphere/ThreeRing）、PositionComposer、ThirdPersonFollow、HardLockToTarget。
- RotationComposer、PanTilt、ThirdPersonAim、HardLookAt、RotateWithFollowTarget、Recomposer。
- CameraOffset、FollowZoom、FreeLookModifier。
- InputAxis、InputAxisController、TargetGroup。
- 示例：FreeLook、ThirdPerson、GroupFraming。

### P3 — 噪声、Impulse、碰撞

目标：补齐画面扰动、冲击、遮挡和约束。

- NoiseProfile、BasicMultiChannelPerlin、GaussianFilter。
- ImpulseManager/Definition/Source/Listener/Filtering/CollisionImpulseSource。
- IPhysicsWorld、Deoccluder、Decollider、Confiner3D。
- ThirdPersonFollow 障碍物避让。
- GroupFraming、ShotQualityEvaluator 基础。
- 示例：Noise/Impulse、Deoccluder、GroupFraming。

### P4 — Manager、Spline、2D

目标：多相机管理与路径约束。

- MixingCamera、ClearShot、SequencerCamera、StateDrivenCamera。
- BezierSpline、SplineDolly、SplineCart、SplineRoll、SplineDollyLookAtTargets、SplineSmoother。
- Confiner2D + 多边形缓存（可选 Clipper2）。
- PixelPerfect 基础。
- 示例：SplineDolly、2D Confiner、ClearShot。

### P5 — 集成与收尾

目标：Timeline 风格覆盖、输入/后处理适配、测试与文档收尾。

- OverrideStack + Timeline 风格示例。
- IInputSource、IPostProcessor、Storyboard、PixelPerfect 适配。
- 调试 HUD/可视化。
- 转写原项目测试场景，补齐 golden 数据。
- 性能基准、功能矩阵、集成文档。

状态：Windows Debug/Release 已完成；Linux 实机验证、许可证复核与发布检查待收尾，见 [release-readiness.md](docs/release-readiness.md)。

## 9. 示例计划

公共层：

- SDL3 窗口、事件、主循环。
- OpenGL 3.3 渲染器：网格、地面、简单几何、调试线、HUD。
- `SdlWorld`：`IEntityWorld` + 简单 AABB/球体物理，作为接入参考。
- `SdlCameraOutput`：`ICameraOutput`，处理 View/Projection 和 Dutch。
- `SdlInput`：`IInputSource`。

首批：

- `01_follow`：移动目标、Follow + RotationComposer、阻尼切换。
- `03_blend`：两个虚拟相机、Brain 混合、Cut/曲线切换。

后续按 P2–P4 增加 FreeLook、ThirdPerson、Noise/Impulse、Deoccluder、GroupFraming、SplineDolly、2D Confiner、ClearShot。

## 10. 测试策略

- 单元测试：数学、状态、镜头、混合曲线、阻尼、输入轴、样条、噪声、Impulse 包络。
- 场景测试：FakeEntityWorld + 脚本化目标，固定 dt，验证相机状态和事件顺序。
- Golden 测试：关键帧 `CameraState` JSON 快照，容差 `1e-4`。
- 原项目测试转写：优先覆盖
  - `Tests/Editor/CameraStateTests.cs`
  - `Tests/Editor/DampingTests.cs`
  - `Tests/Editor/InputAxisTests.cs`
  - `Tests/Editor/UnityVectorExtensionTests.cs`
  - `Tests/Editor/BlendManagerTests.cs`
  - `Tests/Runtime/CamerasBlendingTests.cs`
  - `Tests/Runtime/PriorityTests.cs`
  - `Tests/Runtime/LookaheadTests.cs`
  - `Tests/Runtime/FreelookForcePositionTests.cs`
  - `Tests/Runtime/FollowForcePositionTests.cs`
  - `Tests/Runtime/CmDeoccluderTests.cs`
  - `Tests/Runtime/Confiner2DUnitTests.cs`
  - `Tests/Runtime/SplineDollyCameraTest.cs`
  - `Tests/Runtime/ClearShotTests.cs`
  - `Tests/Runtime/StateDrivenCameraTests.cs`
- 不追求与 Unity 浮点结果逐位一致；记录允许差异：随机噪声种子、样条切线细节、极端多边形、物理查询顺序。

## 11. 风险与缓解

| 风险 | 说明 | 缓解 |
| --- | --- | --- |
| 功能量巨大 | 全量运行时约 4–6 人月 | 分阶段；P1 先交付可运行闭环 |
| 许可证 | 原项目 Unity Companion License 限制在 Unity 相关项目 | 行为规格重写，不复制代码/资源；依赖清单见 [docs/licenses.md](docs/licenses.md)，发布前复核 |
| Confiner2D | Clipper + Oven 缓存复杂 | 接口先留好；P4 实现，可选 Clipper2；先保证基本约束 |
| Spline | Unity Splines 切线/Up/Roll 规则多 | 自定义 BezierSpline + ISpline 接口；示例先自动切线 |
| 引擎差异 | 坐标系、手性、相机前向、Aspect、物理镜头 | 明确接口契约；提供 Unity/OpenGL 转换工具 |
| 更新时序 | Fixed/Late/Smart 与目标运动不同步会抖 | 显式 UpdateClock；场景测试覆盖 |
| 性能 | 每帧分配、曲线/样条计算 | 热路径无分配；slot map/固定容量；基准测试 |
| 依赖 | SDL3/OpenGL/GLAD/Clipper2 | FetchContent + 系统包选项；示例依赖不进入核心库 |
| 平台验证 | Windows 已完成；Linux 实机尚未执行 | 发布前在 Linux/CI 运行 `cmake --preset linux` 与全量 CTest |

## 12. 明确排除

- Unity Editor 面板、Handles、SaveDuringPlay、Presets、CM2 升级工具。
- Unity Timeline Track/Clip、URP/HDRP PostProcessing、Unity Input System、Unity Splines 资产、Unity 2D PixelPerfect 包。
- 原项目 `Deprecated/` 下的 CM2 兼容组件。
- 首版不提供稳定 C ABI；P5 评估结论与后续条件见 [docs/c-api.md](docs/c-api.md)。

## 13. 任务执行规则

1. 每次只执行一个任务文档；完成并验收后再进入下一个。
2. 任务文档状态从 `pending` → `in_progress` → `done`。
3. 实现前先阅读该任务的 `## 参考原项目（Cinemachine）`。
4. 完成时更新文档中的 `## 实现记录`，记录差异、取舍和已知问题。
5. 每个任务必须跑对应测试；P0/P1 每次提交前跑全量 `ctest`。
