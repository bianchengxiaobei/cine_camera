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
            std::cout << "Usage: cinerig_27_spline_cart [--frames N] "
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

const char* updateMethodName(cinerig::SplineCart::UpdateMethod method)
{
    switch (method)
    {
    case cinerig::SplineCart::UpdateMethod::Update:
        return "update";
    case cinerig::SplineCart::UpdateMethod::FixedUpdate:
        return "fixedUpdate";
    case cinerig::SplineCart::UpdateMethod::LateUpdate:
        return "lateUpdate";
    }
    return "unknown";
}

cinerig::SplineCart::UpdateMethod nextUpdateMethod(
    cinerig::SplineCart::UpdateMethod method)
{
    switch (method)
    {
    case cinerig::SplineCart::UpdateMethod::Update:
        return cinerig::SplineCart::UpdateMethod::FixedUpdate;
    case cinerig::SplineCart::UpdateMethod::FixedUpdate:
        return cinerig::SplineCart::UpdateMethod::LateUpdate;
    case cinerig::SplineCart::UpdateMethod::LateUpdate:
        return cinerig::SplineCart::UpdateMethod::Update;
    }
    return cinerig::SplineCart::UpdateMethod::Update;
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

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app(
        "CineRig P4-05 Spline Cart", 1280, 720, appOptions);
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
    const cinerig::math::Vec3 startPosition = track.evaluatePosition(0.0f);
    const cinerig::math::Quat startRotation = cinerig::math::lookRotation(
        track.evaluateTangent(0.0f),
        track.evaluateUp(0.0f));

    const cinerig::EntityId targetEntity = world.createEntity({
        {3.0f, 2.0f, 2.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });
    const cinerig::EntityId cartEntity = world.createEntity({
        startPosition,
        startRotation,
        cinerig::math::Vec3{0.8f, 0.7f, 1.6f},
    });
    const cinerig::EntityId overviewEntity = world.createEntity({
        {0.0f, 21.0f, 24.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId overviewId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Overview");
    auto* overview = cameraSystem.camera(overviewId);
    if (overview == nullptr)
    {
        std::cerr << "Failed to create spline cart overview camera\n";
        return 1;
    }

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

    cinerig::SplineCart cart;
    cart.spline.spline = &track;
    cart.spline.units = cinerig::PathIndexUnit::Normalized;
    cart.spline.position = 0.0f;
    cart.trackingTarget = targetEntity;
    cart.automaticDolly.enabled = true;
    auto nearest = std::make_shared<cinerig::SplineAutoDolly::NearestPointToTarget>();
    nearest->searchResolution = 24;
    nearest->searchIteration = 8;
    cart.automaticDolly.method = nearest;
    cart.validate();

    app.hud().setTextScale(1.3f);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh =
        app.renderer().createSphere(0.38f);
    const cinerig::examples::GlRenderer::MeshHandle knotMesh =
        app.renderer().createSphere(0.16f);
    const cinerig::examples::GlRenderer::MeshHandle cartMesh =
        app.renderer().createCube(1.0f);

    bool autoMode = true;
    float manualPhase = 0.0f;
    bool previousAuto = false;
    bool previousUnits = false;
    bool previousUpdateMethod = false;
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
        overview->lens.aspect = aspect;

        const bool autoDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool unitsDown = app.input().keyDown(SDL_SCANCODE_U);
        const bool methodDown = app.input().keyDown(SDL_SCANCODE_M);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (autoDown && !previousAuto)
        {
            autoMode = !autoMode;
            if (!autoMode)
                manualPhase = cart.spline.normalizedPosition();
        }
        if (unitsDown && !previousUnits)
            cart.spline.changeUnitPreservePosition(nextUnit(cart.spline.units));
        if (methodDown && !previousUpdateMethod)
            cart.updateMethod = nextUpdateMethod(cart.updateMethod);
        if (resetDown && !previousReset)
        {
            autoMode = true;
            manualPhase = 0.0f;
            cart.spline.units = cinerig::PathIndexUnit::Normalized;
            cart.spline.position = 0.0f;
            cart.updateMethod = cinerig::SplineCart::UpdateMethod::Update;
            cart.trackingTarget = targetEntity;
            cart.automaticDolly.enabled = true;
            cameraSystem.resetCameraState();
            output.reset();
        }

        previousAuto = autoDown;
        previousUnits = unitsDown;
        previousUpdateMethod = methodDown;
        previousReset = resetDown;

        const float time = app.elapsedTime();
        const cinerig::math::Vec3 targetPosition{
            3.0f * std::sin(time * 0.62f),
            2.0f,
            1.5f + 3.2f * std::sin(time * 0.47f + 1.1f),
        };
        world.setTransform(
            targetEntity,
            cinerig::Transform{
                targetPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one});

        if (autoMode)
        {
            cart.automaticDolly.enabled = true;
        }
        else
        {
            cart.automaticDolly.enabled = false;
            manualPhase += deltaTime * 0.055f;
            manualPhase -= std::floor(manualPhase);
            cart.spline.position = track.convertPosition(
                manualPhase,
                cinerig::PathIndexUnit::Normalized,
                cart.spline.units);
        }

        switch (cart.updateMethod)
        {
        case cinerig::SplineCart::UpdateMethod::Update:
            cart.update(world, cartEntity, deltaTime);
            break;
        case cinerig::SplineCart::UpdateMethod::FixedUpdate:
            cart.fixedUpdate(world, cartEntity, world.fixedDeltaTime());
            break;
        case cinerig::SplineCart::UpdateMethod::LateUpdate:
            cart.lateUpdate(world, cartEntity, deltaTime);
            break;
        }

        cameraSystem.lateUpdate(deltaTime);

        const cinerig::Transform cartTransform = world.transform(cartEntity);

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
            app.renderer().drawTargetMarker(
                knot.position + cinerig::math::Vec3::up * 0.08f,
                0.45f,
                {0.95f, 0.75f, 0.22f});
        }

        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{
                targetPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.35f, 0.95f, 0.55f});
        app.renderer().drawTargetMarker(
            targetPosition,
            0.9f,
            {0.35f, 0.95f, 0.55f});

        app.renderer().drawMesh(
            cartMesh,
            cartTransform,
            {1.0f, 0.48f, 0.18f});
        app.renderer().drawAxes(cartTransform, 1.4f);
        app.renderer().setLineWidth(2.2f);
        app.renderer().drawLine(
            cartTransform.position,
            cartTransform.position
                + cinerig::math::rotate(
                    cartTransform.rotation,
                    cinerig::math::Vec3::forward) * 3.0f,
            {1.0f, 0.35f, 0.15f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine(
            "CineRig P4-05 spline cart   frame "
            + std::to_string(app.frameCount()));
        app.hud().addLine(
            "Mode", autoMode ? "auto: nearest target" : "manual: constant motion");
        app.hud().addLine(
            "Update", updateMethodName(cart.updateMethod));
        std::ostringstream unitLine;
        unitLine << unitName(cart.spline.units)
                 << "  " << cart.spline.position
                 << " / " << cart.spline.maxPosition();
        app.hud().addLine("Units", unitLine.str());
        std::ostringstream cartLine;
        cartLine << cartTransform.position.x << ", "
                 << cartTransform.position.y << ", "
                 << cartTransform.position.z;
        app.hud().addLine("Cart", cartLine.str());
        app.hud().addLine(
            "Controls", "Space auto/manual  U units  M update  R reset");
        app.hud().addLine(
            "Legend", "blue track  green target  orange cart");
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
