# 05 Orbital Follow

P2-04 vertical slice: a camera uses `cinerig::OrbitalFollow` to travel on a
sphere or the original three-ring orbit surface around a moving target.
`RotationComposer` aims the camera at the target while the orbital body owns the
camera position.

The target follows a smooth path and rotates. The default rig is a sphere with
radius `10`, a horizontal axis of `25` degrees, a vertical axis of `22` degrees,
and horizontal recentering toward the tracking target. Horizontal and vertical
input are driven by `cinerig::InputAxisController`; radial zoom stays direct so
the wheel/Q/E behavior remains explicit.

## Controls

- `A`/`D` or left/right: orbit horizontally
- `W`/`S` or up/down: change vertical orbit angle
- `Q`/`E` or mouse wheel: change the radial axis
- `O`: toggle `Sphere` / `ThreeRing`
- `B`: cycle tracker binding mode
- `C`: toggle horizontal recentering
- `1`: no damping
- `2`: low damping (default)
- `3`: high damping
- `Space`: pause/resume the target
- `R`: reset the orbit, target path, binding mode, and camera state
- `Esc`: quit

The HUD shows the active camera, target and camera positions, axis values, orbit
style, binding mode, damping, recentering state, target state and frame
statistics.

## Run

```powershell
build/examples/examples/05_orbital_follow/Debug/cinerig_05_orbital_follow.exe
```

Headless smoke:

```powershell
build/examples/examples/05_orbital_follow/Debug/cinerig_05_orbital_follow.exe --frames 2 --hidden --no-vsync
```
