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
            std::cout << "Usage: cinerig_23_confiner3d [--frames N] "
                         "[--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

const char* volumeName(std::uint64_t volume, cinerig::EntityId boxId, cinerig::EntityId sphereId)
{
    if (volume == boxId)
        return "box";
    if (volume == sphereId)
        return "sphere";
    return "none";
}

void drawWireBox(
    cinerig::examples::GlRenderer& renderer,
    cinerig::math::Vec3 center,
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
        world[index] = center + local[index];
    for (const auto& edge : edges)
        renderer.drawLine(world[edge[0]], world[edge[1]], color);
}

void drawCircle(
    cinerig::examples::GlRenderer& renderer,
    cinerig::math::Vec3 center,
    float radius,
    int axis,
    cinerig::math::Vec3 color)
{
    constexpr int segments = 64;
    cinerig::math::Vec3 previous{};
    for (int index = 0; index <= segments; ++index)
    {
        const float angle = cinerig::math::TwoPi * static_cast<float>(index)
            / static_cast<float>(segments);
        cinerig::math::Vec3 point = center;
        const float c = std::cos(angle) * radius;
        const float s = std::sin(angle) * radius;
        if (axis == 0)
        {
            point.x += c;
            point.z += s;
        }
        else if (axis == 1)
        {
            point.x += c;
            point.y += s;
        }
        else
        {
            point.y += c;
            point.z += s;
        }
        if (index > 0)
            renderer.drawLine(previous, point, color);
        previous = point;
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
        "CineRig P3-08 Confiner 3D", 1280, 720, appOptions);
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
        cameraSystem.createCamera<cinerig::VirtualCamera>("Confiner Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create confiner camera\n";
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

    auto ownedConfiner = std::make_unique<cinerig::Confiner3D>();
    auto* confiner = ownedConfiner.get();
    camera->addExtension(std::move(ownedConfiner));

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    constexpr cinerig::EntityId boxVolumeId = 9401;
    constexpr cinerig::EntityId sphereVolumeId = 9402;
    const cinerig::math::Vec3 boxCenter{0.0f, 2.0f, 0.0f};
    const cinerig::math::Vec3 boxHalfExtents{5.0f, 2.5f, 10.0f};
    const cinerig::math::Vec3 sphereCenter{0.0f, 2.0f, 6.0f};
    constexpr float sphereRadius = 4.5f;
    physics.addBox(boxVolumeId, cinerig::Bounds{boxCenter, boxHalfExtents});
    physics.addSphere(
        sphereVolumeId,
        cinerig::math::BoundingSphere{sphereCenter, sphereRadius});

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(40.0f);
    const cinerig::examples::GlRenderer::MeshHandle cube =
        app.renderer().createCube(1.0f);
    const cinerig::examples::GlRenderer::MeshHandle sphereMesh =
        app.renderer().createSphere(0.5f);

    bool previousSlowing = false;
    bool previousVolume = false;
    bool previousReset = false;

    const auto configure = [&](cinerig::EntityId volume) {
        const float slowing = confiner->slowingDistance;
        confiner->reset();
        confiner->boundingVolume = volume;
        confiner->slowingDistance = slowing;
    };

    configure(boxVolumeId);
    confiner->slowingDistance = 1.5f;

    const auto reset = [&]() {
        confiner->reset();
        confiner->boundingVolume = boxVolumeId;
        confiner->slowingDistance = 1.5f;
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

        const bool slowingDown = app.input().keyDown(SDL_SCANCODE_S);
        const bool volumeDown = app.input().keyDown(SDL_SCANCODE_V);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (slowingDown && !previousSlowing)
        {
            confiner->slowingDistance =
                confiner->slowingDistance > 0.0f ? 0.0f : 1.5f;
        }
        if (volumeDown && !previousVolume)
        {
            if (confiner->boundingVolume == boxVolumeId)
                configure(sphereVolumeId);
            else if (confiner->boundingVolume == sphereVolumeId)
                configure(0);
            else
                configure(boxVolumeId);
        }
        if (resetDown && !previousReset)
            reset();

        previousSlowing = slowingDown;
        previousVolume = volumeDown;
        previousReset = resetDown;

        const float targetX = 6.0f * std::sin(app.elapsedTime() * 0.6f);
        world.setTransform(
            targetEntity,
            cinerig::Transform{
                {targetX, 1.5f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one});

        cameraSystem.lateUpdate(deltaTime);

        const cinerig::CameraState& state = camera->state();
        const cinerig::math::Vec3 displacement =
            state.correctedPosition() - state.rawPosition;
        const char* currentVolume = volumeName(
            confiner->boundingVolume,
            boxVolumeId,
            sphereVolumeId);

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.01f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.14f, 0.17f, 0.21f});
        app.renderer().drawGrid(40.0f, 1.0f);
        app.renderer().drawMesh(
            sphereMesh,
            cinerig::Transform{
                {targetX, 1.5f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.35f, 0.78f, 1.0f});
        app.renderer().drawTargetMarker({targetX, 1.5f, 0.0f}, 0.8f, {1.0f, 0.75f, 0.25f});
        app.renderer().setLineWidth(2.0f);
        if (confiner->boundingVolume == boxVolumeId)
        {
            drawWireBox(
                app.renderer(),
                boxCenter,
                boxHalfExtents,
                {0.25f, 0.85f, 0.95f});
        }
        else if (confiner->boundingVolume == sphereVolumeId)
        {
            drawCircle(app.renderer(), sphereCenter, sphereRadius, 0, {0.25f, 0.85f, 0.95f});
            drawCircle(app.renderer(), sphereCenter, sphereRadius, 1, {0.25f, 0.85f, 0.95f});
            drawCircle(app.renderer(), sphereCenter, sphereRadius, 2, {0.25f, 0.85f, 0.95f});
        }
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
        app.hud().addLine("CineRig P3-08 confiner 3D");
        std::ostringstream volumeLine;
        volumeLine << currentVolume
                   << "  slowing " << confiner->slowingDistance
                   << "  valid " << (confiner->isValid() ? "yes" : "no");
        app.hud().addLine("Volume", volumeLine.str());
        std::ostringstream positionLine;
        positionLine << "raw x " << state.rawPosition.x
                     << " z " << state.rawPosition.z
                     << " -> corrected x " << state.correctedPosition().x
                     << " z " << state.correctedPosition().z;
        app.hud().addLine("Camera", positionLine.str());
        std::ostringstream displacementLine;
        displacementLine << displacement.length() << " units  displaced "
                         << (confiner->cameraWasDisplaced(cameraId) ? "yes" : "no");
        app.hud().addLine("Displacement", displacementLine.str());
        app.hud().addLine("Target", "x " + std::to_string(targetX));
        app.hud().addLine(
            "Controls",
            "S slowing  V volume  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}

