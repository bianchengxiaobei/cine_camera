# 15 Free Look Modifier

P2-15 vertical slice: `FreeLookModifier` reads the OrbitalFollow vertical axis
and blends camera distance, position damping, screen composition, FOV, and tilt
between bottom and top settings. The vertical axis sweeps automatically so the
full rig range is visible.

## Controls

- `Space`: pause/resume the vertical-axis sweep
- `E`: toggle linear / eased response
- `R`: reset the sweep, easing, and camera state
- `Esc`: quit

The HUD shows the raw vertical axis, normalized and eased values, top/base/bottom
distance and damping, base/top/bottom FOV with the current lens, and easing mode.

## Run

```powershell
build/examples/examples/15_free_look_modifier/Debug/cinerig_15_free_look_modifier.exe
```

Headless smoke:

```powershell
build/examples/examples/15_free_look_modifier/Debug/cinerig_15_free_look_modifier.exe --frames 2 --hidden --no-vsync
```
