# 06 Pan Tilt

P2-06 vertical slice: a fixed-position camera uses `cinerig::PanTilt` to aim
around a rotating target. Pan and tilt are driven by `InputAxisController`;
reference frames and recentering targets can be switched live.

## Controls

- `A`/`D` or left/right: pan
- `W`/`S` or up/down: tilt
- `F`: cycle `ParentObject`, `World`, `TrackingTarget`, `LookAtTarget`
- `V`: cycle recenter target (`axis center`, `tracking forward`, `look-at forward`)
- `C`: toggle pan/tilt recentering
- `Space`: pause/resume target rotation
- `R`: reset pan, tilt, reference frame, recentering, and target
- `Esc`: quit

The HUD shows camera state, pan/tilt values, reference frame, recentering mode,
input source, controls, and frame count.

## Run

```powershell
build/examples/examples/06_pan_tilt/Debug/cinerig_06_pan_tilt.exe
```

Headless smoke:

```powershell
build/examples/examples/06_pan_tilt/Debug/cinerig_06_pan_tilt.exe --frames 2 --hidden --no-vsync
```
