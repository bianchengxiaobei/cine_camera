# 16 Follow Zoom

P3-01 vertical slice: `FollowZoom` adjusts the perspective FOV at the Body stage
so the look-at target keeps a constant apparent size while it moves from near
to far. The orange gauge spans `Width` world units at the target plane; the
target cube keeps a constant fraction of that gauge.

## Controls

- `W` / `S`: increase or decrease the shot width
- `D`: toggle smoothed / instant response
- `Space`: pause or resume the depth sweep
- `R`: reset width, damping, path, and camera state
- `Esc`: quit

The HUD shows target distance, configured width, current FOV and its range,
damping mode, and the motion state.

## Run

```powershell
build/examples/examples/16_follow_zoom/Debug/cinerig_16_follow_zoom.exe
```

Headless smoke:

```powershell
build/examples/examples/16_follow_zoom/Debug/cinerig_16_follow_zoom.exe --frames 2 --hidden --no-vsync
```
