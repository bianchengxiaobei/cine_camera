#include <algorithm>
#include <cmath>
#include <cstdint>
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
            std::cout << "Usage: cinerig_04_position_composer [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 targetPositionAt(float time)
{
    return {
        4.0f * std::sin(time * 0.45f),
        1.0f + 0.2f * std::sin(time * 1.6f),
        2.6f * std::sin(time * 0.32f + 0.7f),
    };
}

cinerig::math::Quat targetRotationAt(float time)
{
    const cinerig::math::Quat yaw = cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::up,
        time * 0.55f);
    const cinerig::math::Quat roll = cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::forward,
        18.0f * cinerig::math::DegToRad);
    return cinerig::math::normalize(yaw * roll);
}

const char* dampingModeName(int mode)
{
    switch (mode)
    {
    case 1:
        return "none";
    case 3:
        return "high";
    case 2:
    default:
        return "low";
    }
}

const char* compositionPresetName(int preset)
{
    switch (preset)
    {
    case 0:
        return "Center";
    case 2:
        return "DeadZone";
    case 3:
        return "HardLimits";
    case 1:
    default:
        return "Offset";
    }
}

void applyCompositionPreset(int preset, cinerig::PositionComposer& composer)
{
    cinerig::ScreenComposerSettings composition = cinerig::ScreenComposerSettings::Default;
    switch (preset)
    {
    case 0:
        break;
    case 2:
        composition.screenPosition = {0.15f, 0.08f};
        composition.deadZone.enabled = true;
        composition.deadZone.size = {0.45f, 0.35f};
        break;
    case 3:
        composition.deadZone.enabled = true;
        composition.deadZone.size = {0.25f, 0.2f};
        composition.hardLimits.enabled = true;
        composition.hardLimits.size = {0.65f, 0.55f};
        composition.hardLimits.offset = {0.1f, 0.05f};
        break;
    case 1:
    default:
        composition.screenPosition = {0.2f, 0.1f};
        break;
    }
    composer.composition = composition;
}

void applyDampingMode(int mode, cinerig::PositionComposer& composer)
{
    float value = 0.5f;
    if (mode == 1)
        value = 0.0f;
    else if (mode == 3)
        value = 1.5f;
    composer.damping = cinerig::math::Vec3{value};
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P2-03 PositionComposer", 1280, 720, appOptions);
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

    cinerig::math::Vec3 targetPosition = targetPositionAt(0.0f);
    cinerig::math::Quat targetRotation = targetRotationAt(0.0f);
    const cinerig::EntityId targetEntity = world.createEntity({
        targetPosition,
        targetRotation,
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Position Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create position composer camera\n";
        return 1;
    }

    camera->targets.tracking = cinerig::TargetRef::entity(targetEntity);
    camera->lens.fieldOfView = 50.0f;
    camera->lens.orthographicSize = 7.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& composer = camera->setBody<cinerig::PositionComposer>();
    composer.cameraDistance = 12.0f;
    composer.deadZoneDepth = 0.0f;
    composer.lookahead.time = 0.25f;
    composer.lookahead.smoothing = 0.15f;

    int compositionPreset = 1;
    applyCompositionPreset(compositionPreset, composer);
    int dampingMode = 2;
    applyDampingMode(dampingMode, composer);

    const cinerig::math::Vec3 initialCameraPosition{0.0f, 7.0f, 13.0f};
    cinerig::Transform cameraTransform;
    cameraTransform.position = initialCameraPosition;
    cameraTransform.rotation = cinerig::math::lookRotation(
        targetPosition - initialCameraPosition,
        world.worldUp());
    camera->ownerEntity = world.createEntity(cameraTransform);

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createSphere(0.4f);

    bool targetPaused = false;
    bool lookaheadEnabled = false;
    bool targetOffsetEnabled = false;
    float pathTime = 0.0f;
    bool previousOne = false;
    bool previousTwo = false;
    bool previousThree = false;
    bool previousComposition = false;
    bool previousLookahead = false;
    bool previousTargetOffset = false;
    bool previousProjection = false;
    bool previousSpace = false;
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

        const bool oneDown = app.input().keyDown(SDL_SCANCODE_1);
        const bool twoDown = app.input().keyDown(SDL_SCANCODE_2);
        const bool threeDown = app.input().keyDown(SDL_SCANCODE_3);
        const bool compositionDown = app.input().keyDown(SDL_SCANCODE_C);
        const bool lookaheadDown = app.input().keyDown(SDL_SCANCODE_L);
        const bool targetOffsetDown = app.input().keyDown(SDL_SCANCODE_T);
        const bool projectionDown = app.input().keyDown(SDL_SCANCODE_O);
        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (oneDown && !previousOne)
        {
            dampingMode = 1;
            applyDampingMode(dampingMode, composer);
        }
        if (twoDown && !previousTwo)
        {
            dampingMode = 2;
            applyDampingMode(dampingMode, composer);
        }
        if (threeDown && !previousThree)
        {
            dampingMode = 3;
            applyDampingMode(dampingMode, composer);
        }
        if (compositionDown && !previousComposition)
        {
            compositionPreset = (compositionPreset + 1) % 4;
            applyCompositionPreset(compositionPreset, composer);
        }
        if (lookaheadDown && !previousLookahead)
        {
            lookaheadEnabled = !lookaheadEnabled;
            composer.lookahead.enabled = lookaheadEnabled;
        }
        if (targetOffsetDown && !previousTargetOffset)
        {
            targetOffsetEnabled = !targetOffsetEnabled;
            composer.targetOffset = targetOffsetEnabled
                ? cinerig::math::Vec3{0.0f, 0.8f, 0.0f}
                : cinerig::math::Vec3::zero;
        }
        if (projectionDown && !previousProjection)
        {
            camera->lens.orthographic = !camera->lens.orthographic;
            output.setDefaultLens(camera->lens);
        }
        if (spaceDown && !previousSpace)
            targetPaused = !targetPaused;
        if (resetDown && !previousReset)
        {
            pathTime = 0.0f;
            targetPaused = false;
            targetPosition = targetPositionAt(0.0f);
            targetRotation = targetRotationAt(0.0f);
            world.setTransform(targetEntity, {
                targetPosition,
                targetRotation,
                cinerig::math::Vec3::one,
            });
            cameraSystem.resetCameraState();
            output.setDefaultLens(camera->lens);
            output.reset();
        }

        previousOne = oneDown;
        previousTwo = twoDown;
        previousThree = threeDown;
        previousComposition = compositionDown;
        previousLookahead = lookaheadDown;
        previousTargetOffset = targetOffsetDown;
        previousProjection = projectionDown;
        previousSpace = spaceDown;
        previousReset = resetDown;

        if (!targetPaused)
        {
            pathTime += deltaTime;
            targetPosition = targetPositionAt(pathTime);
            targetRotation = targetRotationAt(pathTime);
            world.setTransform(targetEntity, {
                targetPosition,
                targetRotation,
                cinerig::math::Vec3::one,
            });
        }

        cameraSystem.lateUpdate(deltaTime);

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{{0.0f, -0.01f, 0.0f}, cinerig::math::Quat::identity(), cinerig::math::Vec3::one},
            {0.15f, 0.18f, 0.22f});
        app.renderer().drawGrid(24.0f, 1.0f);
        app.renderer().drawAxes(cinerig::Transform{{0.0f, 0.0f, 0.0f}, cinerig::math::Quat::identity(), cinerig::math::Vec3::one}, 1.5f);
        app.renderer().drawAxes(cinerig::Transform{targetPosition, targetRotation, cinerig::math::Vec3::one}, 0.8f);
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{targetPosition, targetRotation, cinerig::math::Vec3::one},
            {0.95f, 0.65f, 0.25f});

        const cinerig::math::Vec3 trackingPoint = targetPosition
            + cinerig::math::rotate(targetRotation, composer.targetOffset);
        app.renderer().drawTargetMarker(trackingPoint, 0.5f);

        const cinerig::math::Vec3 cameraPosition = output.state().finalPosition();
        app.renderer().drawLine(cameraPosition, trackingPoint, {0.35f, 0.7f, 1.0f});
        app.renderer().drawCameraFrustum(output, {0.85f, 0.9f, 1.0f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P2-03 position composer");
        app.hud().addLine(
            "Active",
            cameraSystem.brain(brainId)->activeVirtualCamera() != nullptr
                ? std::string(cameraSystem.brain(brainId)->activeVirtualCamera()->name())
                : "none");

        std::ostringstream targetLine;
        targetLine << "(" << targetPosition.x << ", " << targetPosition.y << ", " << targetPosition.z << ")";
        app.hud().addLine("Target", targetLine.str());

        std::ostringstream cameraLine;
        cameraLine << "(" << cameraPosition.x << ", " << cameraPosition.y << ", " << cameraPosition.z << ")";
        app.hud().addLine("Camera", cameraLine.str());

        std::ostringstream distanceLine;
        distanceLine << composer.cameraDistance << " / depth " << composer.deadZoneDepth;
        app.hud().addLine("Distance", distanceLine.str());

        std::ostringstream compositionLine;
        compositionLine << compositionPresetName(compositionPreset)
                        << " (" << composer.composition.screenPosition.x
                        << ", " << composer.composition.screenPosition.y << ")";
        app.hud().addLine("Composition", compositionLine.str());

        std::ostringstream dampingLine;
        dampingLine << dampingModeName(dampingMode)
                    << " (" << composer.damping.x << ")";
        app.hud().addLine("Damping", dampingLine.str());

        app.hud().addLine(
            "Lookahead",
            lookaheadEnabled ? "0.25s" : "off");
        app.hud().addLine(
            "Target offset",
            targetOffsetEnabled ? "local Y 0.8" : "off");
        app.hud().addLine(
            "Projection",
            camera->lens.orthographic ? "orthographic" : "perspective");
        app.hud().addLine("Target state", targetPaused ? "paused" : "moving");
        app.hud().addLine(
            "Keys",
            "1/2/3 damping, C composition, L lookahead, T target offset, O projection, Space pause, R reset, Esc quit");

        std::ostringstream stats;
        stats << "FPS " << (deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f)
              << " | dt " << deltaTime
              << " | frame " << app.frameCount();
        app.hud().addLine(stats.str());
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));
        app.endFrame();

        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}
