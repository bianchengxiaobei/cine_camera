# P3-02 Group Framing

Status: completed  
Depends on: P0-04, P1-02, P2-02, P2-09, P3-01  
Blocks: group-aware automatic framing, framing while orbiting or following

## Goal

Implement the `targets::GroupFraming` camera extension. It fits a look-at or
follow `TargetGroup` into the requested screen area by adjusting the camera
position, orientation, FOV, or orthographic size.

## Scope

- Add `TargetGroup` view-space queries:
  - weighted member bounds in camera space
  - angular bounds and depth range
- Add `VirtualCamera` accessors for the effective look-at and follow groups.
- Implement `targets::GroupFraming` with:
  - Horizontal, Vertical, and HorizontalAndVertical framing modes
  - ZoomOnly, DollyOnly, and DollyThenZoom size adjustment
  - ChangePosition and ChangeRotation lateral adjustment
  - framing size, center offset, damping, FOV range, dolly range, and
    orthographic-size range
  - perspective and orthographic framing
  - Body-stage application when the body applies after Aim, otherwise Aim-stage
- Add unit coverage for defaults, validation, view-space bounds, zoom-only,
  framing modes/aspect, orthographic framing, follow-group fallback, dolly
  clamping, rotation-only lateral adjustment, and after-Aim stage selection.
- Add a `17_group_framing` SDL example with a multi-member moving group,
  bounded group visualization, mode controls, HUD, and smoke test.
- Update the task index, feature matrix, architecture, and example index.

## Non-goals

- Confiner2D compatibility caching or invalidating a 2D lens cache.
- Editor-only group bounds gizmos, inspectors, or serialization.
- Automatic target creation or required-target validation attributes.
- New collision resolution; decollider work remains separate.

## Design

CineRig uses a right-handed camera space with `-Z` forward. The Unity source
uses `+Z` view-space coordinates, so TargetGroup returns CineRig camera-space
bounds and depth-positive angular ranges. GroupFraming converts bounds to a
depth-positive framing space for the original scalar algorithm.

```text
observer -> inverse rotation * (worldPosition - observerPosition)

if body applies after Aim:
    skip the first Body callback
    apply on the second Body callback after the body has run
else:
    apply at Aim
```

Perspective framing:

```text
1. obtain view-space bounds and dolly back to include depth
2. optionally shift position or rotate to center the group
3. recompute angular bounds at the near depth
4. dolly within DollyRange
5. recompute bounds and zoom within FovRange
6. damp FOV, orientation, and position adjustments
7. apply CenterOffset in camera space
```

Orthographic framing adjusts `orthographicSize` and lateral position using the
same bounds and damping model.

## API Sketch

```cpp
class GroupFraming : public CameraExtension
{
public:
    enum class FramingMode { Horizontal, Vertical, HorizontalAndVertical };
    enum class SizeAdjustmentMode { ZoomOnly, DollyOnly, DollyThenZoom };
    enum class LateralAdjustmentMode { ChangePosition, ChangeRotation };

    FramingMode framingMode = FramingMode::HorizontalAndVertical;
    float framingSize = 0.8f;
    math::Vec2 centerOffset{};
    float damping = 2.0f;
    SizeAdjustmentMode sizeAdjustment = SizeAdjustmentMode::DollyThenZoom;
    LateralAdjustmentMode lateralAdjustment = LateralAdjustmentMode::ChangePosition;
    math::Vec2 fovRange{1.0f, 100.0f};
    math::Vec2 dollyRange{-100.0f, 100.0f};
    math::Vec2 orthoSizeRange{1.0f, 1000.0f};

    math::Bounds groupBounds{};
    math::Vec3 groupBoundsPosition{};
    math::Quat groupBoundsRotation{};

    void validate();
    void reset(VirtualCamera& vcam);
};
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineGroupFraming.cs`
  - settings, stage selection, perspective/orthographic framing, angular bounds
    fitting, damping, and center offset
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineTargetGroup.cs`
  - weighted member bounds, view-space bounds, and angular bounds
- `com.unity.cinemachine/Runtime/Core/UnityVectorExtensions.cs`
  - camera rotation to target and camera rotation application
- `Documentation~/CinemachineGroupFraming.md`
  - framing modes, size adjustment, lateral adjustment, and ranges

## Acceptance

- Defaults match the original settings.
- Validation clamps framing size, damping, dolly, FOV, and ortho ranges.
- TargetGroup view-space queries account for weights, radii, and members behind
  the observer.
- ZoomOnly does not move the camera; DollyOnly does not change the lens.
- DollyThenZoom respects DollyRange before applying FOV.
- ChangePosition centers by moving; ChangeRotation centers by rotating.
- Perspective FOV fits Horizontal, Vertical, or both dimensions with aspect.
- Orthographic framing adjusts size and lateral position.
- Framing applies after an after-Aim body and does not double-apply.
- Follow-group fallback works when the look-at target is not a group.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/targets/group_framing.hpp` and
  `src/targets/group_framing.cpp`
  - perspective and orthographic framing
  - Horizontal, Vertical, and HorizontalAndVertical framing modes
  - ZoomOnly, DollyOnly, and DollyThenZoom size adjustment
  - ChangePosition and ChangeRotation lateral adjustment
  - FOV, dolly, and orthographic-size ranges
  - FOV, position, and orientation adjustment caches with damping
  - center-offset application for both lateral modes
  - after-Aim body detection and double Body-callback suppression
  - public group bounds frame for debug visualization
- `TargetGroup::viewSpaceBoundingBox` and
  `TargetGroup::viewSpaceAngularBounds`
  - weighted member positions and radii
  - members behind the observer handling
  - angular bounds and depth range in CineRig camera space
- `VirtualCamera::lookAtTargetAsGroup` and `followTargetAsGroup`
  - effective group resolution, including inherited targets
- `tests/unit/group_framing_test.cpp`
  - 10 cases and 45 assertions covering defaults, validation, view-space
    bounds/angles, zoom-only, framing mode/aspect, dolly clamping,
    rotation-only framing, orthographic framing, follow-group fallback, and
    after-Aim stage selection.
- `examples/17_group_framing`
  - four moving members, weighted group bounds visualization, mode controls,
    framing size, damping and orthographic toggles, HUD, and smoke test.
- Build/export, task index, feature matrix, architecture, and example index
  updated.

### Differences

- CineRig camera space uses `-Z` forward. TargetGroup exposes camera-space
  bounds and depth-positive angles; GroupFraming converts to the depth-positive
  framing space used by the original scalar algorithm.
- Unity stores extension state in `VcamExtraState`; CineRig keeps per-camera
  caches keyed by the virtual camera.
- Confiner2D compatibility caching and editor gizmos are omitted.
- GroupFraming resolves the concrete `targets::TargetGroup`; the base target
  interface does not expose view-space bounds.

### Verification

- Core Debug: **285/285 passed** (204 unit + 75 scenario + 6 golden).
- Core Release: **285/285 passed**.
- Examples Debug: **317/317 passed** (285 core + 32 example tests, including 17
  smoke tests).
- Examples Release: **317/317 passed**.
- `cinerig_17_group_framing --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 17 example exits 0.
- Visible window confirms the four moving members, weighted orange bounds,
  group center, framing modes, damping, FOV, camera distance, and controls.
  Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p3_group_framing.png`.
- Linux was not verified on hardware.

