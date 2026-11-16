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
            std::cout << "Usage: cinerig_35_timeline [--frames N] "
                         "[--hidden] [--no-vsync] [--capture file.bmp]\n";
            std::exit(0);
        }
    }
    return options;
}

constexpr float kTimelineDuration = 6.8f;

struct ClipShot
{
    cinerig::CameraId id = 0;
    cinerig::EntityId entity = 0;
    cinerig::math::Vec3 color{};
    cinerig::math::Vec3 position{};
    float fieldOfView = 40.0f;
    float start = 0.0f;
    float duration = 1.0f;

    float end() const noexcept { return start + duration; }
};

float smoothStep(float value)
{
    const float t = cinerig::math::clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

float clipWeight(
    const std::vector<ClipShot>& clips,
    std::size_t index,
    float time)
{
    const ClipShot& clip = clips[index];
    if (time < clip.start || time >= clip.end())
        return 0.0f;

    float weight = 1.0f;
    if (index > 0)
    {
        const float previousEnd = clips[index - 1].end();
        if (time < previousEnd)
        {
            weight = std::min(
                weight,
                smoothStep(
                    (time - clip.start)
                    / (previousEnd - clip.start)));
        }
    }
    if (index + 1 < clips.size())
    {
        const float nextStart = clips[index + 1].start;
        if (time > nextStart)
        {
            weight = std::min(
                weight,
                1.0f - smoothStep(
                    (time - nextStart)
                    / (clip.end() - nextStart)));
        }
    }
    return weight;
}

std::string activeClipSummary(
    const std::vector<ClipShot>& clips,
    float time)
{
    std::ostringstream stream;
    bool first = true;
    for (std::size_t index = 0; index < clips.size(); ++index)
    {
        const float weight = clipWeight(clips, index, time);
        if (weight <= 0.0f)
            continue;
        if (!first)
            stream << " + ";
        stream << static_cast<char>('A' + index)
               << " " << static_cast<int>(std::lround(weight * 100.0f))
               << "%";
        first = false;
    }
    return first ? "(none)" : stream.str();
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app(
        "CineRig P5-01 Timeline", 1280, 720, appOptions);
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
    const cinerig::BrainId brainId =
        cameraSystem.createBrain(output, brainConfig);
    cinerig::CameraBrain* brain = cameraSystem.brain(brainId);
    if (brain == nullptr)
    {
        std::cerr << "Failed to create brain\n";
        return 1;
    }

    const cinerig::EntityId targetEntity = world.createEntity({
        {0.0f, 1.4f, 0.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::math::Vec3 gameplayPosition{2.0f, 13.0f, 20.0f};
    const cinerig::math::Vec3 gameplayDirection = cinerig::math::normalize(
        cinerig::math::Vec3{0.0f, 1.0f, 0.0f} - gameplayPosition);
    const cinerig::EntityId gameplayEntity = world.createEntity({
        gameplayPosition,
        cinerig::math::lookRotation(
            gameplayDirection, cinerig::math::Vec3::up),
        cinerig::math::Vec3::one,
    });
    const cinerig::CameraId gameplayId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Gameplay");
    auto* gameplay = cameraSystem.camera(gameplayId);
    if (gameplay == nullptr)
    {
        std::cerr << "Failed to create gameplay camera\n";
        return 1;
    }
    gameplay->ownerEntity = gameplayEntity;
    gameplay->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
    gameplay->targets.customLookAt = true;
    gameplay->priority = cinerig::PrioritySettings(100);
    gameplay->lens.fieldOfView = 45.0f;
    gameplay->lens.nearClipPlane = 0.1f;
    gameplay->lens.farClipPlane = 100.0f;
    gameplay->lens.aspect = app.viewportSize().x / app.viewportSize().y;
    gameplay->standbyUpdate = cinerig::StandbyUpdateMode::Always;
    gameplay->setAim<cinerig::HardLookAt>();

    std::vector<ClipShot> clips;
    clips.push_back({
        0,
        0,
        {1.0f, 0.52f, 0.20f},
        {-5.5f, 3.4f, 9.5f},
        40.0f,
        0.0f,
        2.6f,
    });
    clips.push_back({
        0,
        0,
        {0.22f, 0.82f, 1.0f},
        {7.5f, 2.8f, 5.5f},
        50.0f,
        2.05f,
        2.6f,
    });
    clips.push_back({
        0,
        0,
        {0.30f, 0.92f, 0.52f},
        {-5.0f, 4.6f, -6.0f},
        36.0f,
        4.1f,
        2.7f,
    });

    for (std::size_t index = 0; index < clips.size(); ++index)
    {
        ClipShot& clip = clips[index];
        clip.entity = world.createEntity({
            clip.position,
            cinerig::math::Quat::identity(),
            cinerig::math::Vec3::one,
        });
        const std::string name =
            std::string("Clip ") + static_cast<char>('A' + index);
        clip.id = cameraSystem.createCamera<cinerig::VirtualCamera>(name);
        auto* camera = cameraSystem.camera(clip.id);
        camera->ownerEntity = clip.entity;
        camera->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
        camera->targets.customLookAt = true;
        camera->priority = cinerig::PrioritySettings(
            static_cast<int>(10 + index));
        camera->lens.fieldOfView = clip.fieldOfView;
        camera->lens.nearClipPlane = 0.1f;
        camera->lens.farClipPlane = 100.0f;
        camera->lens.aspect =
            app.viewportSize().x / app.viewportSize().y;
        camera->standbyUpdate = cinerig::StandbyUpdateMode::Always;
        camera->setAim<cinerig::HardLookAt>();
    }

    cinerig::TimelineMixer timeline(*brain, 10);
    output.setDefaultLens(gameplay->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    bool paused = false;
    bool timelineDriving = true;
    bool looping = true;
    bool finished = false;
    bool previousSpace = false;
    bool previousDriving = false;
    bool previousLoop = false;
    bool previousReset = false;
    float timelineTime = 0.0f;
    float sceneTime = 0.0f;

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
        const float frameDelta =
            options.frames > 0 ? 1.0f / 60.0f : deltaTime;
        const float simDelta = paused ? 0.0f : frameDelta;

        world.setViewportSize(viewport);
        world.setDeltaTime(cinerig::UpdateClock::Late, simDelta);
        world.setFrame(cinerig::UpdateClock::Late, app.frameCount());
        output.setViewportSize(viewport);
        output.setAspect(aspect);
        gameplay->lens.aspect = aspect;
        for (const ClipShot& clip : clips)
            cameraSystem.camera(clip.id)->lens.aspect = aspect;

        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool drivingDown = app.input().keyDown(SDL_SCANCODE_D);
        const bool loopDown = app.input().keyDown(SDL_SCANCODE_L);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (spaceDown && !previousSpace)
            paused = !paused;
        if (drivingDown && !previousDriving)
        {
            timelineDriving = !timelineDriving;
            if (!timelineDriving)
                timeline.release();
            else if (finished)
            {
                timelineTime = 0.0f;
                finished = false;
            }
        }
        if (loopDown && !previousLoop)
        {
            looping = !looping;
            if (looping && finished)
            {
                timelineTime = 0.0f;
                finished = false;
            }
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            timelineDriving = true;
            looping = true;
            finished = false;
            timelineTime = 0.0f;
            sceneTime = 0.0f;
            timeline.release();
            cameraSystem.resetCameraState();
            output.reset();
            output.setDefaultLens(gameplay->lens);
        }
        previousSpace = spaceDown;
        previousDriving = drivingDown;
        previousLoop = loopDown;
        previousReset = resetDown;

        if (!paused)
            sceneTime += simDelta;

        if (timelineDriving && !finished && !paused)
        {
            timelineTime += simDelta;
            if (timelineTime >= kTimelineDuration)
            {
                if (looping)
                {
                    timelineTime = std::fmod(
                        timelineTime, kTimelineDuration);
                }
                else
                {
                    timelineTime = kTimelineDuration;
                    finished = true;
                }
            }
        }

        std::vector<cinerig::TimelineShot> shots;
        if (timelineDriving && !finished)
        {
            for (std::size_t index = 0; index < clips.size(); ++index)
            {
                const float weight = clipWeight(clips, index, timelineTime);
                if (weight <= 0.0f)
                    continue;
                shots.push_back({
                    cameraSystem.camera(clips[index].id),
                    timelineTime - clips[index].start,
                    clips[index].duration,
                    weight,
                });
            }
            timeline.processFrame(shots, true, simDelta);
        }
        else
        {
            timeline.release();
        }

        world.setTime(sceneTime);

        const cinerig::math::Vec3 targetPosition{
            2.8f * std::sin(sceneTime * 0.52f),
            1.4f + 0.20f * std::sin(sceneTime * 2.0f),
            1.5f * std::cos(sceneTime * 0.45f),
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
            {-3.6f, 0.0f, -2.8f},
            {3.8f, 0.0f, -3.4f},
            {-3.0f, 0.0f, 3.6f},
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
                    pillarPositions[index]
                        + cinerig::math::Vec3{0.0f, 0.7f, 0.0f},
                    cinerig::math::Quat::identity(),
                    {0.7f, 1.4f, 0.7f}},
                pillarColors[index]);
        }

        const auto drawCameraFrustum = [&app, brain, aspect, cameraMesh](
            const cinerig::VirtualCamera& camera,
            const cinerig::math::Vec3& color)
        {
            const cinerig::CameraState& state = camera.state();
            const bool live = brain->isLiveChild(&camera);
            app.renderer().setLineWidth(live ? 2.4f : 1.2f);
            cinerig::LensSettings lens = state.lens;
            lens.farClipPlane = 2.2f;
            app.renderer().drawFrustum(
                state.finalPosition(),
                state.finalOrientation(),
                lens,
                aspect,
                live ? color : color * 0.55f);
            app.renderer().drawMesh(
                cameraMesh,
                cinerig::Transform{
                    state.finalPosition(),
                    cinerig::math::Quat::identity(),
                    cinerig::math::Vec3::one},
                color);
            app.renderer().setLineWidth(1.0f);
        };

        drawCameraFrustum(*gameplay, {0.62f, 0.66f, 0.74f});
        for (const ClipShot& clip : clips)
        {
            const cinerig::VirtualCamera* camera =
                cameraSystem.camera(clip.id);
            if (camera != nullptr)
                drawCameraFrustum(*camera, clip.color);
        }

        const cinerig::ICinemachineCamera* liveCamera =
            brain->activeVirtualCamera();
        if (liveCamera != nullptr)
        {
            const cinerig::CameraState& liveState = liveCamera->state();
            app.renderer().drawAxes(
                cinerig::Transform{
                    liveState.finalPosition(),
                    liveState.finalOrientation(),
                    cinerig::math::Vec3::one},
                1.2f);
        }
        app.renderer().endFrame();

        const cinerig::Blend* blend = brain->activeBlend();
        const std::string clipSummary =
            timelineDriving && !finished
            ? activeClipSummary(clips, timelineTime)
            : std::string("(none)");
        const char* trackState = "gameplay";
        if (timelineDriving && !finished)
            trackState = blend != nullptr ? "blending" : "timeline";
        else if (finished)
            trackState = "finished";

        app.hud().begin();
        app.hud().addLine(
            "CineRig P5-01 timeline   frame "
            + std::to_string(app.frameCount()));
        std::ostringstream timeLine;
        timeLine << "t " << timelineTime << "/" << kTimelineDuration
                 << "s  loop " << (looping ? "on" : "off")
                 << "  " << (paused ? "paused" : "running");
        app.hud().addLine("Timeline", timeLine.str());
        std::ostringstream trackLine;
        trackLine << "priority 10  override #"
                  << timeline.overrideId()
                  << "  state " << trackState;
        app.hud().addLine("Track", trackLine.str());
        app.hud().addLine("Active clips", clipSummary);
        std::ostringstream brainLine;
        brainLine << "live "
                  << (liveCamera != nullptr
                      ? std::string(liveCamera->name())
                      : std::string("(none)"))
                  << "  gameplay priority 100";
        app.hud().addLine("Brain", brainLine.str());
        app.hud().addLine(
            "Clips",
            "A 0.0-2.6   B 2.05-4.65   C 4.1-6.8   overlap 0.55s");
        app.hud().addLine(
            "Controls",
            "D timeline drive  Space pause  L loop  R restart");
        app.hud().addLine(
            "Legend",
            "orange A  cyan B  green C  gray gameplay  thick = live");
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



