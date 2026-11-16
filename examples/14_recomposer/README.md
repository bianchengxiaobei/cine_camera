# 14 Recomposer

P2-14 vertical slice: `Recomposer` applies a final tilt, pan, dutch, and zoom
adjustment after the Aim composer. The example starts with a combined
recomposition and lets you adjust each term interactively; reducing target
attachment shows the effect on tracking.

## Controls

- `Left`/`Right`: adjust pan
- `Up`/`Down`: adjust tilt
- `Q`/`E`: adjust dutch
- `Z`/`X`: adjust zoom scale
- `A`: toggle full / relaxed target attachments
- `Space`: pause/resume target motion
- `R`: reset angles, zoom, attachments, and camera state
- `Esc`: quit

The HUD shows tilt/pan/dutch, zoom scale and resulting FOV, attachments, target
motion state, and frame count.

## Run

```powershell
build/examples/examples/14_recomposer/Debug/cinerig_14_recomposer.exe
```

Headless smoke:

```powershell
build/examples/examples/14_recomposer/Debug/cinerig_14_recomposer.exe --frames 2 --hidden --no-vsync
```
