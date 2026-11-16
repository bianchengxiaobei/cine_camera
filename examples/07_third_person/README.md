# 07 Third Person Follow

P2-07 vertical slice: a target moves and turns while `cinerig::ThirdPersonFollow`
keeps the camera on the shoulder rig. Obstacle avoidance is enabled by default
and uses the example `SdlPhysics` sphere cast; press `C` to compare the
unobstructed rig.

## Controls

- `Space`: pause/resume target motion and rotation
- `C`: toggle built-in obstacle avoidance
- `R`: reset the target, collision setting, and camera state
- `Esc`: quit

The HUD shows the target and camera positions, origin/shoulder/hand rig state,
collision status, rig setup, controls, and frame count.

## Run

```powershell
build/examples/examples/07_third_person/Debug/cinerig_07_third_person.exe
```

Headless smoke:

```powershell
build/examples/examples/07_third_person/Debug/cinerig_07_third_person.exe --frames 2 --hidden --no-vsync
```
