# 12 Rotate With Follow Target

P2-12 vertical slice: `RotateWithFollowTarget` makes the camera orientation
match a moving and banking target. A `Follow` Body keeps a chase-camera offset,
so the whole rig orbits and rolls with the target; the damping toggle shows the
orientation lag directly.

## Controls

- `Space`: pause/resume target motion
- `D`: toggle zero damping / 0.35 s damping
- `R`: reset motion, damping, and camera state
- `Esc`: quit

The HUD shows target position, camera-vs-target orientation error, damping,
body configuration, and frame count.

## Run

```powershell
build/examples/examples/12_rotate_with_follow/Debug/cinerig_12_rotate_with_follow.exe
```

Headless smoke:

```powershell
build/examples/examples/12_rotate_with_follow/Debug/cinerig_12_rotate_with_follow.exe --frames 2 --hidden --no-vsync
```
