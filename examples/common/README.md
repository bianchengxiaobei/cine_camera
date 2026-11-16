# Example Common Layer

Shared SDL3/OpenGL infrastructure for the example applications.

| File | Responsibility |
| --- | --- |
| `app.hpp/cpp` | SDL3 window, OpenGL 3.3 context, timing, events, mouse capture, frame loop |
| `gl_renderer.hpp/cpp` | Procedural meshes, mesh/line shaders, grid, axes, camera frustum |
| `hud.hpp/cpp` | stb_easy_font text and a translucent status panel |
| `sdl_world.hpp/cpp` | `IEntityWorld`: entities, transforms, update clocks, frames, viewport |
| `sdl_camera_output.hpp/cpp` | `ICameraOutput`: OpenGL view/projection matrices from `AppliedCamera` |
| `sdl_input.hpp/cpp` | `IInputSource`: mouse, WASD/arrows, scroll, capture |
| `sdl_physics.hpp/cpp` | AABB/sphere raycast and overlap skeleton for P3 |
| `shaders/` | GLSL 3.30 basic mesh and debug-line programs |

The header-only `cinerig_examples.hpp` includes the public common API.

## Minimal loop

```cpp
cinerig::examples::App app("Demo", 1280, 720);
cinerig::examples::SdlWorld world;
cinerig::examples::SdlCameraOutput output;
cinerig::CameraSystem system(world);
const auto brain = system.createBrain(output, {});

while (app.running())
{
    app.pollEvents();
    world.setViewportSize(app.viewportSize());
    world.setDeltaTime(cinerig::UpdateClock::Late, app.deltaTime());
    system.lateUpdate(app.deltaTime());

    app.beginFrame();
    app.renderer().beginFrame(output);
    app.renderer().drawGrid();
    app.renderer().endFrame();

    app.hud().begin();
    app.hud().addLine("Hello CineRig");
    app.hud().render(static_cast<int>(app.viewportSize().x),
                     static_cast<int>(app.viewportSize().y));
    app.endFrame();
}
```

`SdlCameraOutput` is intentionally independent of SDL. It can be used in
unit tests without creating a window. `SdlWorld` is likewise window-free.
