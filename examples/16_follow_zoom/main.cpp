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
            std::cout << "Usage: cinerig_16_follow_zoom [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 targetPositionAt(float time)
{
    return {0.0f, 3.5f, -4.0f + 8.0f * std::sin(time * 0.35f)};
}

cinerig::math::Quat targetRotationAt(float time)
{
    return cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::up,
        time * 0.6f);
}

void applyDefaults(cinerig::FollowZoom& zoom)
{
    zoom.width = 6.0f;
    zoom.damping = 1.2f;
    zoom.fovRange = {3.0f, 60.0f};
    zoom.validate();
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P3-01 Follow Zoom", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 targetPosition = targetPositionAt(0.0f);
    const cinerig::EntityId targetEntity = world.createEntity({
        targetPosition,
        targetRotationAt(0.0f),
        cinerig::math::Vec3::one,
    });

    const cinerig::math::Vec3 cameraPosition{0.0f, 3.5f, 12.0f};
    const cinerig::EntityId cameraEntity = world.createEntity({
        cameraPosition,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Follow Zoom Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create follow zoom camera\n";
        return 1;
    }

    camera->ownerEntity = cameraEntity;
    camera->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
    camera->targets.customLookAt = true;
    camera->lens.fieldOfView = 40.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& composer = camera->setAim<cinerig::RotationComposer>();
    composer.damping = {0.08f, 0.08f};
    composer.composition.screenPosition = {0.0f, 0.0f};

    auto ownedZoom = std::make_unique<cinerig::FollowZoom>();
    auto* zoom = ownedZoom.get();
    applyDefaults(*zoom);
    camera->addExtension(std::move(ownedZoom));
    zoom->reset(*camera);

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createCube(1.0f);
    const cinerig::examples::GlRenderer::MeshHandle cube = app.renderer().createCube(1.0f);

    bool paused = false;
    bool damped = true;
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
            damped = !damped;
            zoom->damping = damped ? 1.2f : 0.0f;
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            damped = true;
            pathTime = 0.0f;
            applyDefaults(*zoom);
            zoom->reset(*camera);
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousSpace = spaceDown;
        previousDamping = dampingDown;
        previousReset = resetDown;

        const float widthSpeed = 4.0f * deltaTime;
        if (app.input().keyDown(SDL_SCANCODE_W))
            zoom->width += widthSpeed;
        if (app.input().keyDown(SDL_SCANCODE_S))
            zoom->width -= widthSpeed;
        zoom->width = cinerig::math::clamp(zoom->width, 1.0f, 12.0f);

        if (!paused)
            pathTime += deltaTime;

        const cinerig::math::Vec3 targetPositionNow = targetPositionAt(pathTime);
        const cinerig::math::Quat targetRotation = targetRotationAt(pathTime);
        world.setTransform(targetEntity, {
            targetPositionNow,
            targetRotation,
            cinerig::math::Vec3::one,
        });

        cameraSystem.lateUpdate(deltaTime);

        const float targetDistance =
            (targetPositionNow - camera->state().correctedPosition()).length();
        const float lensFov = output.state().lens.fieldOfView;
        const float halfWidth = zoom->width * 0.5f;
        const cinerig::math::Vec3 gaugeTop{
            targetPositionNow.x,
            targetPositionNow.y + halfWidth,
            targetPositionNow.z};
        const cinerig::math::Vec3 gaugeBottom{
            targetPositionNow.x,
            targetPositionNow.y - halfWidth,
            targetPositionNow.z};
        constexpr float tickInset = 0.35f;
        const cinerig::math::Vec3 topTickLeft{
            gaugeTop.x - 0.45f, gaugeTop.y - tickInset, gaugeTop.z};
        const cinerig::math::Vec3 topTickRight{
            gaugeTop.x + 0.45f, gaugeTop.y - tickInset, gaugeTop.z};
        const cinerig::math::Vec3 bottomTickLeft{
            gaugeBottom.x - 0.45f, gaugeBottom.y + tickInset, gaugeBottom.z};
        const cinerig::math::Vec3 bottomTickRight{
            gaugeBottom.x + 0.45f, gaugeBottom.y + tickInset, gaugeBottom.z};

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.01f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.15f, 0.18f, 0.22f});
        app.renderer().drawGrid(30.0f, 1.0f);
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {0.0f, 3.0f, -20.0f},
                cinerig::math::Quat::identity(),
                {34.0f, 6.0f, 0.3f}},
            {0.20f, 0.24f, 0.30f});
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {-6.0f, 2.0f, -7.0f},
                cinerig::math::Quat::identity(),
                {0.7f, 4.0f, 0.7f}},
            {0.27f, 0.32f, 0.40f});
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {6.0f, 2.0f, -7.0f},
                cinerig::math::Quat::identity(),
                {0.7f, 4.0f, 0.7f}},
            {0.27f, 0.32f, 0.40f});
        app.renderer().setLineWidth(2.0f);
        app.renderer().drawLine(gaugeBottom, gaugeTop, {0.98f, 0.68f, 0.24f});
        app.renderer().drawLine(topTickLeft, topTickRight, {0.98f, 0.68f, 0.24f});
        app.renderer().drawLine(bottomTickLeft, bottomTickRight, {0.98f, 0.68f, 0.24f});
        app.renderer().drawLine(
            {targetPositionNow.x - 0.45f, targetPositionNow.y, targetPositionNow.z},
            {targetPositionNow.x + 0.45f, targetPositionNow.y, targetPositionNow.z},
            {0.55f, 0.42f, 0.20f});
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{targetPositionNow, targetRotation, cinerig::math::Vec3::one},
            {0.35f, 0.75f, 1.0f});
        app.renderer().drawAxes(
            cinerig::Transform{targetPositionNow, targetRotation, cinerig::math::Vec3::one},
            0.9f);
        app.renderer().drawTargetMarker(targetPositionNow, 0.7f);
        app.renderer().drawCameraFrustum(output, {0.8f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P3-01 follow zoom");
        std::ostringstream distanceLine;
        distanceLine << targetDistance << " world units";
        app.hud().addLine("Target distance", distanceLine.str());
        std::ostringstream widthLine;
        widthLine << zoom->width << "  target cube height 1.0  ("
                  << (100.0f / zoom->width) << "% of gauge)";
        app.hud().addLine("Width", widthLine.str());
        std::ostringstream fovLine;
        fovLine << lensFov << "  range [" << zoom->fovRange.x
                << ", " << zoom->fovRange.y << "]";
        app.hud().addLine("Lens FOV", fovLine.str());
        app.hud().addLine(
            "Damping",
            damped ? "1.2 (smoothed)" : "0 (instant)");
        app.hud().addLine("Gauge", "orange span = Width at target plane");
        app.hud().addLine("Motion", paused ? "paused" : "depth sweep");
        app.hud().addLine("Controls", "W/S width  D damping  Space pause  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}



