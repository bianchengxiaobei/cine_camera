# P0-02 Math

Status: completed  
Depends on: P0-01  
Blocks: P0-03, P0-04, P1-01, P1-02

## Goal

实现 CineRig 的数学基础库，覆盖相机状态、阻尼、预测、曲线和几何计算，并提供可替换的数学后端边界。

## Scope

- 值类型：`Vec2`、`Vec3`、`Vec4`、`Quat`、`Mat3`、`Mat4`、`Rect`、`Bounds`、`BoundingSphere`、`Ray`、`Plane`。
- 基础运算：加减乘除、点积、叉积、长度、归一化、线性插值、球面插值、投影、夹角、近似比较。
- 旋转：轴角、欧拉角、LookRotation、FromToRotation、Slerp、Angle、Inverse、旋转向量。
- 几何工具：
  - `projectOnPlane`
  - `signedAngle`
  - `normalizeAngle`
  - `safeFromToRotation`
  - `almostZero`
  - `clamp01`
  - `approximately`
- `Curve`：基于关键帧的 Hermite 曲线，支持 `linear`、`easeInOut`、`easeIn`、`easeOut`、`hardIn`、`hardOut`、自定义关键帧。
- `Damper`：标准指数衰减阻尼。
- `PositionPredictor`：与 `Runtime/Core/Predictor.cs` 行为对齐的位置预测器。
- `GaussianFilter`：1D 高斯窗口（用于后续噪声/平滑）。
- Camera 旋转辅助：`getCameraRotationToTarget`、`applyCameraRotation`（供 `CameraState::lerp` 使用）。
- 默认 native 后端；`CINERIG_MATH_BACKEND=glm` 时可通过适配层切换。

## Non-goals

- 不实现样条（P4）。
- 不实现物理查询（P0-05 接口，P3 实现）。
- 不实现 SIMD 优化；先保证正确性和可读性。
- 不实现矩阵分解、投影矩阵等渲染专用函数；由示例适配器负责。

## Deliverables

```text
include/cinerig/math/
├── vec.hpp
├── quat.hpp
├── matrix.hpp
├── geometry.hpp
├── curve.hpp
├── damp.hpp
├── predictor.hpp
├── gaussian_filter.hpp
└── backend.hpp        # native / glm 选择
src/math/
└── math.cpp          # 仅放不适合 inline 的实现
tests/unit/
├── math_vec_test.cpp
├── math_quat_test.cpp
├── math_curve_test.cpp
├── damp_test.cpp
└── predictor_test.cpp
```

## API Sketch

```cpp
namespace cinerig::math
{
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };
struct Quat { float x, y, z, w; };

Vec3 operator+(Vec3, Vec3);
Vec3 operator*(float, Vec3);
float dot(Vec3, Vec3);
Vec3 cross(Vec3, Vec3);
Vec3 normalize(Vec3);
Vec3 lerp(Vec3, Vec3, float t);
Vec3 slerp(Vec3, Vec3, float t);
Vec3 projectOnPlane(Vec3 v, Vec3 normal);
float signedAngle(Vec3 from, Vec3 to, Vec3 axis);

Quat quatFromAxisAngle(Vec3 axis, float radians);
Quat quatFromEuler(Vec3 radians);
Quat lookRotation(Vec3 forward, Vec3 up);
Quat safeFromToRotation(Vec3 from, Vec3 to, Vec3 up);
Quat slerp(Quat, Quat, float t);
Quat inverse(Quat);
float angle(Quat, Quat);
Vec3 rotate(Quat, Vec3);

float standardDamp(float initial, float dampTime, float deltaTime);
Vec3 standardDamp(Vec3 initial, Vec3 dampTime, float deltaTime);
Vec3 smoothDamp(Vec3 current, Vec3 target, Vec3& velocity, float smoothTime,
                float maxSpeed, float deltaTime);

struct PositionPredictor
{
    void reset();
    void addPosition(Vec3 pos, float deltaTime);
    Vec3 predictDelta(float lookaheadTime) const;
    void applyTransformDelta(Vec3 delta);
    void applyRotationDelta(Quat delta);
    Vec3 currentPosition() const;
    bool isEmpty() const;
    float smoothing = 0.0f;
};

struct Curve
{
    static Curve linear();
    static Curve easeInOut();
    static Curve easeIn();
    static Curve easeOut();
    static Curve hardIn();
    static Curve hardOut();
    float evaluate(float t) const; // t in [0,1], result clamped [0,1]
};
}
```

## Behavior Details

### 坐标约定

- 右手系，+Y up，-Z forward。
- `lookRotation(forward, up)` 返回使局部 -Z 指向 `forward` 的旋转。
- 公共配置使用度；数学库内部使用弧度，角度辅助函数在名字中显式标注 `Degrees` 或 `Radians`。

### Damping

对齐 `Runtime/Core/Predictor.cs` 的 `Damper`：

```text
kNegligibleResidual = 0.01
kLogNegligibleResidual = -4.605170186

StandardDamp(initial, dampTime, deltaTime):
    if dampTime < epsilon or abs(initial) < epsilon: return initial
    if deltaTime < epsilon: return 0
    return initial * (1 - exp(kLogNegligibleResidual * deltaTime / dampTime))
```

- 逐轴应用。
- `dampTime <= 0` 时直接返回原值。
- `deltaTime <= 0` 时返回 0（表示本次不推进阻尼）。
- P0 不实现 `CINEMACHINE_EXPERIMENTAL_DAMPING` 的 `StableDamp`；在任务文档记录为后续可选。

### PositionPredictor

对齐 `Runtime/Core/Predictor.cs:10-78`：

- `AddPosition(pos, dt)`：
  - `dt < 0` 时 `Reset()`。
  - 若已有位置且 `dt > epsilon`，计算速度 `vel = (pos - prev) / dt`。
  - 根据速度是否减小，选择 smoothing / 30 或 smoothing / 10 作为 `SmoothDamp` 的平滑时间。
  - 使用 `Vector3.SmoothDamp` 等价实现更新速度。
- `PredictPositionDelta(lookaheadTime) = velocity * lookaheadTime`。
- `ApplyTransformDelta` 只移动当前位置，不改速度。
- `ApplyRotationDelta` 旋转速度和平滑速度。

### Curve

- P0 只需要用于 BlendDefinition 的曲线；先实现 Hermite 关键帧求值。
- 标准曲线语义参考 `Runtime/Core/CinemachineBlend.cs:244-276`：
  - `EaseInOut`：两端切线为 0。
  - `EaseIn`：起点外切线 1.4，终点内切线 0。
  - `EaseOut`：起点外切线 0，终点内切线 1.4。
  - `HardIn`：起点外切线 0，终点内切线 3。
  - `HardOut`：起点外切线 3，终点内切线 0。
  - `Linear`：线性。
- 不要求复刻 Unity `AnimationCurve` 的自动切线算法；标准曲线的数值形状应接近原项目。
- 自定义曲线支持 keyframes（time, value, inTangent, outTangent），超出范围时 clamp 到端点值。

### GaussianFilter

- 对齐 `Runtime/Core/GaussianFilter.cs`：
  - kernel radius = min(maxKernelRadius, floor(abs(sigma) * 2.5))。
  - 权重为高斯函数并归一化。
  - 环形缓冲，`AddValue` 第一次填充整个窗口。
- P0 实现 `GaussianWindow1D<float>` 和 `GaussianWindow1D<Vec3>` 即可。

## Tests

- Vec3：加减、点积、叉积、长度、归一化、lerp、slerp、projectOnPlane、signedAngle。
- Quat：轴角、Euler、LookRotation、FromToRotation、Slerp、Angle、Inverse、旋转向量；验证 ±Z forward 约定。
- Curve：每个标准曲线在 t=0/0.5/1 的值；单调性；clamp。
- Damper：`dampTime=0`、`deltaTime=0`、初始值 0、标准曲线衰减到 residual 范围。
- PositionPredictor：静止、匀速、反向、warp、lookahead 计算。
- GaussianFilter：kernel 归一化、窗口填充、常值输入输出不变。

## Acceptance Checklist

- [x] 所有数学类型可独立包含。
- [x] 数学库无引擎依赖。
- [x] `lookRotation` 符合 -Z forward 约定。
- [x] Damping 与 `Predictor.cs` 公式一致。
- [x] PositionPredictor 行为与 `Predictor.cs` 对齐。
- [x] 标准 Blend 曲线在 0/0.5/1 处正确。
- [x] 单元测试全绿。
- [x] native 后端默认可编译；GLM 选项存在（可暂时不实现分支）。

## 参考原项目（Cinemachine）

> 参考行为、默认值、边界条件；用 C++ 重写，不复制 C# 代码。

### 必读源码

- `com.unity.cinemachine/Runtime/Core/UnityVectorExtensions.cs`
  - `Epsilon`、`AlmostZero`、`NormalizeAngle`、`SignedAngle`、`ProjectOntoPlane`、`SafeFromToRotation`、欧拉角/相机旋转辅助。
- `com.unity.cinemachine/Runtime/Core/Predictor.cs:10-78`
  - `PositionPredictor`。
- `com.unity.cinemachine/Runtime/Core/Predictor.cs:83-216`
  - `Damper`、`StandardDamp`。
- `com.unity.cinemachine/Runtime/Core/Predictor.cs:225-315`
  - `AverageFrameRateTracker`（P0 只记录，不实现实验性阻尼）。
- `com.unity.cinemachine/Runtime/Core/CinemachineBlend.cs:244-276`
  - 标准混合曲线的切线设置。
- `com.unity.cinemachine/Runtime/Core/GaussianFilter.cs`
  - 高斯窗口实现。
- `com.unity.cinemachine/Runtime/Core/CameraState.cs`
  - 数学类型的使用方式（Quat/Vector3 语义）。

### 必读测试

- `com.unity.cinemachine/Tests/Editor/UnityVectorExtensionTests.cs`
- `com.unity.cinemachine/Tests/Editor/DampingTests.cs`
- `com.unity.cinemachine/Tests/Editor/CameraStateTests.cs`
- `com.unity.cinemachine/Tests/Runtime/LookaheadTests.cs`

### 需要核对的行为

- `Epsilon` 的量级和近似比较规则。
- `NormalizeAngle` / `SignedAngle` 的度/弧度与范围。
- `SafeFromToRotation` 在 from/to 平行或反向时的处理。
- `PositionPredictor` 的 smoothing 除数（30/10）和 `SmoothDamp` 参数。
- `Damper` 的 residual 和指数公式。
- 标准混合曲线在 Unity `AnimationCurve` 下的实际形状。

### 差异记录

- CineRig 默认右手 -Z forward；原项目是 Unity 左手 +Z forward。任务文档中的公式需做坐标转换，示例适配器负责最终映射。
- P0 不实现 `CINEMACHINE_EXPERIMENTAL_DAMPING` 的 `StableDamp`；如需与原项目实验模式一致，后续单独任务。
- 角度相关数学 API 统一使用弧度；原项目 `NormalizeAngle` / `SignedAngle` / 相机旋转辅助使用度。P0-04 及后续调用方在公共 API 边界做度/弧度转换。
- 标准曲线使用解析 Hermite 关键帧，不复刻 Unity `AnimationCurve` 的自动切线算法；标准曲线的端点和中点已按原切线设置对齐。
- `safeFromToRotation` 在 from/to 反向且投影退化的极端情况下使用与 `from` 正交的轴，而不是无条件使用 `up`，以避免 180° 翻转失效。正常路径与 `UnityVectorExtensions.SafeFromToRotation` 一致。

## 实现记录

- 新增 `include/cinerig/math/`：`vec.hpp`、`quat.hpp`、`matrix.hpp`、`geometry.hpp`、`curve.hpp`、`damp.hpp`、`predictor.hpp`、`gaussian_filter.hpp`、`backend.hpp`、`math.hpp`。
- 新增 `src/math/math.cpp`，提供 native 后端标识；核心库仍只依赖 C++ 标准库。
- `Vec2/Vec3/Vec4`、`Quat`、`Mat3/Mat4`、`Rect`、`Bounds`、`BoundingSphere`、`Ray`、`Plane` 已实现并通过独立头文件编译检查。
- `lookRotation` 使用局部 -Z forward 基向量；`quatFromEuler` 按 Unity 的 Z-X-Y 顺序，`quatFromYawPitchRoll` 按 Y-X-Z 顺序。
- `Curve` 已实现 linear/easeInOut/easeIn/easeOut/hardIn/hardOut 和自定义关键帧，结果 clamp 到 `[0,1]`。
- `standardDamp` 使用 `kNegligibleResidual` / `kLogNegligibleResidual` 原公式；`smoothDamp` 对齐 Unity `Vector3.SmoothDamp` 参数和极值处理。
- `PositionPredictor` 对齐 `Predictor.cs` 的 30/10 smoothing 除数和 warp/rotation delta 语义。
- `GaussianWindow1D<T>` 支持 `float` 与 `Vec3`，包含环形缓冲、窗口填充和归一化高斯核。
- 新增 `math_vec_test.cpp`、`math_quat_test.cpp`、`math_curve_test.cpp`、`math_geometry_test.cpp`、`damp_test.cpp`、`predictor_test.cpp`、`gaussian_filter_test.cpp`。
- Windows Debug 验证：`cmake --build --preset windows` 无编译警告，`ctest --preset windows` 全部 25 个测试通过（24 unit + 1 scenario）。
- P0-03 期间修复 `angle(Quat, Quat)` 的 `acos` 近零精度问题：先统一符号，再使用 `4 * atan2(|a-b|, |a+b|)`；新增 gimbal Euler 重建测试，容差按 float 精度设为 `1e-3`。
- 额外验证：每个 `include/cinerig/math/*.hpp` 和 `cinerig.hpp` 均可单独编译；`CINERIG_MATH_BACKEND=glm` 配置和 `cinerig` 构建成功（仍使用 native 实现）。
- 未在 Linux 实机执行；数学代码只使用标准库。
