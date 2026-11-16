# 23 Confiner 3D

P3-08 slice: a ThirdPersonFollow camera is confined inside a host volume. The
target moves sideways, pulling the raw camera beyond the volume edge; the
`Confiner3D` snaps the corrected camera back to the closest volume point. The
grey line is the raw target-to-camera offset and the green line is the confined
one.

Two volumes demonstrate the generic closest-point query:

- box: layer-independent AABB volume rendered as a cyan wire box
- sphere: sphere volume rendered as three great circles
- none: disable confinement to see the unmodified camera path

`S` toggles the slowing distance. Near the edge, the camera eases through the
original `t^3 + 0.05` curve instead of moving at full speed.

## Controls

- `S`: toggle edge slowing
- `V`: cycle box, sphere, and no volume
- `R`: restore the box volume and defaults
- `Esc`: quit

## Run

```powershell
build/examples/examples/23_confiner3d/Debug/cinerig_23_confiner3d.exe
```

Headless smoke:

```powershell
build/examples/examples/23_confiner3d/Debug/cinerig_23_confiner3d.exe --frames 2 --hidden --no-vsync
```
