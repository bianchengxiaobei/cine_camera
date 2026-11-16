# P1-05 SDL3 + OpenGL Example Framework

Status: completed  
Depends on: P0-05, P1-01  
Blocks: P1-06, P1-07

## Goal

搭建 SDL3 + OpenGL 3.3 示例公共层，提供窗口、输入、渲染、HUD 和引擎适配器，使后续示例只需关心场景和相机配置。

## Scope

- CMake 示例选项：
  - `CINERIG_BUILD_EXAMPLES=ON`
  - `CINERIG_WITH_SDL3=ON`
  - SDL3 获取方式：FetchContent 优先，系统包可选。
  - GLAD 作为 OpenGL loader（vendored 或 FetchContent）。
- `examples/common/`：
  - `App`：SDL3 初始化、窗口、OpenGL context、主循环、事件。
  - `GlRenderer`：着色器、网格、地面网格、调试线、简单几何。
  - `Hud`：文本/状态显示（stb_easy_font 或 SDL_ttf，优先 stb_easy_font）。
  - `SdlWorld`：实现 `IEntityWorld`，实体表、变换、时间、帧号、视口、world up。
  - `SdlCameraOutput`：实现 `ICameraOutput`，把 `AppliedCamera` 转成 view/projection 并渲染。
  - `SdlInput`：实现 `IInputSource`，键盘/鼠标映射到 axis 名称。
  - `SdlPhysics`（可选骨架）：AABB/球体 raycast，供 P3 使用。
- 坐标转换：
  - Core：右手、+Y up、-Z forward。
  - OpenGL：右手、+Y up、-Z forward（与 core 一致）。
  - 投影矩阵：透视/正交、vertical FOV、aspect、near/far。
  - View 矩阵：由 final position/orientation 构造；Dutch 通过 roll 应用。
- 示例公共 HUD：
  - 当前相机/混合状态。
  - FPS、deltaTime。
  - 操作提示。
- 每个示例独立可执行，链接 `cinerig` + `examples_common`。

## Non-goals

- 不实现完整游戏引擎。
- 不实现阴影、PBR、后处理。
- 不实现物理引擎；P1 只需要 Fake/简单查询骨架。
- 不复制 Unity 示例资源。
- 不实现 macOS 窗口（后续）。

## Deliverables

```text
examples/
├── CMakeLists.txt
├── README.md
└── common/
    ├── app.hpp/cpp
    ├── gl_renderer.hpp/cpp
    ├── hud.hpp/cpp
    ├── sdl_world.hpp/cpp
    ├── sdl_camera_output.hpp/cpp
    ├── sdl_input.hpp/cpp
    ├── sdl_physics.hpp/cpp
    └── shaders/
        ├── basic.vert
        ├── basic.frag
        ├── line.vert
        └── line.frag
third_party/
├── glad/          # 或 FetchContent
└── stb/           # stb_easy_font.h
```

## API Sketch

```cpp
namespace cinerig::examples
{
class App
{
public:
    App(const char* title, int width, int height);
    ~App();

    bool running() const;
    void pollEvents();
    void beginFrame();
    void endFrame();

    float deltaTime() const;
    float elapsedTime() const;
    uint64_t frameCount() const;
    Vec2 viewportSize() const;
    SdlInput& input();
    GlRenderer& renderer();
    Hud& hud();
};

class SdlWorld final : public IEntityWorld
{
public:
    EntityId createEntity(Transform transform);
    void destroyEntity(EntityId id);
    void setTransform(EntityId id, Transform transform);
    void setDeltaTime(UpdateClock clock, float dt);
    // IEntityWorld...
};

class SdlCameraOutput final : public ICameraOutput
{
public:
    void apply(const AppliedCamera& camera) override;
    LensSettings outputLens() const override;
    float aspect() const override;

    Mat4 viewMatrix() const;
    Mat4 projectionMatrix() const;
    Vec3 position() const;
    Quat orientation() const;
};
}
```

## Behavior Details

### 主循环

```text
pollEvents
  ↓
update simulation / input
  ↓
cameraSystem.lateUpdate(app.deltaTime())
  ↓
beginFrame
  ↓
render scene with SdlCameraOutput matrices
  ↓
hud.render
  ↓
endFrame
```

- 示例可在前面调用 `cameraSystem.fixedUpdate(fixedDt)` 模拟物理。
- 时间使用 `std::chrono::steady_clock`，转换为 float 秒。
- `frameCount` 每帧递增；`fixedFrame` 每固定步进递增。

### 坐标与矩阵

- Core 与 OpenGL 都是右手 -Z forward，因此相机 forward 直接映射到 OpenGL 相机 -Z。
- View 矩阵：
  - 由 `finalPosition` 和 `finalOrientation` 构造。
  - 等价于 `inverse(translation(position) * rotation(orientation))`。
- Projection：
  - 透视：`fovY` 使用 `lens.fieldOfView`（vertical degrees），`aspect = width/height`。
  - 正交：`orthographicSize` 是半高；半宽 = `orthographicSize * aspect`。
  - `near/far` 来自 lens。
- Dutch：
  - `finalOrientation()` 已包含 Dutch；view 矩阵直接使用即可。
  - 若实现选择单独应用 roll，必须与 core 的 `finalOrientation` 语义一致。
- `LensSettings::modeOverride`：
  - 示例只支持 perspective/orthographic；physical 字段可读取但不渲染物理镜头效果。

### SdlWorld

- 实体表 `unordered_map<EntityId, Transform>`。
- `exists` / `transform` / `trySetTransform`。
- 时间：
  - `deltaTime(Fixed)` = 固定步长；`deltaTime(Late)` = 帧 delta。
  - `frame(Fixed)` = fixed frame；`frame(Late)` = render frame。
- `worldUp` 默认 `(0,1,0)`。
- `viewportSize` 来自 SDL drawable size。
- `lastChangeClock`：
  - 简单实现：在 `setTransform` 时记录当前阶段（Fixed/Late）。
  - 供 SmartUpdate 使用。

### SdlCameraOutput

- 保存最近一次 `AppliedCamera`。
- `apply` 时更新 position/orientation/`state.lens`/aspect。
- `outputLens` 返回示例相机默认 lens（perspective/ortho 可切换）。
- `viewMatrix` / `projectionMatrix` 供渲染器使用。
- 若 `hasCamera == false`，保持上一帧矩阵。

### SdlInput

- `axis(name)` 支持：
  - `"Mouse X"`、`"Mouse Y"`：鼠标增量。
  - `"Horizontal"`、`"Vertical"`：WASD/方向键。
  - `"Scroll"`：滚轮。
- `axis2D(name)` 组合。
- 鼠标捕获/释放：右键或 Esc。
- 示例 HUD 显示捕获状态。

### 渲染器

- OpenGL 3.3 core profile。
- 简单 shader：位置、法线、颜色、MVP。
- 网格：地面网格、立方体、球体、圆柱（角色/目标）。
- 调试线：坐标轴、目标标记、相机视锥线、射线。
- HUD：
  - 用正交投影绘制文字。
  - 用 `stb_easy_font` 生成顶点；避免字体资源依赖。
- 仅程序生成几何，不使用外部模型/贴图。

### 依赖管理

- SDL3：
  - `FetchContent` 固定 release tag；或 `CINERIG_USE_SYSTEM_SDL3=ON` 使用 `find_package(SDL3)`。
- GLAD：
  - vendored 生成的 OpenGL 3.3 core loader；或 FetchContent。
- stb_easy_font：
  - 单头文件，放入 `third_party/stb/`。
- 所有示例依赖不影响核心库。

## Tests

- CMake 配置：`CINERIG_BUILD_EXAMPLES=ON` 时能找到 SDL3/GLAD。
- `SdlWorld` 的实体/时间/帧号/viewport 单元测试（不创建窗口）。
- 矩阵测试：
  - 透视投影在 near/far 处正确。
  - 正交投影半高/半宽正确。
  - View 矩阵使世界点变换到相机空间正确。
  - Dutch 90 度时 up 方向旋转。
- `SdlCameraOutput` 状态应用测试（不渲染）。
- 示例启动冒烟测试：窗口创建、运行一帧、退出（CI 可跳过）。

## Acceptance Checklist

- [x] `CINERIG_BUILD_EXAMPLES=ON` 可配置和构建。
- [x] SDL3 获取方式可用（FetchContent 或系统包）。
- [x] 窗口/OpenGL context 正常创建。
- [x] 地面网格和简单几何可见。
- [x] HUD 显示相机/混合状态。
- [x] `SdlWorld` / `SdlCameraOutput` 可被核心库使用。
- [x] 矩阵测试通过。
- [x] 示例不依赖 Unity 资源。

## 参考原项目（Cinemachine）

> 参考输出相机应用、Lens 字段和示例场景组织；用 C++/SDL3/OpenGL 重写，不复制 Unity 资源。

### 必读源码

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineBrain.cs:699-795`
  - `PushStateToUnityCamera`：位置/朝向/Dutch/NoLens/LensModeOverride/物理镜头字段的应用方式。
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineCamera.cs:228-230`
  - 把 raw position/orientation 写回 owner transform。
- `com.unity.cinemachine/Runtime/Core/LensSettings.cs`
  - FOV、ortho size、near/far、Dutch、物理镜头字段。
- `com.unity.cinemachine/Runtime/Core/CameraState.cs:452-472`
  - `GetCorrectedPosition/Orientation`、`GetFinalPosition/Orientation`。
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineExternalCamera.cs`
  - 外部相机如何读取/暴露状态。

### 必读示例/场景

- `com.unity.cinemachine/Samples~/Shared Assets/Scripts/SimplePlayerController.cs`
  - 目标移动和输入的组织方式。
- `com.unity.cinemachine/Samples~/2D Samples/2D Platformer/2D Platformer.unity`
  - 示例场景结构（只参考，不复制资源）。
- `Projects/Standalone`（仓库根目录）
  - 示例工程组织方式（只参考目录结构）。

### 需要核对的行为

- Unity Camera 的 FOV 是 vertical 还是 horizontal（取决于设置）；CineRig 统一 vertical。
- Unity 的 orthographic size 是半高；OpenGL 正交矩阵对应关系。
- Unity 左手 +Z forward 与 OpenGL 右手 -Z forward 的差异；CineRig 直接使用 -Z forward。
- Dutch 的应用顺序：correction 之后绕 forward 轴。
- 物理镜头字段只影响投影/后处理，P1 示例不实现景深。

### 差异记录

- 原项目输出到 Unity Camera；CineRig 输出到 `ICameraOutput`，示例用 OpenGL。
- 原项目使用 Unity 资源；CineRig 仅程序生成几何。
- 原项目 HUD 使用 UIElements/IMGUI；CineRig 用 stb_easy_font 或 SDL_ttf。
- 物理镜头/景深不在 P1 示例渲染范围内。

## 实现记录

实现日期：2026-11-13。

### 依赖与构建

- SDL3：FetchContent，固定 `release-3.2.10`；`CINERIG_USE_SYSTEM_SDL3=ON` 时改用 `find_package(SDL3 CONFIG)`。
- GLAD：vendored `third_party/glad/`，由 glad 2.0.8 生成 OpenGL 3.3 core loader（无扩展）。
- 字体：vendored `third_party/stb/stb_easy_font.h`。
- `CINERIG_BUILD_EXAMPLES=ON` 会自动启用 `CINERIG_WITH_SDL3`；所有 SDL/OpenGL 依赖只进入 `examples_common`，不进入 `cinerig`。
- 示例目标链接 `cinerig::cinerig` + `examples_common`；`cinerig_00_sandbox` 是 P1-05 的框架演示程序。

### 实现要点

- `App`：SDL3 初始化、OpenGL 3.3 core context、窗口/帧缓冲尺寸、`steady_clock` 时间、事件、鼠标捕获同步、FPS 相关计数。
- `SdlWorld`：实体表、可写变换、Fixed/Late delta、frame、current clock、viewport、world up。
- `SdlCameraOutput`：保存 `AppliedCamera`，应用 NoPosition/NoOrientation/NoLens hint；透视/正交 projection 与 Core `-Z forward` 对齐。行为对照 `CinemachineBrain.PushStateToUnityCamera`、`LensSettings` 与 `CameraState.GetFinalPosition/GetFinalOrientation`：position/orientation 直接取 final 值，Dutch 已在 orientation 中；物理镜头字段只保留数据，不渲染景深/物理效果。
- `GlRenderer`：程序生成 cube/sphere/cylinder/plane，基础光照 mesh shader、line batch、网格/坐标轴/目标标记/相机视锥；shader 从 `examples/common/shaders` 读取并带内嵌回退。
- `Hud`：stb_easy_font 文本、半透明面板、Framebuffer 像素坐标到 NDC。
- `SdlInput`：Mouse X/Y、Horizontal、Vertical、Scroll、2D 组合、右键捕获、Esc 释放。
- `SdlPhysics`：AABB/球体 raycast、sphereCast、overlapSphere 的可替换骨架。
- 矩阵约定：view = inverse(translation(position) * rotation(orientation))；projection 使用 vertical FOV / orthographic half-height / aspect；Mat4 采用 OpenGL 列主序存储。

### 验证

- `CINERIG_BUILD_EXAMPLES=ON` 配置与构建成功；SDL3/GLAD/OpenGL 均被隔离在示例层。
- Windows Debug 全量测试：**157/157** 通过（70 unit + 68 scenario + 6 golden + 13 example）。
- 启用 `CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS=ON` 后 Debug/Release 均为 **158/158**，包含窗口启动冒烟测试。
- `CINERIG_BUILD_TESTS=OFF` + `CINERIG_BUILD_EXAMPLES=ON` 构建成功，不拉取 Catch2。
- `cinerig_00_sandbox --frames 2 --hidden --no-vsync`：退出码 0；窗口/context 创建、渲染循环、HUD 路径通过。
- 1280x720 可见窗口运行验证：地面网格、cube、sphere、cylinder、HUD、当前相机/Blend 状态可见。
- 示例测试覆盖：SdlWorld 实体与时钟、view/projection/Dutch、NoTransform/NoLens、CameraSystem + ExternalCamera + SdlCameraOutput 集成、SdlInput 轴与捕获、SdlPhysics 查询。
- 未在 Linux 实机验证；macOS 窗口按 Non-goals 暂不实现。当前未做性能基准，只记录为轻量 SDL/GL 示例层。


