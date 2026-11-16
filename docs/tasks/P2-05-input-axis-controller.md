# P2-05 Input Axis Controller

Status: completed  
Depends on: P0-05, P2-04  
Blocks: Input-driven FreeLook/PanTilt examples, ThirdPerson rigs

## Goal

Turn raw named input from `engine::IInputSource` into damped `core::InputAxis`
values, and discover the axes exposed by camera components. This is the C++
counterpart of `CinemachineInputAxisController`, `InputAxisControllerBase`, and
`DefaultInputAxisDriver`.

## Scope

- Add `core::InputAxisHint`, `core::InputAxisDescriptor`,
  `core::IInputAxisOwner`, `core::IInputAxisOwnerProvider`, and
  `core::IInputAxisSource`.
- Make `engine::IInputSource` derive `core::IInputAxisSource`; the controller
  depends only on the core interface.
- Make `components::OrbitalFollow` expose:
  - `Look Orbit X` with an X hint
  - `Look Orbit Y` with a Y hint
  - `Orbit Scale` with a Y hint
- Implement `IInputAxisOwnerProvider` on `VirtualCamera` so the core controller
  discovers owners without including the camera layer.
- Add `core::DefaultInputAxisDriver` with:
  - acceleration and deceleration times
  - endpoint speed limiting for non-wrapping axes
  - momentary-axis processing relative to the axis center
  - validation, reset, and cancel-current-input
- Add `core::InputAxisController` with:
  - recursive-in-pipeline owner discovery through `IInputAxisOwnerProvider`
  - stable bindings that survive repeated discovery
  - source-name, gain, cancel-delta-time, and enabled settings per axis
  - input updates through `IInputAxisSource::axis()`
  - trigger-recentering, reset, cancel-input, lookup, and clear operations
- Add unit tests for driver acceleration, deceleration, wrapping, momentary
  input, controller discovery/binding/update, gain, disabled bindings,
  cancellation, and recentering triggers.
- Integrate the controller into `05_orbital_follow`: horizontal and vertical
  axes come from `IInputSource` through the controller; radial zoom remains
  direct so the example can keep wheel/Q/E behavior explicit.
- Update the task index and feature matrix.

## Non-goals

- Unity Input System actions, multiplayer player indices, or action assets.
- Vector2 action type dispatch beyond the named `IInputSource` contract.
- Automatic per-frame update from `CameraSystem`; the host calls
  `InputAxisController::update()` before the camera update.
- Setter-injection frameworks or reflection; owner discovery uses the camera
  pipeline.
- PanTilt, ThirdPersonFollow/Aim, TargetGroup, or new camera rigs.
- Suppressing input while blending; the host can disable the controller or a
  binding when it knows a blend is active.

## Design

### Axis ownership

`IInputAxisOwner` is a non-owning interface implemented by camera components.
It appends descriptors that point at axes owned by the component:

```cpp
enum class InputAxisHint
{
    Default,
    X,
    Y,
};

struct InputAxisDescriptor
{
    std::string name;
    InputAxis* axis = nullptr;
    InputAxisHint hint = InputAxisHint::Default;
};

class IInputAxisOwner
{
public:
    virtual ~IInputAxisOwner() = default;
    virtual void getInputAxes(std::vector<InputAxisDescriptor>& axes) = 0;
};

class IInputAxisOwnerProvider
{
public:
    virtual ~IInputAxisOwnerProvider() = default;
    virtual void getInputAxisOwners(
        std::vector<IInputAxisOwner*>& owners) const = 0;
};

class IInputAxisSource
{
public:
    virtual ~IInputAxisSource() = default;
    virtual float axis(std::string_view name) const = 0;
};
```

The descriptor names are UI/diagnostic names, not `IInputAxisSource` binding
names. A controller binding maps a descriptor name to a source name.
`engine::IInputSource` derives `IInputAxisSource` and adds `axis2D()`.

### Driver

`DefaultInputAxisDriver` mirrors the original `DefaultInputAxisDriver`:

- target speed is the input value
- exponential damping moves the current speed toward the target using
  accel/decel
- a non-wrapping axis caps speed near its endpoints
- `Momentary` axes treat input as an offset from `axis.center`
- negative delta time stops the driver or snaps momentary axes to center
- `cancelCurrentInput()` zeroes speed without resetting the axis value

### Controller

`InputAxisController::synchronize(provider)` asks an
`IInputAxisOwnerProvider` (implemented by `VirtualCamera`) for the owners in the
four pipeline stages, preserves existing bindings by owner and axis name,
updates axis pointers, and removes stale bindings. `addAxis()` is available for
axes that are not exposed by a camera component.

`update(source, deltaTime)` reads `source.axis(binding.inputAxis)`, applies
`gain`, optionally divides by `deltaTime` when `cancelDeltaTime` is set, and
runs the binding's driver. Disabled bindings and unbound axes are skipped.

## API Sketch

```cpp
namespace cinerig
{

struct DefaultInputAxisDriver
{
    float accelTime = 0.2f;
    float decelTime = 0.2f;

    void validate();
    void processInput(InputAxis& axis, float inputValue, float deltaTime);
    void reset(InputAxis& axis);
    void cancelCurrentInput(InputAxis& axis);
};

class InputAxisController
{
public:
    struct Binding
    {
        const IInputAxisOwner* owner = nullptr;
        std::string name;
        std::string inputAxis;
        InputAxis* axis = nullptr;
        InputAxisHint hint = InputAxisHint::Default;
        float gain = 1.0f;
        bool cancelDeltaTime = false;
        bool enabled = true;
        float inputValue = 0.0f;
        DefaultInputAxisDriver driver;
    };

    std::size_t synchronize(const IInputAxisOwnerProvider& provider);
    Binding* addAxis(
        std::string name,
        InputAxis& axis,
        InputAxisHint hint = InputAxisHint::Default);
    bool bind(std::string_view axisName, std::string_view inputAxisName);
    Binding* find(std::string_view axisName);
    const Binding* find(std::string_view axisName) const;

    bool update(const IInputAxisSource& source, float deltaTime);
    bool triggerRecentering(std::string_view axisName);
    void reset();
    void cancelInput();
    void clear();
    std::size_t size() const noexcept;
    const std::vector<Binding>& bindings() const noexcept;
};

}
```

## Reference

- `com.unity.cinemachine/Runtime/Core/InputAxisControllerBase.cs`
  - controller manager, discovery, enable flags, reset registration, update loop
- `com.unity.cinemachine/Runtime/Core/InputAxis.cs`
  - `DefaultInputAxisDriver`, acceleration/deceleration and momentary behavior
- `com.unity.cinemachine/Runtime/Helpers/CinemachineInputAxisController.cs`
  - reader gain, cancel-delta-time, and source mapping
- `com.unity.cinemachine/Runtime/Components/CinemachineOrbitalFollow.cs`
  - `IInputAxisOwner` names and hints
- `com.unity.cinemachine/Tests/Editor/InputAxisTests.cs`
  - acceleration and deceleration expectations

## Acceptance

- OrbitalFollow is discoverable as an owner with three correctly named axes and
  hints.
- Repeated `synchronize()` preserves binding settings and updates axis
  pointers without duplicating bindings.
- A bound controller drives an axis through acceleration and deceleration and
  clamps/wraps at the axis range.
- Gain, `cancelDeltaTime`, per-binding `enabled`, and controller reset/cancel
  operations behave predictably.
- `triggerRecentering()` cancels current input and requests recentering.
- Unit tests, the orbital example, and the existing suite pass in Debug and
  Release.

## Implementation Record

### Implemented

- `include/cinerig/core/input_axis_owner.hpp`
  - `InputAxisHint`, `InputAxisDescriptor`, `IInputAxisOwner`, and
    `IInputAxisOwnerProvider`.
- `include/cinerig/core/input_axis_source.hpp`
  - core-only named input source used by the controller.
- `include/cinerig/engine/input_source.hpp`
  - `IInputSource` now derives `IInputAxisSource` and adds `axis2D()`.
- `include/cinerig/camera/virtual_camera.hpp` and
  `src/camera/virtual_camera.cpp`
  - `VirtualCamera` implements `IInputAxisOwnerProvider` and reports owners
    from its four pipeline stages.
- `include/cinerig/core/input_axis_controller.hpp` and
  `src/core/input_axis_controller.cpp`
  - `DefaultInputAxisDriver` with acceleration/deceleration, endpoint speed
    limiting, wrapping, momentary axes, negative-time handling, validation,
    reset, and cancel-current-input.
  - `InputAxisController` with provider owner discovery, binding preservation,
    manual axes, source binding, gain, cancel-delta-time, per-binding enable,
    reset/cancel, recentering trigger, lookup, and clear.
- `include/cinerig/components/orbital_follow.hpp` and
  `src/components/orbital_follow.cpp`
  - `OrbitalFollow` implements `IInputAxisOwner`.
  - Exposes `Look Orbit X`, `Look Orbit Y`, and `Orbit Scale` with the original
    hints.
- `tests/unit/input_axis_controller_test.cpp`
  - 7 cases and 59 assertions covering driver acceleration/deceleration,
    endpoint/wrap behavior, momentary and negative time, OrbitalFollow
    discovery, binding preservation, gain, delta cancellation, disabling,
    stale-owner removal, recentering triggers, reset, and clear.
- `examples/05_orbital_follow`
  - Horizontal and vertical axes are driven through `InputAxisController`.
  - Radial zoom remains direct for wheel/Q/E handling.
  - Reset cancels residual input speed; HUD reports the controller setup.
- `include/cinerig/cinerig.hpp` and `CMakeLists.txt`
  - Export and build the new core files.
- `docs/tasks/README.md`, `docs/feature-matrix.md`,
  `docs/coordinate-convention.md`, and `examples/README.md`
  - Status and integration documentation updated.

### Differences

- The controller uses `core::IInputAxisSource` and
  `core::IInputAxisOwnerProvider` so `core` does not depend on the `engine` or
  `camera` layer; `engine::IInputSource` and `VirtualCamera` implement the core
  interfaces.
- Unity input-action assets, player index resolution, and Vector2 action type
  dispatch are not implemented; named `IInputSource` axes are the contract.
- Discovery uses the provider's four pipeline stages instead of a GameObject
  child hierarchy.
- The host calls `update()` explicitly before the camera update. There is no
  automatic `CameraSystem` tick or blend-participation suppression yet.
- Existing bindings are matched by owner pointer and axis name. Bindings added
  manually without an owner survive `synchronize()`.

### Verification

- Core Debug: **221/221 passed** (140 unit + 75 scenario + 6 golden).
- Core Release: **221/221 passed**.
- Examples Debug: **241/241 passed** (221 core + 20 example tests, including 5
  smoke tests).
- Examples Release: **241/241 passed**.
- `cinerig_05_orbital_follow --frames 2 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 05 example exits 0.
- Linux was not verified on hardware.


