#include <algorithm>
#include <array>
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
            std::cout << "Usage: cinerig_05_orbital_follow [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 targetPositionAt(float time)
{
    return {
        3.8f * std::sin(time * 0.45f),
        1.2f + 0.3f * std::sin(time * 1.4f),
        2.4f * std::sin(time * 0.32f + 0.8f),
    };
}

cinerig::math::Quat targetRotationAt(float time)
{
    const cinerig::math::Quat yaw = cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::up,
        time * 0.55f);
    const cinerig::math::Quat roll = cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::forward,
        20.0f * cinerig::math::DegToRad);
    return cinerig::math::normalize(yaw * roll);
}

cinerig::math::Vec3 sphereOffset(
    float radius,
    float verticalDegrees,
    float horizontalDegrees)
{
    const cinerig::math::Quat rotation = cinerig::math::quatFromEuler({
        verticalDegrees * cinerig::math::DegToRad,
        horizontalDegrees * cinerig::math::DegToRad,
        0.0f,
    });
    return cinerig::math::rotate(
        rotation,
        cinerig::math::Vec3{0.0f, 0.0f, -radius});
}

const char* orbitStyleName(int style)
{
    return style == 0 ? "Sphere" : "ThreeRing";
}

const char* bindingModeName(cinerig::BindingMode mode)
{
    switch (mode)
    {
    case cinerig::BindingMode::LockToTargetOnAssign:
        return "OnAssign";
    case cinerig::BindingMode::LockToTargetWithWorldUp:
        return "WithWorldUp";
    case cinerig::BindingMode::LockToTargetNoRoll:
        return "NoRoll";
    case cinerig::BindingMode::LockToTarget:
        return "LockToTarget";
    case cinerig::BindingMode::LazyFollow:
        return "LazyFollow";
    case cinerig::BindingMode::WorldSpace:
    default:
        return "WorldSpace";
    }
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

void applyDampingMode(int mode, cinerig::OrbitalFollow& orbital)
{
    float value = 1.0f;
    if (mode == 1)
        value = 0.0f;
    else if (mode == 3)
        value = 1.5f;
    orbital.trackerSettings.positionDamping = cinerig::math::Vec3{value};
    orbital.trackerSettings.rotationDamping = cinerig::math::Vec3{value};
    orbital.trackerSettings.quaternionDamping = value;
}

void drawCircle(
    cinerig::examples::GlRenderer& renderer,
    cinerig::math::Vec3 center,
    float radius,
    float height,
    cinerig::math::Vec3 color)
{
    constexpr int segments = 48;
    cinerig::math::Vec3 previous = center + cinerig::math::Vec3{radius, height, 0.0f};
    for (int index = 1; index <= segments; ++index)
    {
        const float angle = static_cast<float>(index) / static_cast<float>(segments)
            * cinerig::math::TwoPi;
        const cinerig::math::Vec3 point = center + cinerig::math::Vec3{
            radius * std::cos(angle),
            height,
            radius * std::sin(angle),
        };
        renderer.drawLine(previous, point, color);
        previous = point;
    }
}

void drawOrbitRig(
    cinerig::examples::GlRenderer& renderer,
    cinerig::math::Vec3 target,
    const cinerig::OrbitalFollow& orbital)
{
    const float radial = orbital.radialAxis.getClampedValue();
    if (orbital.orbitStyle == cinerig::OrbitalFollow::OrbitStyle::ThreeRing)
    {
        drawCircle(
            renderer, target, orbital.orbits.bottom.radius * radial,
            orbital.orbits.bottom.height, {0.4f, 0.62f, 0.86f});
        drawCircle(
            renderer, target, orbital.orbits.center.radius * radial,
            orbital.orbits.center.height, {0.5f, 0.74f, 0.96f});
        drawCircle(
            renderer, target, orbital.orbits.top.radius * radial,
            orbital.orbits.top.height, {0.62f, 0.84f, 1.0f});
    }
    else
    {
        drawCircle(
            renderer, target, orbital.radius * radial, 0.0f,
            {0.48f, 0.72f, 0.96f});
    }
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P2-04 OrbitalFollow", 1280, 720, appOptions);
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
        cameraSystem.createCamera<cinerig::VirtualCamera>("Orbital Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create orbital camera\n";
        return 1;
    }

    camera->targets.tracking = cinerig::TargetRef::entity(targetEntity);
    camera->lens.fieldOfView = 50.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& orbital = camera->setBody<cinerig::OrbitalFollow>();
    auto& composer = camera->setAim<cinerig::RotationComposer>();
    composer.damping = {0.15f, 0.15f};
    composer.composition.screenPosition = {0.0f, 0.03f};

    orbital.radius = 10.0f;
    orbital.horizontalAxis.value = 25.0f;
    orbital.verticalAxis.value = 22.0f;
    orbital.radialAxis.range = {1.0f, 2.2f};
    orbital.horizontalAxis.recentering.enabled = true;
    orbital.horizontalAxis.recentering.wait = 0.8f;
    orbital.horizontalAxis.recentering.time = 1.2f;
    int dampingMode = 2;
    applyDampingMode(dampingMode, orbital);

    cinerig::InputAxisController inputController;
    inputController.synchronize(*camera);
    inputController.bind("Look Orbit X", "Horizontal");
    inputController.bind("Look Orbit Y", "Vertical");
    if (auto* binding = inputController.find("Look Orbit X"))
        binding->gain = 80.0f;
    if (auto* binding = inputController.find("Look Orbit Y"))
        binding->gain = 55.0f;
    if (auto* binding = inputController.find("Orbit Scale"))
        binding->enabled = false;

    const cinerig::math::Vec3 initialOffset =
        sphereOffset(orbital.radius, orbital.verticalAxis.value, orbital.horizontalAxis.value);
    cinerig::Transform cameraTransform;
    cameraTransform.position = targetPosition + initialOffset;
    cameraTransform.rotation = cinerig::math::lookRotation(
        targetPosition - cameraTransform.position,
        world.worldUp());
    camera->ownerEntity = world.createEntity(cameraTransform);

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createSphere(0.4f);

    constexpr std::array<cinerig::BindingMode, 6> bindingModes{
        cinerig::BindingMode::WorldSpace,
        cinerig::BindingMode::LockToTarget,
        cinerig::BindingMode::LockToTargetNoRoll,
        cinerig::BindingMode::LockToTargetWithWorldUp,
        cinerig::BindingMode::LockToTargetOnAssign,
        cinerig::BindingMode::LazyFollow,
    };
    int bindingModeIndex = 0;
    int orbitStyle = 0;
    bool targetPaused = false;
    float pathTime = 0.0f;
    bool previousOne = false;
    bool previousTwo = false;
    bool previousThree = false;
    bool previousStyle = false;
    bool previousBinding = false;
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

        const bool oneDown = app.input().keyDown(SDL_SCANCODE_1);
        const bool twoDown = app.input().keyDown(SDL_SCANCODE_2);
        const bool threeDown = app.input().keyDown(SDL_SCANCODE_3);
        const bool styleDown = app.input().keyDown(SDL_SCANCODE_O);
        const bool bindingDown = app.input().keyDown(SDL_SCANCODE_B);
        const bool recenterDown = app.input().keyDown(SDL_SCANCODE_C);
        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (oneDown && !previousOne)
        {
            dampingMode = 1;
            applyDampingMode(dampingMode, orbital);
        }
        if (twoDown && !previousTwo)
        {
            dampingMode = 2;
            applyDampingMode(dampingMode, orbital);
        }
        if (threeDown && !previousThree)
        {
            dampingMode = 3;
            applyDampingMode(dampingMode, orbital);
        }
        if (styleDown && !previousStyle)
        {
            orbitStyle = (orbitStyle + 1) % 2;
            orbital.orbitStyle = orbitStyle == 0
                ? cinerig::OrbitalFollow::OrbitStyle::Sphere
                : cinerig::OrbitalFollow::OrbitStyle::ThreeRing;
            cameraSystem.resetCameraState();
            output.reset();
        }
        if (bindingDown && !previousBinding)
        {
            bindingModeIndex = (bindingModeIndex + 1)
                % static_cast<int>(bindingModes.size());
            orbital.trackerSettings.bindingMode =
                bindingModes[static_cast<std::size_t>(bindingModeIndex)];
            cameraSystem.resetCameraState();
            output.reset();
        }
        if (recenterDown && !previousRecenter)
            orbital.horizontalAxis.recentering.enabled =
                !orbital.horizontalAxis.recentering.enabled;
        if (spaceDown && !previousSpace)
            targetPaused = !targetPaused;
        if (resetDown && !previousReset)
        {
            pathTime = 0.0f;
            targetPaused = false;
            orbitStyle = 0;
            bindingModeIndex = 0;
            orbital.orbitStyle = cinerig::OrbitalFollow::OrbitStyle::Sphere;
            orbital.trackerSettings.bindingMode = cinerig::BindingMode::WorldSpace;
            orbital.horizontalAxis.value = 25.0f;
            orbital.verticalAxis.value = 22.0f;
            orbital.radialAxis.value = 1.0f;
            orbital.horizontalAxis.recentering.enabled = true;
            inputController.cancelInput();
            targetPosition = targetPositionAt(0.0f);
            targetRotation = targetRotationAt(0.0f);
            world.setTransform(targetEntity, {
                targetPosition,
                targetRotation,
                cinerig::math::Vec3::one,
            });
            cameraSystem.resetCameraState();
            output.reset();
        }

        previousOne = oneDown;
        previousTwo = twoDown;
        previousThree = threeDown;
        previousStyle = styleDown;
        previousBinding = bindingDown;
        previousRecenter = recenterDown;
        previousSpace = spaceDown;
        previousReset = resetDown;

        inputController.update(app.input(), deltaTime);

        float radialInput = app.input().scroll() * 0.08f;
        if (app.input().keyDown(SDL_SCANCODE_Q))
            radialInput -= deltaTime;
        if (app.input().keyDown(SDL_SCANCODE_E))
            radialInput += deltaTime;
        orbital.radialAxis.value = orbital.radialAxis.clampValue(
            orbital.radialAxis.value + radialInput);

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
        drawOrbitRig(app.renderer(), targetPosition, orbital);
        app.renderer().drawAxes(
            cinerig::Transform{{0.0f, 0.0f, 0.0f}, cinerig::math::Quat::identity(), cinerig::math::Vec3::one},
            1.5f);
        app.renderer().drawAxes(
            cinerig::Transform{targetPosition, targetRotation, cinerig::math::Vec3::one},
            0.8f);
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{targetPosition, targetRotation, cinerig::math::Vec3::one},
            {0.95f, 0.65f, 0.25f});
        app.renderer().drawTargetMarker(targetPosition, 0.5f);

        const cinerig::math::Vec3 cameraPosition = output.state().finalPosition();
        app.renderer().drawLine(cameraPosition, targetPosition, {0.35f, 0.7f, 1.0f});
        app.renderer().drawCameraFrustum(output, {0.85f, 0.9f, 1.0f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P2-04 orbital follow");
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

        std::ostringstream axisLine;
        axisLine << "H " << orbital.horizontalAxis.getClampedValue()
                 << " / V " << orbital.verticalAxis.value
                 << " / R " << orbital.radialAxis.value;
        app.hud().addLine("Axes", axisLine.str());
        app.hud().addLine("Input", "controller 2 axes / manual radial");
        std::ostringstream orbitLine;
        orbitLine << orbitStyleName(orbitStyle);
        if (orbital.orbitStyle == cinerig::OrbitalFollow::OrbitStyle::ThreeRing)
            orbitLine << " (three rings)";
        else
            orbitLine << " (r " << static_cast<int>(std::lround(orbital.radius)) << ")";
        app.hud().addLine("Orbit", orbitLine.str());
        app.hud().addLine(
            "Binding",
            bindingModeName(orbital.trackerSettings.bindingMode));
        app.hud().addLine(
            "Damping",
            dampingModeName(dampingMode));
        app.hud().addLine(
            "Recenter",
            orbital.horizontalAxis.recentering.enabled
                ? "on (tracking target)"
                : "off");
        app.hud().addLine("Target state", targetPaused ? "paused" : "moving");
        app.hud().addLine(
            "Keys",
            "A/D orbit, W/S pitch, Q/E or wheel zoom, O style, B binding");
        app.hud().addLine(
            "Keys",
            "C recenter, 1/2/3 damping, Space pause, R reset, Esc quit");

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
