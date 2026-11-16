# P0-03 Core Types

Status: completed  
Depends on: P0-02  
Blocks: P0-04, P1-01, P1-02, P1-03

## Goal

实现相机状态、镜头、构图、Lookahead、噪声、优先级、输出通道和目标引用等基础值类型，作为运行时唯一的相机数据契约。

## Scope

- `CameraState` 及其 `BlendHints`。
- `LensSettings` 及物理镜头子结构。
- `ScreenComposerSettings`（DeadZone、HardLimits、ScreenPosition）。
- `LookaheadSettings`。
- `NoiseProfile`（对应原 `NoiseSettings`）。
- `PrioritySettings`、`OutputChannels`。
- `TargetRef`、`CameraTarget`、`TargetGroup` 的接口前身（具体实现 P2/P3）。
- `CameraState::Default`、`LensSettings::Default`、`ScreenComposerSettings::Default`。
- 验证函数和近似比较。

## Non-goals

- 不实现 `CameraState::Lerp`（P0-04）。
- 不实现 Tracker/BindingMode（P2）。
- 不实现 TargetGroup 的包围盒计算（P2/P3）。
- 不实现序列化。

## Deliverables

```text
include/cinerig/core/
├── camera_state.hpp
├── lens_settings.hpp
├── screen_composer_settings.hpp
├── lookahead_settings.hpp
├── noise_profile.hpp
├── priority.hpp
├── output_channels.hpp
└── camera_target.hpp
tests/unit/
├── camera_state_test.cpp
├── lens_settings_test.cpp
├── screen_composer_test.cpp
└── priority_channel_test.cpp
```

## API Sketch

```cpp
namespace cinerig
{
enum class OutputChannels : uint32_t
{
    Default   = 1u << 0,
    Channel01 = 1u << 1,
    // ... Channel15
};

struct PrioritySettings
{
    bool enabled = false;
    int value = 0; // enabled=false 时按 0 处理
};

enum class BlendHint : uint32_t
{
    Nothing = 0,
    SphericalPositionBlend = 1u << 0,
    CylindricalPositionBlend = 1u << 1,
    ScreenSpaceAimWhenTargetsDiffer = 1u << 2,
    InheritPosition = 1u << 3,
    IgnoreLookAtTarget = 1u << 4,
    FreezeWhenBlendingOut = 1u << 5,

    NoPosition = 1u << 16,
    NoOrientation = 1u << 17,
    NoTransform = NoPosition | NoOrientation,
    NoLens = 1u << 18,
};
// 提供位运算辅助

struct LensSettings
{
    float fieldOfView = 40.0f;        // vertical degrees
    float orthographicSize = 10.0f;
    float nearClipPlane = 0.1f;
    float farClipPlane = 5000.0f;
    float dutch = 0.0f;

    enum class OverrideMode { None, Orthographic, Perspective, Physical };
    OverrideMode modeOverride = OverrideMode::None;

    struct PhysicalSettings
    {
        enum class GateFit { None, Horizontal, Vertical, Fill, Overscan };
        GateFit gateFit = GateFit::Horizontal;
        Vec2 sensorSize{21.946f, 16.002f};
        Vec2 lensShift{0.0f, 0.0f};
        float focusDistance = 10.0f;
        int iso = 200;
        float shutterSpeed = 0.005f;
        float aperture = 16.0f;
        int bladeCount = 5;
        Vec2 curvature{2.0f, 11.0f};
        float barrelClipping = 0.25f;
        float anamorphism = 0.0f;
    } physical;

    bool orthographic = false;
    bool physicalCamera = false;
    float aspect = 1.0f;

    void validate();
};

struct ScreenComposerSettings
{
    Vec2 screenPosition{0.0f, 0.0f};

    struct DeadZoneSettings
    {
        bool enabled = false;
        Vec2 size{0.2f, 0.2f};
    } deadZone;

    struct HardLimitSettings
    {
        bool enabled = false;
        Vec2 size{0.8f, 0.8f};
        Vec2 offset{0.0f, 0.0f};
    } hardLimits;

    Vec2 effectiveDeadZoneSize() const;
    Vec2 effectiveHardLimitSize() const;
    void validate();
};

struct LookaheadSettings
{
    bool enabled = false;
    float time = 0.0f;       // 0..1
    float smoothing = 0.0f;  // 0..30
    bool ignoreY = false;
};

struct NoiseProfile
{
    struct Params
    {
        float frequency = 0.0f;
        float amplitude = 0.0f;
        bool constant = false;
    };
    struct TransformParams { Params x, y, z; };

    std::vector<TransformParams> positionNoise;
    std::vector<TransformParams> orientationNoise;
};

struct CameraState
{
    LensSettings lens;
    Vec3 referenceUp{0.0f, 1.0f, 0.0f};
    Vec3 referenceLookAt; // NaN 表示无 LookAt
    Vec3 rawPosition{};
    Quat rawOrientation{}; // identity
    Quat rotationDampingBypass{};
    float shotQuality = 1.0f;
    Vec3 positionCorrection{};
    Quat orientationCorrection{};
    BlendHint blendHint = BlendHint::Nothing;
    // custom blendables: P0 先留接口，具体存储 P0-04 决定

    static CameraState makeDefault();
    bool hasLookAt() const;
    Vec3 correctedPosition() const;
    Quat correctedOrientation() const;
    Vec3 finalPosition() const;
    Quat finalOrientation() const; // 应用 Dutch
};
}
```

## Behavior Details

### CameraState

对齐 `Runtime/Core/CameraState.cs:20-131`：

- `referenceLookAt` 使用 NaN 表示“没有 LookAt”；提供 `hasLookAt()`。
- `rawPosition` / `rawOrientation` 是未被 correction 影响的值。
- `positionCorrection` / `orientationCorrection` 每帧重新计算，不反馈到下一帧。
- `rotationDampingBypass` 供 Body 组件抵消 Aim 阻尼。
- `shotQuality` 默认 1。
- `blendHint` 默认 `Nothing`。
- `finalOrientation()`：先应用 correction，再应用 `lens.dutch` 绕前向轴旋转。
- `correctedOrientation()` 不应用 Dutch。

### LensSettings

对齐 `Runtime/Core/LensSettings.cs:11-386`：

- 默认值见上表。
- `orthographic` / `physicalCamera` 在 `modeOverride == None` 时从输出相机继承；P0 先保存字段，P1 由 `ICameraOutput` 更新。
- `aspect` 在物理镜头下由 `sensorSize.x / sensorSize.y` 决定，否则从输出相机继承。
- `validate()`：
  - `farClipPlane >= nearClipPlane + 0.001`。
  - `fieldOfView` clamp 到 `[0.01, 179]`。
  - sensor size 每轴 >= 0.1。
  - focus distance >= 0.01。
  - shutter speed >= 0。
  - aperture clamp 到 Unity `Camera` 常量 `[0.7, 32]`。
  - blade count clamp 到 `[3, 9]`。
  - barrel clipping `[0,1]`，anamorphism `[-1,1]`。
  - curvature x clamp 到 `[0.7, 32]`，curvature y clamp 到 `[curvature.x, 32]`。
  - aspect 为 0 时重置为 1。
- `LensSettings::Lerp` 在 P0-04 实现。

### ScreenComposerSettings

对齐 `Runtime/Core/ScreenComposerSettings.cs`：

- `screenPosition` 是屏幕中心偏移，`0` 为屏幕中心，`±0.5` 为边缘。
- DeadZone 默认关闭，大小 `(0.2, 0.2)`。
- HardLimits 默认关闭，大小 `(0.8, 0.8)`，offset `(0,0)`。
- `validate()`：
  - screenPosition 每轴 clamp 到 `[-1.5, 1.5]`。
  - deadZone size 每轴 clamp 到 `[0, 2]`。
  - hardLimits size 每轴 clamp 到 `[deadZone, 3]`。
  - hardLimits offset 每轴 clamp 到 `[-1, 1]`。
- `effectiveDeadZoneSize`：关闭时为 0。
- `effectiveHardLimitSize`：关闭时为 `(3,3)`。

### NoiseProfile

对齐 `Runtime/Core/NoiseSettings.cs`：

- 每个通道 `getValueAt(time, offset)`：
  - `t = frequency * time + offset`。
  - `constant == true`：`cos(t * 2π) * amplitude * 0.5`。
  - 否则：`(perlin1D(t) - 0.5) * amplitude`。
- 3 轴通道分别累加。
- P0 只定义数据结构和采样函数；P3 实现 `BasicMultiChannelPerlin`。

### Priority / OutputChannels

对齐 `Runtime/Core/PrioritySettings.cs`、`OutputChannel.cs`：

- `priority.value` 只在 `enabled == true` 时有效；否则按 0。
- `OutputChannels` 是 `uint32_t` flags，默认 `Default`。
- Brain 通过 `channelMask` 过滤相机。

### CameraTarget

对齐 `Runtime/Core/CameraTarget.cs`：

```cpp
struct TargetRef
{
    enum class Kind { None, Entity, Camera, Group };
    Kind kind = Kind::None;
    uint64_t id = 0;

    static TargetRef entity(uint64_t id);
    static TargetRef camera(uint64_t id);
    static TargetRef group(uint64_t id);
};

struct CameraTarget
{
    TargetRef tracking;
    TargetRef lookAt;
    bool customLookAt = false;
};
```

- `customLookAt == false` 时，LookAt 使用 tracking。
- `customLookAt == true` 时，LookAt 使用 lookAt。
- TargetRef 的解析由 `CameraSystem` / `IEntityWorld` 负责，不在结构体里保存指针。

## Tests

- 默认值：CameraState/Lens/ScreenComposer/Lookahead 与上表一致。
- `hasLookAt`：默认 false；设置有限值后 true。
- `finalOrientation`：Dutch 0 与 90 度的结果。
- Lens `validate`：FOV、near/far、sensor、aperture 边界。
- ScreenComposer `validate`：dead zone/hard limit 关系。
- NoiseProfile：constant 通道、perlin 通道、3 轴累加。
- Priority：enabled=false 时值为 0。
- OutputChannels 位运算。

## Acceptance Checklist

- [x] 所有结构体有明确默认值。
- [x] `CameraState::makeDefault()` 与 Unity 默认一致。
- [x] `hasLookAt()` 使用 NaN 判断。
- [x] Lens validate 覆盖所有边界。
- [x] ScreenComposer validate 覆盖所有边界。
- [x] `NoiseProfile` 采样函数与 `NoiseSettings` 公式一致。
- [x] 单元测试全绿。

## 参考原项目（Cinemachine）

> 参考字段、默认值、验证和边界条件；用 C++ 重写，不复制 C# 代码。

### 必读源码

- `com.unity.cinemachine/Runtime/Core/CameraState.cs:20-131`：字段、BlendHints、Default。
- `com.unity.cinemachine/Runtime/Core/LensSettings.cs:11-386`：字段、默认值、Validate、Lerp 前的数据契约。
- `com.unity.cinemachine/Runtime/Core/ScreenComposerSettings.cs`：默认值、Validate、Effective 尺寸。
- `com.unity.cinemachine/Runtime/Core/LookaheadSettings.cs`
- `com.unity.cinemachine/Runtime/Core/NoiseSettings.cs`
- `com.unity.cinemachine/Runtime/Core/PrioritySettings.cs`
- `com.unity.cinemachine/Runtime/Core/OutputChannel.cs`
- `com.unity.cinemachine/Runtime/Core/CameraTarget.cs`
- `com.unity.cinemachine/Runtime/Core/TargetTracking.cs:53-168`：TrackerSettings 仅用于确认后续字段，P0 不实现。

### 必读测试

- `com.unity.cinemachine/Tests/Editor/CameraStateTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CheckPhysicalCameraProperties.cs`
- `com.unity.cinemachine/Tests/Runtime/PriorityTests.cs`
- `com.unity.cinemachine/Tests/Runtime/PriorityTestsImplicitCastingOperators.cs`
- `com.unity.cinemachine/Tests/Editor/InputAxisTests.cs`（仅参考边界校验风格）

### 需要核对的行为

- `CameraState.Default` 的每个字段。
- `kNoPoint` 的 NaN 表示。
- `LensSettings.Default` 的物理镜头默认值。
- `ScreenComposerSettings.Default` 的 DeadZone/HardLimits 默认值和 validate 关系。
- `PrioritySettings` 的隐式转换语义（enabled 与 value 的关系）。
- `NoiseSettings` 的 constant/perlin 公式和 `0.5` 偏移。

### 差异记录

- C++ 的 `TargetRef` 用 `Kind + uint64_t` 替代 Unity `Transform` 指针，以支持实体/相机/组和跨引擎句柄。
- `TargetGroup` 在 P0-03 只定义抽象成员接口（有效性、数量、位置、旋转、权重、半径）；包围盒/角度包围和具体实现留给 P2/P3。
- `LensSettings` 的 `orthographic` / `physicalCamera` / `aspect` 在 Unity 中带内部继承状态；C++ 先用显式字段，P1 由输出适配器填充。
- `PrioritySettings::value` 保留原始值，但只有 `enabled == true` 时才通过 `effectiveValue()` / `operator int()` 生效；`setValue()` 会同时启用。这比 Unity 的属性 setter 更显式，便于 C++ 侧避免隐式状态变化。
- `NoiseProfile` 的 constant 通道严格使用 `cos(t * 2π) * amplitude * 0.5`；非 constant 通道使用 CineRig 自带的确定性一维 Perlin 噪声。Unity `Mathf.PerlinNoise(t, 0)` 是引擎原生二维噪声，P3 需要再用 golden data 校准。
- `LensSettings` 的 aperture/blade 常量已直接对齐 Unity `Camera` 的 `0.7..32` / `3..9`，不再使用任务早期草案中的临时 `1..64`。

## 实现记录

- 新增 `include/cinerig/core/`：`camera_state.hpp`、`lens_settings.hpp`、`screen_composer_settings.hpp`、`lookahead_settings.hpp`、`noise_profile.hpp`、`priority.hpp`、`output_channels.hpp`、`camera_target.hpp`。
- `cinerig.hpp` 已汇总包含全部核心类型头文件。
- `CameraState` 字段、默认值、`kNoPoint`、`Default` / `makeDefault()`、`hasLookAt()`、corrected/final 访问器已实现；Dutch 在 correction 之后绕局部 `-Z` 应用。
- `BlendHint` 已提供 `NoTransform`、`NoLens` 和完整位运算辅助；`CameraState::BlendHints` 别名保持 Unity 命名兼容。
- `LensSettings` 默认值和 `validate()` 已对齐 `LensSettings.cs`；默认 physical 参数、`areEqual()` 和 Unity camera 常量已覆盖。
- `ScreenComposerSettings` 默认值、`validate()`、`effectiveDeadZoneSize()`、`effectiveHardLimitSize()`、`approximately()` 已实现。
- `LookaheadSettings`、`PrioritySettings`、`OutputChannels`、`TargetRef`、`CameraTarget` 已实现；`TargetGroup` 为 P2/P3 预留抽象接口。
- `NoiseProfile` 已实现 constant/Perlin 通道、三轴 `TransformParams` 和多个噪声通道累加；`perlinNoise1D` 为确定性一维实现。
- 新增单元测试：`camera_state_test.cpp`、`lens_settings_test.cpp`、`screen_composer_test.cpp`、`priority_channel_test.cpp`、`noise_profile_test.cpp`、`camera_target_test.cpp`、`lookahead_settings_test.cpp`。
- 同时修复了 P0-02 `math::angle(Quat, Quat)` 在小角度和 gimbal 场景下的 `acos` 精度问题，改用 `4 * atan2(|a-b|, |a+b|)` 的稳定形式。
- Windows Debug/Release 验证：`cmake --build --preset windows` 与 `windows-release` 均无警告，两个 preset 的 CTest 均为 45/45 通过（44 unit + 1 scenario）。
- 所有新增 `include/cinerig/core/*.hpp` 已单独用 MSVC `/W4 /std:c++20` 编译通过。
- 安装后的 `find_package(CineRig CONFIG)` 消费者已包含并使用 `CameraState`、`NoiseProfile`、`OutputChannels`，构建和运行通过。
- 未在 Linux 实机执行；核心类型只使用标准库。
