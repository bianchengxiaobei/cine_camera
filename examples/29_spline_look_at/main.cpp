#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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
            std::cout << "Usage: cinerig_29_spline_look_at [--frames N] "
                         "[--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::BezierSpline makeSpline()
{
    return cinerig::BezierSpline::smooth(
        {
            {0.0f, 2.0f, -8.0f},
            {7.0f, 2.0f, -5.0f},
            {9.0f, 2.0f, 2.0f},
            {3.0f, 2.0f, 8.0f},
            {-4.0f, 2.0f, 8.0f},
            {-9.0f, 2.0f, 2.0f},
            {-7.0f, 2.0f, -6.0f},
        },
        true);
}

std::vector<cinerig::SplineDollyLookAtTargets::Key> makeTargets(
    cinerig::EntityId entityTarget)
{
    return {
        {0.00f, {0, {-9.0f, 6.0f, -4.0f}, 1.0f}},
        {0.25f, {0, {-4.0f, 3.0f, 5.0f}, 1.0f}},
        {0.50f, {entityTarget, {0.0f, 0.0f, 0.0f}, 1.0f}},
        {0.75f, {0, {9.0f, 4.0f, -5.0f}, 1.0f}},
        {1.00f, {0, {-9.0f, 6.0f, -4.0f}, 1.0f}},
    };
}

cinerig::math::Vec3 resolveLookAt(
    const cinerig::SplineDollyLookAtTargets::Item& item,
    const cinerig::examples::SdlWorld& world)
{
    if (item.lookAt != 0 && world.exists(item.lookAt))
    {
        const cinerig::Transform transform = world.transform(item.lookAt);
        return transform.position
            + cinerig::math::rotate(transform.rotation, item.offset);
    }
    return item.offset;
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app(
        "CineRig P4-07 Spline Dolly LookAt Targets", 1280, 720, appOptions);
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

    const cinerig::BezierSpline track = makeSpline();
    const float startPhase = 0.20f;
    const cinerig::math::Vec3 startPosition =
        track.evaluatePosition(startPhase);
    const cinerig::math::Quat startRotation = cinerig::math::lookRotation(
        track.evaluateTangent(startPhase),
        track.evaluateUp(startPhase));

    const cinerig::EntityId entityTarget = world.createEntity({
        {6.0f, 8.0f, 4.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });
    const cinerig::EntityId dollyEntity = world.createEntity({
        startPosition,
        startRotation,
        cinerig::math::Vec3::one,
    });
    const cinerig::EntityId overviewEntity = world.createEntity({
        {0.0f, 21.0f, 24.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId dollyId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("LookAt Dolly");
    const cinerig::CameraId overviewId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Overview");
    auto* dollyCamera = cameraSystem.camera(dollyId);
    auto* overview = cameraSystem.camera(overviewId);
    if (dollyCamera == nullptr || overview == nullptr)
    {
        std::cerr << "Failed to create spline look-at cameras\n";
        return 1;
    }

    dollyCamera->ownerEntity = dollyEntity;
    dollyCamera->priority = cinerig::PrioritySettings(0);
    dollyCamera->standbyUpdate = cinerig::StandbyUpdateMode::Always;
    dollyCamera->lens.fieldOfView = 50.0f;
    dollyCamera->lens.nearClipPlane = 0.1f;
    dollyCamera->lens.farClipPlane = 18.0f;
    dollyCamera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& dolly = dollyCamera->setBody<cinerig::SplineDolly>();
    dolly.spline.spline = &track;
    dolly.spline.units = cinerig::PathIndexUnit::Normalized;
    dolly.spline.position = startPhase;
    dolly.cameraRotation = cinerig::SplineDolly::RotationMode::Spline;

    auto& lookAt =
        dollyCamera->setAim<cinerig::SplineDollyLookAtTargets>();
    lookAt.targets = makeTargets(entityTarget);
    lookAt.defaultItem.easing = 1.0f;
    lookAt.validate();

    const cinerig::math::Vec3 overviewPosition{0.0f, 21.0f, 24.0f};
    overview->ownerEntity = overviewEntity;
    overview->priority = cinerig::PrioritySettings(100);
    overview->lens.fieldOfView = 40.0f;
    overview->lens.nearClipPlane = 0.1f;
    overview->lens.farClipPlane = 100.0f;
    overview->lens.aspect = app.viewportSize().x / app.viewportSize().y;
    world.setTransform(
        overviewEntity,
        cinerig::Transform{
            overviewPosition,
            cinerig::math::lookRotation(
                cinerig::math::normalize(
                    cinerig::math::Vec3{0.0f, 1.0f, 0.0f} - overviewPosition),
                cinerig::math::Vec3::up),
            cinerig::math::Vec3::one});

    output.setDefaultLens(overview->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    app.hud().setTextScale(1.3f);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle knotMesh =
        app.renderer().createSphere(0.16f);
    const cinerig::examples::GlRenderer::MeshHandle keyMesh =
        app.renderer().createSphere(0.18f);
    const cinerig::examples::GlRenderer::MeshHandle lookMesh =
        app.renderer().createSphere(0.42f);

    float phase = startPhase;
    bool paused = false;
    bool previousPause = false;
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
        output.setAspect(aspect);
        dollyCamera->lens.aspect = aspect;
        overview->lens.aspect = aspect;

        const bool pauseDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool easingDown = app.input().keyDown(SDL_SCANCODE_E);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (pauseDown && !previousPause)
            paused = !paused;
        if (easingDown && !previousEasing)
        {
            const bool easing = !(lookAt.targets.front().item.easing > 0.5f);
            for (cinerig::SplineDollyLookAtTargets::Key& key : lookAt.targets)
                key.item.easing = easing ? 1.0f : 0.0f;
            lookAt.defaultItem.easing = easing ? 1.0f : 0.0f;
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            phase = startPhase;
            dolly.spline.position = startPhase;
            lookAt.targets = makeTargets(entityTarget);
            lookAt.defaultItem.easing = 1.0f;
            lookAt.validate();
            cameraSystem.resetCameraState();
            output.reset();
        }

        previousPause = pauseDown;
        previousEasing = easingDown;
        previousReset = resetDown;

        if (!paused)
        {
            phase += deltaTime * 0.045f;
            phase -= std::floor(phase);
        }

        dolly.spline.position = track.convertPosition(
            phase,
            cinerig::PathIndexUnit::Normalized,
            dolly.spline.units);
        cameraSystem.lateUpdate(deltaTime);

        const cinerig::CameraState& state = dollyCamera->state();
        const cinerig::math::Vec3 cameraPosition = state.finalPosition();
        const cinerig::math::Quat cameraRotation = state.finalOrientation();
        const cinerig::math::Vec3 lookAtPoint = state.referenceLookAt;

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.02f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.11f, 0.14f, 0.18f});
        app.renderer().drawGrid(20.0f, 1.0f, {0.20f, 0.24f, 0.30f});

        app.renderer().setLineWidth(3.0f);
        const int pathSamples =
            std::max(48, static_cast<int>(track.curveCount()) * 48);
        cinerig::math::Vec3 previous =
            track.evaluatePosition(0.0f) + cinerig::math::Vec3::up * 0.03f;
        for (int sample = 1; sample <= pathSamples; ++sample)
        {
            const float t = static_cast<float>(sample)
                / static_cast<float>(pathSamples);
            const cinerig::math::Vec3 current =
                track.evaluatePosition(t) + cinerig::math::Vec3::up * 0.03f;
            app.renderer().drawLine(previous, current, {0.25f, 0.84f, 1.0f});
            previous = current;
        }

        for (const cinerig::BezierKnot& knot : track.knots())
        {
            app.renderer().drawMesh(
                knotMesh,
                cinerig::Transform{
                    knot.position + cinerig::math::Vec3::up * 0.18f,
                    cinerig::math::Quat::identity(),
                    cinerig::math::Vec3::one},
                {0.95f, 0.80f, 0.25f});
        }

        for (const cinerig::SplineDollyLookAtTargets::Key& key : lookAt.targets)
        {
            const cinerig::math::Vec3 point =
                resolveLookAt(key.item, world);
            app.renderer().drawMesh(
                keyMesh,
                cinerig::Transform{
                    point,
                    cinerig::math::Quat::identity(),
                    cinerig::math::Vec3::one},
                {0.90f, 0.82f, 0.28f});
        }

        app.renderer().drawMesh(
            lookMesh,
            cinerig::Transform{
                lookAtPoint,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.35f, 0.95f, 0.55f});
        app.renderer().drawTargetMarker(
            lookAtPoint,
            0.95f,
            {0.35f, 0.95f, 0.55f});
        app.renderer().drawLine(
            cameraPosition,
            lookAtPoint,
            {1.0f, 0.55f, 0.22f});

        cinerig::LensSettings frustumLens = dollyCamera->lens;
        frustumLens.farClipPlane = 6.0f;
        app.renderer().setLineWidth(2.0f);
        app.renderer().drawFrustum(
            cameraPosition,
            cameraRotation,
            frustumLens,
            aspect,
            {0.72f, 0.45f, 1.0f});
        app.renderer().drawAxes(
            cinerig::Transform{
                cameraPosition,
                cameraRotation,
                cinerig::math::Vec3::one},
            1.5f);
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine(
            "CineRig P4-07 spline dolly look-at   frame "
            + std::to_string(app.frameCount()));
        app.hud().addLine(
            "Profile", lookAt.targets.front().item.easing > 0.5f
                ? "5 keys, smooth easing"
                : "5 keys, linear");
        std::ostringstream phaseLine;
        phaseLine << phase;
        app.hud().addLine("Position", phaseLine.str());
        std::ostringstream lookLine;
        lookLine << lookAtPoint.x << ", "
                 << lookAtPoint.y << ", "
                 << lookAtPoint.z;
        app.hud().addLine("LookAt", lookLine.str());
        app.hud().addLine(
            "Controls", "Space pause  E easing  R reset");
        app.hud().addLine(
            "Legend", "blue track  yellow keys  green look-at  purple frustum");
        app.hud().render(
            static_cast<int>(viewport.x),
            static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0
            && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
        {
            break;
        }
    }

    return 0;
}
