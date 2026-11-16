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
            std::cout << "Usage: cinerig_33_sequencer [--frames N] "
                         "[--hidden] [--no-vsync] [--capture file.bmp]\n";
            std::exit(0);
        }
    }
    return options;
}

const char* blendStyleName(cinerig::BlendStyle style)
{
    switch (style)
    {
    case cinerig::BlendStyle::Cut:
        return "Cut";
    case cinerig::BlendStyle::EaseInOut:
        return "EaseInOut";
    case cinerig::BlendStyle::EaseIn:
        return "EaseIn";
    case cinerig::BlendStyle::EaseOut:
        return "EaseOut";
    case cinerig::BlendStyle::HardIn:
        return "HardIn";
    case cinerig::BlendStyle::HardOut:
        return "HardOut";
    case cinerig::BlendStyle::Linear:
        return "Linear";
    case cinerig::BlendStyle::Custom:
        return "Custom";
    }
    return "Unknown";
}

struct ShotCamera
{
    cinerig::CameraId id = 0;
    cinerig::EntityId entity = 0;
    cinerig::math::Vec3 color{};
    cinerig::math::Vec3 position{};
    float fieldOfView = 40.0f;
    float hold = 1.0f;
    cinerig::BlendDefinition blend{};
};

void configureInstructions(
    cinerig::managers::SequencerCamera& manager,
    const std::vector<ShotCamera>& shots,
    bool loop)
{
    manager.loop = loop;
    manager.instructions.clear();
    const std::vector<cinerig::VirtualCamera*>& children =
        manager.childCameras();
    for (std::size_t index = 0; index < shots.size() && index < children.size();
         ++index)
    {
        manager.instructions.push_back(
            {children[index], shots[index].blend, shots[index].hold});
    }
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app(
        "CineRig P4-11 Sequencer", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 overviewPosition{0.0f, 17.0f, 25.0f};
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
    overview->lens.fieldOfView = 45.0f;
    overview->lens.nearClipPlane = 0.1f;
    overview->lens.farClipPlane = 100.0f;
    overview->lens.aspect = app.viewportSize().x / app.viewportSize().y;
    overview->standbyUpdate = cinerig::StandbyUpdateMode::Always;

    const cinerig::EntityId targetEntity = world.createEntity({
        {0.0f, 1.4f, 0.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId managerId =
        cameraSystem.createCamera<cinerig::managers::SequencerCamera>(
            "Sequencer");
    auto* manager =
        static_cast<cinerig::managers::SequencerCamera*>(
            cameraSystem.camera(managerId));
    if (manager == nullptr)
    {
        std::cerr << "Failed to create sequencer camera\n";
        return 1;
    }
    manager->priority = cinerig::PrioritySettings(100);
    manager->standbyUpdate = cinerig::StandbyUpdateMode::Always;

    std::vector<ShotCamera> shots;
    shots.push_back({
        0,
        0,
        {1.0f, 0.52f, 0.20f},
        {0.0f, 2.4f, 9.0f},
        38.0f,
        1.0f,
        {cinerig::BlendStyle::EaseInOut, 0.6f},
    });
    shots.push_back({
        0,
        0,
        {0.22f, 0.82f, 1.0f},
        {6.5f, 2.0f, 4.5f},
        52.0f,
        1.3f,
        {cinerig::BlendStyle::Linear, 0.8f},
    });
    shots.push_back({
        0,
        0,
        {0.30f, 0.92f, 0.52f},
        {-5.5f, 4.2f, -4.5f},
        34.0f,
        1.1f,
        {cinerig::BlendStyle::EaseIn, 0.5f},
    });

    for (std::size_t index = 0; index < shots.size(); ++index)
    {
        ShotCamera& shot = shots[index];
        shot.entity = world.createEntity({
            shot.position,
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
        camera->lens.fieldOfView = shot.fieldOfView;
        camera->lens.nearClipPlane = 0.1f;
        camera->lens.farClipPlane = 100.0f;
        camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;
        camera->standbyUpdate = cinerig::StandbyUpdateMode::Always;
        camera->setAim<cinerig::HardLookAt>();

        if (!manager->addChild(*camera))
        {
            std::cerr << "Failed to add sequencer child\n";
            return 1;
        }
    }

    configureInstructions(*manager, shots, true);
    manager->onTransitionFromCamera(
        nullptr, cinerig::math::Vec3::up, 0.0f);
    output.setDefaultLens(overview->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    bool paused = false;
    bool previousSpace = false;
    bool previousLoop = false;
    bool previousScale = false;
    bool previousReset = false;
    bool looping = true;
    int scaleIndex = 1;
    float sequenceTime = 0.0f;
    const float timeScales[] = {0.5f, 1.0f, 1.75f, 2.5f};

    app.hud().setTextScale(1.5f);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh =
        app.renderer().createSphere(0.42f);
    const cinerig::examples::GlRenderer::MeshHandle pedestal =
        app.renderer().createCube(1.0f);
    const cinerig::examples::GlRenderer::MeshHandle cameraMesh =
        app.renderer().createSphere(0.30f);
    const cinerig::examples::GlRenderer::MeshHandle pillar =
        app.renderer().createCube(1.0f);

    while (app.running())
    {
        app.pollEvents();
        if (!app.running())
            break;

        const cinerig::math::Vec2 viewport = app.viewportSize();
        const float deltaTime = app.deltaTime();
        const float aspect = viewport.y > 0.0f ? viewport.x / viewport.y : 1.0f;
        const float simDelta = paused ? 0.0f : deltaTime * timeScales[scaleIndex];

        world.setViewportSize(viewport);
        world.setDeltaTime(cinerig::UpdateClock::Late, simDelta);
        world.setTime(sequenceTime);
        world.setFrame(cinerig::UpdateClock::Late, app.frameCount());
        output.setViewportSize(viewport);
        output.setAspect(aspect);
        overview->lens.aspect = aspect;
        for (const ShotCamera& shot : shots)
            cameraSystem.camera(shot.id)->lens.aspect = aspect;

        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool loopDown = app.input().keyDown(SDL_SCANCODE_L);
        const bool scaleDown = app.input().keyDown(SDL_SCANCODE_T);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (spaceDown && !previousSpace)
            paused = !paused;
        if (loopDown && !previousLoop)
        {
            looping = !looping;
            manager->loop = looping;
        }
        if (scaleDown && !previousScale)
            scaleIndex = (scaleIndex + 1) % 4;
        if (resetDown && !previousReset)
        {
            paused = false;
            looping = true;
            scaleIndex = 1;
            sequenceTime = 0.0f;
            manager->reset();
            configureInstructions(*manager, shots, true);
            cameraSystem.resetCameraState();
            world.setTime(0.0f);
            manager->onTransitionFromCamera(
                nullptr, cinerig::math::Vec3::up, 0.0f);
            output.reset();
        }
        previousSpace = spaceDown;
        previousLoop = loopDown;
        previousScale = scaleDown;
        previousReset = resetDown;

        if (!paused)
            sequenceTime += simDelta;

        const cinerig::math::Vec3 targetPosition{
            1.2f * std::sin(sequenceTime * 0.55f),
            1.4f + 0.25f * std::sin(sequenceTime * 1.1f),
            0.4f * std::cos(sequenceTime * 0.4f),
        };
        world.setTransform(
            targetEntity,
            cinerig::Transform{
                targetPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one,
            });

        cameraSystem.lateUpdate(simDelta);

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
            pedestal,
            cinerig::Transform{
                {0.0f, 0.25f, 0.0f},
                cinerig::math::Quat::identity(),
                {1.2f, 0.5f, 1.2f}},
            {0.28f, 0.31f, 0.36f});
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

        const cinerig::math::Vec3 pillarPositions[] = {
            {-3.2f, 0.0f, -2.4f},
            {3.4f, 0.0f, -3.0f},
            {-3.0f, 0.0f, 3.4f},
        };
        const cinerig::math::Vec3 pillarColors[] = {
            {0.36f, 0.22f, 0.48f},
            {0.20f, 0.34f, 0.50f},
            {0.50f, 0.30f, 0.18f},
        };
        for (std::size_t index = 0; index < 3; ++index)
        {
            app.renderer().drawMesh(
                pillar,
                cinerig::Transform{
                    pillarPositions[index] + cinerig::math::Vec3{0.0f, 0.7f, 0.0f},
                    cinerig::math::Quat::identity(),
                    {0.7f, 1.4f, 0.7f}},
                pillarColors[index]);
        }

        for (const ShotCamera& shot : shots)
        {
            auto* camera = cameraSystem.camera(shot.id);
            if (camera == nullptr)
                continue;
            const cinerig::CameraState& state = camera->state();
            const bool live = camera == manager->liveChild();
            app.renderer().setLineWidth(live ? 2.4f : 1.2f);
            cinerig::LensSettings lens = state.lens;
            lens.farClipPlane = 2.2f;
            app.renderer().drawFrustum(
                state.finalPosition(),
                state.finalOrientation(),
                lens,
                aspect,
                live ? shot.color : shot.color * 0.55f);
            app.renderer().drawMesh(
                cameraMesh,
                cinerig::Transform{
                    state.finalPosition(),
                    cinerig::math::Quat::identity(),
                    cinerig::math::Vec3::one},
                shot.color);
            app.renderer().setLineWidth(1.0f);
            app.renderer().drawLine(
                state.finalPosition(),
                targetPosition,
                shot.color * 0.35f);
        }

        const cinerig::CameraState& liveState = manager->state();
        app.renderer().drawAxes(
            cinerig::Transform{
                liveState.finalPosition(),
                liveState.finalOrientation(),
                cinerig::math::Vec3::one},
            1.2f);
        app.renderer().endFrame();

        const int instruction = manager->currentInstruction();
        const cinerig::VirtualCamera* live = manager->liveChild();
        const ShotCamera* currentShot =
            instruction >= 0
                && static_cast<std::size_t>(instruction) < shots.size()
            ? &shots[static_cast<std::size_t>(instruction)]
            : nullptr;

        app.hud().begin();
        app.hud().addLine(
            "CineRig P4-11 sequencer   frame "
            + std::to_string(app.frameCount()));
        std::ostringstream sequenceLine;
        sequenceLine << "instruction " << (instruction >= 0 ? instruction + 1 : 0)
                     << "/" << shots.size()
                     << "  live "
                     << (live != nullptr ? std::string(live->name()) : "(none)")
                     << "  loop " << (looping ? "on" : "off");
        app.hud().addLine("Sequence", sequenceLine.str());
        if (currentShot != nullptr)
        {
            std::ostringstream shotLine;
            shotLine << "shot " << static_cast<char>('A' + instruction)
                     << "  hold " << currentShot->hold
                     << "s  blend " << blendStyleName(currentShot->blend.style)
                     << " " << currentShot->blend.blendTime() << "s";
            app.hud().addLine("Current", shotLine.str());
        }
        std::ostringstream timeLine;
        timeLine << "t " << sequenceTime
                 << "  scale " << timeScales[scaleIndex] << "x";
        app.hud().addLine("Time", timeLine.str());
        app.hud().addLine(
            "Controls",
            "L loop  T time scale  Space pause  R restart");
        app.hud().addLine(
            "Legend",
            "orange A  cyan B  green C  thick frustum = live");
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
