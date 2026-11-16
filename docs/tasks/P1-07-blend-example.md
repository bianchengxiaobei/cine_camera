# P1-07 Blend Example

Status: completed  
Depends on: P1-05, P1-03, P1-06  
Blocks: none（P1 验收示例）

## Goal

用 SDL3/OpenGL 示例展示两个虚拟相机之间的 Cut、EaseInOut、Linear 和自定义曲线混合，并在 HUD 上显示混合权重和当前相机。

## Scope

- 场景：
  - 地面网格 + 若干参照物（柱子/立方体）。
  - 一个移动目标。
  - 两个虚拟相机：
    - `Wide`：远距离、较大 FOV。
    - `Close`：近距离、较小 FOV，略偏侧。
  - 两个相机都使用 Follow + RotationComposer，指向同一目标。
- Brain：
  - DefaultBlend：EaseInOut 2 秒。
  - 支持运行时切换 cut/linear/ease。
- 输入：
  - `1`：切换到 Wide。
  - `2`：切换到 Close。
  - `C`：下一次切换使用 Cut（0 秒）。
  - `L`：下一次切换使用 Linear（2 秒）。
  - `E`：下一次切换使用 EaseInOut（2 秒）。
  - `T`：测试自定义曲线（例如先慢后快）。
  - `Space`：暂停/继续目标。
  - `Esc`：退出。
- HUD：
  - 当前 active camera 名称。
  - `Blending: true/false`。
  - Blend weight（0..1）和 CamA → CamB。
  - 当前 blend style/duration。
  - FOV、目标位置。
  - FPS。

## Non-goals

- 不实现 ClearShot/MixingCamera（P4）。
- 不实现 Timeline override。
- 不实现自定义 blender 的复杂示例；只保留接口。
- 不实现镜头物理/景深。

## Deliverables

```text
examples/03_blend/
├── CMakeLists.txt
├── main.cpp
└── README.md
```

## API Sketch

```cpp
int main()
{
    examples::App app("CineRig 03 Blend", 1280, 720);
    examples::SdlWorld world;
    examples::SdlCameraOutput output;

    cinerig::CameraSystem system(world);
    cinerig::BrainConfig config;
    config.defaultBlend = { cinerig::BlendStyle::EaseInOut, 2.0f };
    auto brainId = system.createBrain(output, config);

    auto target = world.createEntity(/* ... */);

    auto wideId = createFollowCamera(system, target, /* offset */ { 0, 3, -12 }, /* fov */ 55.0f);
    auto closeId = createFollowCamera(system, target, /* offset */ { 1.5f, 1.8f, -5 }, /* fov */ 35.0f);

    // start with wide
    system.camera(wideId).priority = 10;
    system.camera(closeId).priority = 0;

    while (app.running())
    {
        app.pollEvents();
        // handle switch keys
        system.lateUpdate(app.deltaTime());
        app.beginFrame();
        app.renderer().drawScene(output, world);
        drawBlendHud(app, system, brainId);
        app.endFrame();
    }
}
```

## Behavior Details

### 相机切换

- 两个相机始终 enabled，通过优先级切换：
  - Wide priority 10、Close priority 0 → Wide live。
  - 切换时交换优先级，Brain 检测到 active camera 变化后创建混合。
- 也可以禁用/启用相机；示例优先用优先级，避免 Standby 干扰。
- 切换前若已有混合未完成，Brain 应形成嵌套混合；HUD 显示嵌套状态（至少显示当前 active camera 和 weight）。

### Blend 样式

- Cut：`BlendTime=0`，立即切换，HUD 显示 `Blending: false`。
- EaseInOut：2 秒，0→1 平滑。
- Linear：2 秒，线性权重。
- Custom：例如 `easeIn` 或自定义 keyframes；HUD 显示 `Custom`。
- 切换样式只影响下一次创建的混合，不修改进行中的混合（除非显式重置）。

### HUD

- 顶部：
  - `Active: Wide` 或 `Active: Close`。
  - `Blending: yes/no`。
  - `Weight: 0.42`。
  - `From: Wide -> Close`。
  - `Style: EaseInOut 2.0s`。
- 底部操作提示。
- 混合过程中相机位置应连续；Cut 时允许瞬切。

### 目标运动

- 与 01_follow 相同的圆形/Lissajous 路径。
- 目标运动速度适中，便于观察 2 秒混合。
- 暂停时混合仍按 deltaTime 推进（与 Unity 行为一致）。

## Tests

- 逻辑测试（不需要窗口）：
  - Cut：切换后下一帧 active camera 变化，`IsBlending == false`。
  - EaseInOut 2s：t=0 weight 0，t=1 weight ~0.5，t=2 weight 1。
  - Linear：权重与时间线性。
  - 混合中途反向切换：形成嵌套混合，最终 active camera 正确。
  - 切换样式只影响后续混合。
- 示例冒烟测试（可选）。
- HUD 手动检查。

## Acceptance Checklist

- [x] 两个相机可切换。
- [x] Cut/EaseInOut/Linear/Custom 可观察。
- [x] HUD 显示 active camera、weight、from/to、style。
- [x] 混合中途切换不崩溃、不跳变。
- [x] 示例可构建、运行。
- [x] 逻辑测试通过。

## 参考原项目（Cinemachine）

> 参考 Brain 混合、BlendDefinition 和 HUD 描述；用 C++/SDL3/OpenGL 重写。

### 必读源码

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineBrain.cs:29-160, 580-795`
  - DefaultBlend、CustomBlends、UpdateMethod、ProcessActiveCamera、PushStateToUnityCamera。
- `com.unity.cinemachine/Runtime/Core/BlendManager.cs`
  - ActiveVirtualCamera、ActiveBlend、IsBlending、Description。
- `com.unity.cinemachine/Runtime/Core/CinemachineBlend.cs:10-297`
  - BlendWeight、IsComplete、BlendDefinition、Styles、Description。
- `com.unity.cinemachine/Runtime/Core/CinemachineBlenderSettings.cs`
  - 自定义 blend 查找。

### 必读测试

- `com.unity.cinemachine/Tests/Runtime/CamerasBlendingTests.cs`
- `com.unity.cinemachine/Tests/Editor/BlendManagerTests.cs`
- `com.unity.cinemachine/Tests/Runtime/PriorityTests.cs`

### 必读文档

- `com.unity.cinemachine/Documentation~/CinemachineBlending.md`
- `com.unity.cinemachine/Documentation~/CinemachineBrain.md`
- `com.unity.cinemachine/Documentation~/ControllingAndCustomizingBlends.md`

### 需要核对的行为

- Cut 是否完全不创建混合。
- 默认 blend 时长和曲线。
- 混合中途切换时 active camera 和 weight 的显示。
- `Description` 的格式（`CamB 42% from CamA`）。
- 优先级相同/不同时的选择顺序。

### 差异记录

- 原项目示例使用 Unity 场景和输入；CineRig 用 SDL3/程序几何。
- 原项目 HUD 用 DebugText/IMGUI；CineRig 用 stb_easy_font。
- 原项目可以自定义 `GetBlendOverride`；P1 示例只演示内置样式，接口在 P1-03 提供。

## 实现记录

### 已实现

- 新增 `examples/03_blend/`：
  - `Wide`：offset `(0, 3, -12)`、55° FOV；`Close`：offset `(1.5, 1.8, -5)`、35° FOV。
  - 两个相机始终 enabled，均使用 `Follow` + `RotationComposer`，通过优先级交换触发 Brain 混合；初始 Wide priority 10、Close 0。
  - 两个相机使用 `StandbyUpdateMode::Always` 并绑定 `ownerEntity`，保证非 live 相机状态持续更新且阻尼有上一帧状态。
  - 场景包含地面网格、参考柱/立方体、Lissajous 移动目标、目标路径、两个相机视锥和 active 高亮。
  - 默认 `DefaultBlend = EaseInOut 2s`。
  - `1`/`2` 切换 Wide/Close；`C`/`L`/`E`/`T` 分别设置下一次 Cut/Linear/EaseInOut/Custom；`Space` 暂停目标；`Esc` 退出。
  - 自定义曲线使用 `(0,0,0,0) -> (1,1,3,3)` 的三次 slow-in/fast-out 曲线，归一化权重为 `t³`。
  - HUD 显示 Active、Blending、Weight、From/To、当前 Style 与下一 Style、Target、FOV、Target state、按键和 FPS。
- 样式选择写入 `CameraBrain::config().defaultBlend`，只影响之后新建的混合；正在进行的混合保留其已复制的 curve/duration，符合 `BlendManager` / `CameraBlendStack` 行为。
- 新增 `tests/scenario/blend_style_test.cpp`：
  - Cut 立即切换且 `activeBlend() == nullptr`。
  - EaseInOut 在 t=0/1/2 分别为 0/0.5/1。
  - Linear 权重与归一化时间线性。
  - Custom 曲线在 t=0.5 得到 0.125。
  - 修改默认样式不改变进行中的混合。
  - 第三个相机在前一混合未完成时形成嵌套混合并最终落到正确 active camera。
- 新增 `examples/tests/blend_scene_test.cpp`：验证两个 Follow 相机通过优先级切换、EaseInOut 位置/FOV 混合和最终状态。
- 新增 `example_blend_smoke`；`examples/03_blend` 已注册到 examples CMake。

### 与原项目差异

- 原项目用 Unity 场景、IMGUI/DebugText 和真实相机对象；CineRig 用 SDL3/OpenGL、程序几何和 stb_easy_font。
- C++ `Blend` 暂未提供 `Description` 属性；示例 HUD 直接用 `camA->name()`、`camB->name()` 和 `blendWeight()` 组合出等价信息（原格式为 `CamB 42% from CamA`）。
- 两相机示例在中途反向切换时走 `CameraBlendStack` 的 backing-out 逻辑（缩短并反向），不是创建嵌套混合；嵌套由第三个相机场景测试覆盖。
- 未实现 Timeline override、ClearShot/MixingCamera、自定义 `IBlender` 交互示例和镜头物理/景深；这些属于后续阶段。
- 原项目 HUD 用 DebugText/IMGUI；CineRig 用 stb_easy_font。
- 原项目可以自定义 `GetBlendOverride`；示例只演示内置样式，接口已在 P1-03 提供。

### 验证

- 核心 Debug/Release：**163/163 passed**（82 unit + 75 scenario + 6 golden）。
- 示例 Debug/Release：**181/181 passed**；关闭 smoke 后为 **178/178**（包含 15 个 example 测试）。
- `cinerig_03_blend --frames 2 --hidden --no-vsync` 退出 0。
- 可见窗口确认：初始 `Active: Wide`；按 `2` 后显示 `Blending: yes`、`Weight` 约 0.5、`From: Wide`、`To: Close`、`Style: EaseInOut 2.0s`，FOV 随混合变化。
- 可见窗口确认：先按 `C` 再按 `2` 后显示 `Blending: no`、`Weight: 1.00`、`Style: Cut 0.0s`，相机立即切到 Close。
- Linux 未实机验证。
