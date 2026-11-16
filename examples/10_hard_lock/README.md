# 10 Hard Lock To Target

P2-10 vertical slice: `HardLockToTarget` places the camera exactly on a moving
tool target. A separate look-at point is kept centered by `RotationComposer`,
making it easy to see the lock error and damping behavior.

## Controls

- `Space`: pause/resume target motion
- `D`: toggle zero damping / 0.5 s damping
- `R`: reset motion, damping, and camera state
- `Esc`: quit

The HUD shows target/camera positions, lock error, damping, motion state, and
frame count.

## Run

```powershell
build/examples/examples/10_hard_lock/Debug/cinerig_10_hard_lock.exe
```

Headless smoke:

```powershell
build/examples/examples/10_hard_lock/Debug/cinerig_10_hard_lock.exe --frames 2 --hidden --no-vsync
```
