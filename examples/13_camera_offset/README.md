# 13 Camera Offset

P2-13 vertical slice: `CameraOffset` adds a camera-space offset after Aim. With
composition preservation on, the camera re-aims so the moving target stays
centered while the viewpoint shifts; turning it off shows the raw positional
offset. The apply-after stage can be switched between Aim and Finalize.

## Controls

- `Space`: pause/resume target motion
- `O`: toggle zero offset / `(1.4, 0.8, 0)` camera-space offset
- `P`: toggle composition preservation
- `F`: switch apply-after between Aim and Finalize
- `R`: reset motion, offset, stage, preservation, and camera state
- `Esc`: quit

The HUD shows the offset, apply-after stage, preservation mode, target position,
aim screen error, and frame count.

## Run

```powershell
build/examples/examples/13_camera_offset/Debug/cinerig_13_camera_offset.exe
```

Headless smoke:

```powershell
build/examples/examples/13_camera_offset/Debug/cinerig_13_camera_offset.exe --frames 2 --hidden --no-vsync
```
