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
            std::cout << "Usage: cinerig_22_decollider [--frames N] "
                         "[--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

void configureDefaults(cinerig::Decollider& decollider)
{
    decollider.reset();
    decollider.cameraRadius = 0.4f;
    decollider.terrainResolution.enabled = true;
    decollider.terrainResolution.terrainLayers = cinerig::LayerMask{1u};
    decollider.terrainResolution.maximumRaycast = 10.0f;
    decollider.terrainResolution.damping = 0.5f;
    decollider.decollision.enabled = true;
    decollider.decollision.obstacleLayers = cinerig::LayerMask{0b10u};
    decollider.decollision.damping = 0.5f;
    decollider.decollision.smoothingTime = 0.2f;
    decollider.validate();
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app(
        "CineRig P3-07 Decollider", 1280, 720, appOptions);
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
        cameraSystem.createCamera<cinerig::VirtualCamera>("Decollider Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create decollider camera\n";
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

    auto ownedDecollider = std::make_unique<cinerig::Decollider>();
    auto* decollider = ownedDecollider.get();
    configureDefaults(*decollider);
    camera->addExtension(std::move(ownedDecollider));

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    constexpr cinerig::EntityId terrainId = 9101;
    constexpr cinerig::EntityId obstacleId = 9102;
    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(40.0f);
    const cinerig::examples::GlRenderer::MeshHandle cube =
        app.renderer().createCube(1.0f);
    const cinerig::examples::GlRenderer::MeshHandle sphere =
        app.renderer().createSphere(0.5f);

    bool previousTerrain = false;
    bool previousDecollision = false;
    bool previousDamping = false;
    bool previousSmoothing = false;
    bool previousReset = false;

    const auto reset = [&]() {
        configureDefaults(*decollider);
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

        const bool terrainDown = app.input().keyDown(SDL_SCANCODE_T);
        const bool decollisionDown = app.input().keyDown(SDL_SCANCODE_C);
        const bool dampingDown = app.input().keyDown(SDL_SCANCODE_D);
        const bool smoothingDown = app.input().keyDown(SDL_SCANCODE_S);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (terrainDown && !previousTerrain)
            decollider->terrainResolution.enabled = !decollider->terrainResolution.enabled;
        if (decollisionDown && !previousDecollision)
            decollider->decollision.enabled = !decollider->decollision.enabled;
        if (dampingDown && !previousDamping)
        {
            const bool enabled = decollider->decollision.damping > 0.0f;
            decollider->decollision.damping = enabled ? 0.0f : 0.5f;
            decollider->terrainResolution.damping = enabled ? 0.0f : 0.5f;
        }
        if (smoothingDown && !previousSmoothing)
        {
            decollider->decollision.smoothingTime =
                decollider->decollision.smoothingTime > 0.0f ? 0.0f : 0.2f;
        }
        if (resetDown && !previousReset)
            reset();

        previousTerrain = terrainDown;
        previousDecollision = decollisionDown;
        previousDamping = dampingDown;
        previousSmoothing = smoothingDown;
        previousReset = resetDown;

        const float terrainTop = 1.5f + 0.6f * std::sin(app.elapsedTime() * 0.9f);
        const cinerig::math::Vec3 terrainCenter{0.0f, terrainTop - 0.5f, 8.0f};
        const cinerig::math::Vec3 terrainHalfExtents{4.0f, 0.5f, 3.0f};
        physics.addBox(
            terrainId,
            cinerig::Bounds{terrainCenter, terrainHalfExtents},
            0u);

        const float obstacleX = 2.8f * std::sin(app.elapsedTime() * 0.8f);
        const cinerig::math::Vec3 obstacleCenter{
            obstacleX,
            terrainTop + 0.6f,
            8.0f};
        const cinerig::math::Vec3 obstacleHalfExtents{0.5f, 0.3f, 0.4f};
        physics.addBox(
            obstacleId,
            cinerig::Bounds{obstacleCenter, obstacleHalfExtents},
            1u);

        cameraSystem.lateUpdate(deltaTime);

        const cinerig::CameraState& state = camera->state();
        const cinerig::math::Vec3 correction =
            state.correctedPosition() - state.rawPosition;
        const float distanceToTarget =
            (state.correctedPosition() - state.referenceLookAt).length();
        const float terrainCorrection = correction.y;

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
            cube,
            cinerig::Transform{
                terrainCenter,
                cinerig::math::Quat::identity(),
                terrainHalfExtents * 2.0f},
            {0.20f, 0.46f, 0.38f});
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                obstacleCenter,
                cinerig::math::Quat::identity(),
                obstacleHalfExtents * 2.0f},
            {0.78f, 0.42f, 0.20f});
        app.renderer().drawMesh(
            sphere,
            cinerig::Transform{
                {0.0f, 1.5f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.35f, 0.78f, 1.0f});
        app.renderer().drawTargetMarker({0.0f, 1.5f, 0.0f}, 0.8f, {1.0f, 0.75f, 0.25f});
        app.renderer().setLineWidth(2.0f);
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
        app.hud().addLine("CineRig P3-07 decollider");
        std::ostringstream terrainLine;
        terrainLine << (decollider->terrainResolution.enabled ? "on" : "off")
                    << "  top " << terrainTop
                    << "  correction " << terrainCorrection;
        app.hud().addLine("Terrain", terrainLine.str());
        std::ostringstream decollisionLine;
        decollisionLine << (decollider->decollision.enabled ? "on" : "off")
                        << "  radius " << decollider->cameraRadius
                        << "  layers " << decollider->decollision.obstacleLayers.bits;
        app.hud().addLine("Decollision", decollisionLine.str());
        std::ostringstream positionLine;
        positionLine << "raw y " << state.rawPosition.y
                     << " z " << state.rawPosition.z
                     << " -> corrected y " << state.correctedPosition().y
                     << " z " << state.correctedPosition().z;
        app.hud().addLine("Camera", positionLine.str());
        std::ostringstream correctionLine;
        correctionLine << correction.length() << " units  target distance "
                       << distanceToTarget;
        app.hud().addLine("Correction", correctionLine.str());
        std::ostringstream tuningLine;
        tuningLine << "damping " << decollider->decollision.damping
                   << "  smoothing " << decollider->decollision.smoothingTime;
        app.hud().addLine("Tuning", tuningLine.str());
        app.hud().addLine(
            "Occluders",
            "green platform layer 0 (terrain); orange block layer 1");
        app.hud().addLine(
            "Controls",
            "T terrain  C decollision  D damping  S smoothing  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}

