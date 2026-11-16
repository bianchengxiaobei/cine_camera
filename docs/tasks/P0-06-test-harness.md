# P0-06 Test Harness

Status: completed  
Depends on: P0-01  
Blocks: P0-02, P0-03, P0-04, P0-05

## Goal

建立稳定的单元测试、场景测试和 golden 测试基础设施，使后续每个相机行为都能被验证，并能在 Windows/Linux 上持续运行。

## Scope

- Catch2 v3 接入方式（FetchContent 优先，系统包可选）。
- CTest 集成。
- 测试目录和命名规范。
- `FakeEntityWorld` / `FakeCameraOutput` / `FakePhysicsWorld` 测试替身。
- 固定时间步进测试工具。
- Golden JSON 快照格式和比较工具。
- 测试标签：`unit`、`scenario`、`golden`、`slow`。
- 可选 CI 工作流草稿（不强制修改仓库根 CI）。

## Non-goals

- 不实现具体相机测试（后续任务补）。
- 不引入 GoogleTest。
- 不实现 Unity 测试运行器。
- 不实现性能基准（P5）。

## Deliverables

```text
tests/
├── CMakeLists.txt
├── unit/
│   └── CMakeLists.txt
├── scenario/
│   └── CMakeLists.txt
├── golden/
│   ├── README.md
│   └── compare.hpp
├── support/
│   ├── fake_entity_world.hpp/cpp
│   ├── fake_camera_output.hpp/cpp
│   ├── fake_physics_world.hpp/cpp
│   └── fixed_step.hpp
└── test_main.cpp
```

## API Sketch

```cpp
// tests/support/fake_entity_world.hpp
class FakeEntityWorld final : public IEntityWorld
{
public:
    void add(EntityId id, Transform transform);
    void set(EntityId id, Transform transform);
    void setDeltaTime(UpdateClock clock, float dt);
    // IEntityWorld...
};

// tests/support/fixed_step.hpp
struct FixedStep
{
    float dt = 1.0f / 60.0f;
    int frame = 0;
    void advance(int steps = 1);
};
```

## Behavior Details

### Catch2

- 优先使用 CMake `FetchContent` 拉取固定版本的 Catch2 v3。
- 如果 `CINERIG_USE_SYSTEM_CATCH2=ON`，使用 `find_package(Catch2 3)`。
- 测试 target 命名：
  - `cinerig_unit_tests`
  - `cinerig_scenario_tests`
  - `cinerig_golden_tests`
- 每个测试可执行文件链接 `Catch2::Catch2WithMain`，不手写 main；P0-06 提供 `test_main.cpp` 仅用于自定义配置时可选用。

### 测试目录规则

- `tests/unit/`：纯数学、纯数据结构、无 FakeWorld。
- `tests/scenario/`：使用 `FakeEntityWorld` 和固定步长，验证状态/事件/更新顺序。
- `tests/golden/`：输入场景 + 期望 JSON 快照；用 `compare.hpp` 做容差比较。

### Golden 格式

```json
{
  "scenario": "priority_switch",
  "frames": [
    {
      "frame": 0,
      "activeCamera": "wide",
      "state": {
        "position": [0.0, 2.0, -6.0],
        "orientation": [0.0, 0.0, 0.0, 1.0],
        "fieldOfView": 40.0,
        "dutch": 0.0
      }
    }
  ]
}
```

- 比较容差默认 `1e-4`；位置/旋转可单独设置。
- 生成模式：`CINERIG_UPDATE_GOLDEN=1` 时重写文件；默认只比较。
- Golden 文件使用 LF 换行，数值保留 6 位小数。

### 固定步进

- 场景测试必须使用固定 dt，禁止依赖系统时间。
- 测试设置 `IEntityWorld` 的 `deltaTime` / `frame`，由 `CameraSystem::manualUpdate()` 驱动。
- 每个场景开始前重置相机状态。

### 测试标签

示例：

```cpp
TEST_CASE("Follow maintains offset", "[scenario][follow]")
```

- `[unit]`、`[scenario]`、`[golden]`、`[slow]`。
- CTest 可按标签过滤：`ctest -L unit`。

## Tests

P0-06 自身需要验证：

- Catch2 可发现测试。
- `ctest` 能运行全部测试。
- FakeEntityWorld 读写变换、时间、帧号。
- FakeCameraOutput 能捕获 applied state。
- FakePhysicsWorld 记录查询并返回配置结果。
- FixedStep 固定时间步进和帧计数。
- Golden 比较器：相等、超出容差、缺失文件、生成模式。
- `CINERIG_BUILD_TESTS=OFF` 时不配置测试。
- 标签过滤：`ctest -L unit`、`ctest -L scenario`、`ctest -L golden`。

## Acceptance Checklist

- [x] `ctest --preset windows` / `linux` 可运行。
- [x] 单元、场景、golden target 分离。
- [x] 测试不依赖系统时间。
- [x] 测试失败时输出可定位的字段和期望/实际值。
- [x] Golden 更新模式只影响显式开启的测试。
- [x] CI 草稿可选，不破坏原仓库已有工作流。
- [x] 测试标签可用。

## 参考原项目（Cinemachine）

> 参考测试组织、测试替身和确定性测试方式；用 C++ 重写，不复制 C# 测试代码。

### 必读源码/测试

- `com.unity.cinemachine/Tests/Unity.Cinemachine.Shared.Tests.asmdef`
- `com.unity.cinemachine/Tests/Editor/Unity.Cinemachine.Editor.Tests.asmdef`
- `com.unity.cinemachine/Tests/Runtime/Unity.Cinemachine.Runtime.Tests.asmdef`
- `com.unity.cinemachine/Tests/Editor/CameraStateTests.cs`
- `com.unity.cinemachine/Tests/Editor/DampingTests.cs`
- `com.unity.cinemachine/Tests/Editor/InputAxisTests.cs`
- `com.unity.cinemachine/Tests/Editor/UnityVectorExtensionTests.cs`
- `com.unity.cinemachine/Tests/Editor/BlendManagerTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CameraPositionTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CamerasBlendingTests.cs`
- `com.unity.cinemachine/Tests/Runtime/PriorityTests.cs`
- `com.unity.cinemachine/Tests/Runtime/ManualUpdateTests.cs`
- `com.unity.cinemachine/Tests/Runtime/UpdateCounterForTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CinemachineRuntimeTimeInvariantFixtureBase.cs`

### 需要核对的行为

- 原项目如何固定时间步进和帧计数。
- 原项目测试如何构造相机、设置目标、断言 CameraState。
- 原项目如何测试混合序列和事件顺序。
- 原项目如何避免依赖 Unity 全局时间。

### 差异记录

- 原项目使用 Unity Test Framework + NUnit；CineRig 使用 Catch2 v3 + CTest。
- 原项目测试依赖 `MonoBehaviour` 生命周期；CineRig 用显式 `CameraSystem` 更新和 `FakeEntityWorld`。
- 原项目通过 `Time.frameCount` 驱动更新；CineRig 通过 `IEntityWorld::frame()` 和固定步进工具驱动；P0 尚无 `CameraSystem`，场景测试先验证时钟和测试替身。
- 默认链接 `Catch2::Catch2WithMain`。`CINERIG_USE_CUSTOM_TEST_MAIN=ON` 时改用 `tests/test_main.cpp` + `Catch2::Catch2`，用于需要自定义 Catch2 配置的构建。
- 测试替身放在 `cinerig::testing` 命名空间，默认不读系统时间；`FakeEntityWorld::trySetTransform` 会写入并把 `lastChangeClock` 标为 `Late`，可用 `setLastChangeClock` 覆盖。
- Golden 比较器不使用第三方 JSON 库；`tests/golden/compare.hpp` 内置小型 JSON parser/serializer，按字段路径报告中英文错误信息。对象键顺序不影响比较。
- Golden 数值统一格式化为 6 位小数，换行统一为 LF。生成模式必须显式传 `update=true` 或设置 `CINERIG_UPDATE_GOLDEN=1`，默认比较不会修改文件。
- 标签由 Catch2 `[unit]` / `[scenario]` / `[golden]` 和 CTest `LABELS` 同时提供；`ctest -L unit` 等过滤可用。
- 未新增仓库根 CI 草稿，避免影响父仓库现有 Unity 工作流；本地 Windows CTest 已验证，Linux 实机未执行。

## 实现记录

- `tests/CMakeLists.txt`：
  - Catch2 v3.8.1 FetchContent 固定版本；`CINERIG_USE_SYSTEM_CATCH2=ON` 时改用 `find_package(Catch2 3)`。
  - 新增 `CINERIG_USE_CUSTOM_TEST_MAIN` 选项。
  - 新增 `cinerig_test_support` 静态库，聚合 `tests/support/*.cpp`。
  - `cinerig_register_test()` 统一链接 core、测试支持和 Catch2，并注册 CTest 标签。
- `tests/test_main.cpp`：可选自定义 Catch2 main，默认不编译。
- `tests/support/`：
  - `fake_entity_world.hpp/.cpp`：实体表、可写标记、Fixed/Late 时间/帧、worldUp、viewport、lastChangeClock。
  - `fake_camera_output.hpp/.cpp`：applied state、历史、apply 次数、继承 lens/aspect。
  - `fake_physics_world.hpp/.cpp`：可配置射线/球扫/overlap/bounds 结果，记录查询参数和次数。
  - `fixed_step.hpp`：`dt`、`frame`、`time`、`advance()`、`reset()`。
- `tests/scenario/test_support_test.cpp`：验证固定步进、FakeEntityWorld、FakeCameraOutput、FakePhysicsWorld。
- `tests/golden/compare.hpp`：
  - JSON parser 支持对象/数组/字符串/数字/布尔/null、转义和 Unicode；
  - 递归容差比较，支持 `absolute`、`relative`、`position`、`rotation`；
  - 失败消息包含 JSON 路径、期望值、实际值和容差；
  - `writeGoldenFile()` 规范化 LF 和 6 位小数；
  - `compareGoldenFile()` 支持缺失文件、显式 update 和 `CINERIG_UPDATE_GOLDEN=1`。
- `tests/golden/golden_compare_test.cpp`：覆盖相等、超差、position/rotation 独立容差、缺失文件、显式/环境生成模式、LF 和 6 位小数。
- `tests/golden/README.md`：记录格式、API、容差、更新命令和确定性规则。
- `tests/golden/CMakeLists.txt`：新增 `cinerig_golden_tests`，标签为 `golden`。
- 验证结果：
  - Windows Debug：81/81 通过（70 unit + 5 scenario + 6 golden）。
  - Windows Release：81/81 通过。
  - GLM 配置：81/81 通过。
  - `CINERIG_USE_CUSTOM_TEST_MAIN=ON`：81/81 通过。
  - `CINERIG_BUILD_TESTS=OFF`：仅构建 core，不配置测试。
  - `ctest -L unit`、`ctest -L scenario`、`ctest -L golden` 标签过滤可用。
  - Linux 未实机执行。
