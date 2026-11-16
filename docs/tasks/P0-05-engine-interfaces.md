# P0-05 Engine Interfaces

Status: completed  
Depends on: P0-02, P0-03  
Blocks: P1-01, P1-05

## Goal

定义 CineRig 与宿主引擎之间的最小适配接口。核心库通过这些接口读取实体变换、时间、物理查询、样条、输入和后处理；不直接依赖任何引擎。

## Scope

- `IEntityWorld`：实体变换、存在性、时间、帧号、视口、更新时钟。
- `ICameraOutput`：把最终相机状态应用到引擎相机。
- `IPhysicsWorld`：射线、球扫、重叠查询、碰撞体包围盒。
- `ISpline` / `ISplineProvider`：样条采样和最近点。
- `IAnimationStateProvider`：状态机哈希（StateDrivenCamera 用）。
- `IInputSource`：轴输入。
- `IPostProcessor`：可选后处理。
- `AppliedCamera`：输出相机收到的最终数据。
- Null 实现：`NullPhysicsWorld`、`NullInputSource`、`NullPostProcessor`。
- 坐标约定和单位约定文档。

## Non-goals

- 不实现具体引擎适配（SDL 示例在 P1-05）。
- 不定义 `ICinemachineCamera`（P0-04 定义；P1-02 扩展）。
- 不实现物理查询算法。
- 不实现样条求值算法（P4）。
- 不实现输入系统。

## Deliverables

```text
include/cinerig/engine/
├── entity_world.hpp
├── camera_output.hpp
├── physics_world.hpp
├── spline.hpp
├── animation_state.hpp
├── input_source.hpp
├── post_processor.hpp
└── null_adapters.hpp
docs/
└── coordinate-convention.md
tests/unit/
└── engine_interfaces_test.cpp
```

## API Sketch

```cpp
namespace cinerig
{
using EntityId = uint64_t;
using CameraId = uint64_t;
using SplineId = uint64_t;
using GroupId = uint64_t;

enum class UpdateClock { Fixed, Late };

struct Transform
{
    Vec3 position{};
    Quat rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct LayerMask
{
    uint32_t bits = 0xFFFFFFFFu;
    bool contains(uint32_t layer) const;
};

struct QueryFilter
{
    LayerMask layers;
    EntityId ignoreEntity = 0;
};

struct Ray
{
    Vec3 origin{};
    Vec3 direction{0.0f, 0.0f, -1.0f};
};

struct RaycastHit
{
    EntityId entity = 0;
    Vec3 point{};
    Vec3 normal{};
    float distance = 0.0f;
};

struct SphereCastHit : RaycastHit
{
    float radius = 0.0f;
};

struct OverlapHit
{
    EntityId entity = 0;
    Vec3 point{};
    float distance = 0.0f;
};

class IEntityWorld
{
public:
    virtual ~IEntityWorld() = default;

    virtual bool exists(EntityId id) const = 0;
    virtual Transform transform(EntityId id) const = 0;

    // 可选：虚拟相机的 owner transform 需要回写时使用。
    // 返回 false 表示该实体不可写，调用方只更新内部状态。
    virtual bool trySetTransform(EntityId id, const Transform& transform)
    {
        (void)id; (void)transform;
        return false;
    }

    // 可选：SmartUpdate 用。返回目标最近一次变换发生在哪个时钟。
    // 默认 Late；引擎可实现更精确的跟踪。
    virtual UpdateClock lastChangeClock(EntityId id) const
    {
        (void)id;
        return UpdateClock::Late;
    }

    virtual float deltaTime(UpdateClock clock) const = 0;
    virtual float fixedDeltaTime() const = 0;
    virtual float time() const = 0;
    virtual uint64_t frame(UpdateClock clock) const = 0;
    virtual Vec3 worldUp() const = 0;
    virtual Vec2 viewportSize() const = 0;
};

struct AppliedCamera
{
    CameraState state{};   // state.lens 是最终镜头设置
    float aspect = 1.0f;
    bool hasCamera = false;
};

class ICameraOutput
{
public:
    virtual ~ICameraOutput() = default;

    // 由 Brain 在混合完成后调用。
    virtual void apply(const AppliedCamera& camera) = 0;

    // 当 LensSettings::modeOverride == None 时，用于继承输出相机设置。
    virtual LensSettings outputLens() const = 0;
    virtual float aspect() const = 0;
};

class IPhysicsWorld
{
public:
    virtual ~IPhysicsWorld() = default;

    virtual std::optional<RaycastHit> raycast(
        const Ray& ray, float maxDistance, const QueryFilter& filter) const = 0;

    virtual std::optional<SphereCastHit> sphereCast(
        const Ray& ray, float radius, float maxDistance,
        const QueryFilter& filter) const = 0;

    virtual std::vector<OverlapHit> overlapSphere(
        Vec3 center, float radius, const QueryFilter& filter) const = 0;

    virtual std::optional<Bounds> bounds(EntityId id) const = 0;
};

class ISpline
{
public:
    virtual ~ISpline() = default;

    virtual float length() const = 0;
    virtual Vec3 evaluatePosition(float t) const = 0; // normalized 0..1
    virtual Vec3 evaluateTangent(float t) const = 0;
    virtual Vec3 evaluateUp(float t) const = 0;
    virtual float closestPoint(Vec3 worldPosition) const = 0; // normalized 0..1
};

class ISplineProvider
{
public:
    virtual ~ISplineProvider() = default;
    virtual const ISpline* get(SplineId id) const = 0;
};

class IAnimationStateProvider
{
public:
    virtual ~IAnimationStateProvider() = default;

    // 返回当前状态和下一个状态的哈希，以及归一化时间。
    virtual int currentStateHash(EntityId entity, int layer) const = 0;
    virtual int nextStateHash(EntityId entity, int layer) const = 0;
    virtual float normalizedTime(EntityId entity, int layer) const = 0;
};

class IInputSource
{
public:
    virtual ~IInputSource() = default;
    virtual float axis(std::string_view name) const = 0;
    virtual Vec2 axis2D(std::string_view name) const = 0;
};

class IPostProcessor
{
public:
    virtual ~IPostProcessor() = default;
    virtual void apply(const AppliedCamera& camera) = 0;
};

struct NullPhysicsWorld final : IPhysicsWorld { /* 返回 nullopt/空 vector */ };
struct NullInputSource final : IInputSource { /* 返回 0 */ };
struct NullPostProcessor final : IPostProcessor { /* no-op */ };
}
```

## Behavior Details

### 坐标与单位

- 所有 `Vec3`、`Quat` 都在核心约定下：右手系、+Y up、-Z forward。
- 引擎适配器负责把引擎坐标转换到核心约定；CineRig 不猜测引擎手性。
- 时间单位为秒；`deltaTime` 可以为 0，负数表示“本次不做时间相关更新”。
- `viewportSize` 单位为像素；宽高用于 aspect 和屏幕空间构图。

### UpdateClock 语义

- `Fixed`：物理/固定步长更新。
- `Late`：渲染前更新。
- `frame(UpdateClock)` 返回对应时钟的帧号：
  - `Late` 使用全局渲染帧号。
  - `Fixed` 使用固定步长帧号。
- `lastChangeClock` 用于 SmartUpdate。默认实现返回 `Late`，保证没有精确跟踪时不会误判。

### 输出相机

- `AppliedCamera::state` 已经包含 correction、Dutch 和最终 `state.lens`；适配器可直接使用 `state.finalPosition()` / `state.finalOrientation()` / `state.lens`。
- `aspect` 是输出相机当前 aspect。
- 若 `lens.modeOverride == None`，适配器在应用前可调用 `outputLens()` 继承引擎相机的正交/物理设置；Core 层在 P1 处理该逻辑。
- `AppliedCamera::hasCamera == false` 表示本帧没有 live 相机，适配器应保持上一帧或使用默认相机。

### 物理查询

- 统一使用 `QueryFilter`，不直接使用 Unity 的 tag/layer 概念。
- `ignoreEntity` 替代原项目的 `IgnoreTag`：由适配器决定是否忽略整个实体。
- `overlapSphere` 返回的顺序不保证；需要最近结果的调用方自行排序。
- 物理接口是可选依赖：没有物理世界时，Deoccluder/Confiner3D 应安全禁用。

### 样条接口

- `ISpline` 使用 normalized 0..1 参数；距离/节点单位由 P4 的 `SplineSettings` 转换。
- `evaluateUp` 返回核心坐标下的 up 向量；`SplineRoll` 在 P4 处理。
- `closestPoint` 可以先粗采样再细化；接口不规定算法。

### 输入接口

- `axis(name)` 和 `axis2D(name)` 允许适配器按名称查表；名称在示例中使用固定字符串。
- P2 的 `InputAxisController` 会把 `InputAxis` 绑定到 `IInputSource`。

### Null 实现

- 所有可选接口必须提供 Null 实现，避免调用方到处判空。
- `NullPhysicsWorld::raycast` 返回 `std::nullopt`；`overlapSphere` 返回空 vector。
- `NullInputSource` 返回 0；`NullPostProcessor` 无操作。

## Tests

- FakeEntityWorld：实体存在/变换/时间/帧号/worldUp/viewport。
- FakeCameraOutput：`apply` 收到正确 final position/orientation/lens/aspect。
- `trySetTransform` 默认返回 false；可写实现返回 true 并更新变换。
- `lastChangeClock` 默认 Late。
- NullPhysicsWorld 返回空结果；NullInputSource 返回 0；NullPostProcessor 无操作。
- QueryFilter 的 layer 位判断，包含 layer 32 边界。
- FakeSpline / FakeSplineProvider / FakeAnimationState 覆盖可选接口。

## Acceptance Checklist

- [x] 核心库不包含任何引擎头文件。
- [x] 所有接口都有明确的所有权和生命周期注释。
- [x] `IEntityWorld` 能覆盖 Fixed/Late、时间、帧号、视口。
- [x] `ICameraOutput` 能应用位置、朝向、Lens、aspect。
- [x] 可选接口都有 Null 实现。
- [x] 坐标约定文档完成。
- [x] 单元测试全绿。

## 参考原项目（Cinemachine）

> 参考接口职责和调用时机；用 C++ 重写，不复制 C# 代码。

### 必读源码

- `com.unity.cinemachine/Runtime/Core/ICinemachineCamera.cs`：相机抽象的最小职责。
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineBrain.cs:162-171, 400-520`
  - `m_OutputCamera` 获取、状态应用、镜头模式覆盖。
- `com.unity.cinemachine/Runtime/Core/TargetTracking.cs:170-424`
  - Tracker 如何读取目标位置/旋转；P0 只参考接口需求。
- `com.unity.cinemachine/Runtime/Core/RuntimeUtility.cs:45-294`
  - 射线/球扫/IgnoreTag 的语义；C++ 用 `QueryFilter` 替代。
- `com.unity.cinemachine/Runtime/Core/SplineSettings.cs`
  - `ISplineReferencer`、位置单位、缓存；P0 只定义 `ISpline`。
- `com.unity.cinemachine/Runtime/Core/InputAxis.cs:73-83`
  - `IInputAxisReader`；C++ 用 `IInputSource`。
- `com.unity.cinemachine/Runtime/PostProcessing/CinemachinePostProcessing.cs`
  - 后处理扩展的职责边界。
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineExternalCamera.cs`
  - 外部相机如何暴露状态。
- `com.unity.cinemachine/Runtime/Core/CinemachineCameraManagerBase.cs`
  - 管理器如何读取子相机状态。

### 必读测试

- `com.unity.cinemachine/Tests/Runtime/RuntimeUtilityTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CinemachineInputAxisControllerTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CinemachineBrainSetupTest.cs`
- `com.unity.cinemachine/Tests/Runtime/ManualUpdateTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CheckPhysicalCameraProperties.cs`

### 需要核对的行为

- Brain 什么时候读取输出相机的 lens/aspect。
- `LensSettings.ModeOverride == None` 的继承逻辑。
- `RuntimeUtility` 的忽略标签/穿透逻辑，C++ 用 `ignoreEntity` 后行为如何变化。
- `IInputAxisReader` 的 `hint` 语义是否需要在 `IInputSource` 中保留。
- Spline 的位置单位、缓存和 transform 变换。

### 差异记录

- 原项目用 Unity `Transform`/`Camera`/`Collider` 指针；C++ 用 `EntityId`/`CameraId`/句柄，避免生命周期问题。
- 原项目用 tag 忽略碰撞体；C++ 用 `QueryFilter::ignoreEntity`，引擎适配器可扩展为 tag/layer。
- 原项目 `IInputAxisReader` 有 `hint` 参数；C++ P0 先简化为 `axis(name)`，P2 如需多维映射再扩展。
- 原项目 `ICinemachineCamera` 包含激活事件和父相机；C++ 在 P0-04/P1-02 分层实现。
- `Ray` 复用 `math::Ray`，`Bounds` 复用 `math::Bounds`，避免在 engine 层复制几何类型；physics 查询结果使用独立的 `RaycastHit` / `SphereCastHit` / `OverlapHit`。
- `IEntityWorld::transform()` 对不存在的实体返回 identity；调用方应先调用 `exists()`。原项目直接返回 `Transform` 引用，因此没有该协议，但 C++ 句柄方案必须定义缺失实体行为。
- `AppliedCamera` 不携带引擎相机句柄；`ICameraOutput` 就是宿主相机的所有权边界。`BlendHint::NoPosition` / `NoOrientation` / `NoLens` 保留在 state 中，适配器可按 Brain 的规则选择是否应用对应分量。
- `outputLens()` 只提供继承来源；`LensSettings::modeOverride == None` 的最终合并逻辑在 P1 的运行时层处理，P0 不提前复制 Unity 的 `LensModeOverride` 行为。
- `NullPhysicsWorld` / `NullInputSource` / `NullPostProcessor` 都是 header-only、无状态、可复制的 no-op 实现；调用方可以安全地将其作为默认依赖注入。
- 时间接口不拥有计时器：`Fixed`/`Late` 的 `deltaTime`、`frame` 和 `time` 由宿主引擎提供。负 `deltaTime` 表示跳过时间相关更新。

## 实现记录

- 新增 `include/cinerig/engine/entity_world.hpp`：实体/相机/样条/组句柄、`UpdateClock`、`Transform`、`IEntityWorld`。
- 新增 `include/cinerig/engine/camera_output.hpp`：`AppliedCamera`、`ICameraOutput`。
- 新增 `include/cinerig/engine/physics_world.hpp`：`LayerMask`、`QueryFilter`、`Ray`/`Bounds` 别名、命中结构、`IPhysicsWorld`。
- 新增 `include/cinerig/engine/spline.hpp`：`ISpline`、`ISplineProvider`。
- 新增 `include/cinerig/engine/animation_state.hpp`：`IAnimationStateProvider`。
- 新增 `include/cinerig/engine/input_source.hpp`：`IInputSource`。
- 新增 `include/cinerig/engine/post_processor.hpp`：`IPostProcessor`。
- 新增 `include/cinerig/engine/null_adapters.hpp`：`NullPhysicsWorld`、`NullInputSource`、`NullPostProcessor`。
- `include/cinerig/cinerig.hpp` 已暴露全部 engine 头文件。
- `docs/coordinate-convention.md` 已补充实体、UpdateClock、输出相机、物理、样条、输入单位及适配器职责。
- 新增 `tests/unit/engine_interfaces_test.cpp`：
  - FakeEntityWorld 覆盖存在性、transform、trySetTransform、lastChangeClock、Fixed/Late 时间与帧号、worldUp、viewport。
  - ReadOnlyEntityWorld 验证默认 `trySetTransform() == false`、默认 `lastChangeClock() == Late`。
  - FakeCameraOutput 验证 final position/orientation/lens/aspect、`outputLens()` 和 `hasCamera == false`。
  - NullPhysicsWorld / NullInputSource / NullPostProcessor 验证空结果和安全 no-op。
  - FakeSpline / FakeSplineProvider / FakeAnimationState 验证接口可正常实现和调用。
  - LayerMask 位判断（含 layer 32 边界）和 QueryFilter 默认值。
- Windows Debug 与 Release：71/71 测试通过（70 unit + 1 scenario）。
- 8 个 engine 头文件均通过 MSVC `/W4 /std:c++20 /permissive-` 独立编译。
- 安装后的 `find_package(CineRig CONFIG)` 消费者已包含并使用 `NullPhysicsWorld`、`NullInputSource`、`QueryFilter`，构建和运行通过。
- GLM 配置 CTest 71/71 通过；`CINERIG_BUILD_TESTS=OFF` 配置可构建。
- 未在 Linux 实机执行。
