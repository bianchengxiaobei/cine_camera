# P0-04 CameraState and Blending

Status: completed  
Depends on: P0-02, P0-03  
Blocks: P1-03

## Goal

实现相机状态插值和混合定义，使任意两个 `CameraState` 能按 Unity Cinemachine 的规则进行位置、朝向、镜头和 LookAt 的混合。

## Scope

- `CameraState::lerp(stateA, stateB, t)`。
- `LensSettings::lerp(a, b, t)`。
- `BlendDefinition`：Cut、EaseInOut、EaseIn、EaseOut、HardIn、HardOut、Linear、Custom。
- `Blend`：CamA/CamB、curve、duration、time、weight、isComplete、custom blender 接口。
- `BlendHints` 的组合语义。
- `CustomBlendable` 的固定槽位 + overflow 设计。
- `CameraState::isTargetOffscreen`。
- 最小 `ICinemachineCamera` 接口（P1-02 扩展为完整虚拟相机接口）。

## Non-goals

- 不实现 `BlendManager`（P1-03）。
- 不实现 `CameraBlendStack` / override frames（P1-03）。
- 不实现序列化。
- 不实现自定义曲线的 Unity `AnimationCurve` 资产导入。

## Deliverables

```text
include/cinerig/core/
├── camera_state_blend.hpp
├── blend_definition.hpp
├── blend.hpp
└── camera_interface.hpp
src/core/
├── camera_state_blend.cpp
└── blend.cpp
tests/unit/
├── camera_state_lerp_test.cpp
├── lens_lerp_test.cpp
└── blend_definition_test.cpp
```

## API Sketch

```cpp
namespace cinerig
{
class ICinemachineMixer;

// P0 最小接口：Blend 只需要读取状态、名称和有效性。
// P1-02 的 VirtualCamera 实现它，并增加激活事件和父相机能力。
class ICinemachineCamera
{
public:
    virtual ~ICinemachineCamera() = default;
    virtual std::string_view name() const = 0;
    virtual const CameraState& state() const = 0;
    virtual bool isValid() const = 0;
    virtual ICinemachineMixer* parentCamera() const { return nullptr; }
};

enum class BlendStyle
{
    Cut,
    EaseInOut,
    EaseIn,
    EaseOut,
    HardIn,
    HardOut,
    Linear,
    Custom,
};

struct BlendDefinition
{
    BlendStyle style = BlendStyle::EaseInOut;
    float time = 2.0f;
    math::Curve customCurve{}; // 仅 style == Custom

    float blendTime() const;   // Cut -> 0
    math::Curve curve() const; // 返回对应曲线
};

class IBlender
{
public:
    virtual ~IBlender() = default;
    virtual CameraState getIntermediateState(
        const ICinemachineCamera& camA,
        const ICinemachineCamera& camB,
        float t) = 0;
};

class Blend
{
public:
    ICinemachineCamera* camA = nullptr;
    ICinemachineCamera* camB = nullptr;
    math::Curve blendCurve{};
    float timeInBlend = 0.0f;
    float duration = 0.0f;
    IBlender* customBlender = nullptr;

    float blendWeight() const;
    bool isComplete() const;
    bool isValid() const;
    bool uses(const ICinemachineCamera* cam) const;
    void clearBlend();
    void copyFrom(const Blend& src);
};

CameraState lerp(const CameraState& a, const CameraState& b, float t);
}
```

## Behavior Details

### CameraState::Lerp

严格对齐 `Runtime/Core/CameraState.cs:213-362`，按以下顺序：

1. `t = clamp01(t)`；`adjustedT = t`。
2. 合并 BlendHint：
   - 两个状态都带 `NoPosition` → 结果带 `NoPosition`。
   - 两个状态都带 `NoOrientation` → 结果带 `NoOrientation`。
   - 两个状态都带 `NoLens` → 结果带 `NoLens`。
   - 任一状态带 `SphericalPositionBlend` → 结果带该 hint。
   - 任一状态带 `CylindricalPositionBlend` → 结果带该 hint。
   - 任一状态带 `FreezeWhenBlendingOut` → 结果带该 hint。
3. Lens：
   - 若任一状态不是 `NoLens`，使用 `LensSettings::Lerp`。
   - 否则根据两个状态的 `NoLens` 情况选择 A 或 B 的 lens。
4. `referenceUp = slerp(a.referenceUp, b.referenceUp, t)`。
5. `shotQuality = lerp(a.shotQuality, b.shotQuality, t)`。
6. PositionCorrection / OrientationCorrection：
   - 若两个状态都带 `NoPosition` → 保持原值。
   - 若仅 A 带 `NoPosition` → 使用 B 的值。
   - 若仅 B 带 `NoPosition` → 使用 A 的值。
   - 否则线性/slerp。
7. LookAt：
   - 任一状态没有 LookAt，或任一状态带 `IgnoreLookAtTarget` → 结果无 LookAt。
   - 否则：
     - 若非正交且 FOV 不同，按目标距离插值 FOV，保持目标在画面中的高度。
     - 用调整后的 `adjustedT` 插值 `referenceLookAt`。
8. RawPosition：
   - 使用 `InterpolatePosition`：
     - 若两个 LookAt 都有效且带 `CylindricalPositionBlend`：在 up 平面上围绕 pivot 做圆柱插值。
     - 否则若带 `SphericalPositionBlend`：围绕 pivot 做球面插值。
     - 否则线性插值。
   - 应用 `NoPosition` hint 的取舍规则。
9. ScreenSpaceAimWhenTargetsDiffer：
   - 若结果有 LookAt 且任一状态带该 hint：
     - `referenceLookAt = rawPosition + slerp(a.lookAt - rawPosition, b.lookAt - rawPosition, adjustedT)`。
10. RawOrientation：
    - 若两个状态都没有 `NoOrientation`：
      - 若结果有 LookAt 且两个 raw orientation 夹角 > epsilon：
        - 计算目标方向 `dirTarget = lookAt - correctedPosition`。
        - 若 `dirTarget` 几乎为 0 或带 `IgnoreLookAtTarget`：slerp。
        - 否则：
          - 用 `lookRotation(dirTarget, up)` 得到基础朝向。
          - 计算 A/B 的相机到目标旋转偏移（`GetCameraRotationToTarget`）。
          - 用 `adjustedT` 插值偏移，再 `ApplyCameraRotation`。
      - 否则 slerp。
    - 应用 `NoOrientation` hint 的取舍规则。
11. CustomBlendables：
    - 固定 4 个槽位 + overflow vector。
    - A 的权重乘 `(1-t)`，B 的权重乘 `t`。
    - 同类型合并权重。
    - P0 先实现 `CustomBlendable { std::shared_ptr<void> data; float weight; }` 或 `typeId + shared_ptr<void>`；在实现记录中确定最终方案。

### LensSettings::Lerp

对齐 `Runtime/Core/LensSettings.cs:294-334`：

- `t < 0.5`：以 A 为基础，向 B 插值 `t`。
- `t >= 0.5`：以 B 为基础，向 A 插值 `1 - t`。
- 插值字段：
  - far/near clip、FOV、ortho size、Dutch。
  - sensor size、lens shift、focus distance、ISO（四舍五入）、shutter、aperture、blade count（四舍五入）、curvature、barrel clipping、anamorphism。
- 不可插值字段（mode override、gate fit、orthographic/physical 状态）保持基础状态的值。

### FOV 插值

对齐 `Runtime/Core/CameraState.cs:364-375`：

```text
hA = dA * 2 * tan(fovA / 2)
hB = dB * 2 * tan(fovB / 2)
h  = lerp(hA, hB, t)
d  = lerp(dA, dB, t)
fov = d > epsilon ? 2 * atan(h / (2d)) : 179
fov = clamp(fov, min(fovA, fovB), max(fovA, fovB))
```

- `dA` / `dB` 为目标到相机 corrected position 的距离，至少 clamp 到 near clip。
- 使用弧度计算，返回度。

### BlendDefinition

- `BlendTime`：`Cut` 返回 0，否则返回 `time`。
- 标准曲线：
  - `Cut` 不需要曲线，weight 在 Eval 时直接视为 1。
  - `EaseInOut`：两端切线 0。
  - `EaseIn`：起点外切线 1.4，终点内切线 0。
  - `EaseOut`：起点外切线 0，终点内切线 1.4。
  - `HardIn`：起点外切线 0，终点内切线 3。
  - `HardOut`：起点外切线 3，终点内切线 0。
  - `Linear`：线性。
  - `Custom`：使用 `customCurve`；为空时回退到 EaseInOut。
- 权重 clamp 到 `[0,1]`。

### Blend

对齐 `Runtime/Core/CinemachineBlend.cs:10-164`：

- `BlendWeight`：如果 curve 为空、关键帧少于 2 或已完成 → 1；否则 `clamp01(curve.evaluate(timeInBlend / duration))`。
- `IsComplete`：`timeInBlend >= duration` 或无效。
- `IsValid`：至少一个相机有效。
- `Uses(cam)`：支持嵌套 blend source。
- `ClearBlend`：保留 CamB，清空 CamA/curve/time/duration/custom blender。
- `CopyFrom`：复制所有字段。
- `CustomBlender`：非空时优先于 `CameraState::lerp`。

### isTargetOffscreen

对齐 `Runtime/Core/CameraState.cs:532-559`：

- 无 LookAt → false。
- 将目标方向变换到相机局部空间。
- 正交：检查 `abs(dir.y) > orthoSize` 或 `abs(dir.x) > orthoSize * aspect`。
- 透视：检查水平/垂直半 FOV。

## Tests

- `lerp` t=0/t=1 时返回 A/B 的对应字段。
- BlendHint 组合：NoPosition/NoOrientation/NoLens 的所有组合。
- 球面/圆柱位置插值：目标在侧方时路径不穿目标。
- LookAt 插值：目标保持画面位置；FOV 调整范围正确。
- orientation 插值：当 A/B 朝向不同且都有 LookAt 时，目标不偏离。
- 自定义 blendable 权重合并。
- Lens Lerp：t=0/0.5/1、不可插值字段保持。
- FOV 插值公式：边界 clamp。
- BlendDefinition 各曲线在 0/0.5/1 的值。
- Blend 权重、完成条件、Cut 行为。
- `isTargetOffscreen`：正交/透视、屏幕边缘。

## Acceptance Checklist

- [x] `CameraState::lerp` 覆盖所有 BlendHint 分支。
- [x] FOV 保持构图的公式与原项目一致。
- [x] Lens 不可插值字段不会被错误覆盖。
- [x] Cut 的 blendTime 为 0。
- [x] 自定义曲线为空时回退到 EaseInOut。
- [x] 嵌套 Blend 的 `Uses` 正确。
- [x] 单元测试全绿。

## 参考原项目（Cinemachine）

> 参考混合算法、BlendHint 组合和边界条件；用 C++ 重写，不复制 C# 代码。

### 必读源码

- `com.unity.cinemachine/Runtime/Core/CameraState.cs:20-561`
  - 字段、BlendHints、Lerp、InterpolateFOV、ApplyPosBlendHint、ApplyRotBlendHint、InterpolatePosition、HasLookAt、GetCorrectedPosition/Orientation、GetFinalOrientation、CustomBlendables、IsTargetOffscreen。
- `com.unity.cinemachine/Runtime/Core/LensSettings.cs:294-334`
  - `Lerp` 和不可插值字段。
- `com.unity.cinemachine/Runtime/Core/CinemachineBlend.cs:10-164`
  - `Blend`、`BlendWeight`、`IsComplete`、`Uses`、`ClearBlend`、`CopyFrom`。
- `com.unity.cinemachine/Runtime/Core/CinemachineBlend.cs:166-297`
  - `CinemachineBlendDefinition`、标准曲线切线。
- `com.unity.cinemachine/Runtime/Core/CinemachineBlend.cs:299-345`
  - `NestedBlendSource`，为 P1-03 预留。
- `com.unity.cinemachine/Runtime/Core/UnityVectorExtensions.cs`
  - `GetCameraRotationToTarget`、`ApplyCameraRotation`、`SafeFromToRotation`。

### 必读测试

- `com.unity.cinemachine/Tests/Editor/CameraStateTests.cs`
- `com.unity.cinemachine/Tests/Editor/BlendManagerTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CamerasBlendingTests.cs`
- `com.unity.cinemachine/Tests/Editor/UnityVectorExtensionTests.cs`

### 需要核对的行为

- `Lerp` 中 `adjustedT` 的两次调整：FOV 插值和 ScreenSpaceAim。
- `InterpolateFOV` 的最小距离使用 near clip。
- `ApplyPosBlendHint` / `ApplyRotBlendHint` 在 hint 冲突时的取舍。
- `InterpolatePosition` 对 NaN pivot 的检查。
- `GetFinalOrientation` 中 Dutch 的应用顺序。
- `BlendWeight` 在 curve 为空/长度不足/完成时的返回值。
- 标准曲线切线的实际数值（Unity `AnimationCurve` 对 tangent 的插值）。

### 差异记录

- CineRig 的标准曲线使用 P0-02 的解析 Hermite 曲线，不复刻 Unity `AnimationCurve` 的自动切线；端点和标准切线已对齐。
- `CustomBlendable` 采用 `std::shared_ptr<const void> + std::type_index` 做身份识别，固定 4 个槽位，超出部分进入 `std::vector`；相同对象/类型合并权重。
- `CameraState::lerp` 从 `CameraState::makeDefault()` 初始化结果，因此在两端都带 `NoLens` / `NoOrientation` 时保留 CineRig 的默认 lens 和 identity bypass；C# 原始实现的零初始化结构会留下零 lens/零四元数。相机状态契约使用默认值更安全。
- `LensSettings::lerp` 使用 `std::lround` 插值 ISO 和 blade count；不可插值字段（mode override、gate fit、ortho/physical/aspect）取自 t<0.5 的 A 或 t>=0.5 的 B。
- `Blend::blendWeight` 在 duration<=0 时直接返回 1，避免除零；正常完成路径仍与原项目一致。
- `CameraState::isTargetOffscreen` 使用弧度计算，原项目使用角度；垂直/水平 FOV 转换已按原公式对齐。
- `NestedBlendSource` 已纳入 `blend.hpp`，为 P1-03 预留；`Blend::uses` 通过它递归遍历嵌套 blend。

## 实现记录

- 新增 `include/cinerig/core/`：`camera_interface.hpp`、`camera_state_blend.hpp`、`blend_definition.hpp`、`blend.hpp`。
- 新增 `src/core/camera_state_blend.cpp`、`src/core/blend.cpp`，并加入 `CMakeLists.txt`。
- `LensSettings::lerp` 已实现静态插值；`CameraState::lerp` 同时提供静态成员和 `cinerig::lerp` 自由函数。
- `CameraState` 已扩展 `CustomBlendable` / `CustomBlendableItems` 固定槽位存储、`addCustomBlendable`、`customBlendable`、`numCustomBlendables`、`isTargetOffscreen`。
- `BlendDefinition`、`Blend`、`IBlender`、`NestedBlendSource` 已实现；`Cut`、自定义曲线回退、嵌套 `uses`、clear/copy 语义均已覆盖。
- 新增测试：`lens_lerp_test.cpp`、`camera_state_lerp_test.cpp`、`blend_definition_test.cpp`、`blend_test.cpp`。
- Windows Debug/Release 验证：两个 preset 的 CTest 均为 65/65 通过（64 unit + 1 scenario）。
- 新增混合相关头文件均可单独用 MSVC `/W4 /std:c++20` 编译通过。
- 安装后的 `find_package(CineRig CONFIG)` 消费者已包含并使用 `BlendDefinition`、`Blend`，构建和运行通过。
- 未在 Linux 实机执行。
