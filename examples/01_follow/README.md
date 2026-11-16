# 01 Follow

P2-02 vertical slice: a moving, rotating target is followed by a
`cinerig::Follow` body using the full Tracker binding modes and aimed at by the
full `cinerig::RotationComposer`, then rendered through
`cinerig::CameraSystem` and `SdlCameraOutput`.

The target follows a smooth Lissajous path and slowly yaws while rolling. The
camera offset is `(0, 2, -6)`.

## Controls

- `1`: no damping
- `2`: low damping (default)
- `3`: high damping
- `B`: cycle binding mode (`WorldSpace`, `LockToTarget`, `NoRoll`, `WithWorldUp`, `OnAssign`, `LazyFollow`)
- `C`: cycle composition preset (`Offset`, `Center`, `DeadZone`, `HardLimits`)
- `L`: toggle lookahead (`0.25s`, smoothing `0.15`)
- `Space`: pause/resume the target
- `R`: reset the target path and camera state
- `Esc`: quit

The HUD shows the active camera, target and camera positions, damping mode,
binding mode, composition preset, lookahead state, FOV and frame statistics.

The composer caches perspective or orthographic FOV, applies target-local
`targetOffset`, maps screen position/dead zone/hard limits into angular space,
damps only the soft dead zone, clamps hard limits without damping, predicts with
lookahead, and snaps on activation or negative time.

## Run

```powershell
build/examples/examples/01_follow/Debug/cinerig_01_follow.exe
```

Headless smoke:

```powershell
build/examples/examples/01_follow/Debug/cinerig_01_follow.exe --frames 2 --hidden --no-vsync
```
