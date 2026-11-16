# 17 Group Framing

P3-02 vertical slice: `targets::GroupFraming` fits a four-member moving target
group into the requested screen area. The orange box is the weighted group
bounds, and the central marker is the group framing target.

## Controls

- `M`: cycle horizontal / vertical / horizontal-and-vertical framing
- `S`: cycle zoom-only / dolly-only / dolly-then-zoom
- `L`: toggle change-position / change-rotation lateral framing
- `D`: toggle damping between 2 and 0 seconds
- `O`: toggle perspective / orthographic framing
- `Up` / `Down`: adjust framing size
- `Space`: pause or resume member motion
- `R`: reset modes, size, path, camera, and output
- `Esc`: quit

The HUD shows member count, group center and bounds, camera distance, framing
modes, framing size, damping, and the active lens settings.

## Confiner2D compatibility

When an enabled `components::Confiner2D` extension is attached to the same
camera, `GroupFraming` automatically selects the Body stage instead of its usual
Aim stage. If framing changes the orthographic size, it invalidates the
Confiner2D lens cache so the next confiner pass bakes against the new frustum.
Attach or detach the confiner before the first update, or call
`GroupFraming::reset()` after changing the extension set.

## Run

```powershell
build/examples/examples/17_group_framing/Debug/cinerig_17_group_framing.exe
```

Headless smoke:

```powershell
build/examples/examples/17_group_framing/Debug/cinerig_17_group_framing.exe --frames 2 --hidden --no-vsync
```
