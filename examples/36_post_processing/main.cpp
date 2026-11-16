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
            std::cout << "Usage: cinerig_36_post_processing [--frames N] "
                         "[--hidden] [--no-vsync] [--capture file.bmp]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 graded(
    cinerig::math::Vec3 color,
    cinerig::math::Vec3 grade)
{
    return {
        color.x * grade.x,
        color.y * grade.y,
        color.z * grade.z,
    };
}

struct ExampleProfile
{
    std::string name;
    cinerig::math::Vec3 tint{};
};

struct PostProcessResult
{
    cinerig::math::Vec3 tint{};
    float totalWeight = 0.0f;
    float focusDistance = 0.0f;
    std::string profileSummary = "(none)";
    int profileCount = 0;
};

class ExamplePostProcessor final : public cinerig::IPostProcessor
{
public:
    void apply(const cinerig::AppliedCamera& camera) override
    {
        result = PostProcessResult{};
        if (!camera.hasCamera)
            return;

        result.focusDistance = camera.state.lens.physical.focusDistance;

        std::ostringstream summary;
        for (std::size_t index = 0;
             index < camera.state.numCustomBlendables();
             ++index)
        {
            const cinerig::CustomBlendable& blendable =
                camera.state.customBlendable(index);
            if (blendable.type != typeid(ExampleProfile)
                || blendable.data == nullptr
                || blendable.weight <= 0.0f)
            {
                continue;
            }

            const auto* profile =
                static_cast<const ExampleProfile*>(blendable.data.get());
            if (profile == nullptr)
                continue;

            if (result.profileCount > 0)
                summary << " + ";
            summary << profile->name
                    << " " << static_cast<int>(
                        std::lround(blendable.weight * 100.0f))
                    << "%";
            result.tint += profile->tint * blendable.weight;
            result.totalWeight += blendable.weight;
            ++result.profileCount;
        }

        if (result.profileCount > 0)
            result.profileSummary = summary.str();
    }

    void resetHistory() override
    {
        ++historyResets;
    }

    PostProcessResult result;
    int historyResets = 0;
};

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
        "CineRig P5-02 Post Processing", 1280, 720, appOptions);
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

    ExamplePostProcessor postProcessor;
    brain->setPostProcessor(&postProcessor);

    const cinerig::EntityId targetEntity = world.createEntity({
        {0.0f, 1.4f, 0.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId managerId =
        cameraSystem.createCamera<cinerig::managers::SequencerCamera>(
            "Profile Sequence");
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
    manager->defaultBlend = {cinerig::BlendStyle::EaseInOut, 0.35f};

    std::vector<ShotCamera> shots;
    shots.push_back({
        0,
        0,
        {1.0f, 0.52f, 0.20f},
        {0.0f, 2.4f, 9.0f},
        38.0f,
        1.4f,
        {cinerig::BlendStyle::EaseInOut, 0.8f},
    });
    shots.push_back({
        0,
        0,
        {0.22f, 0.82f, 1.0f},
        {6.5f, 2.0f, 4.5f},
        52.0f,
        1.4f,
        {cinerig::BlendStyle::Linear, 0.8f},
    });
    shots.push_back({
        0,
        0,
        {0.30f, 0.92f, 0.52f},
        {-5.5f, 4.2f, -4.5f},
        34.0f,
        1.8f,
        {cinerig::BlendStyle::EaseIn, 0.8f},
    });

    const auto warmProfile = std::make_shared<ExampleProfile>(
        ExampleProfile{"Warm", {0.28f, 0.09f, 0.015f}});
    const auto coolProfile = std::make_shared<ExampleProfile>(
        ExampleProfile{"Cool", {0.015f, 0.11f, 0.24f}});
    const auto greenProfile = std::make_shared<ExampleProfile>(
        ExampleProfile{"Verdant", {0.04f, 0.22f, 0.07f}});
    const std::shared_ptr<ExampleProfile> profiles[] = {
        warmProfile,
        coolProfile,
        greenProfile,
    };

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
        camera->targets.tracking = cinerig::TargetRef::entity(targetEntity);
        camera->lens.fieldOfView = shot.fieldOfView;
        camera->lens.nearClipPlane = 0.1f;
        camera->lens.farClipPlane = 100.0f;
        camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;
        camera->standbyUpdate = cinerig::StandbyUpdateMode::Always;
        camera->setAim<cinerig::HardLookAt>();

        auto post = std::make_unique<cinerig::PostProcessing>();
        post->profile = cinerig::PostProcessProfile(profiles[index]);
        post->weight = 1.0f;
        if (index == 0)
        {
            post->focusTracking =
                cinerig::PostProcessing::FocusTrackingMode::LookAtTarget;
            post->focusOffset = 0.4f;
        }
        else if (index == 1)
        {
            post->focusTracking =
                cinerig::PostProcessing::FocusTrackingMode::FollowTarget;
            post->focusOffset = -0.3f;
        }
        post->validate();
        post->reset(*camera);
        camera->addExtension(std::move(post));

        if (index == 2)
        {
            auto autoFocus = std::make_unique<cinerig::AutoFocus>();
            autoFocus->focusMode =
                cinerig::AutoFocus::FocusMode::CustomTarget;
            autoFocus->customTarget =
                cinerig::TargetRef::entity(targetEntity);
            autoFocus->focusOffset = 0.6f;
            autoFocus->damping = 0.35f;
            autoFocus->validate();
            autoFocus->reset(*camera);
            camera->addExtension(std::move(autoFocus));
        }

        if (!manager->addChild(*camera))
        {
            std::cerr << "Failed to add sequencer child\n";
            return 1;
        }
    }

    configureInstructions(*manager, shots, true);
    manager->onTransitionFromCamera(
        nullptr, cinerig::math::Vec3::up, 0.0f);
    output.setDefaultLens(cameraSystem.camera(shots[0].id)->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    bool paused = false;
    bool looping = true;
    bool previousSpace = false;
    bool previousLoop = false;
    bool previousReset = false;
    float sequenceTime = 0.0f;

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
        for (const ShotCamera& shot : shots)
            cameraSystem.camera(shot.id)->lens.aspect = aspect;

        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool loopDown = app.input().keyDown(SDL_SCANCODE_L);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (spaceDown && !previousSpace)
            paused = !paused;
        if (loopDown && !previousLoop)
        {
            looping = !looping;
            manager->loop = looping;
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            looping = true;
            sequenceTime = 0.0f;
            manager->reset();
            configureInstructions(*manager, shots, true);
            cameraSystem.resetCameraState();
            world.setTime(0.0f);
            manager->onTransitionFromCamera(
                nullptr, cinerig::math::Vec3::up, 0.0f);
            output.reset();
            output.setDefaultLens(cameraSystem.camera(shots[0].id)->lens);
        }
        previousSpace = spaceDown;
        previousLoop = loopDown;
        previousReset = resetDown;

        if (!paused)
            sequenceTime += simDelta;
        world.setTime(sequenceTime);

        const cinerig::math::Vec3 targetPosition{
            2.5f * std::sin(sequenceTime * 0.55f),
            1.4f + 0.25f * std::sin(sequenceTime * 1.1f),
            0.8f * std::cos(sequenceTime * 0.4f),
        };
        world.setTransform(
            targetEntity,
            cinerig::Transform{
                targetPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one,
            });

        cameraSystem.lateUpdate(simDelta);

        const cinerig::math::Vec3 grade{
            1.0f + postProcessor.result.tint.x * 2.0f,
            1.0f + postProcessor.result.tint.y * 2.0f,
            1.0f + postProcessor.result.tint.z * 2.0f,
        };
        const cinerig::math::Vec3 clearColor{
            0.045f + postProcessor.result.tint.x * 0.55f,
            0.055f + postProcessor.result.tint.y * 0.55f,
            0.075f + postProcessor.result.tint.z * 0.55f,
        };

        app.beginFrame();
        app.renderer().beginFrame(output, clearColor);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.02f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            graded({0.11f, 0.14f, 0.18f}, grade));
        app.renderer().drawGrid(
            26.0f, 1.0f, graded({0.20f, 0.24f, 0.30f}, grade));

        app.renderer().drawMesh(
            pedestal,
            cinerig::Transform{
                {0.0f, 0.25f, 0.0f},
                cinerig::math::Quat::identity(),
                {1.2f, 0.5f, 1.2f}},
            graded({0.28f, 0.31f, 0.36f}, grade));
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{
                targetPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            graded({0.35f, 0.95f, 0.55f}, grade));
        app.renderer().drawTargetMarker(
            targetPosition,
            0.85f,
            graded({0.35f, 0.95f, 0.55f}, grade));

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
                    pillarPositions[index]
                        + cinerig::math::Vec3{0.0f, 0.7f, 0.0f},
                    cinerig::math::Quat::identity(),
                    {0.7f, 1.4f, 0.7f}},
                graded(pillarColors[index], grade));
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
            "CineRig P5-02 post processing   frame "
            + std::to_string(app.frameCount()));
        std::ostringstream sequenceLine;
        sequenceLine << "instruction "
                     << (instruction >= 0 ? instruction + 1 : 0)
                     << "/" << shots.size()
                     << "  live "
                     << (live != nullptr ? std::string(live->name()) : "(none)")
                     << "  loop " << (looping ? "on" : "off");
        app.hud().addLine("Sequence", sequenceLine.str());
        std::ostringstream profileLine;
        profileLine << postProcessor.result.profileSummary
                    << "  total " << postProcessor.result.totalWeight;
        app.hud().addLine("Profiles", profileLine.str());
        std::ostringstream focusLine;
        focusLine << postProcessor.result.focusDistance
                  << "m  history resets "
                  << postProcessor.historyResets;
        app.hud().addLine("Focus/PP", focusLine.str());
        if (currentShot != nullptr)
        {
            std::ostringstream shotLine;
            shotLine << "shot " << static_cast<char>('A' + instruction)
                     << "  hold " << currentShot->hold
                     << "s  blend " << currentShot->blend.blendTime()
                     << "s  FOV " << currentShot->fieldOfView;
            app.hud().addLine("Current", shotLine.str());
        }
        app.hud().addLine(
            "Controls",
            "Space pause  L loop  R restart");
        app.hud().addLine(
            "Adapters",
            "opaque profiles -> weighted tint + focus distance");
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
