#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <cinerig/cinerig.hpp>

#include "app.hpp"
#include "sdl_camera_output.hpp"
#include "sdl_world.hpp"

namespace
{

struct Options
{
    int frames = 0;
    bool hidden = false;
    bool vsync = true;
};

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--frames" && index + 1 < argc)
            options.frames = std::max(1, std::atoi(argv[++index]));
        else if (argument == "--hidden")
            options.hidden = true;
        else if (argument == "--no-vsync")
            options.vsync = false;
        else if (argument == "--help")
        {
            std::cout << "Usage: cinerig_31_mixing_camera [--frames N] "
                         "[--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

struct ChildCamera
{
    cinerig::CameraId id = 0;
    cinerig::EntityId entity = 0;
    cinerig::math::Vec3 color{};
    float phase = 0.0f;
    float radius = 7.0f;
    float height = 3.0f;
};

void applyPreset(
    cinerig::managers::MixingCamera& manager,
    float a,
    float b,
    float c)
{
    manager.setWeight(0, a);
    manager.setWeight(1, b);
    manager.setWeight(2, c);
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app(
        "CineRig P4-09 Mixing Camera", 1280, 720, appOptions);
    if (!app.ok())
    {
        std::cerr << app.error() << '\n';
        return 1;
    }

    cinerig::examples::SdlWorld world;
    world.setViewportSize(app.viewportSize());

    cinerig::examples::SdlCameraOutput output;
    output.setViewportSize(app.viewportSize());

    cinerig::CameraSystem cameraSystem(world);
    cinerig::BrainConfig brainConfig;
    brainConfig.defaultBlend = {cinerig::BlendStyle::Cut, 0.0f};
    cameraSystem.createBrain(output, brainConfig);

    const cinerig::math::Vec3 overviewPosition{0.0f, 16.0f, 20.0f};
    const cinerig::math::Vec3 overviewDirection = cinerig::math::normalize(
        cinerig::math::Vec3{0.0f, 1.0f, 0.0f} - overviewPosition);
    const cinerig::EntityId overviewEntity = world.createEntity({
        overviewPosition,
        cinerig::math::lookRotation(overviewDirection, cinerig::math::Vec3::up),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId overviewId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Overview");
    auto* overview = cameraSystem.camera(overviewId);
    if (overview == nullptr)
    {
        std::cerr << "Failed to create overview camera\n";
        return 1;
    }
    overview->ownerEntity = overviewEntity;
    overview->priority = cinerig::PrioritySettings(1000);
    overview->lens.fieldOfView = 42.0f;
    overview->lens.nearClipPlane = 0.1f;
    overview->lens.farClipPlane = 100.0f;
    overview->lens.aspect = app.viewportSize().x / app.viewportSize().y;
    overview->standbyUpdate = cinerig::StandbyUpdateMode::Always;

    output.setDefaultLens(overview->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::CameraId managerId =
        cameraSystem.createCamera<cinerig::managers::MixingCamera>("Mixing");
    auto* manager =
        static_cast<cinerig::managers::MixingCamera*>(
            cameraSystem.camera(managerId));
    if (manager == nullptr)
    {
        std::cerr << "Failed to create mixing camera\n";
        return 1;
    }
    manager->priority = cinerig::PrioritySettings(100);
    manager->standbyUpdate = cinerig::StandbyUpdateMode::Always;
    manager->reset();
    applyPreset(*manager, 0.5f, 0.3f, 0.2f);

    std::vector<ChildCamera> children;
    children.push_back({
        0,
        0,
        {1.0f, 0.48f, 0.18f},
        0.0f,
        7.5f,
        3.0f,
    });
    children.push_back({
        0,
        0,
        {0.22f, 0.82f, 1.0f},
        2.1f,
        8.5f,
        5.0f,
    });
    children.push_back({
        0,
        0,
        {0.30f, 0.92f, 0.52f},
        4.2f,
        6.5f,
        2.0f,
    });

    const float fieldsOfView[] = {32.0f, 55.0f, 42.0f};
    for (std::size_t index = 0; index < children.size(); ++index)
    {
        ChildCamera& child = children[index];
        child.entity = world.createEntity(cinerig::Transform{
            {child.radius, child.height, 0.0f},
            cinerig::math::Quat::identity(),
            cinerig::math::Vec3::one});
        const std::string name =
            std::string("Child ") + static_cast<char>('A' + index);
        child.id = cameraSystem.createCamera<cinerig::VirtualCamera>(name);
        auto* camera = cameraSystem.camera(child.id);
        camera->ownerEntity = child.entity;
        camera->lens.fieldOfView = fieldsOfView[index];
        camera->lens.nearClipPlane = 0.1f;
        camera->lens.farClipPlane = 100.0f;
        camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;
        camera->standbyUpdate = cinerig::StandbyUpdateMode::Always;
        if (!manager->addChild(*camera))
        {
            std::cerr << "Failed to add mixing child\n";
            return 1;
        }
    }

    bool paused = false;
    bool previousSpace = false;
    bool previousReset = false;
    bool previousKeys[6] = {};
    float orbitTime = 0.0f;

    const cinerig::math::Vec3 lookTarget{0.0f, 1.0f, 0.0f};
    app.hud().setTextScale(1.5f);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle childMesh =
        app.renderer().createSphere(0.38f);
    const cinerig::examples::GlRenderer::MeshHandle mixedMesh =
        app.renderer().createSphere(0.55f);

    while (app.running())
    {
        app.pollEvents();
        if (!app.running())
            break;

        const cinerig::math::Vec2 viewport = app.viewportSize();
        const float deltaTime = app.deltaTime();
        const float aspect = viewport.y > 0.0f ? viewport.x / viewport.y : 1.0f;

        world.setViewportSize(viewport);
        world.setDeltaTime(cinerig::UpdateClock::Late, deltaTime);
        world.setTime(app.elapsedTime());
        world.setFrame(cinerig::UpdateClock::Late, app.frameCount());
        output.setViewportSize(viewport);
        output.setAspect(aspect);
        overview->lens.aspect = aspect;
        for (const ChildCamera& child : children)
        {
            if (auto* camera = cameraSystem.camera(child.id))
                camera->lens.aspect = aspect;
        }

        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);
        if (spaceDown && !previousSpace)
            paused = !paused;
        if (resetDown && !previousReset)
        {
            paused = false;
            orbitTime = 0.0f;
            manager->reset();
            applyPreset(*manager, 0.5f, 0.3f, 0.2f);
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousSpace = spaceDown;
        previousReset = resetDown;

        const SDL_Scancode presetKeys[] = {
            SDL_SCANCODE_0,
            SDL_SCANCODE_1,
            SDL_SCANCODE_2,
            SDL_SCANCODE_3,
            SDL_SCANCODE_4,
            SDL_SCANCODE_5,
        };
        for (int index = 0; index < 6; ++index)
        {
            const bool down = app.input().keyDown(presetKeys[index]);
            if (!down || previousKeys[index])
            {
                previousKeys[index] = down;
                continue;
            }
            previousKeys[index] = true;
            switch (index)
            {
            case 0:
                applyPreset(*manager, 1.0f, 1.0f, 1.0f);
                break;
            case 1:
                applyPreset(*manager, 1.0f, 0.0f, 0.0f);
                break;
            case 2:
                applyPreset(*manager, 0.0f, 1.0f, 0.0f);
                break;
            case 3:
                applyPreset(*manager, 0.0f, 0.0f, 1.0f);
                break;
            case 4:
                applyPreset(*manager, 0.5f, 0.3f, 0.2f);
                break;
            case 5:
                applyPreset(*manager, 0.2f, 0.3f, 0.5f);
                break;
            }
        }

        if (!paused)
            orbitTime += deltaTime * 0.18f;

        for (const ChildCamera& child : children)
        {
            const float angle = orbitTime + child.phase;
            const cinerig::math::Vec3 position{
                std::cos(angle) * child.radius,
                child.height,
                std::sin(angle) * child.radius,
            };
            const cinerig::math::Vec3 direction = cinerig::math::normalize(
                lookTarget - position);
            world.setTransform(
                child.entity,
                cinerig::Transform{
                    position,
                    cinerig::math::lookRotation(
                        direction,
                        cinerig::math::Vec3::up),
                    cinerig::math::Vec3::one,
                });
        }

        cameraSystem.lateUpdate(deltaTime);

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.02f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.11f, 0.14f, 0.18f});
        app.renderer().drawGrid(26.0f, 1.0f, {0.20f, 0.24f, 0.30f});

        app.renderer().drawTargetMarker(
            lookTarget,
            0.9f,
            {0.35f, 0.95f, 0.55f});
        app.renderer().setLineWidth(1.3f);
        for (const ChildCamera& child : children)
        {
            auto* camera = cameraSystem.camera(child.id);
            if (camera == nullptr)
                continue;
            const cinerig::CameraState& state = camera->state();
            cinerig::LensSettings lens = state.lens;
            lens.farClipPlane = 2.2f;
            app.renderer().drawFrustum(
                state.finalPosition(),
                state.finalOrientation(),
                lens,
                aspect,
                child.color);
            app.renderer().drawMesh(
                childMesh,
                cinerig::Transform{
                    state.finalPosition(),
                    cinerig::math::Quat::identity(),
                    cinerig::math::Vec3::one},
                child.color);
        }

        const cinerig::CameraState& mixed = manager->state();
        cinerig::LensSettings mixedLens = mixed.lens;
        mixedLens.farClipPlane = 3.0f;
        app.renderer().setLineWidth(2.4f);
        app.renderer().drawFrustum(
            mixed.finalPosition(),
            mixed.finalOrientation(),
            mixedLens,
            aspect,
            {0.78f, 0.42f, 1.0f});
        app.renderer().drawAxes(
            cinerig::Transform{
                mixed.finalPosition(),
                mixed.finalOrientation(),
                cinerig::math::Vec3::one},
            1.4f);
        app.renderer().drawMesh(
            mixedMesh,
            cinerig::Transform{
                mixed.finalPosition(),
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.78f, 0.42f, 1.0f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine(
            "CineRig P4-09 mixing camera   frame "
            + std::to_string(app.frameCount()));
        std::ostringstream weightsLine;
        weightsLine << "A " << manager->getWeight(0)
                    << "  B " << manager->getWeight(1)
                    << "  C " << manager->getWeight(2);
        app.hud().addLine("Weights", weightsLine.str());
        std::ostringstream mixedLine;
        mixedLine << "pos " << mixed.finalPosition().x << ", "
                  << mixed.finalPosition().y << ", "
                  << mixed.finalPosition().z
                  << "  fov " << mixed.lens.fieldOfView;
        app.hud().addLine("Mixed", mixedLine.str());
        std::ostringstream liveLine;
        liveLine << (manager->liveChild() != nullptr
                ? std::string(manager->liveChild()->name())
                : std::string("(none)"))
                 << "  " << manager->liveChildPercent() << "%"
                 << "  children " << manager->childCameras().size()
                 << " / 8 slots";
        app.hud().addLine("Live", liveLine.str());
        app.hud().addLine(
            "Controls",
            "0 equal  1 A  2 B  3 C  4 mix  5 alt  Space pause  R reset");
        app.hud().addLine(
            "Legend",
            "orange/cyan/green children  magenta mixed  green target");
        app.hud().render(
            static_cast<int>(viewport.x),
            static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0
            && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
        {
            break;
        }
    }

    return 0;
}
