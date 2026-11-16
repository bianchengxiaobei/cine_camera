# 22 Decollider

P3-07 slice: a ThirdPersonFollow camera is post-processed by a `Decollider`.
The green platform under the camera rises and falls; terrain resolution lifts
the camera above it. The orange block sweeps across layer 1 and pushes the
camera out when its sphere overlaps. The grey line is the raw target-to-camera
offset and the green line is the corrected one.

Terrain uses layer 0. Decollision uses layer 1, so terrain-layer subtraction
does not disable the obstacle response. The HUD shows raw/corrected camera
positions, terrain and total correction, target distance, damping, and
smoothing.

## Controls

- `T`: toggle terrain resolution
- `C`: toggle camera decollision
- `D`: toggle damping
- `S`: toggle distance smoothing
- `R`: restore defaults and reset the camera
- `Esc`: quit

## Run

```powershell
build/examples/examples/22_decollider/Debug/cinerig_22_decollider.exe
```

Headless smoke:

```powershell
build/examples/examples/22_decollider/Debug/cinerig_22_decollider.exe --frames 2 --hidden --no-vsync
```

