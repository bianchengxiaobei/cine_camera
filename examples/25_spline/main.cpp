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
            std::cout << "Usage: cinerig_25_spline [--frames N] "
                         "[--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

const char* unitName(cinerig::PathIndexUnit unit)
{
    switch (unit)
    {
    case cinerig::PathIndexUnit::Distance:
        return "distance";
    case cinerig::PathIndexUnit::Normalized:
        return "normalized";
    case cinerig::PathIndexUnit::Knot:
        return "knot";
    }
    return "unknown";
}

cinerig::PathIndexUnit nextUnit(cinerig::PathIndexUnit unit)
{
    switch (unit)
    {
    case cinerig::PathIndexUnit::Distance:
        return cinerig::PathIndexUnit::Normalized;
    case cinerig::PathIndexUnit::Normalized:
        return cinerig::PathIndexUnit::Knot;
    case cinerig::PathIndexUnit::Knot:
        return cinerig::PathIndexUnit::Distance;
    }
    return cinerig::PathIndexUnit::Normalized;
}

cinerig::BezierSpline makeSpline()
{
    return cinerig::BezierSpline::smooth(
        {
            {0.0f, 0.0f, -7.0f},
            {6.5f, 0.0f, -4.5f},
            {8.0f, 0.0f, 1.5f},
            {3.0f, 0.0f, 6.5f},
            {-3.5f, 0.0f, 7.0f},
            {-8.0f, 0.0f, 1.5f},
            {-6.0f, 0.0f, -5.0f},
        },
        true);
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app(
        "CineRig P4-03 Spline Abstractions", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 cameraPosition{0.0f, 15.0f, 20.0f};
    const cinerig::math::Vec3 lookDirection =
        cinerig::math::normalize(cinerig::math::Vec3{0.0f, 0.0f, 0.0f} - cameraPosition);
    const cinerig::EntityId cameraEntity = world.createEntity({
        cameraPosition,
        cinerig::math::lookRotation(lookDirection, cinerig::math::Vec3::up),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Spline Overview");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create spline overview camera\n";
        return 1;
    }

    camera->ownerEntity = cameraEntity;
    camera->priority = cinerig::PrioritySettings(100);
    camera->lens.fieldOfView = 40.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 100.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::BezierSpline spline = makeSpline();
    cinerig::SplineSettings settings;
    settings.spline = &spline;
    settings.units = cinerig::PathIndexUnit::Normalized;
    settings.position = 0.0f;

    const std::vector<cinerig::math::Vec3> controlPoints{
        {0.0f, 0.0f, -7.0f},
        {6.5f, 0.0f, -4.5f},
        {8.0f, 0.0f, 1.5f},
        {3.0f, 0.0f, 6.5f},
        {-3.5f, 0.0f, 7.0f},
        {-8.0f, 0.0f, 1.5f},
        {-6.0f, 0.0f, -5.0f},
    };

    app.hud().setTextScale(1.5f);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(28.0f);
    const cinerig::examples::GlRenderer::MeshHandle markerMesh =
        app.renderer().createSphere(0.45f);
    const cinerig::examples::GlRenderer::MeshHandle knotMesh =
        app.renderer().createSphere(0.18f);

    float phase = 0.0f;
    bool paused = false;
    bool previousSpace = false;
    bool previousUnits = false;
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
        camera->lens.aspect = aspect;

        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool unitsDown = app.input().keyDown(SDL_SCANCODE_U);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (spaceDown && !previousSpace)
            paused = !paused;
        if (unitsDown && !previousUnits)
            settings.changeUnitPreservePosition(nextUnit(settings.units));
        if (resetDown && !previousReset)
        {
            paused = false;
            phase = 0.0f;
            settings.units = cinerig::PathIndexUnit::Normalized;
            settings.position = 0.0f;
            cameraSystem.resetCameraState();
            output.reset();
        }

        previousSpace = spaceDown;
        previousUnits = unitsDown;
        previousReset = resetDown;

        if (!paused)
        {
            phase += deltaTime * 0.075f;
            phase -= std::floor(phase);
        }

        settings.position = spline.convertPosition(
            phase,
            cinerig::PathIndexUnit::Normalized,
            settings.units);

        const float normalized = settings.normalizedPosition();
        const cinerig::math::Vec3 point = spline.evaluatePosition(normalized);
        const cinerig::math::Vec3 tangent = spline.evaluateTangent(normalized);
        const cinerig::math::Vec3 up = spline.evaluateUp(normalized);
        const cinerig::math::Vec3 right = cinerig::math::normalize(
            cinerig::math::cross(tangent, up));
        const cinerig::math::Quat rotation = spline.evaluateRotation(normalized);
        const cinerig::math::Vec3 markerPosition =
            point + cinerig::math::Vec3::up * 0.55f;

        cameraSystem.lateUpdate(deltaTime);

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.02f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.11f, 0.14f, 0.18f});
        app.renderer().drawGrid(18.0f, 1.0f, {0.20f, 0.24f, 0.30f});

        app.renderer().setLineWidth(1.2f);
        for (std::size_t index = 0; index < controlPoints.size(); ++index)
        {
            const cinerig::math::Vec3 a = controlPoints[index];
            const cinerig::math::Vec3 b =
                controlPoints[(index + 1) % controlPoints.size()];
            app.renderer().drawLine(
                {a.x, 0.01f, a.z},
                {b.x, 0.01f, b.z},
                {0.28f, 0.31f, 0.38f});
        }

        app.renderer().setLineWidth(2.6f);
        const int pathSamples =
            std::max(32, static_cast<int>(spline.curveCount()) * 32);
        cinerig::math::Vec3 previous =
            spline.evaluatePosition(0.0f) + cinerig::math::Vec3::up * 0.03f;
        for (int sample = 1; sample <= pathSamples; ++sample)
        {
            const float t = static_cast<float>(sample)
                / static_cast<float>(pathSamples);
            const cinerig::math::Vec3 current =
                spline.evaluatePosition(t) + cinerig::math::Vec3::up * 0.03f;
            app.renderer().drawLine(previous, current, {0.25f, 0.84f, 1.0f});
            previous = current;
        }

        for (const cinerig::BezierKnot& knot : spline.knots())
        {
            app.renderer().drawMesh(
                knotMesh,
                cinerig::Transform{
                    knot.position + cinerig::math::Vec3::up * 0.18f,
                    cinerig::math::Quat::identity(),
                    cinerig::math::Vec3::one},
                {0.95f, 0.80f, 0.25f});
            app.renderer().drawTargetMarker(
                knot.position + cinerig::math::Vec3::up * 0.08f,
                0.5f,
                {0.95f, 0.75f, 0.22f});
        }

        app.renderer().drawMesh(
            markerMesh,
            cinerig::Transform{
                markerPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {1.0f, 0.48f, 0.18f});
        app.renderer().drawAxes(
            cinerig::Transform{
                point + cinerig::math::Vec3::up * 0.12f,
                rotation,
                cinerig::math::Vec3::one},
            1.5f);
        app.renderer().setLineWidth(2.0f);
        app.renderer().drawLine(
            point + cinerig::math::Vec3::up * 0.12f,
            point + tangent * 2.2f,
            {1.0f, 0.42f, 0.16f});
        app.renderer().drawLine(
            point + cinerig::math::Vec3::up * 0.12f,
            point + up * 1.8f,
            {0.28f, 0.95f, 0.55f});
        app.renderer().drawLine(
            point + cinerig::math::Vec3::up * 0.12f,
            point + right * 1.4f,
            {0.32f, 0.55f, 1.0f});
        app.renderer().endFrame();

        const float distance = spline.convertPosition(
            normalized,
            cinerig::PathIndexUnit::Normalized,
            cinerig::PathIndexUnit::Distance);
        const float knot = spline.convertPosition(
            normalized,
            cinerig::PathIndexUnit::Normalized,
            cinerig::PathIndexUnit::Knot);

        app.hud().begin();
        app.hud().addLine(
            "CineRig P4-03 spline abstractions   frame "
            + std::to_string(app.frameCount()));
        std::ostringstream splineLine;
        splineLine << "closed  knots " << spline.knotCount()
                   << "  curves " << spline.curveCount()
                   << "  length " << spline.length();
        app.hud().addLine("Spline", splineLine.str());
        std::ostringstream unitLine;
        unitLine << unitName(settings.units)
                 << "  position " << settings.position
                 << " / " << settings.maxPosition();
        app.hud().addLine("Units", unitLine.str());
        std::ostringstream equivalentLine;
        equivalentLine << "norm " << normalized
                       << "  dist " << distance
                       << "  knot " << knot
                       << "  point " << point.x << ", " << point.y
                       << ", " << point.z;
        app.hud().addLine("Equivalent", equivalentLine.str());
        app.hud().addLine(
            "Controls", "U units  Space pause  R reset");
        app.hud().addLine(
            "Legend", "orange tangent  green up  blue right");
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
