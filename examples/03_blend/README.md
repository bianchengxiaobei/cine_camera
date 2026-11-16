# 03 Blend

Two `cinerig::VirtualCamera` instances follow the same moving target and blend
through `cinerig::CameraBrain`. The `Wide` camera uses a 55 degree lens and a
distant offset; the `Close` camera uses a 35 degree lens and a closer, slightly
offset view. Both use `cinerig::Follow` and `cinerig::RotationComposer`.

The scene contains a ground grid, reference pillars and cubes, a moving target,
and frustums for both cameras. The active camera is highlighted.

## Controls

- `1`: switch to Wide
- `2`: switch to Close
- `C`: use Cut for the next switch
- `L`: use Linear 2s for the next switch
- `E`: use EaseInOut 2s for the next switch
- `T`: use a custom slow-in/fast-out curve for the next switch
- `Space`: pause/resume the target
- `Esc`: quit

The HUD shows the active camera, whether a blend is in progress, the current
weight, the outgoing and incoming cameras, the current/next blend style,
FOV, target position and frame statistics.

Switching back during a blend reverses and shortens that blend, matching the
Cinemachine `BlendManager` / `CameraBlendStack` behavior. A third camera can
produce a nested blend; the P1 example intentionally uses the two cameras
specified by the task.

## Run

```powershell
build/examples/examples/03_blend/Debug/cinerig_03_blend.exe
```

Headless smoke:

```powershell
build/examples/examples/03_blend/Debug/cinerig_03_blend.exe --frames 2 --hidden --no-vsync
```
