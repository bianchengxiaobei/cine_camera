# 18 Noise

P3-03 slice: `BasicMultiChannelPerlin` adds two position channels and two
orientation channels from a `NoiseProfile` at the Noise stage. The camera is
framed on a static target by `HardLookAt`; the orange marker and line show the
uncorrected camera position, while the grey and orange rays show raw and noisy
forward directions.

## Controls

- `A` / `D`: decrease or increase amplitude gain
- `F` / `G`: decrease or increase frequency gain
- `P`: toggle a two-unit forward pivot offset
- `Space`: freeze or resume noise sampling
- `R`: restore defaults and re-seed
- `Esc`: quit

The HUD shows amplitude/frequency gain, position and orientation correction,
noise time, seed, pivot mode, and motion state.

## Run

```powershell
build/examples/examples/18_noise/Debug/cinerig_18_noise.exe
```

Headless smoke:

```powershell
build/examples/examples/18_noise/Debug/cinerig_18_noise.exe --frames 2 --hidden --no-vsync
```
