#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

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
            std::cout << "Usage: cinerig_10_hard_lock [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 targetPositionAt(float time)
{
    return {
        5.0f * std::sin(time * 0.55f),
        1.5f + 0.5f * std::sin(time * 1.1f),
        5.0f * std::cos(time * 0.55f),
    };
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P2-10 HardLockToTarget", 1280, 720, appOptions);
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
    const cinerig::BrainId brainId = cameraSystem.createBrain(output, brainConfig);

    const cinerig::math::Vec3 initialTarget = targetPositionAt(0.0f);
    const cinerig::EntityId targetEntity = world.createEntity({
        initialTarget,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });
    const cinerig::EntityId focusEntity = world.createEntity({
        {0.0f, 1.0f, 0.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Hard Lock Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create hard-lock camera\n";
        return 1;
    }

    camera->targets.tracking = cinerig::TargetRef::entity(targetEntity);
    camera->targets.lookAt = cinerig::TargetRef::entity(focusEntity);
    camera->targets.customLookAt = true;
    camera->lens.fieldOfView = 55.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& lock = camera->setBody<cinerig::HardLockToTarget>();
    auto& composer = camera->setAim<cinerig::RotationComposer>();
    composer.damping = {0.1f, 0.1f};

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createSphere(0.35f);
    const cinerig::examples::GlRenderer::MeshHandle focusMesh = app.renderer().createSphere(0.45f);

    bool paused = false;
    bool dampingMode = false;
    float pathTime = 0.0f;
    bool previousSpace = false;
    bool previousDamping = false;
    bool previousReset = false;

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
        camera->lens.aspect = aspect;

        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool dampingDown = app.input().keyDown(SDL_SCANCODE_D);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);
        if (spaceDown && !previousSpace)
            paused = !paused;
        if (dampingDown && !previousDamping)
        {
            dampingMode = !dampingMode;
            lock.damping = dampingMode ? 0.5f : 0.0f;
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            dampingMode = false;
            pathTime = 0.0f;
            lock.damping = 0.0f;
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousSpace = spaceDown;
        previousDamping = dampingDown;
        previousReset = resetDown;

        if (!paused)
            pathTime += deltaTime;
        const cinerig::math::Vec3 targetPosition = targetPositionAt(pathTime);
        world.setTransform(targetEntity, {
            targetPosition,
            cinerig::math::Quat::identity(),
            cinerig::math::Vec3::one,
        });

        cameraSystem.lateUpdate(deltaTime);

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.01f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.15f, 0.18f, 0.22f});
        app.renderer().drawGrid(24.0f, 1.0f);
        app.renderer().drawAxes(
            cinerig::Transform{
                {0.0f, 0.0f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            1.5f);
        app.renderer().drawMesh(
            focusMesh,
            cinerig::Transform{{0.0f, 1.0f, 0.0f}, cinerig::math::Quat::identity(), cinerig::math::Vec3::one},
            {0.35f, 0.75f, 1.0f});
        app.renderer().drawTargetMarker({0.0f, 1.0f, 0.0f}, 0.5f);
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P2-10 hard lock to target");
        app.hud().addLine(
            "Active",
            cameraSystem.brain(brainId)->activeVirtualCamera() != nullptr
                ? std::string(cameraSystem.brain(brainId)->activeVirtualCamera()->name())
                : "none");

        const cinerig::math::Vec3 cameraPosition = output.state().finalPosition();
        std::ostringstream targetLine;
        targetLine << "(" << targetPosition.x << ", " << targetPosition.y << ", " << targetPosition.z << ")";
        app.hud().addLine("Target", targetLine.str());
        std::ostringstream cameraLine;
        cameraLine << "(" << cameraPosition.x << ", " << cameraPosition.y << ", " << cameraPosition.z << ")";
        app.hud().addLine("Camera", cameraLine.str());
        std::ostringstream errorLine;
        errorLine << "lock error " << (cameraPosition - targetPosition).length();
        app.hud().addLine("Position", errorLine.str());
        app.hud().addLine("Damping", dampingMode ? "0.5 s" : "none");
        app.hud().addLine("Target state", paused ? "paused" : "moving");
        app.hud().addLine("Controls", "Space pause  D damping  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}


