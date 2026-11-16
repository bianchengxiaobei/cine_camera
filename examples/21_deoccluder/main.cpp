#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

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
            std::cout << "Usage: cinerig_21_deoccluder [--frames N] "
                         "[--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

const char* strategyName(cinerig::Deoccluder::ResolutionStrategy strategy)
{
    using Strategy = cinerig::Deoccluder::ResolutionStrategy;
    switch (strategy)
    {
    case Strategy::PullCameraForward:
        return "pull forward";
    case Strategy::PreserveCameraHeight:
        return "preserve height";
    case Strategy::PreserveCameraDistance:
        return "preserve distance";
    }
    return "unknown";
}

void drawWireBox(
    cinerig::examples::GlRenderer& renderer,
    cinerig::Transform transform,
    cinerig::math::Vec3 halfExtents,
    cinerig::math::Vec3 color)
{
    const cinerig::math::Vec3 local[8] = {
        {-halfExtents.x, -halfExtents.y, -halfExtents.z},
        {halfExtents.x, -halfExtents.y, -halfExtents.z},
        {halfExtents.x, -halfExtents.y, halfExtents.z},
        {-halfExtents.x, -halfExtents.y, halfExtents.z},
        {-halfExtents.x, halfExtents.y, -halfExtents.z},
        {halfExtents.x, halfExtents.y, -halfExtents.z},
        {halfExtents.x, halfExtents.y, halfExtents.z},
        {-halfExtents.x, halfExtents.y, halfExtents.z},
    };
    constexpr int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };
    cinerig::math::Vec3 world[8];
    for (int index = 0; index < 8; ++index)
    {
        world[index] = transform.position
            + cinerig::math::rotate(transform.rotation, local[index]);
    }
    for (const auto& edge : edges)
        renderer.drawLine(world[edge[0]], world[edge[1]], color);
}

void configureDefaults(
    cinerig::Deoccluder& deoccluder,
    cinerig::EntityId targetEntity)
{
    deoccluder.reset();
    deoccluder.collideAgainst = cinerig::LayerMask{1u};
    deoccluder.transparentLayers = cinerig::LayerMask{0b10u};
    deoccluder.ignoreEntity = targetEntity;
    deoccluder.minimumDistanceFromTarget = 0.3f;
    deoccluder.avoidObstacles.enabled = true;
    deoccluder.avoidObstacles.cameraRadius = 0.3f;
    deoccluder.avoidObstacles.distanceLimit = 0.0f;
    deoccluder.avoidObstacles.minimumOcclusionTime = 0.0f;
    deoccluder.avoidObstacles.strategy =
        cinerig::Deoccluder::ResolutionStrategy::PullCameraForward;
    deoccluder.avoidObstacles.maximumEffort = 4;
    deoccluder.avoidObstacles.smoothingTime = 0.12f;
    deoccluder.avoidObstacles.damping = 0.35f;
    deoccluder.avoidObstacles.dampingWhenOccluded = 0.15f;
    deoccluder.qualityEvaluation.enabled = true;
    deoccluder.qualityEvaluation.optimalDistance = 8.0f;
    deoccluder.qualityEvaluation.nearLimit = 3.0f;
    deoccluder.qualityEvaluation.farLimit = 20.0f;
    deoccluder.qualityEvaluation.maxQualityBoost = 0.2f;
    deoccluder.validate();
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app(
        "CineRig P3-06 Deoccluder", 1280, 720, appOptions);
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
        {0.0f, 1.5f, 0.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });
    const cinerig::EntityId cameraEntity = world.createEntity({
        {0.0f, 1.6f, 8.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Deoccluder Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create deoccluder camera\n";
        return 1;
    }

    camera->ownerEntity = cameraEntity;
    camera->targets.tracking = cinerig::TargetRef::entity(targetEntity);
    camera->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
    camera->targets.customLookAt = true;
    camera->lens.fieldOfView = 55.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& follow = camera->setBody<cinerig::ThirdPersonFollow>();
    follow.cameraDistance = 8.0f;
    follow.cameraSide = 0.0f;
    follow.shoulderOffset = cinerig::math::Vec3::zero;
    follow.verticalArmLength = 0.1f;
    follow.damping = cinerig::math::Vec3::zero;
    follow.avoidObstacles.enabled = false;
    camera->setAim<cinerig::HardLookAt>();

    auto ownedDeoccluder = std::make_unique<cinerig::Deoccluder>();
    auto* deoccluder = ownedDeoccluder.get();
    configureDefaults(*deoccluder, targetEntity);
    camera->addExtension(std::move(ownedDeoccluder));

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    constexpr cinerig::EntityId movingOccluderId = 9001;
    constexpr cinerig::EntityId glassId = 9002;
    const cinerig::math::Vec3 glassCenter{0.0f, 2.0f, 6.0f};
    const cinerig::math::Vec3 glassHalfExtents{1.5f, 2.0f, 0.1f};
    physics.addBox(
        glassId,
        cinerig::Bounds{glassCenter, glassHalfExtents},
        1u);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(36.0f);
    const cinerig::examples::GlRenderer::MeshHandle cube =
        app.renderer().createCube(1.0f);
    const cinerig::examples::GlRenderer::MeshHandle sphere =
        app.renderer().createSphere(0.5f);

    bool previousAvoid = false;
    bool previousTransparent = false;
    bool previousDamping = false;
    bool previousQuality = false;
    bool previousReset = false;
    bool previousStrategy1 = false;
    bool previousStrategy2 = false;
    bool previousStrategy3 = false;

    const auto reset = [&]() {
        configureDefaults(*deoccluder, targetEntity);
        cameraSystem.resetCameraState();
        output.reset();
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
        camera->lens.aspect = aspect;

        const bool avoidDown = app.input().keyDown(SDL_SCANCODE_A);
        const bool transparentDown = app.input().keyDown(SDL_SCANCODE_T);
        const bool dampingDown = app.input().keyDown(SDL_SCANCODE_D);
        const bool qualityDown = app.input().keyDown(SDL_SCANCODE_Q);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);
        const bool strategy1Down = app.input().keyDown(SDL_SCANCODE_1);
        const bool strategy2Down = app.input().keyDown(SDL_SCANCODE_2);
        const bool strategy3Down = app.input().keyDown(SDL_SCANCODE_3);

        if (avoidDown && !previousAvoid)
            deoccluder->avoidObstacles.enabled = !deoccluder->avoidObstacles.enabled;
        if (transparentDown && !previousTransparent)
        {
            deoccluder->transparentLayers = deoccluder->transparentLayers.bits != 0u
                ? cinerig::LayerMask{0u}
                : cinerig::LayerMask{0b10u};
        }
        if (dampingDown && !previousDamping)
        {
            const bool enabled = deoccluder->avoidObstacles.damping > 0.0f;
            deoccluder->avoidObstacles.damping = enabled ? 0.0f : 0.35f;
            deoccluder->avoidObstacles.dampingWhenOccluded = enabled ? 0.0f : 0.15f;
        }
        if (qualityDown && !previousQuality)
            deoccluder->qualityEvaluation.enabled =
                !deoccluder->qualityEvaluation.enabled;
        if (resetDown && !previousReset)
            reset();
        if (strategy1Down && !previousStrategy1)
        {
            deoccluder->avoidObstacles.strategy =
                cinerig::Deoccluder::ResolutionStrategy::PullCameraForward;
        }
        if (strategy2Down && !previousStrategy2)
        {
            deoccluder->avoidObstacles.strategy =
                cinerig::Deoccluder::ResolutionStrategy::PreserveCameraHeight;
        }
        if (strategy3Down && !previousStrategy3)
        {
            deoccluder->avoidObstacles.strategy =
                cinerig::Deoccluder::ResolutionStrategy::PreserveCameraDistance;
        }

        previousAvoid = avoidDown;
        previousTransparent = transparentDown;
        previousDamping = dampingDown;
        previousQuality = qualityDown;
        previousReset = resetDown;
        previousStrategy1 = strategy1Down;
        previousStrategy2 = strategy2Down;
        previousStrategy3 = strategy3Down;

        const float occluderX = 3.6f * std::sin(app.elapsedTime() * 0.9f);
        const cinerig::math::Vec3 occluderCenter{occluderX, 2.0f, 4.0f};
        const cinerig::math::Vec3 occluderHalfExtents{0.6f, 2.0f, 0.2f};
        physics.addBox(
            movingOccluderId,
            cinerig::Bounds{occluderCenter, occluderHalfExtents},
            0u);

        cameraSystem.lateUpdate(deltaTime);

        const cinerig::CameraState& state = camera->state();
        const cinerig::math::Vec3 correction =
            state.correctedPosition() - state.rawPosition;
        const float distanceToTarget =
            (state.correctedPosition() - state.referenceLookAt).length();

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.01f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.14f, 0.17f, 0.21f});
        app.renderer().drawGrid(36.0f, 1.0f);
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                occluderCenter,
                cinerig::math::Quat::identity(),
                occluderHalfExtents * 2.0f},
            {0.72f, 0.34f, 0.24f});
        app.renderer().drawMesh(
            sphere,
            cinerig::Transform{
                {0.0f, 1.5f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.35f, 0.78f, 1.0f});
        app.renderer().drawTargetMarker({0.0f, 1.5f, 0.0f}, 0.8f, {1.0f, 0.75f, 0.25f});
        app.renderer().setLineWidth(2.0f);
        drawWireBox(
            app.renderer(),
            cinerig::Transform{
                glassCenter,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            glassHalfExtents,
            deoccluder->transparentLayers.bits != 0u
                ? cinerig::math::Vec3{0.25f, 0.85f, 0.95f}
                : cinerig::math::Vec3{0.95f, 0.55f, 0.25f});
        app.renderer().drawLine(
            state.referenceLookAt,
            state.rawPosition,
            {0.55f, 0.58f, 0.64f});
        app.renderer().drawLine(
            state.referenceLookAt,
            state.correctedPosition(),
            {0.35f, 0.95f, 0.55f});
        app.renderer().drawCameraFrustum(output, {0.8f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P3-06 deoccluder");
        app.hud().addLine(
            "Strategy",
            strategyName(deoccluder->avoidObstacles.strategy));
        std::ostringstream filterLine;
        filterLine << "collide " << deoccluder->collideAgainst.bits
                   << "  transparent " << deoccluder->transparentLayers.bits
                   << "  ignore entity " << deoccluder->ignoreEntity;
        app.hud().addLine("Filter", filterLine.str());
        app.hud().addLine(
            "Avoidance",
            deoccluder->avoidObstacles.enabled ? "on" : "off");
        std::ostringstream distanceLine;
        distanceLine << "camera " << state.rawPosition.z << " -> "
                     << state.correctedPosition().z
                     << "  target distance " << distanceToTarget;
        app.hud().addLine("Position", distanceLine.str());
        std::ostringstream correctionLine;
        correctionLine << correction.length() << " units  displaced "
                       << (deoccluder->cameraWasDisplaced(cameraId) ? "yes" : "no");
        app.hud().addLine("Correction", correctionLine.str());
        std::ostringstream qualityLine;
        qualityLine << "obscured "
                    << (deoccluder->isTargetObscured(cameraId) ? "yes" : "no")
                    << "  shot quality " << state.shotQuality;
        app.hud().addLine("Quality", qualityLine.str());
        app.hud().addLine(
            "Occluders",
            "red box layer 0; cyan glass layer 1 (transparent when enabled)");
        app.hud().addLine(
            "Controls",
            "1/2/3 strategy  A avoid  T glass  D damping  Q quality  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}
