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
            std::cout << "Usage: cinerig_12_rotate_with_follow [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 targetPositionAt(float time)
{
    return {
        2.4f * std::sin(time * 0.45f),
        1.1f + 0.18f * std::sin(time * 0.9f),
        1.2f * std::sin(time * 0.32f),
    };
}

cinerig::math::Quat targetRotationAt(float time)
{
    const cinerig::math::Quat yaw = cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::up,
        time * 0.55f);
    const cinerig::math::Quat roll = cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::forward,
        0.22f * std::sin(time * 0.8f));
    return cinerig::math::normalize(yaw * roll);
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P2-12 RotateWithFollowTarget", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 initialTargetPosition = targetPositionAt(0.0f);
    const cinerig::math::Quat initialTargetRotation = targetRotationAt(0.0f);
    const cinerig::EntityId targetEntity = world.createEntity({
        initialTargetPosition,
        initialTargetRotation,
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Rotate With Follow Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create rotate-with-follow camera\n";
        return 1;
    }

    camera->targets.tracking = cinerig::TargetRef::entity(targetEntity);
    camera->lens.fieldOfView = 55.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& follow = camera->setBody<cinerig::Follow>();
    follow.followOffset = {0.0f, 1.6f, 7.5f};
    follow.trackerSettings.bindingMode = cinerig::BindingMode::LockToTarget;
    follow.trackerSettings.positionDamping = {0.18f, 0.18f, 0.18f};
    follow.trackerSettings.rotationDamping = {0.05f, 0.05f, 0.05f};

    auto& rotate = camera->setAim<cinerig::RotateWithFollowTarget>();
    rotate.damping = 0.35f;

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createCylinder(0.42f, 1.3f);

    bool paused = false;
    bool dampingEnabled = true;
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
            dampingEnabled = !dampingEnabled;
            rotate.damping = dampingEnabled ? 0.35f : 0.0f;
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            dampingEnabled = true;
            pathTime = 0.0f;
            rotate.damping = 0.35f;
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousSpace = spaceDown;
        previousDamping = dampingDown;
        previousReset = resetDown;

        if (!paused)
            pathTime += deltaTime;
        const cinerig::math::Vec3 targetPosition = targetPositionAt(pathTime);
        const cinerig::math::Quat targetRotation = targetRotationAt(pathTime);
        world.setTransform(targetEntity, {
            targetPosition,
            targetRotation,
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
                targetPosition,
                targetRotation,
                cinerig::math::Vec3::one},
            1.8f);
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{targetPosition, targetRotation, cinerig::math::Vec3::one},
            {0.35f, 0.75f, 1.0f});
        app.renderer().drawCameraFrustum(output, {0.8f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P2-12 rotate with follow target");
        app.hud().addLine(
            "Active",
            cameraSystem.brain(brainId)->activeVirtualCamera() != nullptr
                ? std::string(cameraSystem.brain(brainId)->activeVirtualCamera()->name())
                : "none");

        const cinerig::math::Quat cameraOrientation = output.state().finalOrientation();
        const float orientationError =
            cinerig::math::angle(cameraOrientation, targetRotation) * cinerig::math::RadToDeg;
        std::ostringstream targetLine;
        targetLine << "(" << targetPosition.x << ", "
                   << targetPosition.y << ", " << targetPosition.z << ")";
        app.hud().addLine("Target", targetLine.str());
        std::ostringstream errorLine;
        errorLine << orientationError << " deg";
        app.hud().addLine("Orientation error", errorLine.str());
        app.hud().addLine("Damping", dampingEnabled ? "0.35 s" : "none");
        app.hud().addLine("Body", "Follow: LockToTarget offset (0, 1.6, 7.5)");
        app.hud().addLine("Target state", paused ? "paused" : "moving/rotating");
        app.hud().addLine("Controls", "Space pause  D damping  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}

