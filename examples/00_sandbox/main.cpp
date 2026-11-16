#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include <cinerig/cinerig.hpp>
#include <cinerig/camera/external_camera.hpp>

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
            std::cout << "Usage: cinerig_00_sandbox [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

}

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P1-05 Sandbox", 1280, 720, appOptions);
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

    const cinerig::EntityId targetEntity = world.createEntity({
        {0.0f, 1.2f, 0.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::math::Vec3 cameraPosition{7.0f, 5.5f, 10.0f};
    const cinerig::math::Vec3 targetPosition{0.0f, 1.2f, 0.0f};
    cinerig::Transform cameraTransform;
    cameraTransform.position = cameraPosition;
    cameraTransform.rotation =
        cinerig::math::lookRotation(targetPosition - cameraPosition, world.worldUp());
    const cinerig::EntityId cameraEntity = world.createEntity(cameraTransform);

    const cinerig::CameraId cameraId = cameraSystem.createCamera<cinerig::ExternalCamera>();
    auto* externalCamera = static_cast<cinerig::ExternalCamera*>(cameraSystem.camera(cameraId));
    if (externalCamera != nullptr)
    {
        externalCamera->cameraEntity = cameraEntity;
        externalCamera->lookAtTarget = cinerig::TargetRef::entity(targetEntity);
        externalCamera->output = &output;
        externalCamera->priority.setValue(10);
    }

    const cinerig::CameraId secondCameraId = cameraSystem.createCamera<cinerig::ExternalCamera>();
    auto* secondCamera = static_cast<cinerig::ExternalCamera*>(cameraSystem.camera(secondCameraId));
    if (secondCamera != nullptr)
    {
        const cinerig::math::Vec3 secondPosition{-7.0f, 4.0f, 9.0f};
        cinerig::Transform secondTransform;
        secondTransform.position = secondPosition;
        secondTransform.rotation =
            cinerig::math::lookRotation(targetPosition - secondPosition, world.worldUp());
        secondCamera->cameraEntity = world.createEntity(secondTransform);
        secondCamera->lookAtTarget = cinerig::TargetRef::entity(targetEntity);
        secondCamera->output = &output;
        secondCamera->priority.setValue(5);
        secondCamera->setEnabled(false);
    }

    const cinerig::examples::GlRenderer::MeshHandle cube = app.renderer().createCube(1.0f);
    const cinerig::examples::GlRenderer::MeshHandle sphere = app.renderer().createSphere(0.55f);
    const cinerig::examples::GlRenderer::MeshHandle cylinder = app.renderer().createCylinder(0.3f, 1.2f);
    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(24.0f);

    bool previousOne = false;
    bool previousTwo = false;
    bool previousOrtho = false;

    while (app.running())
    {
        app.pollEvents();
        if (!app.running())
            break;

        const cinerig::math::Vec2 viewport = app.viewportSize();
        world.setViewportSize(viewport);
        world.setDeltaTime(cinerig::UpdateClock::Late, app.deltaTime());
        world.setTime(app.elapsedTime());
        world.setFrame(cinerig::UpdateClock::Late, app.frameCount());
        output.setViewportSize(viewport);

        const bool oneDown = app.input().keyDown(SDL_SCANCODE_1);
        const bool twoDown = app.input().keyDown(SDL_SCANCODE_2);
        const bool orthoDown = app.input().keyDown(SDL_SCANCODE_O);
        if (oneDown && !previousOne && externalCamera != nullptr)
        {
            externalCamera->setEnabled(true);
            if (secondCamera != nullptr)
                secondCamera->setEnabled(false);
        }
        if (twoDown && !previousTwo && secondCamera != nullptr)
        {
            secondCamera->setEnabled(true);
            if (externalCamera != nullptr)
                externalCamera->setEnabled(false);
        }
        if (orthoDown && !previousOrtho)
            output.toggleProjection();
        previousOne = oneDown;
        previousTwo = twoDown;
        previousOrtho = orthoDown;

        cameraSystem.lateUpdate(app.deltaTime());

        app.beginFrame();
        app.renderer().beginFrame(output);
        cinerig::Transform groundTransform;
        groundTransform.position = {0.0f, -0.01f, 0.0f};
        app.renderer().drawMesh(ground, groundTransform, {0.15f, 0.18f, 0.22f});
        app.renderer().drawGrid(20.0f, 1.0f);
        app.renderer().drawAxes(cinerig::Transform{}, 1.5f);
        app.renderer().drawMesh(
            cube,
            {{0.0f, 0.5f, 0.0f}, cinerig::math::quatFromEuler({0.0f, 0.5f, 0.0f}), cinerig::math::Vec3::one},
            {0.55f, 0.72f, 0.95f});
        app.renderer().drawMesh(
            sphere,
            {{2.5f, 0.55f, 0.0f}, cinerig::math::Quat::identity(), cinerig::math::Vec3::one},
            {0.95f, 0.55f, 0.35f});
        app.renderer().drawMesh(
            cylinder,
            {{-2.5f, 0.6f, 0.0f}, cinerig::math::Quat::identity(), cinerig::math::Vec3::one},
            {0.45f, 0.9f, 0.55f});
        app.renderer().drawTargetMarker(targetPosition, 0.45f);
        app.renderer().drawCameraFrustum(output, {0.85f, 0.9f, 1.0f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P1-05 sandbox");
        app.hud().addLine("Camera", externalCamera != nullptr && externalCamera->isEnabled() ? "A (priority 10)" : "B (priority 5)");
        app.hud().addLine("Active", cameraSystem.brain(brainId)->activeVirtualCamera() != nullptr
                ? std::string(cameraSystem.brain(brainId)->activeVirtualCamera()->name())
                : "none");
        app.hud().addLine("Blending", cameraSystem.brain(brainId)->isBlending() ? "yes" : "no");
        app.hud().addLine("Projection", output.projectionMatrix().at(3, 2) == 0.0f ? "perspective/ortho?" : "perspective");
        app.hud().addLine("Mouse", app.input().mouseCaptured() ? "captured (Esc/right button)" : "released (right button)");
        app.hud().addLine("Keys", "1/2 camera, O projection, WASD+arrows");
        std::ostringstream stats;
        stats << "FPS " << (app.deltaTime() > 0.0f ? 1.0f / app.deltaTime() : 0.0f)
              << " | dt " << app.deltaTime() << " | frame " << app.frameCount();
        app.hud().addLine(stats.str());
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));
        app.endFrame();

        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}

