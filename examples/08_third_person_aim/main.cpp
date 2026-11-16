#include <algorithm>
#include <cstdint>
#include <cmath>
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
            std::cout << "Usage: cinerig_08_third_person_aim [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 targetPositionAt(float time)
{
    return {
        2.2f * std::sin(time * 0.45f),
        1.0f + 0.15f * std::sin(time * 1.3f),
        1.4f * std::sin(time * 0.32f),
    };
}

cinerig::math::Quat targetRotationAt(float time)
{
    return cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::up,
        std::sin(time * 0.4f) * 55.0f * cinerig::math::DegToRad);
}

const char* obstacleName(cinerig::EntityId id)
{
    return id == 0 ? "none" : "wall";
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P2-08 ThirdPersonAim", 1280, 720, appOptions);
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
    const cinerig::BrainId brainId = cameraSystem.createBrain(output, brainConfig);

    const cinerig::math::Vec3 initialTargetPosition = targetPositionAt(0.0f);
    const cinerig::math::Quat initialTargetRotation = targetRotationAt(0.0f);
    const cinerig::EntityId targetEntity = world.createEntity({
        initialTargetPosition,
        initialTargetRotation,
        cinerig::math::Vec3::one,
    });

    const cinerig::math::Vec3 wallPosition{0.0f, 1.2f, 3.0f};
    const cinerig::math::Vec3 wallScale{16.0f, 2.4f, 0.4f};
    const cinerig::EntityId wallEntity = world.createEntity({
        wallPosition,
        cinerig::math::Quat::identity(),
        wallScale,
    });
    physics.addBox(
        wallEntity,
        cinerig::Bounds{wallPosition, wallScale * 0.5f},
        0);

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Third Person Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create third-person camera\n";
        return 1;
    }

    camera->targets.tracking = cinerig::TargetRef::entity(targetEntity);
    camera->lens.fieldOfView = 55.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& follow = camera->setBody<cinerig::ThirdPersonFollow>();
    follow.damping = {0.18f, 0.35f, 0.25f};
    follow.shoulderOffset = {0.65f, -0.35f, 0.0f};
    follow.verticalArmLength = 0.55f;
    follow.cameraSide = 1.0f;
    follow.cameraDistance = 4.2f;
    follow.avoidObstacles.enabled = true;
    follow.avoidObstacles.collisionFilter.bits = 1u;
    follow.avoidObstacles.ignoreEntity = targetEntity;
    follow.avoidObstacles.cameraRadius = 0.25f;
    follow.avoidObstacles.dampingIntoCollision = 0.05f;
    follow.avoidObstacles.dampingFromCollision = 0.35f;

    camera->addExtension(std::make_unique<cinerig::ThirdPersonAim>());
    auto* aim = dynamic_cast<cinerig::ThirdPersonAim*>(camera->extension(0));
    if (aim == nullptr)
    {
        std::cerr << "Failed to create third-person aim extension\n";
        return 1;
    }
    aim->aimDistance = 60.0f;
    aim->aimCollisionFilter.bits = 1u;
    aim->ignoreEntity = targetEntity;
    aim->noiseCancellation = true;
    aim->validate();

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle cube = app.renderer().createCube(1.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetBody = app.renderer().createCylinder(0.35f, 1.1f);
    const cinerig::examples::GlRenderer::MeshHandle targetHead = app.renderer().createSphere(0.28f);
    const cinerig::examples::GlRenderer::MeshHandle aimMesh = app.renderer().createSphere(0.22f);
    const cinerig::EntityId aimColliderEntity = 1000;

    bool targetPaused = false;
    float pathTime = 0.0f;
    bool previousSpace = false;
    bool previousCollision = false;
    bool previousNoise = false;
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
        const bool collisionDown = app.input().keyDown(SDL_SCANCODE_C);
        const bool noiseDown = app.input().keyDown(SDL_SCANCODE_N);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);
        if (spaceDown && !previousSpace)
            targetPaused = !targetPaused;
        if (collisionDown && !previousCollision)
            follow.avoidObstacles.enabled = !follow.avoidObstacles.enabled;
        if (noiseDown && !previousNoise)
            aim->noiseCancellation = !aim->noiseCancellation;
        if (resetDown && !previousReset)
        {
            pathTime = 0.0f;
            targetPaused = false;
            follow.avoidObstacles.enabled = true;
            aim->noiseCancellation = true;
            world.setTransform(targetEntity, {
                initialTargetPosition,
                initialTargetRotation,
                cinerig::math::Vec3::one,
            });
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousSpace = spaceDown;
        previousCollision = collisionDown;
        previousNoise = noiseDown;
        previousReset = resetDown;

        if (!targetPaused)
            pathTime += deltaTime;
        const cinerig::math::Vec3 targetPosition = targetPositionAt(pathTime);
        const cinerig::math::Quat targetRotation = targetRotationAt(pathTime);
        const cinerig::math::Vec3 aimPoint = targetPosition
            + cinerig::math::rotate(targetRotation, cinerig::math::Vec3::forward) * 6.0f;
        physics.addSphere(
            aimColliderEntity,
            cinerig::math::BoundingSphere{aimPoint, 0.35f},
            0);
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
                {0.0f, 0.0f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            1.5f);
        app.renderer().drawAxes(
            cinerig::Transform{
                targetPosition,
                targetRotation,
                cinerig::math::Vec3::one},
            0.9f);
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{wallPosition, cinerig::math::Quat::identity(), wallScale},
            {0.38f, 0.42f, 0.5f});
        app.renderer().drawMesh(
            targetBody,
            cinerig::Transform{
                targetPosition + cinerig::math::Vec3{0.0f, 0.55f, 0.0f},
                targetRotation,
                cinerig::math::Vec3::one},
            {0.95f, 0.65f, 0.25f});
        app.renderer().drawMesh(
            targetHead,
            cinerig::Transform{
                targetPosition + cinerig::math::Vec3{0.0f, 1.25f, 0.0f},
                targetRotation,
                cinerig::math::Vec3::one},
            {1.0f, 0.78f, 0.4f});
        app.renderer().drawTargetMarker(targetPosition, 0.5f);
        app.renderer().drawMesh(
            aimMesh,
            cinerig::Transform{aim->aimTarget(), cinerig::math::Quat::identity(), cinerig::math::Vec3::one},
            {1.0f, 0.25f, 0.2f});
        app.renderer().drawLine(targetPosition, aim->aimTarget(), {0.9f, 0.3f, 0.2f});

        cinerig::math::Vec3 root{};
        cinerig::math::Vec3 shoulder{};
        cinerig::math::Vec3 hand{};
        follow.getRigPositions(root, shoulder, hand);
        app.renderer().drawLine(root, shoulder, {0.95f, 0.9f, 0.45f});
        app.renderer().drawLine(shoulder, hand, {0.45f, 0.95f, 0.9f});
        app.renderer().drawLine(hand, output.state().finalPosition(), {0.35f, 0.7f, 1.0f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P2-08 third person aim");
        app.hud().addLine(
            "Active",
            cameraSystem.brain(brainId)->activeVirtualCamera() != nullptr
                ? std::string(cameraSystem.brain(brainId)->activeVirtualCamera()->name())
                : "none");

        std::ostringstream targetLine;
        targetLine << "(" << targetPosition.x << ", " << targetPosition.y << ", " << targetPosition.z << ")";
        app.hud().addLine("Target", targetLine.str());

        std::ostringstream cameraLine;
        const cinerig::math::Vec3 cameraPosition = output.state().finalPosition();
        cameraLine << "(" << cameraPosition.x << ", " << cameraPosition.y << ", " << cameraPosition.z << ")";
        app.hud().addLine("Camera", cameraLine.str());

        std::ostringstream rigLine;
        rigLine << "root " << root.x << "," << root.y << "," << root.z
                << "  hand " << hand.x << "," << hand.y << "," << hand.z;
        app.hud().addLine("Rig", rigLine.str());

        app.hud().addLine(
            "Collision",
            std::string(follow.avoidObstacles.enabled ? "on" : "off")
                + " / obstacle: " + obstacleName(follow.currentObstacle()));

        std::ostringstream setupLine;
        setupLine << "side " << follow.cameraSide
                  << "  distance " << follow.cameraDistance
                  << "  radius " << follow.avoidObstacles.cameraRadius;
        app.hud().addLine("Setup", setupLine.str());

        app.hud().addLine("Target state", targetPaused ? "paused" : "moving + rotating");
        std::ostringstream aimLine;
        aimLine << (aim->noiseCancellation ? "cancel noise" : "detect only")
                << "  target " << aim->aimTarget().x << "," << aim->aimTarget().y
                << "," << aim->aimTarget().z;
        app.hud().addLine("Aim", aimLine.str());
        app.hud().addLine("Controls", "Space pause  C collision  N aim  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}






