#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <cinerig/cinerig.hpp>

#include "app.hpp"
#include "sdl_camera_output.hpp"
#include "sdl_physics.hpp"
#include "sdl_world.hpp"

namespace
{

struct Options
{
    int frames = 0;
    bool hidden = false;
    bool vsync = true;
    std::string capturePath;
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
        else if (argument == "--capture" && index + 1 < argc)
            options.capturePath = argv[++index];
        else if (argument == "--help")
        {
            std::cout << "Usage: cinerig_32_clear_shot [--frames N] "
                         "[--hidden] [--no-vsync] [--capture file.bmp]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 targetPositionAt(float time)
{
    return {
        4.5f * std::sin(time * 0.8f),
        1.4f,
        0.0f,
    };
}

struct ShotCamera
{
    cinerig::CameraId id = 0;
    cinerig::EntityId entity = 0;
    cinerig::math::Vec3 color{};
};

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app(
        "CineRig P4-10 Clear Shot", 1280, 720, appOptions);
    if (!app.ok())
    {
        std::cerr << app.error() << '\n';
        return 1;
    }

    cinerig::examples::SdlWorld world;
    world.setViewportSize(app.viewportSize());
    cinerig::examples::SdlPhysics physics;

    cinerig::examples::SdlCameraOutput output;
    output.setViewportSize(app.viewportSize());

    cinerig::CameraSystem cameraSystem(world, &physics);
    cinerig::BrainConfig brainConfig;
    brainConfig.defaultBlend = {cinerig::BlendStyle::Cut, 0.0f};
    cameraSystem.createBrain(output, brainConfig);

    const cinerig::EntityId targetEntity = world.createEntity({
        targetPositionAt(0.0f),
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId managerId =
        cameraSystem.createCamera<cinerig::managers::ClearShot>("Clear Shot");
    auto* manager = static_cast<cinerig::managers::ClearShot*>(
        cameraSystem.camera(managerId));
    if (manager == nullptr)
    {
        std::cerr << "Failed to create clear shot camera\n";
        return 1;
    }
    manager->reset();
    manager->priority = cinerig::PrioritySettings(100);
    manager->standbyUpdate = cinerig::StandbyUpdateMode::Always;
    manager->activateAfter = 0.6f;
    manager->minDuration = 0.8f;
    manager->defaultBlend = {cinerig::BlendStyle::EaseInOut, 0.35f};

    std::vector<ShotCamera> cameras;
    cameras.push_back({0, 0, {1.0f, 0.52f, 0.20f}});
    cameras.push_back({0, 0, {0.22f, 0.82f, 1.0f}});

    const cinerig::math::Vec3 cameraPositions[] = {
        {0.0f, 2.2f, 8.0f},
        {0.0f, 2.2f, -2.0f},
    };
    const float fieldsOfView[] = {40.0f, 58.0f};
    const int priorities[] = {20, 10};

    for (std::size_t index = 0; index < cameras.size(); ++index)
    {
        ShotCamera& shot = cameras[index];
        shot.entity = world.createEntity({
            cameraPositions[index],
            cinerig::math::Quat::identity(),
            cinerig::math::Vec3::one,
        });
        const std::string name =
            std::string("Shot ") + static_cast<char>('A' + index);
        shot.id = cameraSystem.createCamera<cinerig::VirtualCamera>(name);
        auto* camera = cameraSystem.camera(shot.id);
        camera->ownerEntity = shot.entity;
        camera->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
        camera->targets.customLookAt = true;
        camera->lens.fieldOfView = fieldsOfView[index];
        camera->lens.nearClipPlane = 0.1f;
        camera->lens.farClipPlane = 100.0f;
        camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;
        camera->priority = cinerig::PrioritySettings(priorities[index]);
        camera->standbyUpdate = cinerig::StandbyUpdateMode::Always;
        camera->setAim<cinerig::HardLookAt>();

        auto deoccluder = std::make_unique<cinerig::Deoccluder>();
        deoccluder->reset();
        deoccluder->ignoreEntity = targetEntity;
        deoccluder->avoidObstacles.enabled = false;
        deoccluder->qualityEvaluation.enabled = false;
        deoccluder->validate();
        camera->addExtension(std::move(deoccluder));

        auto shotQuality = std::make_unique<cinerig::ShotQualityEvaluator>();
        shotQuality->reset();
        shotQuality->ignoreEntity = targetEntity;
        shotQuality->distanceEvaluation.enabled = true;
        shotQuality->distanceEvaluation.optimalDistance = 6.0f;
        shotQuality->distanceEvaluation.nearLimit = 1.0f;
        shotQuality->distanceEvaluation.farLimit = 12.0f;
        shotQuality->distanceEvaluation.maxQualityBoost = 0.2f;
        shotQuality->validate();
        camera->addExtension(std::move(shotQuality));

        if (!manager->addChild(*camera))
        {
            std::cerr << "Failed to add clear shot child\n";
            return 1;
        }
    }

    const cinerig::EntityId wallEntity = world.createEntity({
        {0.0f, 1.8f, 4.0f},
        cinerig::math::Quat::identity(),
        {2.4f, 2.8f, 0.4f},
    });
    physics.addBox(
        wallEntity,
        cinerig::Bounds{{0.0f, 1.8f, 4.0f}, {1.2f, 1.4f, 0.2f}},
        0u);

    output.setDefaultLens(cameraSystem.camera(cameras[0].id)->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    bool paused = false;
    bool previousSpace = false;
    bool previousTiming = false;
    bool previousRandomize = false;
    bool previousReset = false;
    int timingIndex = 2;
    float pathTime = 0.0f;
    bool randomizeChoice = false;

    app.hud().setTextScale(1.5f);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle cube =
        app.renderer().createCube(1.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh =
        app.renderer().createSphere(0.45f);
    const cinerig::examples::GlRenderer::MeshHandle cameraMesh =
        app.renderer().createSphere(0.30f);

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
        for (const ShotCamera& shot : cameras)
            cameraSystem.camera(shot.id)->lens.aspect = aspect;

        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool timingDown = app.input().keyDown(SDL_SCANCODE_T);
        const bool randomizeDown = app.input().keyDown(SDL_SCANCODE_X);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (spaceDown && !previousSpace)
            paused = !paused;
        if (timingDown && !previousTiming)
        {
            timingIndex = (timingIndex + 1) % 4;
            const float activateAfter[] = {0.0f, 0.6f, 0.6f, 0.0f};
            const float minDuration[] = {0.0f, 0.0f, 0.8f, 0.8f};
            manager->activateAfter = activateAfter[timingIndex];
            manager->minDuration = minDuration[timingIndex];
        }
        if (randomizeDown && !previousRandomize)
        {
            randomizeChoice = !randomizeChoice;
            manager->randomizeChoice = randomizeChoice;
            manager->resetRandomization();
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            timingIndex = 2;
            randomizeChoice = false;
            pathTime = 0.0f;
            manager->reset();
            manager->activateAfter = 0.6f;
            manager->minDuration = 0.8f;
            manager->defaultBlend = {cinerig::BlendStyle::EaseInOut, 0.35f};
            manager->resetRandomization();
            cameraSystem.resetCameraState();
            output.reset();
        }

        previousSpace = spaceDown;
        previousTiming = timingDown;
        previousRandomize = randomizeDown;
        previousReset = resetDown;

        if (!paused)
            pathTime += deltaTime;

        const cinerig::math::Vec3 targetPosition = targetPositionAt(pathTime);
        world.setTransform(
            targetEntity,
            cinerig::Transform{
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
                {0.0f, -0.02f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.11f, 0.14f, 0.18f});
        app.renderer().drawGrid(26.0f, 1.0f, {0.20f, 0.24f, 0.30f});
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {0.0f, 1.8f, 4.0f},
                cinerig::math::Quat::identity(),
                {2.4f, 2.8f, 0.4f}},
            {0.42f, 0.38f, 0.34f});
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{
                targetPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.35f, 0.95f, 0.55f});
        app.renderer().drawTargetMarker(
            targetPosition,
            0.85f,
            {0.35f, 0.95f, 0.55f});

        for (const ShotCamera& shot : cameras)
        {
            auto* camera = cameraSystem.camera(shot.id);
            if (camera == nullptr || camera == manager->liveChild())
                continue;
            const cinerig::CameraState& state = camera->state();
            app.renderer().drawMesh(
                cameraMesh,
                cinerig::Transform{
                    state.finalPosition(),
                    cinerig::math::Quat::identity(),
                    cinerig::math::Vec3::one},
                shot.color);
            app.renderer().setLineWidth(1.4f);
            app.renderer().drawLine(
                state.finalPosition(),
                targetPosition,
                shot.color * 0.5f);
        }

        app.renderer().endFrame();

        const cinerig::CameraState& a = cameraSystem.camera(cameras[0].id)->state();
        const cinerig::CameraState& b = cameraSystem.camera(cameras[1].id)->state();
        const cinerig::VirtualCamera* live = manager->liveChild();

        app.hud().begin();
        app.hud().addLine(
            "CineRig P4-10 clear shot   frame "
            + std::to_string(app.frameCount()));
        app.hud().addLine(
            "Live", live != nullptr ? std::string(live->name()) : "(none)");
        std::ostringstream qualityLine;
        qualityLine << "A " << a.shotQuality
                    << "  B " << b.shotQuality
                    << "  target x " << targetPosition.x;
        app.hud().addLine("Quality", qualityLine.str());
        std::ostringstream timingLine;
        timingLine << "activateAfter " << manager->activateAfter
                   << "  minDuration " << manager->minDuration;
        app.hud().addLine("Timing", timingLine.str());
        app.hud().addLine(
            "Randomize",
            randomizeChoice
                ? "on  seed " + std::to_string(manager->randomSeed)
                : "off");
        app.hud().addLine(
            "Controls", "T timing  X randomize  Space pause  R reset");
        app.hud().addLine(
            "Scene", "wall blocks A; move target for B to take over");
        app.hud().render(
            static_cast<int>(viewport.x),
            static_cast<int>(viewport.y));

        const bool finalFrame = options.frames > 0
            && app.frameCount() >= static_cast<std::uint64_t>(options.frames);
        if (finalFrame && !options.capturePath.empty())
        {
            std::string captureError;
            if (!app.captureFrame(options.capturePath, &captureError))
                std::cerr << "Capture failed: " << captureError << '\n';
        }

        app.endFrame();
        if (finalFrame)
            break;
    }

    return 0;
}
