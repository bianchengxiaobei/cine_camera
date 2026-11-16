#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
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
            std::cout << "Usage: cinerig_03_blend [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 targetPositionAt(float time)
{
    return {
        4.0f * std::sin(time * 0.45f),
        1.2f + 0.3f * std::sin(time * 1.1f),
        1.5f * std::sin(time * 0.3f + 0.6f),
    };
}

cinerig::BlendDefinition customBlendDefinition()
{
    cinerig::BlendDefinition definition;
    definition.style = cinerig::BlendStyle::Custom;
    definition.time = 2.0f;
    definition.customCurve = cinerig::math::Curve({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 3.0f, 3.0f},
    });
    return definition;
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

std::string blendLabel(const cinerig::BlendDefinition& definition, float duration)
{
    std::ostringstream stream;
    stream << blendStyleName(definition.style) << ' '
           << std::fixed << std::setprecision(1) << duration << "s";
    return stream.str();
}

std::string cameraLabel(const cinerig::ICinemachineCamera* camera)
{
    return camera != nullptr ? std::string(camera->name()) : "(none)";
}

cinerig::CameraId createFollowCamera(
    cinerig::CameraSystem& system,
    cinerig::examples::SdlWorld& world,
    const char* name,
    cinerig::EntityId target,
    cinerig::math::Vec3 targetPosition,
    cinerig::math::Vec3 offset,
    float fieldOfView,
    cinerig::math::Vec2 screenPosition,
    int priority)
{
    const cinerig::CameraId id = system.createCamera<cinerig::VirtualCamera>(name);
    auto* camera = system.camera(id);
    if (camera == nullptr)
        return 0;

    camera->targets.tracking = cinerig::TargetRef::entity(target);
    camera->lens.fieldOfView = fieldOfView;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = world.viewportSize().x / world.viewportSize().y;
    camera->priority = cinerig::PrioritySettings(priority);
    camera->standbyUpdate = cinerig::StandbyUpdateMode::Always;

    auto& follow = camera->setBody<cinerig::Follow>();
    follow.followOffset = offset;
    follow.trackerSettings.positionDamping = cinerig::math::Vec3{0.35f};

    auto& composer = camera->setAim<cinerig::RotationComposer>();
    composer.composition.screenPosition = screenPosition;
    composer.damping = {0.35f, 0.35f};

    cinerig::Transform transform;
    transform.position = targetPosition + offset;
    transform.rotation = cinerig::math::lookRotation(
        targetPosition - transform.position,
        world.worldUp());
    camera->ownerEntity = world.createEntity(transform);
    return id;
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P1-07 Blend", 1280, 720, appOptions);
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
    brainConfig.defaultBlend = {cinerig::BlendStyle::EaseInOut, 2.0f};
    const cinerig::BrainId brainId = cameraSystem.createBrain(output, brainConfig);
    auto* brain = cameraSystem.brain(brainId);
    if (brain == nullptr)
    {
        std::cerr << "Failed to create camera brain\n";
        return 1;
    }

    cinerig::math::Vec3 targetPosition = targetPositionAt(0.0f);
    const cinerig::EntityId targetEntity = world.createEntity({
        targetPosition,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId wideId = createFollowCamera(
        cameraSystem,
        world,
        "Wide",
        targetEntity,
        targetPosition,
        {0.0f, 3.0f, -12.0f},
        55.0f,
        {0.0f, 0.08f},
        10);
    const cinerig::CameraId closeId = createFollowCamera(
        cameraSystem,
        world,
        "Close",
        targetEntity,
        targetPosition,
        {1.5f, 1.8f, -5.0f},
        35.0f,
        {0.0f, 0.0f},
        0);

    auto* wide = cameraSystem.camera(wideId);
    auto* close = cameraSystem.camera(closeId);
    if (wide == nullptr || close == nullptr)
    {
        std::cerr << "Failed to create blend cameras\n";
        return 1;
    }

    output.setDefaultLens(wide->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(32.0f);
    const cinerig::examples::GlRenderer::MeshHandle cube = app.renderer().createCube(1.2f);
    const cinerig::examples::GlRenderer::MeshHandle cylinder = app.renderer().createCylinder(0.45f, 3.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createSphere(0.32f);

    const cinerig::math::Vec3 pillarPositions[] = {
        {-6.0f, 1.5f, 5.5f},
        {6.0f, 1.5f, 5.5f},
        {-3.0f, 1.5f, 8.0f},
        {3.0f, 1.5f, 8.0f},
    };
    const cinerig::math::Vec3 cubePositions[] = {
        {-6.0f, 0.6f, 1.0f},
        {6.0f, 0.6f, 1.0f},
        {-2.0f, 0.6f, 6.5f},
        {2.0f, 0.6f, 6.5f},
    };

    cinerig::BlendDefinition pendingBlend = brainConfig.defaultBlend;
    cinerig::BlendDefinition currentBlend = pendingBlend;
    bool currentIsClose = false;
    bool targetPaused = false;
    float pathTime = 0.0f;

    bool previousOne = false;
    bool previousTwo = false;
    bool previousCut = false;
    bool previousLinear = false;
    bool previousEase = false;
    bool previousCustom = false;
    bool previousSpace = false;

    auto selectBlend = [&](const cinerig::BlendDefinition& definition)
    {
        pendingBlend = definition;
        brain->config().defaultBlend = definition;
    };

    auto activateCamera = [&](bool useClose)
    {
        if (currentIsClose == useClose)
            return;

        currentIsClose = useClose;
        currentBlend = pendingBlend;
        wide->priority = cinerig::PrioritySettings(useClose ? 0 : 20);
        close->priority = cinerig::PrioritySettings(useClose ? 20 : 0);
    };

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
        wide->lens.aspect = aspect;
        close->lens.aspect = aspect;

        const bool oneDown = app.input().keyDown(SDL_SCANCODE_1);
        const bool twoDown = app.input().keyDown(SDL_SCANCODE_2);
        const bool cutDown = app.input().keyDown(SDL_SCANCODE_C);
        const bool linearDown = app.input().keyDown(SDL_SCANCODE_L);
        const bool easeDown = app.input().keyDown(SDL_SCANCODE_E);
        const bool customDown = app.input().keyDown(SDL_SCANCODE_T);
        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);

        if (oneDown && !previousOne)
            activateCamera(false);
        if (twoDown && !previousTwo)
            activateCamera(true);
        if (cutDown && !previousCut)
            selectBlend({cinerig::BlendStyle::Cut, 0.0f});
        if (linearDown && !previousLinear)
            selectBlend({cinerig::BlendStyle::Linear, 2.0f});
        if (easeDown && !previousEase)
            selectBlend({cinerig::BlendStyle::EaseInOut, 2.0f});
        if (customDown && !previousCustom)
            selectBlend(customBlendDefinition());
        if (spaceDown && !previousSpace)
            targetPaused = !targetPaused;

        previousOne = oneDown;
        previousTwo = twoDown;
        previousCut = cutDown;
        previousLinear = linearDown;
        previousEase = easeDown;
        previousCustom = customDown;
        previousSpace = spaceDown;

        if (!targetPaused)
        {
            pathTime += deltaTime;
            targetPosition = targetPositionAt(pathTime);
            world.setTransform(targetEntity, {
                targetPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one,
            });
        }

        cameraSystem.lateUpdate(deltaTime);

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{{0.0f, -0.01f, 0.0f}, cinerig::math::Quat::identity(), cinerig::math::Vec3::one},
            {0.14f, 0.17f, 0.21f});
        app.renderer().drawGrid(28.0f, 1.0f);

        for (const cinerig::math::Vec3& position : pillarPositions)
        {
            app.renderer().drawMesh(
                cylinder,
                {position, cinerig::math::Quat::identity(), cinerig::math::Vec3::one},
                {0.28f, 0.42f, 0.58f});
        }
        for (const cinerig::math::Vec3& position : cubePositions)
        {
            app.renderer().drawMesh(
                cube,
                {position, cinerig::math::quatFromEuler({0.0f, 0.35f, 0.0f}), cinerig::math::Vec3::one},
                {0.42f, 0.36f, 0.3f});
        }

        for (int segment = 0; segment < 64; ++segment)
        {
            const float t0 = static_cast<float>(segment) * 0.25f;
            const float t1 = static_cast<float>(segment + 1) * 0.25f;
            app.renderer().drawLine(
                targetPositionAt(t0),
                targetPositionAt(t1),
                {0.22f, 0.28f, 0.34f});
        }

        app.renderer().drawMesh(
            targetMesh,
            {targetPosition, cinerig::math::Quat::identity(), cinerig::math::Vec3::one},
            {0.95f, 0.65f, 0.25f});
        app.renderer().drawTargetMarker(targetPosition, 0.5f);

        const cinerig::ICinemachineCamera* activeCamera = brain->activeVirtualCamera();
        auto drawCamera = [&](const cinerig::VirtualCamera* camera, cinerig::math::Vec3 color)
        {
            if (camera == nullptr)
                return;

            cinerig::LensSettings lens = camera->state().lens;
            lens.farClipPlane = 18.0f;
            app.renderer().drawFrustum(
                camera->state().finalPosition(),
                camera->state().finalOrientation(),
                lens,
                aspect,
                color);
            app.renderer().drawTargetMarker(camera->state().finalPosition(), 0.35f, color);
            app.renderer().drawLine(camera->state().finalPosition(), targetPosition, color * 0.55f);
        };

        drawCamera(
            wide,
            activeCamera == wide ? cinerig::math::Vec3{0.3f, 0.75f, 1.0f} : cinerig::math::Vec3{0.18f, 0.3f, 0.42f});
        drawCamera(
            close,
            activeCamera == close ? cinerig::math::Vec3{1.0f, 0.48f, 0.22f} : cinerig::math::Vec3{0.42f, 0.28f, 0.2f});
        app.renderer().endFrame();

        const cinerig::Blend* blend = brain->activeBlend();
        const float weight = blend != nullptr ? blend->blendWeight() : 1.0f;

        app.hud().begin();
        app.hud().addLine("CineRig P1-07 blend");
        app.hud().addLine("Active", activeCamera != nullptr ? std::string(activeCamera->name()) : "none");
        app.hud().addLine("Blending", blend != nullptr ? "yes" : "no");

        std::ostringstream weightLine;
        weightLine << std::fixed << std::setprecision(2) << weight;
        app.hud().addLine("Weight", weightLine.str());
        app.hud().addLine("From", blend != nullptr ? cameraLabel(blend->camA) : "-");
        app.hud().addLine(
            "To",
            blend != nullptr
                ? cameraLabel(blend->camB)
                : (activeCamera != nullptr ? cameraLabel(activeCamera) : "-"));
        app.hud().addLine(
            "Style",
            blendLabel(currentBlend, blend != nullptr ? blend->duration : currentBlend.time));
        app.hud().addLine("Next style", blendLabel(pendingBlend, pendingBlend.time));

        std::ostringstream targetLine;
        targetLine << std::fixed << std::setprecision(2)
                   << "(" << targetPosition.x << ", " << targetPosition.y << ", " << targetPosition.z << ")";
        app.hud().addLine("Target", targetLine.str());
        std::ostringstream fovLine;
        fovLine << std::fixed << std::setprecision(1) << output.outputLens().fieldOfView;
        app.hud().addLine("FOV", fovLine.str());
        app.hud().addLine("Target state", targetPaused ? "paused" : "moving");
        app.hud().addLine("Keys", "1 Wide, 2 Close, C cut, L linear, E ease, T custom");
        app.hud().addLine("Keys", "Space pause, Esc quit");

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
