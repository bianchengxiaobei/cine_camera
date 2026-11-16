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
            std::cout << "Usage: cinerig_34_state_driven [--frames N] "
                         "[--hidden] [--no-vsync] [--capture file.bmp]\n";
            std::exit(0);
        }
    }
    return options;
}

constexpr int kIdleHash = 101;
constexpr int kWalkHash = 202;
constexpr int kRunHash = 303;
constexpr int kJumpHash = 404;

const char* stateName(int hash)
{
    switch (hash)
    {
    case kIdleHash:
        return "Idle";
    case kWalkHash:
        return "Walk";
    case kRunHash:
        return "Run";
    case kJumpHash:
        return "Jump";
    default:
        return "(none)";
    }
}

int nextAutoState(int current)
{
    switch (current)
    {
    case kIdleHash:
        return kWalkHash;
    case kWalkHash:
        return kRunHash;
    case kRunHash:
        return kJumpHash;
    default:
        return kIdleHash;
    }
}

class DemoAnimationState final : public cinerig::IAnimationStateProvider
{
public:
    int currentStateHash(cinerig::EntityId entity, int layer) const override
    {
        return entity == target && layer == layerIndex ? current_ : 0;
    }

    int nextStateHash(cinerig::EntityId entity, int layer) const override
    {
        return entity == target && layer == layerIndex && transitionTime_ > 0.0f
            ? incoming_
            : 0;
    }

    float normalizedTime(cinerig::EntityId entity, int layer) const override
    {
        return entity == target && layer == layerIndex ? 0.5f : 0.0f;
    }

    bool isInTransition(cinerig::EntityId entity, int layer) const override
    {
        return entity == target && layer == layerIndex && transitionTime_ > 0.0f;
    }

    void transitionTo(int state)
    {
        if (state == current_ && transitionTime_ <= 0.0f)
            return;
        if (state == incoming_ && transitionTime_ > 0.0f)
            return;
        incoming_ = state;
        transitionTime_ = 0.18f;
    }

    void update(float deltaTime)
    {
        if (transitionTime_ <= 0.0f)
            return;
        transitionTime_ -= deltaTime;
        if (transitionTime_ > 0.0f)
            return;
        current_ = incoming_;
        incoming_ = 0;
        transitionTime_ = 0.0f;
    }

    void reset()
    {
        current_ = kIdleHash;
        incoming_ = 0;
        transitionTime_ = 0.0f;
    }

    int current() const noexcept { return current_; }
    int incoming() const noexcept { return incoming_; }
    float transitionTime() const noexcept { return transitionTime_; }

    cinerig::EntityId target = 0;
    int layerIndex = 0;

private:
    int current_ = kIdleHash;
    int incoming_ = 0;
    float transitionTime_ = 0.0f;
};

struct ShotCamera
{
    cinerig::CameraId id = 0;
    cinerig::EntityId entity = 0;
    cinerig::math::Vec3 color{};
    cinerig::math::Vec3 position{};
    float fieldOfView = 40.0f;
    int stateHash = 0;
    float activateAfter = 0.0f;
    float minDuration = 0.0f;
};

void configureInstructions(
    cinerig::managers::StateDrivenCamera& manager,
    const std::vector<ShotCamera>& shots,
    cinerig::EntityId target,
    cinerig::IAnimationStateProvider* animation)
{
    manager.animatedTarget = target;
    manager.layerIndex = 0;
    manager.animationState = animation;
    manager.instructions.clear();
    const std::vector<cinerig::VirtualCamera*>& children =
        manager.childCameras();
    for (std::size_t index = 0; index < shots.size() && index < children.size();
         ++index)
    {
        manager.instructions.push_back({
            shots[index].stateHash,
            children[index],
            shots[index].activateAfter,
            shots[index].minDuration,
        });
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
        "CineRig P4-12 State Driven", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 overviewPosition{0.0f, 16.0f, 24.0f};
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

    DemoAnimationState animation;
    animation.target = targetEntity;
    animation.reset();

    const cinerig::CameraId managerId =
        cameraSystem.createCamera<cinerig::managers::StateDrivenCamera>(
            "State Driven");
    auto* manager =
        static_cast<cinerig::managers::StateDrivenCamera*>(
            cameraSystem.camera(managerId));
    if (manager == nullptr)
    {
        std::cerr << "Failed to create state-driven camera\n";
        return 1;
    }
    manager->priority = cinerig::PrioritySettings(100);
    manager->standbyUpdate = cinerig::StandbyUpdateMode::Always;
    manager->defaultBlend = {cinerig::BlendStyle::EaseInOut, 0.35f};

    std::vector<ShotCamera> shots;
    shots.push_back({
        0,
        0,
        {0.42f, 0.68f, 1.0f},
        {0.0f, 3.0f, 11.0f},
        40.0f,
        kIdleHash,
        0.15f,
        0.25f,
    });
    shots.push_back({
        0,
        0,
        {1.0f, 0.52f, 0.20f},
        {7.0f, 2.5f, 6.0f},
        46.0f,
        kWalkHash,
        0.25f,
        0.35f,
    });
    shots.push_back({
        0,
        0,
        {0.30f, 0.92f, 0.52f},
        {-7.0f, 2.5f, 6.0f},
        52.0f,
        kRunHash,
        0.20f,
        0.45f,
    });
    shots.push_back({
        0,
        0,
        {0.82f, 0.38f, 1.0f},
        {0.0f, 1.4f, 7.0f},
        60.0f,
        kJumpHash,
        0.05f,
        0.25f,
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
            std::cerr << "Failed to add state-driven child\n";
            return 1;
        }
    }

    configureInstructions(*manager, shots, targetEntity, &animation);
    output.setDefaultLens(overview->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    bool paused = false;
    bool autoCycle = true;
    bool previousSpace = false;
    bool previousAuto = false;
    bool previousSpeed = false;
    bool previousCancel = false;
    bool previousReset = false;
    bool previousStates[4] = {};
    int speedIndex = 1;
    float simTime = 0.0f;
    float stateTimer = 0.0f;
    const float speedScales[] = {0.5f, 1.0f, 1.75f, 2.5f};
    const int autoStates[] = {kIdleHash, kWalkHash, kRunHash, kJumpHash};

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
        const float simDelta =
            paused ? 0.0f : deltaTime * speedScales[speedIndex];

        world.setViewportSize(viewport);
        world.setDeltaTime(cinerig::UpdateClock::Late, simDelta);
        world.setTime(simTime);
        world.setFrame(cinerig::UpdateClock::Late, app.frameCount());
        output.setViewportSize(viewport);
        output.setAspect(aspect);
        overview->lens.aspect = aspect;
        for (const ShotCamera& shot : shots)
            cameraSystem.camera(shot.id)->lens.aspect = aspect;

        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool autoDown = app.input().keyDown(SDL_SCANCODE_A);
        const bool speedDown = app.input().keyDown(SDL_SCANCODE_T);
        const bool cancelDown = app.input().keyDown(SDL_SCANCODE_X);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (spaceDown && !previousSpace)
            paused = !paused;
        if (autoDown && !previousAuto)
        {
            autoCycle = !autoCycle;
            stateTimer = 0.0f;
        }
        if (speedDown && !previousSpeed)
            speedIndex = (speedIndex + 1) % 4;
        if (cancelDown && !previousCancel)
            manager->cancelWait();
        if (resetDown && !previousReset)
        {
            paused = false;
            autoCycle = true;
            speedIndex = 1;
            simTime = 0.0f;
            stateTimer = 0.0f;
            animation.reset();
            manager->reset();
            manager->defaultBlend = {cinerig::BlendStyle::EaseInOut, 0.35f};
            configureInstructions(*manager, shots, targetEntity, &animation);
            cameraSystem.resetCameraState();
            output.reset();
        }

        const SDL_Scancode stateKeys[] = {
            SDL_SCANCODE_1,
            SDL_SCANCODE_2,
            SDL_SCANCODE_3,
            SDL_SCANCODE_4,
        };
        for (int index = 0; index < 4; ++index)
        {
            const bool down = app.input().keyDown(stateKeys[index]);
            if (down && !previousStates[index])
            {
                autoCycle = false;
                stateTimer = 0.0f;
                animation.transitionTo(autoStates[index]);
            }
            previousStates[index] = down;
        }

        if (!paused)
            simTime += simDelta;
        world.setTime(simTime);
        if (autoCycle && !paused)
        {
            stateTimer += simDelta;
            if (stateTimer >= 1.7f)
            {
                stateTimer = 0.0f;
                animation.transitionTo(nextAutoState(animation.current()));
            }
        }
        animation.update(simDelta);

        const cinerig::math::Vec3 targetPosition{
            2.6f * std::sin(simTime * 0.55f),
            1.35f + 0.18f * std::sin(simTime * 2.1f),
            1.5f * std::cos(simTime * 0.45f),
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

        app.renderer().endFrame();

        const int activeInstruction = manager->activeInstruction();
        const int pendingInstruction = manager->pendingInstruction();
        const cinerig::VirtualCamera* live = manager->liveChild();
        const ShotCamera* activeShot =
            activeInstruction >= 0
                && static_cast<std::size_t>(activeInstruction) < shots.size()
            ? &shots[static_cast<std::size_t>(activeInstruction)]
            : nullptr;
        const ShotCamera* pendingShot =
            pendingInstruction >= 0
                && static_cast<std::size_t>(pendingInstruction) < shots.size()
            ? &shots[static_cast<std::size_t>(pendingInstruction)]
            : nullptr;

        app.hud().begin();
        app.hud().addLine(
            "CineRig P4-12 state driven   frame "
            + std::to_string(app.frameCount()));
        std::ostringstream stateLine;
        stateLine << stateName(animation.current());
        if (animation.transitionTime() > 0.0f)
        {
            stateLine << " -> " << stateName(animation.incoming())
                      << "  transition " << animation.transitionTime();
        }
        app.hud().addLine("Animation", stateLine.str());
        if (activeShot != nullptr)
        {
            std::ostringstream activeLine;
            activeLine << (activeInstruction + 1) << "/" << shots.size()
                       << "  " << stateName(activeShot->stateHash)
                       << "  live "
                       << (live != nullptr
                               ? std::string(live->name())
                               : std::string("(none)"));
            app.hud().addLine("Active", activeLine.str());
            std::ostringstream ruleLine;
            ruleLine << "wait " << activeShot->activateAfter
                     << "s  min " << activeShot->minDuration << "s";
            app.hud().addLine("Rule", ruleLine.str());
        }
        app.hud().addLine(
            "Pending",
            pendingShot != nullptr
                ? std::string(stateName(pendingShot->stateHash))
                    + " waiting"
                : std::string("(none)"));
        std::ostringstream modeLine;
        modeLine << "auto " << (autoCycle ? "on" : "off")
                 << "  speed " << speedScales[speedIndex] << "x";
        app.hud().addLine("Mode", modeLine.str());
        app.hud().addLine(
            "Controls",
            "1-4 state  A auto  T speed  X cancel wait  Space pause  R reset");
        app.hud().addLine(
            "Legend",
            "blue Idle  orange Walk  green Run  violet Jump  thick = live");
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
