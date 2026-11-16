# P3-05 Collision Impulse Source

Status: completed  
Depends on: P3-04  
Blocks: physics-driven camera reactions, deoccluder impulse integration

## Goal

Add the 3D collision/trigger impulse source from Cinemachine. The host physics
adapter supplies collision or trigger contact data; CineRig applies layer/tag
filtering, derives impact velocity and mass scaling, and generates a normal
`ImpulseEvent` through the existing source/definition pipeline.

## Scope

- Add `impulse::CollisionImpulseSource` deriving from `impulse::ImpulseSource`.
- Support layer-mask filtering and exact ignore-tag filtering.
- Support collision and trigger entry paths, with trigger contacts starting at
  zero relative velocity and falling back to body velocities.
- Support:
  - `useImpactDirection`
  - `scaleImpactWithMass`
  - `scaleImpactWithSpeed` using `sqrt(speed)`
  - optional source and other-body mass/velocity data
  - an enabled flag and manager-null behavior
- Define a host-neutral `ImpactInfo` value carrying the other object's layer,
  tag, body data, relative velocity, and presence.
- Add unit tests for defaults, filters, direction, mass, speed, triggers,
  disabled sources, body-less contacts, and zero-velocity fallback.
- Add a `20_collision_impulse` SDL example with simulated collisions,
  selectable filters/scaling, wave rings, HUD, and a smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- 2D physics contact callbacks or `Collider2D` parity; CineRig remains 3D.
- Owning a physics engine, collision world, or Unity callback dispatch.
- Unity `MonoBehaviour` lifecycle, tags as engine-owned objects, serialization,
  editors, or `SaveDuringPlay`.
- The deoccluder/decollider/confiner components; those remain separate tasks.

## Design

Unity receives contacts from its physics callbacks. CineRig has no engine
callback dispatch, so the host translates engine contact data into `ImpactInfo`
and calls `onCollision()` or `onTrigger()`. This keeps the source independent of
any particular physics engine while preserving the original filter and scaling
rules.

```text
host collision callback
  -> build ImpactInfo
  -> CollisionImpulseSource::onCollision / onTrigger
  -> layer/tag filter
  -> mass + velocity derivation
  -> ImpulseSource::generateImpulseWithVelocity
  -> ImpulseManager event
```

`relativeVelocity` is used when non-zero. For trigger entry, or a collision with
an exactly zero relative velocity, the source uses `-rigidbodyVelocity +
otherVelocity` when those bodies are present. `useImpactDirection` replaces the
default-velocity direction with the opposite impact velocity while preserving
the configured default-velocity magnitude. Speed scaling multiplies amplitude by
the square root of the impact speed, matching the original.

`ImpactInfo` is a transient value. Its `tag` is a `std::string_view` valid for
the call only and is compared synchronously; CineRig does not retain it.

## API Sketch

```cpp
class CollisionImpulseSource : public ImpulseSource
{
public:
    struct ImpactInfo
    {
        bool hasOther = true;
        std::uint32_t layer = 0;
        std::string_view tag{};
        bool otherHasRigidbody = false;
        float otherMass = 1.0f;
        math::Vec3 otherVelocity{};
        math::Vec3 relativeVelocity{};
    };

    explicit CollisionImpulseSource(ImpulseManager* manager = nullptr);

    LayerMask layerMask{1u};
    std::string ignoreTag{};
    bool useImpactDirection = false;
    bool scaleImpactWithMass = false;
    bool scaleImpactWithSpeed = false;
    bool enabled = true;
    bool hasRigidbody = false;
    float rigidbodyMass = 1.0f;
    math::Vec3 rigidbodyVelocity{};

    void reset();
    ImpulseEvent* onCollision(const ImpactInfo& impact);
    ImpulseEvent* onTrigger(const ImpactInfo& impact);
};
```

## Reference

- `com.unity.cinemachine/Runtime/Impulse/CinemachineCollisionImpulseSource.cs`
  - layer/tag filters, impact direction, mass and speed scaling, trigger entry,
    body velocity fallback, and default reset values.

## Acceptance

- Default layer mask is `1`; ignore tag is empty; all scaling and impact-direction
  flags are off; source and other bodies are optional.
- Contacts outside the layer mask or matching the ignore tag produce no event.
- Contacts passing the filter produce one event at the source position with the
  configured impulse definition.
- `useImpactDirection` uses the opposite of the impact velocity and preserves
  the default velocity's magnitude; near-zero impacts keep the default direction.
- Mass scaling multiplies by source and other body masses when present.
- Speed scaling multiplies by `sqrt(impact speed)`.
- Trigger entry ignores a supplied relative velocity and derives zero-velocity
  contact velocity from source/other bodies.
- Disabled sources, missing managers, filtered contacts, and missing other
  bodies behave safely.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/impulse/collision_impulse_source.hpp` and
  `src/impulse/collision_impulse_source.cpp`
  - `CollisionImpulseSource` deriving from `ImpulseSource`
  - host-neutral `ImpactInfo` with other-object presence, layer, transient tag,
    optional body mass/velocity, and collision relative velocity
  - layer-mask and exact ignore-tag filtering
  - collision and trigger entry points
  - source/other body mass scaling
  - `sqrt(speed)` amplitude scaling
  - impact-direction derivation with default-velocity magnitude preservation
  - zero-relative-velocity fallback to `-sourceVelocity + otherVelocity`
  - explicit enable, optional body flags, manager-null safety, and reset
- `ImpulseSource` now has a virtual destructor for safe public derivation.
- `CMakeLists.txt` and `include/cinerig/cinerig.hpp` export/build wiring.
- `tests/unit/collision_impulse_source_test.cpp`
  - 6 cases and 34 assertions covering defaults, layer/tag filters, other-object
    absence, impact direction, default-direction fallback, source/other mass,
    speed scaling, trigger velocity combination, disabled sources, null
    managers, and reset.
- `examples/20_collision_impulse`
  - simulated wall collisions and trigger crossing, accepted/rejected HUD
    counters, layer/tag/filter toggles, direction/mass/speed toggles, impact and
    trigger wave rings, and a smoke test.
- `examples/README.md`, task index, feature matrix, and architecture updated.

### Differences

- The host translates engine collision/trigger callbacks into `ImpactInfo`;
  CineRig does not own callback dispatch or a collision world.
- Source and other-body mass/velocity are explicit fields populated by the host
  rather than being queried from Unity `Rigidbody` components.
- Tags are transient `std::string_view` values compared during the call; CineRig
  does not retain them.
- 2D physics callbacks and `Collider2D` behavior are not implemented.
- `reset()` resets both the inherited `ImpulseSource` configuration and the
  collision filter/scaling fields.

### Verification

- Core Debug: **311/311 passed** (230 unit + 75 scenario + 6 golden).
- Core Release: **311/311 passed**.
- Examples Debug: **346/346 passed** (311 core + 35 example tests, including 20
  smoke tests).
- Examples Release: **346/346 passed**.
- `cinerig_20_collision_impulse --frames 120 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 20 example exits 0.
- Visible window shows layer-0 wall impulses, rejected layer-1 wall contacts,
  accepted trigger contacts, event pooling, and camera correction. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p3_collision_impulse.png`.
- Install includes `cinerig/impulse/collision_impulse_source.hpp`.
- Linux was not verified on hardware.



