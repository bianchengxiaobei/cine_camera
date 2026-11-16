#include <algorithm>
#include <cstdint>
#include <cmath>
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
            std::cout << "Usage: cinerig_11_hard_look_at [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 targetPositionAt(float time)
{
    return {
        2.5f * std::sin(time * 0.55f),
        1.2f + 0.35f * std::sin(time * 1.2f),
        1.0f + 0.8f * std::cos(time * 0.4f),
    };
}

cinerig::math::Quat targetRotationAt(float time)
{
    return cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::up,
        time * 0.7f);
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P2-11 HardLookAt", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 initialTargetPosition = targetPositionAt(0.0f);
    const cinerig::math::Quat initialTargetRotation = targetRotationAt(0.0f);
    const cinerig::EntityId targetEntity = world.createEntity({
        initialTargetPosition,
        initialTargetRotation,
        cinerig::math::Vec3::one,
    });

    const cinerig::math::Vec3 cameraPosition{0.0f, 2.0f, 8.0f};
    const cinerig::EntityId cameraEntity = world.createEntity({
        cameraPosition,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Hard Look At Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create hard-look-at camera\n";
        return 1;
    }

    camera->ownerEntity = cameraEntity;
    camera->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
    camera->targets.customLookAt = true;
    camera->lens.fieldOfView = 55.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& hardLook = camera->setAim<cinerig::HardLookAt>();
    const cinerig::math::Vec3 enabledOffset{0.9f, 0.35f, 0.0f};
    hardLook.lookAtOffset = enabledOffset;

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createSphere(0.45f);
    const cinerig::examples::GlRenderer::MeshHandle aimMesh = app.renderer().createSphere(0.18f);

    bool offsetEnabled = true;
    bool paused = false;
    float pathTime = 0.0f;
    bool previousSpace = false;
    bool previousOffset = false;
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
        const bool offsetDown = app.input().keyDown(SDL_SCANCODE_O);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);
        if (spaceDown && !previousSpace)
            paused = !paused;
        if (offsetDown && !previousOffset)
        {
            offsetEnabled = !offsetEnabled;
            hardLook.lookAtOffset = offsetEnabled ? enabledOffset : cinerig::math::Vec3{};
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            offsetEnabled = true;
            pathTime = 0.0f;
            hardLook.lookAtOffset = enabledOffset;
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousSpace = spaceDown;
        previousOffset = offsetDown;
        previousReset = resetDown;

        if (!paused)
            pathTime += deltaTime;
        const cinerig::math::Vec3 targetPosition = targetPositionAt(pathTime);
        const cinerig::math::Quat targetRotation = targetRotationAt(pathTime);
        world.setTransform(targetEntity, {
            targetPosition,
            targetRotation,
            cinerig::math::Vec3::one,
        });

        cameraSystem.lateUpdate(deltaTime);

        const cinerig::math::Vec3 aimPoint =
            targetPosition + cinerig::math::rotate(targetRotation, hardLook.lookAtOffset);

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
                targetPosition,
                targetRotation,
                cinerig::math::Vec3::one},
            1.2f);
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{targetPosition, targetRotation, cinerig::math::Vec3::one},
            {0.35f, 0.75f, 1.0f});
        app.renderer().drawMesh(
            aimMesh,
            cinerig::Transform{aimPoint, cinerig::math::Quat::identity(), cinerig::math::Vec3::one},
            {1.0f, 0.55f, 0.2f});
        app.renderer().drawTargetMarker(aimPoint, 0.45f, {1.0f, 0.75f, 0.25f});
        app.renderer().drawLine(
            cameraPosition,
            aimPoint,
            {0.75f, 0.78f, 0.85f});
        app.renderer().drawCameraFrustum(output, {0.8f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P2-11 hard look at");
        app.hud().addLine(
            "Active",
            cameraSystem.brain(brainId)->activeVirtualCamera() != nullptr
                ? std::string(cameraSystem.brain(brainId)->activeVirtualCamera()->name())
                : "none");

        const cinerig::math::Vec3 cameraNow = output.state().finalPosition();
        const cinerig::math::Vec3 cameraForward =
            cinerig::math::rotate(output.state().finalOrientation(), cinerig::math::Vec3::forward);
        const cinerig::math::Vec3 toAim = aimPoint - cameraNow;
        float aimError = 0.0f;
        if (toAim.length() > cinerig::math::Epsilon)
        {
            const float cosine = std::clamp(
                cinerig::math::dot(cameraForward, cinerig::math::normalize(toAim)),
                -1.0f,
                1.0f);
            aimError = std::acos(cosine) * cinerig::math::RadToDeg;
        }

        std::ostringstream targetLine;
        targetLine << "(" << targetPosition.x << ", "
                   << targetPosition.y << ", " << targetPosition.z << ")";
        app.hud().addLine("Target", targetLine.str());
        std::ostringstream aimLine;
        aimLine << "(" << aimPoint.x << ", " << aimPoint.y << ", " << aimPoint.z << ")";
        app.hud().addLine("Aim point", aimLine.str());
        std::ostringstream errorLine;
        errorLine << aimError << " deg";
        app.hud().addLine("Aim error", errorLine.str());
        app.hud().addLine("Offset", offsetEnabled ? "target-local (0.9, 0.35, 0)" : "none");
        app.hud().addLine("Target state", paused ? "paused" : "moving/rotating");
        app.hud().addLine("Controls", "Space pause  O offset  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}
