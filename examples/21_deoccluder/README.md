# 21 Deoccluder

P3-06 slice: a ThirdPersonFollow camera is moved by a `Deoccluder` extension to
preserve line of sight to the target. A red box slides across the sight line and
a cyan glass panel sits farther back. The grey line is the raw target-to-camera
sight line; the green line is the corrected one.

The collision filter uses layer 0. The glass uses layer 1 and is ignored while
the transparent mask includes it. Switching the transparent mask off makes the
glass block the view as well. The HUD shows the strategy, filter, raw/corrected
camera positions, correction distance, occlusion state, and shot quality.

## Controls

- `1` / `2` / `3`: pull forward, preserve height, or preserve distance strategy
- `A`: toggle obstacle avoidance
- `T`: toggle the transparent glass layer
- `D`: toggle damping
- `Q`: toggle shot-quality evaluation
- `R`: restore defaults and reset the camera
- `Esc`: quit

## Run

```powershell
build/examples/examples/21_deoccluder/Debug/cinerig_21_deoccluder.exe
```

Headless smoke:

```powershell
build/examples/examples/21_deoccluder/Debug/cinerig_21_deoccluder.exe --frames 2 --hidden --no-vsync
```
