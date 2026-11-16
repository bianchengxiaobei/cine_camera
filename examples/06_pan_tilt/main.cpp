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
            std::cout << "Usage: cinerig_06_pan_tilt [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

const char* referenceFrameName(cinerig::PanTilt::ReferenceFrame frame)
{
    switch (frame)
    {
    case cinerig::PanTilt::ReferenceFrame::World:
        return "World";
    case cinerig::PanTilt::ReferenceFrame::TrackingTarget:
        return "TrackingTarget";
    case cinerig::PanTilt::ReferenceFrame::LookAtTarget:
        return "LookAtTarget";
    case cinerig::PanTilt::ReferenceFrame::ParentObject:
    default:
        return "ParentObject";
    }
}

const char* recenterModeName(cinerig::PanTilt::RecenterTargetMode mode)
{
    switch (mode)
    {
    case cinerig::PanTilt::RecenterTargetMode::TrackingTargetForward:
        return "tracking forward";
    case cinerig::PanTilt::RecenterTargetMode::LookAtTargetForward:
        return "look-at forward";
    case cinerig::PanTilt::RecenterTargetMode::AxisCenter:
    default:
        return "axis center";
    }
}

cinerig::math::Quat targetRotationAt(float time)
{
    const cinerig::math::Quat yaw = cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::up,
        time * 0.45f);
    return cinerig::math::normalize(yaw);
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P2-06 PanTilt", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 targetPosition{0.0f, 1.0f, 0.0f};
    const cinerig::EntityId targetEntity = world.createEntity({
        targetPosition,
        targetRotationAt(0.0f),
        cinerig::math::Vec3::one,
    });

    const cinerig::math::Vec3 cameraPosition{0.0f, 1.6f, 7.0f};
    const cinerig::EntityId cameraEntity = world.createEntity({
        cameraPosition,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("PanTilt Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create pan/tilt camera\n";
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

    auto& panTilt = camera->setAim<cinerig::PanTilt>();
    panTilt.panAxis.value = 0.0f;
    panTilt.tiltAxis.value = 5.0f;
    panTilt.panAxis.recentering.wait = 0.8f;
    panTilt.panAxis.recentering.time = 1.2f;
    panTilt.tiltAxis.recentering.wait = 0.8f;
    panTilt.tiltAxis.recentering.time = 1.2f;

    cinerig::InputAxisController inputController;
    inputController.synchronize(*camera);
    inputController.bind("Look X (Pan)", "Horizontal");
    inputController.bind("Look Y (Tilt)", "Vertical");
    if (auto* binding = inputController.find("Look X (Pan)"))
        binding->gain = 80.0f;
    if (auto* binding = inputController.find("Look Y (Tilt)"))
        binding->gain = -55.0f;

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createSphere(0.4f);

    bool targetPaused = false;
    float pathTime = 0.0f;
    bool previousReference = false;
    bool previousRecenterMode = false;
    bool previousRecenter = false;
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

        const bool referenceDown = app.input().keyDown(SDL_SCANCODE_F);
        const bool recenterModeDown = app.input().keyDown(SDL_SCANCODE_V);
        const bool recenterDown = app.input().keyDown(SDL_SCANCODE_C);
        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (referenceDown && !previousReference)
        {
            const int next = (static_cast<int>(panTilt.referenceFrame) + 1) % 4;
            panTilt.referenceFrame = static_cast<cinerig::PanTilt::ReferenceFrame>(next);
            inputController.cancelInput();
        }
        if (recenterModeDown && !previousRecenterMode)
        {
            const int next = (static_cast<int>(panTilt.recenterTarget) + 1) % 3;
            panTilt.recenterTarget = static_cast<cinerig::PanTilt::RecenterTargetMode>(next);
        }
        if (recenterDown && !previousRecenter)
        {
            const bool enabled = !panTilt.panAxis.recentering.enabled;
            panTilt.panAxis.recentering.enabled = enabled;
            panTilt.tiltAxis.recentering.enabled = enabled;
        }
        if (spaceDown && !previousSpace)
            targetPaused = !targetPaused;
        if (resetDown && !previousReset)
        {
            pathTime = 0.0f;
            targetPaused = false;
            panTilt.panAxis.value = 0.0f;
            panTilt.tiltAxis.value = 5.0f;
            panTilt.panAxis.recentering.enabled = false;
            panTilt.tiltAxis.recentering.enabled = false;
            panTilt.referenceFrame = cinerig::PanTilt::ReferenceFrame::ParentObject;
            panTilt.recenterTarget = cinerig::PanTilt::RecenterTargetMode::AxisCenter;
            inputController.cancelInput();
            world.setTransform(targetEntity, {
                targetPosition,
                targetRotationAt(0.0f),
                cinerig::math::Vec3::one,
            });
            cameraSystem.resetCameraState();
            output.reset();
        }

        previousReference = referenceDown;
        previousRecenterMode = recenterModeDown;
        previousRecenter = recenterDown;
        previousSpace = spaceDown;
        previousReset = resetDown;

        if (!targetPaused)
            pathTime += deltaTime;
        const cinerig::math::Quat targetRotation = targetRotationAt(pathTime);
        world.setTransform(targetEntity, {
            targetPosition,
            targetRotation,
            cinerig::math::Vec3::one,
        });

        inputController.update(app.input(), deltaTime);
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
            targetMesh,
            cinerig::Transform{
                targetPosition,
                targetRotation,
                cinerig::math::Vec3::one},
            {0.95f, 0.65f, 0.25f});
        app.renderer().drawTargetMarker(targetPosition, 0.5f);

        const cinerig::math::Vec3 cameraForward =
            cinerig::math::rotate(output.state().finalOrientation(), cinerig::math::Vec3::forward);
        app.renderer().drawLine(
            cameraPosition,
            cameraPosition + cameraForward * 4.0f,
            {0.35f, 0.7f, 1.0f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P2-06 pan tilt");
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

        std::ostringstream forwardLine;
        forwardLine << "(" << cameraForward.x << ", " << cameraForward.y << ", " << cameraForward.z << ")";
        app.hud().addLine("Forward", forwardLine.str());

        std::ostringstream axisLine;
        axisLine << "pan " << panTilt.panAxis.getClampedValue()
                 << " / tilt " << panTilt.tiltAxis.getClampedValue();
        app.hud().addLine("Axes", axisLine.str());

        app.hud().addLine("Reference", referenceFrameName(panTilt.referenceFrame));
        app.hud().addLine(
            "Recenter",
            panTilt.panAxis.recentering.enabled
                ? std::string("on / ") + recenterModeName(panTilt.recenterTarget)
                : "off");
        app.hud().addLine("Target state", targetPaused ? "paused" : "rotating");
        app.hud().addLine("Input", "controller: 2 axes");
        app.hud().addLine(
            "Controls",
            "A/D pan  W/S tilt  F frame  V goal  C recenter  Space pause  R reset");
        app.hud().addLine(
            "Frame",
            std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}




