# 19 Impulse

P3-04 slice: the `ImpulseManager` owns propagating impulse events created by an
`ImpulseSource`; an `ImpulseListener` camera extension retrieves them at the
Noise stage, applies gain/camera-space conversion, and adds a secondary noise
reaction. P6-05 adds an `ExternalImpulseListener` that shakes the orange prop
through the same manager without a camera extension. The expanding ground ring
shows the propagated wave front.

## Controls

- `1` / `2` / `3` / `4`: select and fire Recoil, Bump, Explosion, or Rumble
- `Space`: fire the current shape
- `A`: toggle automatic fire
- `C`: clear all active impulses
- `R`: restore defaults and clear impulses
- `Esc`: quit

The HUD shows the active event count, recycled event count, impulse correction,
listener settings, the external prop's applied position/rotation offset, and
automatic-fire state. The orange prop's base position is marked on the ground.

## Run

```powershell
build/examples/examples/19_impulse/Debug/cinerig_19_impulse.exe
```

Headless smoke:

```powershell
build/examples/examples/19_impulse/Debug/cinerig_19_impulse.exe --frames 2 --hidden --no-vsync
```
