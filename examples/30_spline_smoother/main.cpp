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
            std::cout << "Usage: cinerig_30_spline_smoother [--frames N] "
                         "[--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

const std::vector<cinerig::math::Vec3> kControlPoints{
    {-9.0f, 1.5f, -5.0f},
    {-5.0f, 2.5f, -6.0f},
    {-2.0f, 1.0f, -1.0f},
    {1.0f, 3.0f, 3.0f},
    {4.0f, 1.5f, 5.0f},
    {7.0f, 2.5f, 1.0f},
    {9.0f, 1.0f, -4.0f},
};

void drawPath(
    cinerig::examples::GlRenderer& renderer,
    const cinerig::BezierSpline& spline,
    cinerig::math::Vec3 color,
    float width)
{
    renderer.setLineWidth(width);
    const int samples = std::max(
        32,
        static_cast<int>(spline.curveCount()) * 28);
    cinerig::math::Vec3 previous =
        spline.evaluatePosition(0.0f) + cinerig::math::Vec3::up * 0.04f;
    for (int sample = 1; sample <= samples; ++sample)
    {
        const float t = static_cast<float>(sample)
            / static_cast<float>(samples);
        const cinerig::math::Vec3 current =
            spline.evaluatePosition(t) + cinerig::math::Vec3::up * 0.04f;
        renderer.drawLine(previous, current, color);
        previous = current;
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
        "CineRig P4-08 Spline Smoother", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 cameraPosition{0.0f, 17.0f, 19.0f};
    const cinerig::math::Vec3 lookDirection = cinerig::math::normalize(
        cinerig::math::Vec3{0.0f, 1.0f, 0.0f} - cameraPosition);
    const cinerig::EntityId cameraEntity = world.createEntity({
        cameraPosition,
        cinerig::math::lookRotation(lookDirection, cinerig::math::Vec3::up),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Smoother Overview");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create smoother overview camera\n";
        return 1;
    }

    camera->ownerEntity = cameraEntity;
    camera->priority = cinerig::PrioritySettings(100);
    camera->lens.fieldOfView = 44.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 100.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    cinerig::BezierSpline rawSpline =
        cinerig::BezierSpline::linear(kControlPoints, false);
    cinerig::BezierSpline smoothedSpline = rawSpline;

    cinerig::SplineSmoother smoother;
    smoother.spline = &smoothedSpline;
    if (!smoother.smoothNow())
    {
        std::cerr << "Failed to smooth spline\n";
        return 1;
    }

    bool smoothingEnabled = true;
    bool paused = false;
    bool previousSpace = false;
    bool previousSmoothing = false;
    bool previousReset = false;
    float phase = 0.18f;
    int direction = 1;

    app.hud().setTextScale(1.5f);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle markerMesh =
        app.renderer().createSphere(0.48f);
    const cinerig::examples::GlRenderer::MeshHandle knotMesh =
        app.renderer().createSphere(0.20f);

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
        const bool smoothingDown = app.input().keyDown(SDL_SCANCODE_S);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (spaceDown && !previousSpace)
            paused = !paused;
        if (smoothingDown && !previousSmoothing)
        {
            smoothingEnabled = !smoothingEnabled;
            if (smoothingEnabled)
                smoother.smoothNow();
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            smoothingEnabled = true;
            phase = 0.18f;
            direction = 1;
            smoother.smoothNow();
            cameraSystem.resetCameraState();
            output.reset();
        }

        previousSpace = spaceDown;
        previousSmoothing = smoothingDown;
        previousReset = resetDown;

        if (!paused)
        {
            phase += deltaTime * 0.11f * static_cast<float>(direction);
            if (phase >= 1.0f)
            {
                phase = 1.0f;
                direction = -1;
            }
            else if (phase <= 0.0f)
            {
                phase = 0.0f;
                direction = 1;
            }
        }

        const cinerig::BezierSpline& activeSpline =
            smoothingEnabled ? smoothedSpline : rawSpline;
        const cinerig::math::Vec3 point =
            activeSpline.evaluatePosition(phase);
        const cinerig::math::Vec3 tangent =
            activeSpline.evaluateTangent(phase);
        const cinerig::math::Vec3 up = activeSpline.evaluateUp(phase);

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
        app.renderer().drawGrid(26.0f, 1.0f, {0.20f, 0.24f, 0.30f});

        if (smoothingEnabled)
        {
            drawPath(app.renderer(), rawSpline, {0.62f, 0.34f, 0.16f}, 1.7f);
            drawPath(app.renderer(), smoothedSpline, {0.22f, 0.84f, 1.0f}, 3.2f);
        }
        else
        {
            drawPath(app.renderer(), smoothedSpline, {0.14f, 0.45f, 0.55f}, 1.7f);
            drawPath(app.renderer(), rawSpline, {1.0f, 0.52f, 0.20f}, 3.2f);
        }

        for (const cinerig::BezierKnot& knot : activeSpline.knots())
        {
            app.renderer().drawMesh(
                knotMesh,
                cinerig::Transform{
                    knot.position + cinerig::math::Vec3::up * 0.18f,
                    cinerig::math::Quat::identity(),
                    cinerig::math::Vec3::one},
                {0.96f, 0.80f, 0.24f});
            app.renderer().drawTargetMarker(
                knot.position + cinerig::math::Vec3::up * 0.06f,
                0.55f,
                {0.96f, 0.74f, 0.20f});

            app.renderer().setLineWidth(1.8f);
            const cinerig::math::Vec3 handleOrigin =
                knot.position + cinerig::math::Vec3::up * 0.10f;
            app.renderer().drawLine(
                handleOrigin,
                handleOrigin + knot.tangentOut,
                {1.0f, 0.50f, 0.18f});
            app.renderer().drawLine(
                handleOrigin,
                handleOrigin + knot.tangentIn,
                {0.28f, 0.52f, 1.0f});

            const cinerig::math::Vec3 knotUp =
                cinerig::math::rotate(knot.rotation, cinerig::math::Vec3::up);
            app.renderer().drawLine(
                handleOrigin,
                handleOrigin + knotUp * 0.8f,
                {0.28f, 0.92f, 0.55f});
        }

        const cinerig::math::Vec3 markerPosition =
            point + cinerig::math::Vec3::up * 0.52f;
        app.renderer().drawMesh(
            markerMesh,
            cinerig::Transform{
                markerPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            smoothingEnabled
                ? cinerig::math::Vec3{1.0f, 0.48f, 0.18f}
                : cinerig::math::Vec3{1.0f, 0.70f, 0.24f});
        app.renderer().setLineWidth(2.2f);
        app.renderer().drawLine(
            markerPosition,
            markerPosition + tangent * 2.0f,
            {1.0f, 0.42f, 0.16f});
        app.renderer().drawLine(
            markerPosition,
            markerPosition + up * 1.5f,
            {0.28f, 0.95f, 0.55f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine(
            "CineRig P4-08 spline smoother   frame "
            + std::to_string(app.frameCount()));
        app.hud().addLine(
            "Active",
            smoothingEnabled ? "smoothed" : "raw linear");
        std::ostringstream lengthLine;
        lengthLine << "raw " << rawSpline.length()
                   << "  smoothed " << smoothedSpline.length()
                   << "  knots " << activeSpline.knotCount();
        app.hud().addLine("Length", lengthLine.str());
        std::ostringstream tangentLine;
        tangentLine << "phase " << phase
                    << "  tangent " << tangent.x << ", " << tangent.y
                    << ", " << tangent.z;
        app.hud().addLine("Sample", tangentLine.str());
        app.hud().addLine(
            "Smoothing",
            smoothingEnabled
                ? "on  autoSmooth " + std::string(
                    smoother.autoSmooth ? "true" : "false")
                : "off  press S to apply");
        app.hud().addLine(
            "Controls", "S smoothing  Space pause  R reset");
        app.hud().addLine(
            "Legend", "orange raw/handle-out  cyan smooth  blue handle-in  green up");
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
