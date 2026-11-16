# 08 Third Person Aim

P2-08 vertical slice: `ThirdPersonFollow` drives the shoulder camera and the
`ThirdPersonAim` extension raycasts an aim target in front of the player. The
camera either cancels rotational noise to hold the target centered or only
detects the aim point, depending on the aim mode.

## Controls

- `Space`: pause/resume target motion and rotation
- `C`: toggle built-in ThirdPersonFollow obstacle avoidance
- `N`: toggle aim noise cancellation (center stabilization)
- `R`: reset the target, collision setting, aim mode, and camera state
- `Esc`: quit

The HUD shows target/camera positions, rig state, collision status, aim mode and
aim target, controls, and frame count.

## Run

```powershell
build/examples/examples/08_third_person_aim/Debug/cinerig_08_third_person_aim.exe
```

Headless smoke:

```powershell
build/examples/examples/08_third_person_aim/Debug/cinerig_08_third_person_aim.exe --frames 2 --hidden --no-vsync
```
