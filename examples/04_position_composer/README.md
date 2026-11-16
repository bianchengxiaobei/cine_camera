# 04 Position Composer

P2-03 vertical slice: a fixed-orientation camera uses
`cinerig::PositionComposer` as a post-Aim body component to maintain a
screen-space relationship with a moving target. The camera does not rotate; only
its position changes.

The target follows a smooth Lissajous path while yawing and rolling. The camera
distance is `12`, with the initial camera looking at the target from a raised
perspective angle. Composition is applied in camera-local screen space.

## Controls

- `1`: no damping
- `2`: low damping (default)
- `3`: high damping
- `C`: cycle composition preset (`Offset`, `Center`, `DeadZone`, `HardLimits`)
- `L`: toggle lookahead (`0.25s`, smoothing `0.15`)
- `T`: toggle target-local offset `(0, 0.8, 0)`
- `O`: toggle perspective / orthographic projection
- `Space`: pause/resume the target
- `R`: reset the target path and camera state
- `Esc`: quit

The HUD shows the active camera, target and camera positions, camera distance,
composition preset, damping, lookahead, target offset, projection and frame
statistics.

The composer moves along camera local Z to maintain the distance, converts the
normalized dead zone and hard limits into camera-local units, snaps on
activation or missing history, damps the soft correction, and clamps the real
target to the hard limits without damping. Changing the screen position is
applied immediately instead of being damped.

## Run

```powershell
build/examples/examples/04_position_composer/Debug/cinerig_04_position_composer.exe
```

Headless smoke:

```powershell
build/examples/examples/04_position_composer/Debug/cinerig_04_position_composer.exe --frames 2 --hidden --no-vsync
```
