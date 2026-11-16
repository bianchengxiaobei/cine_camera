# P0-01 Project Skeleton

Status: completed  
Depends on: none  
Blocks: P0-02, P0-03, P0-04, P0-05, P0-06

## Goal

建立一个可配置、可编译、可测试的 C++20 工程骨架，为后续数学库、核心类型和运行时提供稳定的构建边界。

## Scope

- 顶层 `CMakeLists.txt`、`CMakePresets.json`。
- `cinerig` 静态库 target 和 `cinerig::cinerig` 别名。
- 公共头文件目录 `include/cinerig/`。
- 源文件目录 `src/`。
- 测试目录 `tests/unit`、`tests/scenario`、`tests/golden`。
- 示例目录 `examples/common`、`examples/01_follow`、`examples/03_blend` 的占位。
- CMake 选项：
  - `CINERIG_BUILD_TESTS`（默认 ON）
  - `CINERIG_BUILD_EXAMPLES`（默认 OFF）
  - `CINERIG_MATH_BACKEND`（`native` 或 `glm`，默认 `native`）
  - `CINERIG_WITH_SDL3`（默认 OFF）
  - `CINERIG_WITH_CLIPPER2`（默认 OFF）
- 编译器警告：MSVC `/W4`，GCC/Clang `-Wall -Wextra -Wpedantic`。
- C++20，禁止在核心库中使用异常逃逸热路径。
- `.gitignore`：构建目录、CMake 用户文件、IDE 文件。
- `cmake/CineRigConfig.cmake.in`，为后续 `find_package(CineRig)` 做准备。

## Non-goals

- 不实现任何相机逻辑。
- 不引入 SDL3/OpenGL 到核心库。
- 不创建完整示例场景。
- 不实现序列化/编辑器工具。

## Deliverables

```text
CineRig/
├── CMakeLists.txt
├── CMakePresets.json
├── .gitignore
├── cmake/CineRigConfig.cmake.in
├── include/cinerig/cinerig.hpp
├── src/cinerig.cpp
├── tests/CMakeLists.txt
├── tests/unit/CMakeLists.txt
├── tests/scenario/CMakeLists.txt
├── tests/golden/README.md
├── examples/CMakeLists.txt
└── examples/README.md
```

### Target 结构

```cmake
add_library(cinerig STATIC
    src/cinerig.cpp
)
add_library(cinerig::cinerig ALIAS cinerig)

target_compile_features(cinerig PUBLIC cxx_std_20)
target_include_directories(cinerig
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
```

- 核心库只链接标准库。
- 测试 target 使用 Catch2 v3（FetchContent 或系统包）。
- 示例 target 后续按需链接 SDL3/GLAD；P0 只保留空目录和 CMake 占位。

### CMakePresets

至少提供：

- `windows`：MSVC，Ninja 或 Visual Studio generator。
- `linux`：Ninja，Debug。
- `debug` / `release` 基础 preset。
- `tests` preset：`CINERIG_BUILD_TESTS=ON`。

## API Sketch

```cpp
// include/cinerig/cinerig.hpp
#pragma once

namespace cinerig
{
inline constexpr int versionMajor = 0;
inline constexpr int versionMinor = 1;
}
```

P0 只要求头文件可独立包含，不要求实际 API。

## Behavior Details

- 公共头文件不得包含 SDL、OpenGL、平台头文件。
- `CINERIG_MATH_BACKEND=glm` 时只影响数学后端，不影响公共头文件结构；P0 允许先只接受该选项但暂不实现 GLM 分支。
- 构建目录统一放在 `CineRig/build/<preset>`，并加入 `.gitignore`。
- 安装规则 P0 可暂不实现，但保留 `CineRigConfig.cmake.in` 以便后续补齐。

## Tests

- 编译测试：`cinerig` 空库可编译、可链接。
- 头文件测试：一个最小 `.cpp` 只包含 `<cinerig/cinerig.hpp>` 并编译。
- CMake 配置测试：`CINERIG_BUILD_TESTS=OFF` 和 `ON` 都能配置成功。

验收命令：

```powershell
cmake --preset windows
cmake --build --preset windows
ctest --preset windows
```

Linux：

```bash
cmake --preset linux
cmake --build --preset linux
ctest --preset linux
```

## Acceptance Checklist

- [x] `cinerig` target 可编译。
- [x] `cinerig::cinerig` 别名可用。
- [x] 核心库不链接 SDL/OpenGL。
- [x] `CMakePresets.json` 至少包含 Windows 和 Linux Debug preset。
- [x] `CINERIG_BUILD_TESTS=ON` 时 CTest 可运行。
- [x] `.gitignore` 覆盖 `build/`、`.vs/`、`.idea/`、`CMakeUserPresets.json`。
- [x] 头文件可独立包含。
- [x] README 说明构建方式。

## 参考原项目（Cinemachine）

> 仅参考模块划分、依赖边界和测试组织方式，不复制代码或资源。

### 必读源码/配置

- `com.unity.cinemachine/Runtime/Unity.Cinemachine.asmdef`：原项目 runtime 依赖边界。
- `com.unity.cinemachine/Editor/Unity.Cinemachine.Editor.asmdef`：Editor 与 Runtime 分离方式。
- `com.unity.cinemachine/package.json`：包依赖和 samples 组织。
- `com.unity.cinemachine/Tests/Unity.Cinemachine.Shared.Tests.asmdef`
- `com.unity.cinemachine/Tests/Editor/Unity.Cinemachine.Editor.Tests.asmdef`
- `com.unity.cinemachine/Tests/Runtime/Unity.Cinemachine.Runtime.Tests.asmdef`

### 参考点

- 原项目把运行时、Editor、测试、示例分成独立程序集；CineRig 对应为 `cinerig`、`examples`、`tests` 独立 target。
- 原项目 runtime 依赖 Unity 模块；CineRig 核心库反过来要求零引擎依赖，因此不照搬 asmdef 依赖列表。
- 原项目测试分为 Editor/Runtime 两套；CineRig 对应为 `tests/unit`、`tests/scenario`、`tests/golden`。

### 差异记录

- 原项目使用 Unity asmdef；CineRig 使用 CMake target。
- 原项目测试依赖 Unity Test Framework；CineRig 使用 Catch2 + CTest。
- 原项目示例位于 `Samples~`；CineRig 示例位于 `examples/`，且不复制 Unity 资源。

## 实现记录

- 参考 `Runtime/Unity.Cinemachine.asmdef`、`Editor/Unity.Cinemachine.Editor.asmdef` 和 `Tests/*.asmdef` 的程序集分离，映射为独立的 `cinerig`、测试和示例 target。
- 已创建 `cinerig` 静态库、`cinerig::cinerig` 别名、公共 include 路径和安装导出。
- 已创建 `cinerig.hpp` / `cinerig.cpp`，提供版本常量和可链接的 `versionString()`。
- 已接入 Catch2 v3.8.1：默认 FetchContent，`CINERIG_USE_SYSTEM_CATCH2=ON` 时改用系统包。
- 已创建 `cinerig_unit_tests` 与 `cinerig_scenario_tests`，通过 CTest 注册并带 `unit` / `scenario` 标签。
- `CMakePresets.json` 提供 `tests`（host 默认 generator）、`windows`、`windows-release`、`linux`、`linux-release`，并配套 build/test presets。
- Windows 验证通过：`cmake --preset windows`、`cmake --build --preset windows`、`ctest --preset windows`（2/2）。
- Windows Release 与 host `tests` preset 也验证通过（各 2/2）。
- 额外验证通过：`CINERIG_BUILD_TESTS=OFF`、`CINERIG_MATH_BACKEND=glm`、`CINERIG_BUILD_EXAMPLES=ON` 均可配置；安装后 `find_package(CineRig CONFIG)` 消费成功。
- 未在 Linux 实机执行；`linux` preset 使用 Ninja + Debug，待 CI/实机验证。
- 当前核心库只链接 C++ 标准库；SDL3/OpenGL/Clipper2 选项仅保留为后续配置开关。
