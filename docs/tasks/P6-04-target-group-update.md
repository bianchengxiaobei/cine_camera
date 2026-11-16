# P6-04 TargetGroup Update Semantics

Status: completed
Depends on: P2-09, P1-01
Blocks: external consumers that use a target group as a normal transform

## Goal

Bring `targets::TargetGroup` closer to the reference
`CinemachineTargetGroup` update model. The group must support an explicit
update method, maintain a per-frame cache, and write its computed
position/rotation back to an optional owner entity when the host allows
transform writes.

The current implementation computes on demand and uses `manualPosition` /
`manualRotation` because it has no group transform. That is sufficient when the
group is only consumed by CineRig cameras, but it does not match the reference
for external systems that read the group transform or expect the group to update
once per frame.

## Scope

- Add a `GroupUpdateMethod` enum:
  - `Fixed` - update on `UpdateClock::Fixed`;
  - `Late` - update on `UpdateClock::Late` (default).
- Add:
  - `GroupUpdateMethod updateMethod = GroupUpdateMethod::Late;`
  - `EntityId groupEntity = 0;`
  - `void update(UpdateClock clock);`
  - `void doUpdate();`
- Maintain:
  - a cached valid-member list;
  - cached average position, group position, group rotation, bounds, and
    bounding sphere;
  - a `lastUpdateFrame_`/`lastUpdateClock_` guard so the group updates at most
    once per selected clock frame;
  - a dirty flag for member list, weight, radius, mode, or manual-transform
    changes.
- Make `groupPosition()`, `groupRotation()`, `boundingBox()`,
  `boundingSphere()`, `isEmpty()`, `viewSpaceBoundingBox()`, and
  `viewSpaceAngularBounds()` use the cache and refresh it when the selected
  clock frame has advanced.
- `doUpdate()` must:
  - refresh member validity using `EntityId`, `world.exists()`, and weight;
  - compute weighted average, group center, bounds, sphere, and rotation;
  - write the owner transform when `groupEntity != 0` using
    `IEntityWorld::trySetTransform()`;
  - keep the cached values usable when write-back returns false.
- If `groupEntity != 0` and the entity exists, use its transform as the manual
  position/rotation fallback. Otherwise keep the existing
  `manualPosition`/`manualRotation` fields.
- Call `update(clock)` for all registered groups from `CameraSystem` before
  camera updates. `TargetGroup` still remains usable standalone; its accessors
  perform a lazy update if the host does not call `update()`.
- `CameraSystem::createTargetGroup()` continues to own the group; no new handle
  semantics are needed.
- Add tests for:
  - default update method and default manual fallback;
  - fixed vs late frame gating;
  - lazy accessor update when the host does not call `update()`;
  - writable owner entity write-back;
  - read-only owner entity preserving cached values;
  - manual fallback from the owner entity;
  - member add/remove/weight/radius invalidation;
  - view-space bounds using the cache;
  - unchanged behavior for existing camera target resolution.
- Update the `09_target_group` example HUD to show the selected update method
  and group-entity write-back status.
- Update the task index, feature matrix, architecture notes, and example docs.

## Non-goals

- Unity `MonoBehaviour` lifecycle, `activeInHierarchy`, `Transform`, tags, or
  editor inspectors.
- `TargetPositionCache` playback. The group reads current `IEntityWorld`
  transforms.
- Changing group position/rotation math, bounding-sphere approximation, or
  weighted-member semantics.
- Adding a global target-group update bus outside `CameraSystem`.
- Serialization.

## Design

Reference behavior:

```text
DoUpdate():
    validate members
    averagePosition = weighted average
    boundingBox = weighted bounds
    boundingSphere = Ritter approximation
    if PositionMode == GroupCenter: transform.position = sphere.position
    if PositionMode == GroupAverage: transform.position = averagePosition
    if RotationMode == GroupAverage: transform.rotation = averageOrientation

FixedUpdate() -> if UpdateMethod == FixedUpdate: DoUpdate()
Update()      -> if UpdateMethod == Update: DoUpdate()
LateUpdate()  -> if UpdateMethod == LateUpdate: DoUpdate()
```

CineRig has Fixed and Late clocks but no normal `Update` clock. `Late` is the
default and represents the host's render/update clock. A host that needs normal
update can call `doUpdate()` explicitly or call `update(UpdateClock::Late)`.
`CameraSystem` drives registered groups at the same clock as cameras.

```text
CameraSystem::update(clock, dt)
  updateTargetGroups(clock)
  updateCameras(clock, dt, ...)
  processBrains(clock, dt)
```

The group cache must not allocate after the valid-member vector reaches its
high-water mark. `targetCount()` continues to include invalid members, matching
the current API.

## API Sketch

```cpp
class TargetGroup : public ::cinerig::TargetGroup
{
public:
    enum class GroupUpdateMethod
    {
        Fixed,
        Late,
    };

    GroupUpdateMethod updateMethod = GroupUpdateMethod::Late;
    EntityId groupEntity = 0;

    PositionMode positionMode = PositionMode::GroupCenter;
    RotationMode rotationMode = RotationMode::Manual;
    math::Vec3 manualPosition{};
    math::Quat manualRotation{};

    void update(UpdateClock clock);
    void doUpdate();
    void validate();

    math::Vec3 groupPosition() const override;
    math::Quat groupRotation() const override;
    math::Bounds boundingBox() const;
    math::BoundingSphere boundingSphere() const;

private:
    void refresh() const;
    void invalidateCache() noexcept;
    std::uint64_t currentUpdateFrame() const noexcept;

    mutable std::vector<ValidMember> validMembers_;
    mutable math::Vec3 averagePosition_{};
    mutable math::Vec3 groupPosition_{};
    mutable math::Quat groupRotation_{};
    mutable math::Bounds boundingBox_{};
    mutable math::BoundingSphere boundingSphere_{};
    mutable std::uint64_t lastUpdateFrame_ = 0;
    mutable bool cacheValid_ = false;
    bool dirty_ = true;
};
```

Member mutation methods (`addMember`, `removeMember`, and `clear`) must mark the
cache dirty. Direct public field writes to `positionMode`, `rotationMode`,
`manualPosition`, `manualRotation`, or `groupEntity` cannot be observed until
the next update; document this as a C++ value-object constraint.

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineTargetGroup.cs`
  - `UpdateMethods`, `DoUpdate`, weighted average/bounds/sphere/rotation,
    transform write-back, and lazy accessor refresh.
- `CineRig/src/targets/target_group.cpp`
  - current on-demand `refresh()` and validity filtering.
- `CineRig/include/cinerig/engine/entity_world.hpp`
  - `trySetTransform()`, `frame()`, `UpdateClock`, and optional write-back.
- `CineRig/src/runtime/camera_system.cpp`
  - update loop and target-group ownership.

## Acceptance

- `updateMethod` defaults to `Late`; `groupEntity` defaults to `0`.
- Fixed-clock updates do not run on the late clock, and vice versa.
- Accessors lazily update when the host does not call `update()`.
- A writable group entity receives the computed transform.
- A read-only group entity leaves the host transform unchanged while cached
  group queries still return the computed values.
- An owner entity's transform supplies manual position/rotation fallback when
  no explicit manual values are configured.
- Member changes invalidate the cache immediately.
- Existing weighted group position/rotation, bounds, sphere, view-space queries,
  and camera target resolution tests remain green.
- Core, example, install, no-tests, and performance checks pass on Windows.

## Implementation Record

Implemented `GroupUpdateMethod::{Fixed, Late}` on `targets::TargetGroup`, with
`updateMethod` defaulting to `Late` and `groupEntity` defaulting to `0`. The
camera-layer `TargetGroup` base now has a no-op virtual `update(UpdateClock)`, so
`CameraSystem` can drive every registered group before camera updates without a
concrete `targets` dependency or a dynamic cast.

`TargetGroup` now owns a reusable valid-member cache plus cached average
position, group position, group rotation, bounding box, bounding sphere, weight
sum, and maximum weight. `lastUpdateFrame_`, `lastUpdateClock_`, and a dirty flag
gate updates to at most once per selected clock frame. `addMember`,
`removeMember`, `clear`, `validate`, and the non-const `members()` overload
invalidate the cache; the const accessors refresh lazily when the host does not
call `update()`.

`doUpdate()` validates members with `EntityId`, `world.exists()`, and weight,
rebuilds the weighted bounds and Ritter sphere, then writes position/rotation to
`groupEntity` through `IEntityWorld::trySetTransform()` while preserving the
owner's scale. A failed write-back leaves the computed cache intact and is
reported by `writeBackSucceeded()`. When the owner entity exists, its transform
supplies the empty-group/manual-rotation fallback; otherwise `manualPosition` and
`manualRotation` remain the fallback. `CameraSystem::update()` calls
`updateTargetGroups(clock)` before `prepareBrains()` and `updateCameras()`.

Reference differences:

- CineRig has Fixed and Late clocks only; there is no separate normal `Update`
  clock. `Late` is the default host/update clock, and explicit `doUpdate()` is
  the escape hatch for a host that needs a normal-update pass.
- Unity's lifecycle and `activeInHierarchy` checks are replaced by
  `EntityId`, `IEntityWorld::exists()`, and weight validity.
- `TargetPositionCache` playback is still out of scope; members are read from
  current host transforms.
- Public value fields cannot be observed by the cache until the next `update()`
  or explicit `doUpdate()`. This is a documented C++ value-object constraint;
  member mutation methods and `validate()` invalidate immediately.

## Verification

- `[target-group]`: 12 cases / 85 assertions passed.
- Unit: 345 cases / 3098 assertions passed.
- Scenario: 142 cases / 1161 assertions passed.
- Core Debug and Release: 518/518 CTest passed.
- Examples Debug and Release: 571/571 CTest passed.
- AddressSanitizer: 511/511 core tests passed, performance excluded.
- Performance: 7/7 cases passed; target-group steady-state allocations were 0 in
  Release.
- Installed `find_package(CineRig)` P6-04 consumer built and exited 0.
- `CINERIG_BUILD_TESTS=OFF` Release build passed.

The full verification baseline is recorded in
[runtime-parity.md](../runtime-parity.md) and
[release-readiness.md](../release-readiness.md).
