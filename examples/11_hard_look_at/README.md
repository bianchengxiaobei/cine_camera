# 11 Hard Look At

P2-11 vertical slice: `HardLookAt` points a fixed camera directly at a moving
and rotating target. The orange marker is the LookAt target plus its local
offset, so toggling the offset shows how it rotates with the target.

## Controls

- `Space`: pause/resume target motion
- `O`: toggle zero offset / target-local offset
- `R`: reset motion, offset, and camera state
- `Esc`: quit

The HUD shows target and aim-point positions, aim error, offset state, and frame
count.

## Run

```powershell
build/examples/examples/11_hard_look_at/Debug/cinerig_11_hard_look_at.exe
```

Headless smoke:

```powershell
build/examples/examples/11_hard_look_at/Debug/cinerig_11_hard_look_at.exe --frames 2 --hidden --no-vsync
```
