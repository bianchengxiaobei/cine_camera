#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
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

enum class ShapeMode
{
    LRoom = 0,
    Box,
    None,
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
            std::cout << "Usage: cinerig_24_confiner2d [--frames N] "
                         "[--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

const char* shapeName(ShapeMode mode)
{
    switch (mode)
    {
    case ShapeMode::LRoom:
        return "L-room";
    case ShapeMode::Box:
        return "box";
    case ShapeMode::None:
        return "none";
    }
    return "unknown";
}

const char* bakeStateName(cinerig::ConfinerOven::BakingState state)
{
    switch (state)
    {
    case cinerig::ConfinerOven::BakingState::Baking:
        return "baking";
    case cinerig::ConfinerOven::BakingState::Baked:
        return "baked";
    case cinerig::ConfinerOven::BakingState::Timeout:
        return "timeout";
    }
    return "unknown";
}

std::vector<std::vector<cinerig::math::Vec2>> shapePaths(ShapeMode mode)
{
    switch (mode)
    {
    case ShapeMode::LRoom:
        return {{
            {-11.0f, -7.0f},
            {11.0f, -7.0f},
            {11.0f, 7.0f},
            {0.0f, 7.0f},
            {0.0f, 0.0f},
            {-11.0f, 0.0f},
        }};
    case ShapeMode::Box:
        return {{
            {-9.0f, -6.0f},
            {9.0f, -6.0f},
            {9.0f, 6.0f},
            {-9.0f, 6.0f},
        }};
    case ShapeMode::None:
        break;
    }
    return {};
}

void drawPath(
    cinerig::examples::GlRenderer& renderer,
    const std::vector<cinerig::math::Vec2>& path,
    float z,
    cinerig::math::Vec3 color)
{
    if (path.empty())
        return;
    for (std::size_t index = 0; index < path.size(); ++index)
    {
        const cinerig::math::Vec2 a = path[index];
        const cinerig::math::Vec2 b = path[(index + 1) % path.size()];
        renderer.drawLine({a.x, a.y, z}, {b.x, b.y, z}, color);
    }
}

void drawScreenRectangle(
    cinerig::examples::GlRenderer& renderer,
    cinerig::math::Vec2 center,
    float halfWidth,
    float halfHeight,
    float z,
    cinerig::math::Vec3 color)
{
    const cinerig::math::Vec3 topLeft{center.x - halfWidth, center.y + halfHeight, z};
    const cinerig::math::Vec3 topRight{center.x + halfWidth, center.y + halfHeight, z};
    const cinerig::math::Vec3 bottomRight{center.x + halfWidth, center.y - halfHeight, z};
    const cinerig::math::Vec3 bottomLeft{center.x - halfWidth, center.y - halfHeight, z};
    renderer.drawLine(topLeft, topRight, color);
    renderer.drawLine(topRight, bottomRight, color);
    renderer.drawLine(bottomRight, bottomLeft, color);
    renderer.drawLine(bottomLeft, topLeft, color);
}

void drawGrid(cinerig::examples::GlRenderer& renderer)
{
    constexpr int halfLines = 12;
    const cinerig::math::Vec3 color{0.18f, 0.21f, 0.27f};
    for (int index = -halfLines; index <= halfLines; ++index)
    {
        const float value = static_cast<float>(index);
        renderer.drawLine({value, -8.0f, -0.02f}, {value, 8.0f, -0.02f}, color);
        renderer.drawLine({-12.0f, value, -0.02f}, {12.0f, value, -0.02f}, color);
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
        "CineRig P4-02 Confiner 2D", 1280, 720, appOptions);
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

    const cinerig::EntityId trackingEntity = world.createEntity({
        {0.0f, -2.0f, 10.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });
    const cinerig::EntityId cameraEntity = world.createEntity({
        {0.0f, -2.0f, 10.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });
    const cinerig::EntityId overviewEntity = world.createEntity({
        {0.0f, -0.5f, 35.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Confiner 2D Camera");
    auto* camera = cameraSystem.camera(cameraId);
    const cinerig::CameraId overviewId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Overview Camera");
    auto* overview = cameraSystem.camera(overviewId);
    if (camera == nullptr || overview == nullptr)
    {
        std::cerr << "Failed to create confiner 2D cameras\n";
        return 1;
    }

    camera->ownerEntity = cameraEntity;
    camera->priority = cinerig::PrioritySettings(0);
    camera->standbyUpdate = cinerig::StandbyUpdateMode::Always;
    camera->targets.tracking = cinerig::TargetRef::entity(trackingEntity);
    camera->lens.orthographic = true;
    camera->lens.orthographicSize = 4.5f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 100.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& hardLock = camera->setBody<cinerig::HardLockToTarget>();
    hardLock.damping = 0.0f;

    auto ownedConfiner = std::make_unique<cinerig::Confiner2D>();
    auto* confiner = ownedConfiner.get();
    camera->addExtension(std::move(ownedConfiner));

    overview->ownerEntity = overviewEntity;
    overview->priority = cinerig::PrioritySettings(100);
    overview->lens.orthographic = true;
    overview->lens.orthographicSize = 9.0f;
    overview->lens.nearClipPlane = 0.1f;
    overview->lens.farClipPlane = 100.0f;
    overview->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    output.setDefaultLens(overview->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle targetMesh =
        app.renderer().createSphere(0.35f);

    ShapeMode shapeMode = ShapeMode::LRoom;
    bool firstSlowing = false;
    bool firstDamping = false;
    bool firstShape = false;
    bool firstReset = false;

    const auto configureOversize = [&]() {
        confiner->oversizeWindow.enabled = true;
        confiner->oversizeWindow.maxWindowSize = 0.0f;
        confiner->oversizeWindow.padding = 3.0f;
    };

    const auto applyShape = [&]() {
        confiner->boundingShape = shapePaths(shapeMode);
        confiner->boundingShapeOffset = {};
        confiner->boundingShapeToWorld = cinerig::math::Mat4::identity();
        confiner->invalidateBoundingShapeCache();
        cameraSystem.resetCameraState();
        output.reset();
    };

    confiner->reset();
    confiner->slowingDistance = 2.0f;
    configureOversize();
    applyShape();

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
        overview->lens.aspect = aspect;

        const bool slowingDown = app.input().keyDown(SDL_SCANCODE_S);
        const bool dampingDown = app.input().keyDown(SDL_SCANCODE_D);
        const bool shapeDown = app.input().keyDown(SDL_SCANCODE_V);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (slowingDown && !firstSlowing)
        {
            confiner->slowingDistance =
                confiner->slowingDistance > 0.0f ? 0.0f : 2.0f;
        }
        if (dampingDown && !firstDamping)
        {
            confiner->damping = confiner->damping > 0.0f ? 0.0f : 0.5f;
        }
        if (shapeDown && !firstShape)
        {
            shapeMode = shapeMode == ShapeMode::LRoom
                ? ShapeMode::Box
                : (shapeMode == ShapeMode::Box ? ShapeMode::None : ShapeMode::LRoom);
            applyShape();
        }
        if (resetDown && !firstReset)
        {
            confiner->reset();
            confiner->slowingDistance = 2.0f;
            configureOversize();
            shapeMode = ShapeMode::LRoom;
            applyShape();
        }

        firstSlowing = slowingDown;
        firstDamping = dampingDown;
        firstShape = shapeDown;
        firstReset = resetDown;

        const float time = app.elapsedTime();
        const float targetX = 8.5f * std::sin(time * 0.55f);
        const float targetY = -3.5f + 2.3f * std::sin(time * 0.85f);
        world.setTransform(
            trackingEntity,
            cinerig::Transform{
                {targetX, targetY, 10.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one});
        cameraSystem.lateUpdate(deltaTime);

        const cinerig::CameraState& state = camera->state();
        const cinerig::math::Vec3 corrected = state.correctedPosition();
        const float halfWidth = camera->lens.orthographicSize * aspect;
        const float halfHeight = camera->lens.orthographicSize;
        const float displacement = (corrected - state.rawPosition).length();

        app.beginFrame();
        app.renderer().beginFrame(output);
        drawGrid(app.renderer());

        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{
                {targetX, targetY, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.35f, 0.78f, 1.0f});
        app.renderer().drawTargetMarker(
            {targetX, targetY, 0.0f},
            0.8f,
            {1.0f, 0.75f, 0.25f});

        app.renderer().setLineWidth(1.5f);
        drawScreenRectangle(
            app.renderer(),
            {state.rawPosition.x, state.rawPosition.y},
            halfWidth,
            halfHeight,
            0.01f,
            {0.55f, 0.58f, 0.64f});
        drawScreenRectangle(
            app.renderer(),
            {corrected.x, corrected.y},
            halfWidth,
            halfHeight,
            0.02f,
            {0.35f, 0.95f, 0.55f});
        app.renderer().drawLine(
            {state.rawPosition.x, state.rawPosition.y, 0.03f},
            {corrected.x, corrected.y, 0.03f},
            {1.0f, 0.65f, 0.25f});

        app.renderer().setLineWidth(2.5f);
        for (const auto& path : confiner->boundingShape)
            drawPath(app.renderer(), path, 0.025f, {0.25f, 0.85f, 0.95f});

        app.renderer().setLineWidth(1.5f);
        for (const auto& path : confiner->bakedSolutionPaths(cameraId))
            drawPath(app.renderer(), path, 0.03f, {0.95f, 0.55f, 1.0f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P6-02 confiner 2D");
        std::ostringstream shapeLine;
        shapeLine << shapeName(shapeMode)
                  << "  baked " << (confiner->boundingShapeIsBaked() ? "yes" : "no")
                  << "  ortho " << camera->lens.orthographicSize;
        app.hud().addLine("Shape", shapeLine.str());
        std::ostringstream bakeLine;
        bakeLine << bakeStateName(confiner->bakingState())
                 << "  " << static_cast<int>(confiner->bakeProgress() * 100.0f)
                 << "%  padding " << confiner->oversizeWindow.padding;
        app.hud().addLine("Bake", bakeLine.str());
        std::ostringstream positionLine;
        positionLine << "raw " << state.rawPosition.x << ", " << state.rawPosition.y
                     << " -> corrected " << corrected.x << ", " << corrected.y;
        app.hud().addLine("Camera", positionLine.str());
        std::ostringstream displacementLine;
        displacementLine << displacement << " units  displaced "
                         << (confiner->cameraWasDisplaced(cameraId) ? "yes" : "no");
        app.hud().addLine("Displacement", displacementLine.str());
        std::ostringstream tuningLine;
        tuningLine << "damping " << confiner->damping
                   << "  slowing " << confiner->slowingDistance;
        app.hud().addLine("Tuning", tuningLine.str());
        app.hud().addLine(
            "Screen",
            "green = confined view, grey = desired, magenta = baked solution");
        app.hud().addLine("Controls", "V shape  D damping  S slowing  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
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
