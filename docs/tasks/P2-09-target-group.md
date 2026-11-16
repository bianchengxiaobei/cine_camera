# P2-09 Target Group

Status: completed  
Depends on: P0-03, P1-01, P1-02, P2-01  
Blocks: GroupFraming, group-driven follow/aim rigs

## Goal

Implement the concrete `targets::TargetGroup` and make `TargetRef::Kind::Group`
resolve through its configured position and rotation modes. The group owns a
list of weighted entity members and computes average positions, group-center
positions, orientations, bounds, and a bounding sphere.

## Scope

- Add `targets::TargetGroup` with:
  - entity members, weight, and radius
  - `PositionModes::GroupCenter` and `GroupAverage`
  - `RotationModes::Manual` and `GroupAverage`
  - add/remove/find/clear member operations
  - validity filtering (`EntityId` exists and weight > 0)
  - axis-aligned bounding box and Ritter bounding sphere
  - manual position/rotation fallbacks for empty groups
- Extend the core `TargetGroup` interface with default `groupPosition()` and
  `groupRotation()` helpers so existing custom groups remain compatible.
- Make `VirtualCamera` resolve group targets through those methods instead of
  always using a weighted average.
- Add `09_target_group` SDL example with several moving/weighted members, mode
  switching, group-center visualization, and HUD.
- Add unit/scenario coverage for member validation, position/rotation modes,
  bounds, empty groups, and camera target resolution.
- Update the task index, feature matrix, architecture, and example index.

## Non-goals

- GroupFraming, screen-space group bounds, or lens framing.
- Unity transforms, activeInHierarchy, or tags.
- Serialization or editor inspectors.
- TargetPositionCache; the group reads the current `IEntityWorld` transform.

## Design

### Members and validity

```cpp
struct Member
{
    EntityId entity = 0;
    float weight = 1.0f;
    float radius = 0.5f;
};
```

A member is valid when `entity != 0`, the entity exists in the supplied
`IEntityWorld`, and `weight > Epsilon`. Invalid members are ignored by all
calculations but remain in the member list until removed.

### Position and rotation

`GroupAverage` uses the weighted average of valid member positions.
`GroupCenter` uses the center of the weighted bounding sphere, matching the
original Cinemachine behavior. `RotationModes::Manual` uses the group's
`manualRotation`; `GroupAverage` averages member forward/up vectors weighted by
member weight and falls back to `manualRotation` when the vectors cancel.

For an empty group, position falls back to `manualPosition` and rotation to
`manualRotation`.

### Bounds

Weighted member bounds use:

```text
weightRatio = weight / maxWeight
boundsPosition = lerp(averagePosition, memberPosition, weightRatio)
boundsRadius = memberRadius * weightRatio
```

The axis-aligned box encapsulates each member sphere. The bounding sphere uses
the original two-pass approximation: start with the first weighted member
sphere, then expand for each remaining member.

### Camera resolution

`core::TargetGroup` gains default virtual `groupPosition()` and
`groupRotation()` methods. `targets::TargetGroup` overrides them with its mode
logic. `VirtualCamera` uses these methods for `TargetRef::Group` while keeping
its existing null/empty checks.

## API Sketch

```cpp
namespace cinerig::targets
{

class TargetGroup : public ::cinerig::TargetGroup
{
public:
    enum class PositionMode { GroupCenter, GroupAverage };
    enum class RotationMode { Manual, GroupAverage };

    struct Member
    {
        EntityId entity = 0;
        float weight = 1.0f;
        float radius = 0.5f;
    };

    explicit TargetGroup(IEntityWorld& world);

    PositionMode positionMode = PositionMode::GroupCenter;
    RotationMode rotationMode = RotationMode::Manual;
    math::Vec3 manualPosition{};
    math::Quat manualRotation{};

    void addMember(EntityId entity, float weight = 1.0f, float radius = 0.5f);
    bool removeMember(EntityId entity);
    std::size_t findMember(EntityId entity) const;
    void clear();

    bool isValid() const override;
    bool isEmpty() const override;
    std::size_t targetCount() const override;
    math::Vec3 targetPosition(std::size_t index) const override;
    math::Quat targetRotation(std::size_t index) const override;
    float targetWeight(std::size_t index) const override;
    float targetRadius(std::size_t index) const override;

    math::Vec3 groupPosition() const override;
    math::Quat groupRotation() const override;
    math::Bounds boundingBox() const;
    math::BoundingSphere boundingSphere() const;
    void validate();

private:
    struct ValidMember { /* entity, position, rotation, weight, radius */ };
    void refresh() const;
    std::vector<ValidMember> validMembers() const;

    IEntityWorld& world_;
    std::vector<Member> members_;
    mutable math::Vec3 groupPosition_{};
    mutable math::Quat groupRotation_{};
    mutable math::Bounds boundingBox_{};
    mutable math::BoundingSphere boundingSphere_{};
    mutable bool hasValidMembers_ = false;
};

} // namespace cinerig::targets
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineTargetGroup.cs`
  - member validity, position/rotation modes, weighted bounds, Ritter sphere,
    and manual fallbacks
- `com.unity.cinemachine/Runtime/Core/ICinemachineTargetGroup.cs`
  - group bounding box/sphere and view-space bounds API
- `docs/tasks/P0-03-core-types.md`
  - original abstract `TargetGroup` member interface
- `docs/tasks/P1-02-virtual-camera-pipeline.md`
  - temporary group weighted-average resolution to replace

## Acceptance

- Weighted average and group-center positions match the original semantics.
- Manual and average rotations behave correctly, including empty/cancelled
  vector fallbacks.
- Invalid/missing/zero-weight members are ignored.
- Bounds and bounding sphere include member radii with weight scaling.
- `TargetRef::Group` camera targets use group position/rotation modes.
- Debug/Release builds and the existing test suite pass.

## Implementation Record

To be completed.

## Implementation Record

### Implemented

- `include/cinerig/targets/target_group.hpp` and
  `src/targets/target_group.cpp`
  - Weighted entity members with radius, add/remove/find/clear and validation.
  - `GroupCenter` and `GroupAverage` position modes.
  - `Manual` and `GroupAverage` rotation modes.
  - Weighted axis-aligned bounds and the original two-pass approximate
    bounding sphere.
  - Empty/missing/zero-weight member filtering and manual position/rotation
    fallbacks.
- `include/cinerig/core/camera_target.hpp`
  - Default `groupPosition()` and `groupRotation()` virtual helpers keep custom
    groups source-compatible.
- `src/camera/virtual_camera.cpp`
  - Group targets now resolve through the group's configured position and
    rotation modes.
- `tests/unit/target_group_test.cpp`
  - 5 cases and 32 assertions covering member operations, weighted position
    and rotation, group-center/bounds/sphere, invalid members, and camera
    target resolution.
- `examples/09_target_group`
  - Three moving weighted members, position/rotation mode switching, weight
    presets, bounds and sphere visualization, pause/reset, HUD, and headless
    smoke test.
- `include/cinerig/cinerig.hpp`, `CMakeLists.txt`, and
  `tests/unit/CMakeLists.txt`
  - Export and build the target group implementation and tests.
- Task index, feature matrix, architecture notes, and example index updated.

### Differences

- CineRig members use `EntityId` and current `IEntityWorld` transforms instead
  of Unity `Transform`/`activeInHierarchy`.
- The group has explicit `manualPosition` and `manualRotation` fields because
  there is no host group transform.
- Group data is computed on demand from the current world; there is no
  `UpdateMethod` or editor `DoUpdate` loop.
- The core interface exposes default group position/rotation helpers; custom
  groups remain compatible without implementing new pure virtual methods.
- View-space bounds and GroupFraming are deferred to P3.
- `targetCount()` includes invalid members; invalid weights resolve to zero and
  are ignored by group calculations.

### Verification

- Core Debug: **246/246 passed** (165 unit + 75 scenario + 6 golden).
- Core Release: **246/246 passed**.
- Examples Debug: **270/270 passed** (246 core + 24 example tests, including 9
  smoke tests).
- Examples Release: **270/270 passed**.
- `cinerig_09_target_group --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 09 example exits 0.
- Visible window confirms the P2-09 title, group position, position/rotation
  modes, members, sphere, bounds, weights, motion, controls, and frame count.
  Screenshot: `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_target_group.png`.
- Linux was not verified on hardware.
