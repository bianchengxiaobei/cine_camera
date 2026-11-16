# 00 Sandbox

P1-05 framework demo. It exercises the shared SDL3/OpenGL layer with a
procedural ground, grid, cube, sphere, cylinder, camera frustum and HUD. The
scene is driven through `CameraSystem` and `ExternalCamera`; it is not the
P1-06 follow example.

## Controls

- `1` / `2`: switch between the two external cameras.
- `O`: toggle perspective / orthographic projection.
- Right mouse button: capture relative mouse; release to show the cursor.
- `Esc`: release mouse capture, or exit when capture is already released.
- `WASD` / arrow keys: expose input axes for later examples.

## Command line

```text
cinerig_00_sandbox [--frames N] [--hidden] [--no-vsync]
```

`--frames 2 --hidden --no-vsync` is the window/context/render smoke path.
