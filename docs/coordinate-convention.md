# Coordinate Convention

CineRig uses one explicit convention in the core library. Engine adapters are
responsible for converting to and from their own convention.

## Axes

- Right-handed coordinate system.
- `+Y` is up.
- `-Z` is forward.
- `+X` is right.

```text
        +Y
         │
         │
         └──── +X
        /
      -Z (forward)
```

This matches OpenGL's default camera space and keeps the SDL3/OpenGL examples
free of extra flips.

## Vectors

| Name | Value |
| --- | --- |
| `Vec3::up` | `(0, 1, 0)` |
| `Vec3::right` | `(1, 0, 0)` |
| `Vec3::forward` | `(0, 0, -1)` |

## Quaternions

- `Quat` is a unit quaternion `(x, y, z, w)`.
- `quat * vector` rotates the vector.
- `lookRotation(forward, up)` returns a rotation whose local `-Z` points along
  `forward` and whose local `+Y` is aligned with `up`.
- `slerp` takes the shortest path and handles opposite quaternions.
- `angle(a, b)` returns the unsigned angle in radians.

## Euler angles

Two helpers are provided to avoid ambiguity:

- `quatFromEulerXYZ(radians)` applies rotations in **Z, then X, then Y** order.
  This matches Unity's `Quaternion.Euler(x, y, z)` convention and is used when
  porting formulas that were authored for Unity.
- `quatFromYawPitchRoll(yaw, pitch, roll)` applies **Y, then X, then Z** in a
  different composition order, for independently authored camera controls.
  `OrbitalFollow` uses `quatFromEuler` for the original sphere offset so the
  Unity `Quaternion.Euler(y, x, 0)` placement is preserved.
- `PanTilt` negates its local pan and tilt Euler terms for the `-Z` camera
  forward: positive pan looks right and positive tilt looks down.
- `ThirdPersonFollow` keeps the target rotation as camera orientation and
  places the camera behind the target along `-targetForward`; with `-Z`
  forward this is the target position plus `+Z` in target-local space.
- `ThirdPersonAim` checks whether the camera is behind the player with the
  player-forward component of `playerPosition - cameraPosition`; its local
  `z` sign is inverted for the `-Z` forward convention.
- `targets::TargetGroup` computes world-space positions and bounds;
  `GroupCenter` uses the weighted bounding-sphere center and `GroupAverage`
  uses the weighted average of valid member positions.

Public configuration uses degrees. Math functions use radians unless the name
contains `Degrees`.

## Camera state

- `CameraState::rawPosition` and `rawOrientation` are world-space.
- `CameraState::correctedPosition()` = `rawPosition + positionCorrection`.
- `CameraState::correctedOrientation()` = `rawOrientation * orientationCorrection`.
- `CameraState::finalOrientation()` applies `lens.dutch` around the local
  forward axis (`-Z`) after correction.
- `referenceUp` is a unit world-space vector, normally `(0,1,0)`.
- `referenceLookAt` uses NaN to mean "no LookAt"; use `hasLookAt()`.

## Matrices

- `Mat4` is column-major, compatible with OpenGL.
- `view = inverse(translation(position) * rotation(orientation))`.
- Perspective projection uses a vertical FOV in radians, `aspect = width/height`,
  and OpenGL clip space `z ∈ [-1, 1]`.
- Orthographic projection uses `orthographicSize` as the half-height; the
  half-width is `orthographicSize * aspect`.

## Engine interfaces

The host engine is connected through the non-owning interfaces in
`include/cinerig/engine/`. CineRig never includes an engine header.

### Entities and transforms

- `EntityId`, `CameraId`, `SplineId`, and `GroupId` are opaque 64-bit handles;
  `0` means "no entity".
- `Transform` uses the same right-handed, `+Y`-up, `-Z`-forward convention as
  `CameraState`. The adapter is responsible for converting the host engine's
  transform convention.
- `Transform::scale` defaults to `(1, 1, 1)`.
- `IEntityWorld::exists()` must be checked before treating a missing entity as
  meaningful. `transform()` on a missing entity returns an identity transform.
- `trySetTransform()` is an optional write-back hook. Returning `false` means
  the host entity is read-only; the core keeps its own state.
- A negative `deltaTime` means that no time-dependent update should be applied
  for that clock.

### Update clocks

- `UpdateClock::Fixed` is the physics/fixed step.
- `UpdateClock::Late` is the update immediately before rendering.
- `frame(Fixed)` is the fixed-step frame; `frame(Late)` is the global render
  frame.
- `lastChangeClock()` defaults to `Late` so SmartUpdate is conservative when
  the host does not track when an entity changed.

### Output camera

- `AppliedCamera::state` is already final: corrections, Dutch, and
  `state.lens` are applied before `ICameraOutput::apply()`.
- Use `state.finalPosition()` and `state.finalOrientation()` to drive the host
  camera.
- `AppliedCamera::aspect` is `viewportWidth / viewportHeight`.
- When `hasCamera` is `false`, keep the previous camera or use the host's
  default camera.
- When `state.lens.modeOverride == LensSettings::OverrideMode::None`, the
  adapter can use `ICameraOutput::outputLens()` to inherit the host camera's
  orthographic/physical mode. The final inheritance decision belongs to the
  runtime layer in P1.
- `CameraState::BlendHint::NoPosition`, `NoOrientation`, and `NoLens` are
  retained in the applied state. An adapter that applies the state directly
  should honor those hints; the current CineRig runtime also preserves the
  host values for those components.

### Physics queries

- `Ray` is the shared `math::Ray` type; `Bounds` is the shared `math::Bounds`
  type.
- `QueryFilter::ignoreEntity` replaces Unity's `IgnoreTag` for the core API.
  The adapter may also apply its own tag/layer policy.
- `overlapSphere()` results are unordered; callers needing the nearest result
  must sort by `distance`.
- Physics is optional. A missing physics world is represented by
  `NullPhysicsWorld`, which returns no hits.

### Splines and input

- `ISpline` parameters are normalized global arc-length fractions in `[0, 1]`;
  distances are in world units. `evaluateUp()` is in the core coordinate
  convention.
- `PathIndexUnit::Knot` combines a knot index and a fraction in that segment's
  Bezier parameter. Closed splines use `[0, knotCount]`, open splines use
  `[0, knotCount - 1]`.
- `SplineSettings` stores a non-owning `ISpline` pointer and standardizes open
  positions by clamping and closed positions by wrapping. `BezierSpline`
  overrides unit conversion to account for non-uniform segment lengths.
- `SplineDolly::splineOffset` is expressed in the spline frame: X is the
  spline right axis, Y the spline up axis, and Z the tangent-forward axis.
- `SplineRoll` values are degrees around the local spline forward axis.
  CineRig composes the roll on the right of the tangent/up rotation using
  `Vec3::forward` (`-Z`), matching the camera viewing direction.
- `IInputSource` uses named axes. Unbound names return `0`;
  `core::InputAxisController` maps descriptor names to source names, and
  `IInputSource` derives `core::IInputAxisSource`.
- `IAnimationStateProvider` state hashes are opaque host state identifiers.

The optional Null adapters are `NullPhysicsWorld`, `NullInputSource`, and
`NullPostProcessor`.

## Unity adapter notes

Unity uses a left-handed system with `+Z` forward and `+Y` up. An adapter for
Unity (or a Unity-like engine) should convert:

- positions: negate `z` if mirroring the world, or use a full handedness
  conversion depending on the desired mapping;
- rotations: convert the quaternion between handedness conventions;
- camera forward: Unity `+Z` maps to CineRig `-Z`.

The conversion is intentionally not hidden inside the core library. Keeping it
in the adapter makes the convention explicit and testable.

## Units

- Distance and spline length: world units.
- Time: seconds. A negative delta time disables time-dependent updates.
- Viewport and screen space: pixels; aspect is width / height.
- Angular damping: seconds.
- FOV / Dutch / PanTilt: degrees at the public API boundary.
- Internally, trigonometric functions use radians.



