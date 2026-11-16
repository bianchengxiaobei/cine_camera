# 24 Confiner 2D

P4-02 slice: an orthographic camera follows a target over an XY plane while
`Confiner2D` keeps the camera's viewport inside a closed polygon. The green
rectangle is the confined screen footprint, the grey rectangle is the desired
footprint, and the cyan outline is the bounding shape.

The default L-room is concave. `V` cycles the L-room, a box, and no confining
shape. `D` toggles corner damping and `S` toggles edge slowing.

## Controls

- `V`: cycle L-room, box, and no shape
- `D`: toggle corner damping
- `S`: toggle edge slowing
- `R`: restore the L-room and defaults
- `Esc`: quit

## Run

```powershell
build/examples/examples/24_confiner2d/Debug/cinerig_24_confiner2d.exe
```

Headless smoke:

```powershell
build/examples/examples/24_confiner2d/Debug/cinerig_24_confiner2d.exe --frames 2 --hidden --no-vsync
```
