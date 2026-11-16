# Examples

The examples are a separate SDL3 + OpenGL 3.3 layer. SDL, GLAD and OpenGL are
linked only into `examples_common`; they never leak into `cinerig::cinerig`.

- `common/`: shared window, renderer, HUD, input, world, output and physics adapters.
- `00_sandbox/`: P1-05 framework demo (ground, procedural primitives, HUD).
- `01_follow/`: P2-02 full Follow + full RotationComposer slice (binding modes, composition presets, lookahead, moving/rotating target, damping modes, pause/reset).
- `03_blend/`: P1-07 two-camera blend demo (Wide/Close, Cut/EaseInOut/Linear/Custom, priority switching and blend HUD).
- `04_position_composer/`: P2-03 fixed-orientation PositionComposer slice (screen composition, depth dead zone, hard limits, lookahead, target offset, orthographic toggle).
- `05_orbital_follow/`: P2-04/P2-05 OrbitalFollow slice (sphere/three-ring surfaces, InputAxisController-driven orbit, radial zoom, recentering, binding modes, damping).
- `06_pan_tilt/`: P2-06 PanTilt slice (controller-driven pan/tilt, reference frames, recenter targets, rotating target).
- `07_third_person/`: P2-07 ThirdPersonFollow slice (shoulder rig, local damping, obstacle avoidance, rig visualization, moving/rotating target).
- `08_third_person_aim/`: P2-08 ThirdPersonAim slice (aim raycast, aim marker, noise-cancellation toggle, combined with ThirdPersonFollow).
- `09_target_group/`: P2-09 TargetGroup slice (weighted members, center/average position, manual/average rotation, bounds and sphere visualization).
- `10_hard_lock/`: P2-10 HardLockToTarget slice (tool-target lock, lock-error HUD, damping toggle, separate look-at point).
- `11_hard_look_at/`: P2-11 HardLookAt slice (fixed camera, moving/rotating target, target-local offset marker, aim-error HUD).
- `12_rotate_with_follow/`: P2-12 RotateWithFollowTarget slice (Follow chase rig, moving/banking target, orientation damping and error HUD).
- `13_camera_offset/`: P2-13 CameraOffset slice (camera-space offset, preserve composition, Aim/Finalize stage selection, parallax references).
- `14_recomposer/`: P2-14 Recomposer slice (interactive tilt/pan/dutch/zoom, relaxed target attachments, tilted composition and HUD).
- `15_free_look_modifier/`: P2-15 FreeLookModifier slice (OrbitalFollow vertical sweep, eased top/bottom distance, damping, composition, FOV and tilt).
- `16_follow_zoom/`: P3-01 FollowZoom slice (depth sweep, constant target/gauge ratio, FOV range, damping/width controls).
- `17_group_framing/`: P3-02 GroupFraming slice (four moving members, weighted bounds, framing/size/lateral modes, orthographic toggle).
- `18_noise/`: P3-03 BasicMultiChannelPerlin slice (position/orientation noise, correction HUD, gains, pivot, freeze, re-seed).
- `19_impulse/`: P3-04 impulse core slice (propagating shapes, source/listener, auto fire, secondary reaction, wave visualization).
- `20_collision_impulse/`: P3-05 collision source slice (wall/trigger contacts, layer/tag filters, direction/mass/speed scaling, wave rings).
- `21_deoccluder/`: P3-06 Deoccluder slice (sliding occluder, transparent glass, resolution strategies, damping, shot-quality HUD).
- `22_decollider/`: P3-07 Decollider slice (terrain lift, camera overlap push-out, smoothing/damping, correction HUD).
- `23_confiner3d/`: P3-08 Confiner3D slice (box/sphere/no volume, edge slowing, displacement HUD).
- `24_confiner2d/`: P4-02 Confiner2D slice (overview camera, L-room/box/no shape, confined screen footprint, damping/slowing).
- `25_spline/`: P4-03 spline abstractions slice (smooth closed Bezier spline, knots, arc-length units, tangent/up/right frame).
- `26_spline_dolly/`: P4-04 SplineDolly slice (nearest-target/manual dolly, spline-local offset, unit/damping controls, overview frustum).
- `27_spline_cart/`: P4-05 SplineCart slice (entity cart on a closed spline, auto/manual motion, update-method and unit controls).
- `28_spline_roll/`: P4-06 SplineRoll slice (eased roll profile, rolled dolly frustum and cart, pause/easing/roll toggles).
- `29_spline_look_at/`: P4-07 SplineDollyLookAtTargets slice (keyed aim targets, per-item easing, target markers and aim frustum).
- `30_spline_smoother/`: P4-08 SplineSmoother slice (raw versus smoothed path, mirrored tangent handles, knot frames and smoothing toggle).
- `31_mixing_camera/`: P4-09 MixingCamera slice (three orbiting children, weighted state averaging, child/mixed frustums and weight presets).
- `32_clear_shot/`: P4-10 ClearShot slice (quality-evaluated shots, wall occlusion, activation timing, deterministic randomization and live HUD).
- `33_sequencer/`: P4-11 SequencerCamera slice (three-shot instruction list, per-shot blends/holds, looping, timing controls and live HUD).
- `34_state_driven/`: P4-12 StateDrivenCamera slice (simulated Idle/Walk/Run/Jump states, four mapped shots, activation rules, pending waits and live HUD).
- `35_timeline/`: P5-01 timeline track slice (looping clips, overlapping blends, track priority, gameplay fallback, live clip weights and HUD).
- `36_post_processing/`: P5-02 post-processing slice (opaque profiles, blended profile weights, focus tracking, auto-focus damping, adapter-driven tint and HUD).
- `37_storyboard/`: P5-03 storyboard slice (opaque images, owner-only overlay blendables, blended weights, BestFit/Crop/Stretch, split view, camera mute, global mute and a textured screen-space adapter).
- `38_pixel_perfect/`: P5-04 pixel-perfect slice (reference resolution, crop frames, grid snapping, live orthographic-size correction, final-blit math, a 2D pixel-art test card and HUD).

## Build

Examples are disabled by default. `CINERIG_BUILD_EXAMPLES=ON` enables SDL3
automatically; SDL3 is fetched at configure time by default.

```powershell
cmake -S . -B build/examples -G "Visual Studio 17 2022" -A x64 -DCINERIG_BUILD_EXAMPLES=ON
cmake --build build/examples --config Debug
ctest --test-dir build/examples -C Debug --output-on-failure
```

Linux with Ninja:

```bash
cmake -S . -B build/examples -G Ninja -DCINERIG_BUILD_EXAMPLES=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/examples
ctest --test-dir build/examples --output-on-failure
```

### Dependencies

- SDL3: FetchContent tag `release-3.2.10` by default.
  Use `-DCINERIG_USE_SYSTEM_SDL3=ON` for an installed SDL3 package.
- GLAD: vendored OpenGL 3.3 core loader in `third_party/glad/`.
- stb_easy_font: vendored single header in `third_party/stb/`.

### Smoke test

A windowed smoke test is available but disabled by default because CI machines
may not provide an OpenGL context:

```powershell
cmake -S . -B build/examples -G "Visual Studio 17 2022" -A x64 `
  -DCINERIG_BUILD_EXAMPLES=ON -DCINERIG_ENABLE_EXAMPLE_SMOKE_TESTS=ON
cmake --build build/examples --config Debug
ctest --test-dir build/examples -C Debug -R "example_.*_smoke" --output-on-failure
```

The same executable can be run directly:

```powershell
build/examples/examples/00_sandbox/Debug/cinerig_00_sandbox.exe --frames 2 --hidden --no-vsync
build/examples/examples/01_follow/Debug/cinerig_01_follow.exe --frames 2 --hidden --no-vsync
build/examples/examples/03_blend/Debug/cinerig_03_blend.exe --frames 2 --hidden --no-vsync
build/examples/examples/04_position_composer/Debug/cinerig_04_position_composer.exe --frames 2 --hidden --no-vsync
build/examples/examples/05_orbital_follow/Debug/cinerig_05_orbital_follow.exe --frames 2 --hidden --no-vsync
build/examples/examples/06_pan_tilt/Debug/cinerig_06_pan_tilt.exe --frames 2 --hidden --no-vsync
build/examples/examples/07_third_person/Debug/cinerig_07_third_person.exe --frames 2 --hidden --no-vsync
build/examples/examples/08_third_person_aim/Debug/cinerig_08_third_person_aim.exe --frames 2 --hidden --no-vsync
build/examples/examples/09_target_group/Debug/cinerig_09_target_group.exe --frames 2 --hidden --no-vsync
build/examples/examples/10_hard_lock/Debug/cinerig_10_hard_lock.exe --frames 2 --hidden --no-vsync
build/examples/examples/11_hard_look_at/Debug/cinerig_11_hard_look_at.exe --frames 2 --hidden --no-vsync
build/examples/examples/12_rotate_with_follow/Debug/cinerig_12_rotate_with_follow.exe --frames 2 --hidden --no-vsync
build/examples/examples/13_camera_offset/Debug/cinerig_13_camera_offset.exe --frames 2 --hidden --no-vsync
build/examples/examples/14_recomposer/Debug/cinerig_14_recomposer.exe --frames 2 --hidden --no-vsync
build/examples/examples/15_free_look_modifier/Debug/cinerig_15_free_look_modifier.exe --frames 2 --hidden --no-vsync
build/examples/examples/16_follow_zoom/Debug/cinerig_16_follow_zoom.exe --frames 2 --hidden --no-vsync
build/examples/examples/17_group_framing/Debug/cinerig_17_group_framing.exe --frames 2 --hidden --no-vsync
build/examples/examples/18_noise/Debug/cinerig_18_noise.exe --frames 2 --hidden --no-vsync
build/examples/examples/19_impulse/Debug/cinerig_19_impulse.exe --frames 2 --hidden --no-vsync
build/examples/examples/20_collision_impulse/Debug/cinerig_20_collision_impulse.exe --frames 2 --hidden --no-vsync
build/examples/examples/21_deoccluder/Debug/cinerig_21_deoccluder.exe --frames 2 --hidden --no-vsync
build/examples/examples/22_decollider/Debug/cinerig_22_decollider.exe --frames 2 --hidden --no-vsync
build/examples/examples/23_confiner3d/Debug/cinerig_23_confiner3d.exe --frames 2 --hidden --no-vsync
build/examples/examples/24_confiner2d/Debug/cinerig_24_confiner2d.exe --frames 2 --hidden --no-vsync
build/examples/examples/25_spline/Debug/cinerig_25_spline.exe --frames 2 --hidden --no-vsync
build/examples/examples/26_spline_dolly/Debug/cinerig_26_spline_dolly.exe --frames 2 --hidden --no-vsync
build/examples/examples/27_spline_cart/Debug/cinerig_27_spline_cart.exe --frames 2 --hidden --no-vsync
build/examples/examples/28_spline_roll/Debug/cinerig_28_spline_roll.exe --frames 2 --hidden --no-vsync
build/examples/examples/29_spline_look_at/Debug/cinerig_29_spline_look_at.exe --frames 2 --hidden --no-vsync
build/examples/examples/30_spline_smoother/Debug/cinerig_30_spline_smoother.exe --frames 2 --hidden --no-vsync
build/examples/examples/31_mixing_camera/Debug/cinerig_31_mixing_camera.exe --frames 2 --hidden --no-vsync
build/examples/examples/32_clear_shot/Debug/cinerig_32_clear_shot.exe --frames 2 --hidden --no-vsync
build/examples/examples/33_sequencer/Debug/cinerig_33_sequencer.exe --frames 2 --hidden --no-vsync
build/examples/examples/34_state_driven/Debug/cinerig_34_state_driven.exe --frames 2 --hidden --no-vsync
build/examples/examples/35_timeline/Debug/cinerig_35_timeline.exe --frames 2 --hidden --no-vsync
build/examples/examples/36_post_processing/Debug/cinerig_36_post_processing.exe --frames 2 --hidden --no-vsync
build/examples/examples/37_storyboard/Debug/cinerig_37_storyboard.exe --frames 2 --hidden --no-vsync
build/examples/examples/38_pixel_perfect/Debug/cinerig_38_pixel_perfect.exe --frames 2 --hidden --no-vsync
```

`32_clear_shot`, `33_sequencer`, `34_state_driven`, `35_timeline`,
`36_post_processing`, `37_storyboard`, and `38_pixel_perfect` also accept
`--capture file.bmp` to save the rendered framebuffer without depending on
desktop window stacking.

## Coordinates and matrices

- Core and OpenGL both use a right-handed system with `+Y` up and `-Z` forward.
- View: `inverse(translation(finalPosition) * rotation(finalOrientation))`.
- Perspective: vertical FOV from `LensSettings::fieldOfView`, `aspect = width / height`.
- Orthographic: `orthographicSize` is half-height; half-width is
  `orthographicSize * aspect`.
- Dutch is already included in `CameraState::finalOrientation()`.
- Physical lens fields are exposed but P1 does not render depth of field or
  physical lens effects.

















