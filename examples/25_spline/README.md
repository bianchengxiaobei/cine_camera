# 25 Spline

P4-03 slice: a smooth closed `BezierSpline` is sampled through
`SplineSettings` while a marker travels along it. The cyan curve is the
arc-length-parameterized spline, yellow markers are knots, and the grey lines
show the control polygon.

The HUD reports the current position in the selected unit plus the equivalent
normalized, distance, and knot values. Press `U` to cycle units without moving
the marker. The orange, green, and blue rays at the marker show the spline
tangent, up, and right axes.

## Controls

- `U`: cycle distance, normalized, and knot units
- `Space`: pause or resume the marker
- `R`: reset the marker and units
- `Esc`: quit

## Run

```powershell
build/examples/examples/25_spline/Debug/cinerig_25_spline.exe
```

Headless smoke:

```powershell
build/examples/examples/25_spline/Debug/cinerig_25_spline.exe --frames 2 --hidden --no-vsync
```
