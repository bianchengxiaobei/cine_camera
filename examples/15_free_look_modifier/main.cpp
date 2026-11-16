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
            std::cout << "Usage: cinerig_15_free_look_modifier [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Quat targetRotationAt(float time)
{
    return cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::up,
        time * 0.2f);
}

void applyModifierSettings(cinerig::FreeLookModifier& freeLook, float easing)
{
    freeLook.validate();
    freeLook.easing = easing;

    for (auto& modifier : freeLook.modifiers)
    {
        if (auto* distance = dynamic_cast<cinerig::FreeLookModifier::DistanceModifier*>(modifier.get()))
            distance->distance = {11.0f, 4.0f};
        else if (auto* damping = dynamic_cast<cinerig::FreeLookModifier::PositionDampingModifier*>(modifier.get()))
            damping->damping = {{0.65f, 0.65f, 0.65f}, {0.04f, 0.04f, 0.04f}};
        else if (auto* composition = dynamic_cast<cinerig::FreeLookModifier::CompositionModifier*>(modifier.get()))
        {
            composition->composition.top.screenPosition = {0.12f, 0.18f};
            composition->composition.bottom.screenPosition = {0.12f, -0.05f};
        }
        else if (auto* lens = dynamic_cast<cinerig::FreeLookModifier::LensModifier*>(modifier.get()))
        {
            lens->lens.top.fieldOfView = 40.0f;
            lens->lens.bottom.fieldOfView = 65.0f;
        }
        else if (auto* tilt = dynamic_cast<cinerig::FreeLookModifier::TiltModifier*>(modifier.get()))
            tilt->tilt = {-4.0f, 4.0f};
    }
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P2-15 FreeLookModifier", 1280, 720, appOptions);
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
    cameraSystem.createBrain(output, brainConfig);

    const cinerig::math::Vec3 targetPosition{0.0f, 1.0f, 0.0f};
    const cinerig::math::Quat initialTargetRotation = targetRotationAt(0.0f);
    const cinerig::EntityId targetEntity = world.createEntity({
        targetPosition,
        initialTargetRotation,
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Free Look Modifier Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create free-look modifier camera\n";
        return 1;
    }

    camera->targets.tracking = cinerig::TargetRef::entity(targetEntity);
    camera->lens.fieldOfView = 55.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& orbital = camera->setBody<cinerig::OrbitalFollow>();
    orbital.radius = 7.0f;
    orbital.horizontalAxis.value = 22.0f;
    orbital.verticalAxis.range = {-15.0f, 45.0f};
    orbital.verticalAxis.value = 17.5f;
    orbital.trackerSettings.positionDamping = {0.2f, 0.2f, 0.2f};

    auto& composer = camera->setAim<cinerig::RotationComposer>();
    composer.damping = {0.12f, 0.12f};
    composer.composition.screenPosition = {0.12f, 0.05f};

    auto ownedFreeLook = std::make_unique<cinerig::FreeLookModifier>();
    auto* freeLook = ownedFreeLook.get();
    freeLook->modifiers.push_back(std::make_unique<cinerig::FreeLookModifier::DistanceModifier>());
    freeLook->modifiers.push_back(std::make_unique<cinerig::FreeLookModifier::PositionDampingModifier>());
    freeLook->modifiers.push_back(std::make_unique<cinerig::FreeLookModifier::CompositionModifier>());
    freeLook->modifiers.push_back(std::make_unique<cinerig::FreeLookModifier::LensModifier>());
    freeLook->modifiers.push_back(std::make_unique<cinerig::FreeLookModifier::TiltModifier>());
    camera->addExtension(std::move(ownedFreeLook));
    freeLook->refreshCache(*camera);
    freeLook->reset(*camera);
    const float defaultEasing = 0.55f;
    applyModifierSettings(*freeLook, defaultEasing);

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createSphere(0.55f);
    const cinerig::examples::GlRenderer::MeshHandle cube = app.renderer().createCube(1.0f);

    bool paused = false;
    float sweepTime = 0.0f;
    float easing = defaultEasing;
    bool previousSpace = false;
    bool previousEasing = false;
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
        const bool easingDown = app.input().keyDown(SDL_SCANCODE_E);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);
        if (spaceDown && !previousSpace)
            paused = !paused;
        if (easingDown && !previousEasing)
        {
            easing = easing > 0.0f ? 0.0f : defaultEasing;
            freeLook->easing = easing;
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            sweepTime = 0.0f;
            easing = defaultEasing;
            orbital.verticalAxis.value = 17.5f;
            freeLook->refreshCache(*camera);
            freeLook->reset(*camera);
            applyModifierSettings(*freeLook, easing);
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousSpace = spaceDown;
        previousEasing = easingDown;
        previousReset = resetDown;

        if (!paused)
            sweepTime += deltaTime;
        const float sweep = 0.5f + 0.45f * std::sin(sweepTime * 0.55f);
        orbital.verticalAxis.value = cinerig::math::lerp(
            orbital.verticalAxis.range.x,
            orbital.verticalAxis.range.y,
            sweep);
        const cinerig::math::Quat targetRotation = targetRotationAt(sweepTime);
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
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {0.0f, 2.0f, -8.0f},
                cinerig::math::Quat::identity(),
                {12.0f, 4.0f, 0.4f}},
            {0.20f, 0.24f, 0.30f});
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {-3.4f, 1.0f, 0.0f},
                cinerig::math::Quat::identity(),
                {0.8f, 2.0f, 0.8f}},
            {0.28f, 0.33f, 0.42f});
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {3.4f, 1.0f, 0.0f},
                cinerig::math::Quat::identity(),
                {0.8f, 2.0f, 0.8f}},
            {0.28f, 0.33f, 0.42f});
        app.renderer().drawAxes(
            cinerig::Transform{targetPosition, targetRotation, cinerig::math::Vec3::one},
            1.6f);
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{targetPosition, targetRotation, cinerig::math::Vec3::one},
            {0.35f, 0.75f, 1.0f});
        app.renderer().drawTargetMarker(targetPosition, 0.65f);
        app.renderer().drawCameraFrustum(output, {0.8f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P2-15 free look modifier");
        const float normalized = orbital.getNormalizedModifierValue();
        std::ostringstream valueLine;
        valueLine << "axis " << orbital.verticalAxis.value
                  << "  norm " << normalized
                  << "  eased " << freeLook->currentValue();
        app.hud().addLine("Vertical", valueLine.str());
        app.hud().addLine("Distance", "7 base  top 11  bottom 4");
        app.hud().addLine("Damping", "0.2 base  top 0.65  bottom 0.04");
        std::ostringstream lensLine;
        lensLine << "55 base  top 40  bottom 65  now "
                 << output.state().lens.fieldOfView;
        app.hud().addLine("FOV", lensLine.str());
        app.hud().addLine("Easing", easing > 0.0f ? "0.55 (smooth center)" : "0 (linear)");
        app.hud().addLine("Controls", "Space pause  E easing  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}

