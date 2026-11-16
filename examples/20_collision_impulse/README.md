# 20 Collision Impulse

P3-05 slice: a host-simulated ball collides with two walls and crosses a trigger
zone. Each contact is translated into a `CollisionImpulseSource::ImpactInfo` and
passed to `onCollision()` or `onTrigger()`. The source applies the layer/tag
filter and optional impact direction, mass, and speed scaling, then generates a
normal propagating `ImpulseEvent`.

The blue left wall is layer 0 and the orange right wall is layer 1. The trigger
zone uses layer 0 and the `NoImpulse` tag. The default filter accepts layer 0, so
left-wall collisions and trigger crossings generate impulses while right-wall
collisions are rejected.

## Controls

- `L`: toggle the layer mask between layer 0 and all layers
- `T`: toggle ignoring the `NoImpulse` tag
- `D`: toggle impact-direction generation
- `M`: toggle impact scaling by source and other-body mass
- `S`: toggle impact scaling by `sqrt(speed)`
- `R`: restore defaults, clear impulses, reset the ball and camera
- `Esc`: quit

The HUD shows the filter, scaling flags, accepted/rejected collision and trigger
counts, active/recycled impulse events, and the current camera correction. The
orange ring is a wall impact wave; the cyan ring is a trigger wave.

## Run

```powershell
build/examples/examples/20_collision_impulse/Debug/cinerig_20_collision_impulse.exe
```

Headless smoke:

```powershell
build/examples/examples/20_collision_impulse/Debug/cinerig_20_collision_impulse.exe --frames 2 --hidden --no-vsync
```
