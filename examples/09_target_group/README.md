# 09 Target Group

P2-09 vertical slice: three weighted, moving members drive a
`cinerig::targets::TargetGroup`. The camera follows the group through
`TargetRef::Group`; position and rotation modes, weights, bounds, and the
group sphere are shown live.

## Controls

- `P`: toggle `GroupCenter` / `GroupAverage` position mode
- `O`: toggle `Manual` / `GroupAverage` rotation mode
- `W`: cycle weight presets (balanced, default, heavy member B)
- `Space`: pause/resume member motion
- `R`: reset modes, weights, and camera state
- `Esc`: quit

The HUD shows group position, modes, member count, bounding sphere, bounds,
weight preset, controls, and frame count.

## Run

```powershell
build/examples/examples/09_target_group/Debug/cinerig_09_target_group.exe
```

Headless smoke:

```powershell
build/examples/examples/09_target_group/Debug/cinerig_09_target_group.exe --frames 2 --hidden --no-vsync
```
