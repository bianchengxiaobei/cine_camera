# P1-06 Minimal Follow Components and Example

Status: completed  
Depends on: P1-02, P1-03, P1-05  
Blocks: P1-07

## Goal

实现一个用于纵向切片的最小 Follow + RotationComposer，并用 SDL3/OpenGL 示例验证“目标移动 → Body → Aim → Brain → 输出相机”的闭环。

> 这不是 P2 的完整 Follow/Composer。P1 只实现 WorldSpace 跟随、简单阻尼和屏幕构图；完整 BindingMode、Lookahead、DeadZone/HardLimits、GroupFraming 在 P2 扩展。

## Scope

- 最小 `Follow` 组件：
  - WorldSpace 偏移。
  - 逐轴位置阻尼。
  - Follow target 无效时不修改位置。
  - 不实现 LockToTarget / LazyFollow / 旋转绑定。
- 最小 `RotationComposer` 组件：
  - 指向 LookAt/tracking target。
  - `screenPosition` 构图（水平/垂直偏移）。
  - 水平/垂直阻尼。
  - 不实现 Lookahead、DeadZone、HardLimits、Group 视角。
- 示例 `01_follow`：
  - 地面网格、移动目标、虚拟相机。
  - 阻尼切换、目标暂停、HUD。
- 单元/场景测试：最小 Follow/Composer 的数学行为和示例场景。

## Non-goals

- 不实现完整 Tracker/BindingMode（P2）。
- 不实现 OrbitalFollow/PanTilt/ThirdPerson（P2）。
- 不实现 Lookahead（P2）。
- 不实现 DeadZone/HardLimits（P2）。
- 不实现物理碰撞/遮挡（P3）。

## Deliverables

```text
include/cinerig/components/
├── follow.hpp
└── rotation_composer.hpp
src/components/
├── follow.cpp
└── rotation_composer.cpp
tests/unit/
├── follow_minimal_test.cpp
└── rotation_composer_minimal_test.cpp
examples/01_follow/
├── CMakeLists.txt
├── main.cpp
└── README.md
```

## API Sketch

```cpp
namespace cinerig
{
class Follow : public CameraComponent
{
public:
    Vec3 followOffset{0.0f, 0.0f, -10.0f};
    Vec3 positionDamping{1.0f, 1.0f, 1.0f};

    Stage stage() const override { return Stage::Body; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
};

class RotationComposer : public CameraComponent
{
public:
    Vec2 screenPosition{0.0f, 0.0f};
    Vec2 damping{0.5f, 0.5f};
    Vec3 targetOffset{0.0f, 0.0f, 0.0f};

    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
};
}
```

## Behavior Details

### Minimal Follow

对齐 `Runtime/Components/CinemachineFollow.cs` 的 WorldSpace 路径，P1 简化为：

1. `target = followTargetPosition()`。
2. `desired = target + followOffset`。
3. 若 `deltaTime < 0` 或 `!previousStateIsValid`：
   - `state.rawPosition = desired`。
4. 否则：
   - `delta = desired - state.rawPosition`。
   - 逐轴应用 `Damper::standardDamp(delta, positionDamping, deltaTime)`。
   - `state.rawPosition += dampedDelta`。
5. `state.referenceUp` 保持传入值。
6. 不修改朝向。

说明：完整 Follow 会通过 `Tracker` 按 BindingMode 计算参考朝向并旋转 offset；P2 再替换。

### Minimal RotationComposer

对齐 `Runtime/Components/CinemachineRotationComposer.cs` 的基础路径，P1 简化为：

1. `lookAt = lookAtTargetPosition()`（无 customLookAt 时使用 tracking）。
2. 应用 `targetOffset`（目标局部空间，P1 可先按世界空间处理，P2 修正）。
3. `current = state.rawPosition`。
4. 计算相机到目标的期望方向。
5. 用 `lookRotation(direction, state.referenceUp)` 得到期望朝向。
6. 用 `screenPosition` 偏移：
   - 水平偏移：绕 referenceUp 旋转 `-screenPosition.x * horizontalFov/2`。
   - 垂直偏移：绕相机 right 旋转 `screenPosition.y * verticalFov/2`。
   - P1 使用近似角度；P2 实现完整屏幕空间构图。
7. 若 `deltaTime < 0` 或 `!previousStateIsValid`：
   - `state.rawOrientation = desired`。
8. 否则：
   - `delta = desired * inverse(state.rawOrientation)`。
   - 转成欧拉角，按 `damping` 逐轴阻尼。
   - 重新构造朝向。
9. 不修改位置。

说明：完整 RotationComposer 使用屏幕空间投影和 DeadZone/HardLimits；P2 替换。

### 示例

- 目标沿圆形/Lissajous 路径移动。
- 相机配置：
  - `followOffset = (0, 2, -6)`。
  - `positionDamping = (0.5, 0.5, 0.5)`。
  - `screenPosition = (0, 0.1)`。
  - `damping = (0.5, 0.5)`。
- 输入：
  - `1` 无阻尼；`2` 低阻尼；`3` 高阻尼。
  - `Space` 暂停/继续目标。
  - `R` 重置。
  - `Esc` 退出。
- HUD：active camera、目标/相机位置、FOV、阻尼、FPS。

## Tests

- Follow：
  - 目标在原点时，相机位置 = offset。
  - 目标沿 +X 移动，阻尼 0 时立即跟上。
  - 阻尼 > 0 时阶跃响应不立即到位。
  - 目标无效时位置不变。
  - `deltaTime < 0` 时 snap。
- RotationComposer：
  - 目标正前方时朝向 identity（在 -Z forward 约定下）。
  - 目标在右侧时 yaw 方向正确。
  - `screenPosition` 偏移方向正确。
  - 阻尼 0 时立即到位。
- 示例场景：相机与目标距离保持、构图点稳定。
- 示例冒烟测试（可选）。

## Acceptance Checklist

- [x] 最小 Follow 可用。
- [x] 最小 RotationComposer 可用。
- [x] 01_follow 示例可运行。
- [x] 阻尼切换可观察。
- [x] 目标暂停/继续正常。
- [x] HUD 显示正确。
- [x] 单元/场景测试通过。
- [x] 文档明确标注这是 P1 最小实现。

## 参考原项目（Cinemachine）

> 参考 WorldSpace Follow 和 RotationComposer 的基础行为；P1 只实现子集，P2 扩展。

### 必读源码

- `com.unity.cinemachine/Runtime/Components/CinemachineFollow.cs`
  - `FollowOffset`、`EffectiveOffset`、`MutateCameraState`、`TrackerSettings`。
- `com.unity.cinemachine/Runtime/Components/CinemachineRotationComposer.cs`
  - `Composition`、`Damping`、`MutateCameraState`、`TrackedPoint`。
- `com.unity.cinemachine/Runtime/Core/TargetTracking.cs:9-35`
  - BindingMode 列表；P1 只实现 WorldSpace 语义。
- `com.unity.cinemachine/Runtime/Core/TargetTracking.cs:53-168`
  - TrackerSettings 默认值。
- `com.unity.cinemachine/Runtime/Core/ScreenComposerSettings.cs`
  - screenPosition/deadZone/hardLimits；P1 只用 screenPosition。
- `com.unity.cinemachine/Runtime/Core/CameraState.cs`
  - raw/corrected/final 字段。
- `com.unity.cinemachine/Runtime/Core/Predictor.cs`
  - Damper 公式。

### 必读测试

- `com.unity.cinemachine/Tests/Runtime/FollowForcePositionTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CameraPositionTests.cs`
- `com.unity.cinemachine/Tests/Editor/DampingTests.cs`

### 必读文档

- `com.unity.cinemachine/Documentation~/CinemachineFollow.md`
- `com.unity.cinemachine/Documentation~/CinemachineRotationComposer.md`
- `com.unity.cinemachine/Documentation~/setup-follow-camera.md`

### 需要核对的行为

- Follow 默认 offset `(0,0,-10)`。
- WorldSpace 绑定下目标旋转不影响相机位置。
- RotationComposer 的 screenPosition 语义（0 为中心，±0.5 为边缘）。
- 阻尼为 0 时立即到位。
- `deltaTime < 0` 时 snap。
- 目标无效时的安全行为。

### 差异记录

- P1 只实现 WorldSpace Follow，不实现 BindingMode/Tracker。
- P1 的 screenPosition 偏移是近似角度，不是完整屏幕空间投影；P2 修正。
- P1 不实现 Lookahead、DeadZone、HardLimits。
- 原项目组件是 `MonoBehaviour`；CineRig 组件由 `VirtualCamera` 显式添加。
- 坐标约定为右手 -Z forward，原项目为 Unity 左手 +Z forward。

## 实现记录

### 已实现

- `include/cinerig/components/follow.hpp`、`src/components/follow.cpp`
  - 默认 `followOffset = (0, 0, -10)`、`positionDamping = (1, 1, 1)`。
  - WorldSpace：`desired = followTargetPosition + followOffset`，忽略目标旋转。
  - 逐轴 `math::standardDamp`；无效 follow target 直接返回；`deltaTime < 0` 或 `!previousStateIsValid` 时 snap。
  - `maxDampTime()` 返回最大位置阻尼。
- `include/cinerig/components/rotation_composer.hpp`、`src/components/rotation_composer.cpp`
  - 默认 `damping = (0.5, 0.5)`、`screenPosition = (0, 0)`。
  - 目标点 = look-at target + `targetOffset`；P1 先按世界空间处理。
  - 使用 `lookRotation(direction, referenceUp)`，再按 `-screenPosition.x * horizontalFov/2` 绕 up、`screenPosition.y * verticalFov/2` 绕相机 local right 做近似构图。
  - 水平/垂直 FOV 由 `LensSettings::fieldOfView` 与 `aspect` 推导。
  - 阻尼通过 `desired * inverse(current)` 转欧拉角后逐轴 `standardDamp`，再重构朝向；零阻尼立即到位；无效 target 不修改朝向。
  - `referenceLookAt` 更新为应用 `targetOffset` 后的目标点。
- 新增 `examples/01_follow`：
  - Lissajous 路径目标、地面网格、目标球体、目标坐标轴、相机到目标连线、相机视锥和 HUD。
  - 相机实体通过 `VirtualCamera::ownerEntity` 持久化 raw transform；这是运行时 `pullStateFromVirtualCamera` 在阻尼下保持上一帧状态的必要条件。
  - `Follow` offset `(0, 2, -6)`；`RotationComposer` screenPosition `(0, 0.1)`；默认低阻尼 `0.5`。
  - `1` 无阻尼、`2` 低阻尼、`3` 高阻尼、`Space` 暂停/继续、`R` 重置、`Esc` 退出。
  - HUD 显示 active camera、目标/相机位置、阻尼、目标状态、FOV、FPS 和按键提示。
- 新增测试：
  - `tests/unit/follow_minimal_test.cpp`：首次 snap、逐轴阻尼、负时间 snap、零阻尼、无效目标。
  - `tests/unit/rotation_composer_minimal_test.cpp`：默认值、-Z 正前方、右侧目标、screenPosition 近似角度、targetOffset、阻尼/负时间 snap、无效目标。
  - `tests/scenario/follow_pipeline_test.cpp`：Follow + RotationComposer 经过 `CameraSystem` 管线。
  - `examples/tests/follow_scene_test.cpp`：`SdlWorld` + `SdlCameraOutput` 示例场景中保持相机/目标 offset。

### 与原项目差异

- P1 Follow 只有 WorldSpace；没有 Tracker、LockToTarget、LazyFollow、rotation binding、minimum target distance。
- P1 RotationComposer 只有 screenPosition 的近似半 FOV 角度偏移；没有 DeadZone、HardLimits、Lookahead、CenterOnActivate、完整屏幕空间投影和 vertical clamp。
- `targetOffset` 暂按世界空间处理，P2 改为目标局部空间。
- 原项目组件是 `MonoBehaviour`；CineRig 由 `VirtualCamera::setBody/setAim` 显式安装。
- 坐标约定相反：CineRig 为右手 `-Z` forward，原项目为 Unity 左手 `+Z` forward。

### 验证

- 核心 Debug：**157/157 passed**（82 unit + 69 scenario + 6 golden）。
- 示例 Debug/Release：**173/173 passed**；关闭 smoke 后为 **171/171**（包含 14 个 example 测试）。
- `cinerig_01_follow --frames 2 --hidden --no-vsync` 退出 0。
- 可见窗口确认地面、目标、HUD、相机跟随和视锥渲染正常。
