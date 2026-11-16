#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
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

struct Member
{
    cinerig::EntityId entity = 0;
    float radius = 0.5f;
    cinerig::math::Vec3 color{1.0f, 1.0f, 1.0f};
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
            std::cout << "Usage: cinerig_17_group_framing [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 memberPosition(int index, float time)
{
    switch (index)
    {
    case 0:
        return {
            2.4f * std::sin(time * 0.45f),
            1.2f + 0.35f * std::sin(time * 0.7f),
            1.8f * std::cos(time * 0.5f)};
    case 1:
        return {
            -2.8f + 0.55f * std::sin(time * 0.6f),
            2.0f,
            -1.2f + 0.4f * std::cos(time * 0.35f)};
    case 2:
        return {
            2.6f,
            2.9f + 0.7f * std::cos(time * 0.55f),
            -2.1f};
    default:
        return {
            0.2f * std::sin(time * 0.8f),
            4.3f + 0.4f * std::sin(time * 0.9f),
            1.6f + 0.3f * std::cos(time * 0.4f)};
    }
}

void applyDefaults(cinerig::targets::GroupFraming& framing)
{
    framing.framingMode = cinerig::targets::GroupFraming::FramingMode::HorizontalAndVertical;
    framing.framingSize = 0.62f;
    framing.centerOffset = {};
    framing.damping = 2.0f;
    framing.sizeAdjustment = cinerig::targets::GroupFraming::SizeAdjustmentMode::DollyThenZoom;
    framing.lateralAdjustment = cinerig::targets::GroupFraming::LateralAdjustmentMode::ChangePosition;
    framing.fovRange = {1.0f, 100.0f};
    framing.dollyRange = {-100.0f, 100.0f};
    framing.orthoSizeRange = {1.0f, 1000.0f};
    framing.validate();
}

const char* framingModeName(cinerig::targets::GroupFraming::FramingMode mode)
{
    switch (mode)
    {
    case cinerig::targets::GroupFraming::FramingMode::Horizontal:
        return "horizontal";
    case cinerig::targets::GroupFraming::FramingMode::Vertical:
        return "vertical";
    default:
        return "horizontal + vertical";
    }
}

const char* sizeModeName(cinerig::targets::GroupFraming::SizeAdjustmentMode mode)
{
    switch (mode)
    {
    case cinerig::targets::GroupFraming::SizeAdjustmentMode::ZoomOnly:
        return "zoom only";
    case cinerig::targets::GroupFraming::SizeAdjustmentMode::DollyOnly:
        return "dolly only";
    default:
        return "dolly then zoom";
    }
}

void drawBounds(
    cinerig::examples::GlRenderer& renderer,
    const cinerig::math::Bounds& bounds,
    cinerig::math::Vec3 color)
{
    const cinerig::math::Vec3 min = bounds.min();
    const cinerig::math::Vec3 max = bounds.max();
    const std::array<cinerig::math::Vec3, 8> corners{
        cinerig::math::Vec3{min.x, min.y, min.z},
        cinerig::math::Vec3{max.x, min.y, min.z},
        cinerig::math::Vec3{max.x, max.y, min.z},
        cinerig::math::Vec3{min.x, max.y, min.z},
        cinerig::math::Vec3{min.x, min.y, max.z},
        cinerig::math::Vec3{max.x, min.y, max.z},
        cinerig::math::Vec3{max.x, max.y, max.z},
        cinerig::math::Vec3{min.x, max.y, max.z},
    };
    constexpr std::array<std::array<int, 2>, 12> edges{{
        {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
        {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
        {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}},
    }};
    for (const auto& edge : edges)
        renderer.drawLine(corners[edge[0]], corners[edge[1]], color);
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P3-02 Group Framing", 1280, 720, appOptions);
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

    std::array<Member, 4> members;
    const std::array<cinerig::math::Vec3, 4> colors{{
        {0.35f, 0.75f, 1.0f},
        {0.45f, 0.92f, 0.62f},
        {1.0f, 0.58f, 0.35f},
        {0.85f, 0.55f, 1.0f},
    }};
    const std::array<float, 4> radii{{0.7f, 0.55f, 0.75f, 0.5f}};
    for (int index = 0; index < 4; ++index)
    {
        members[static_cast<std::size_t>(index)] = {
            world.createEntity({
                memberPosition(index, 0.0f),
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one}),
            radii[static_cast<std::size_t>(index)],
            colors[static_cast<std::size_t>(index)],
        };
    }

    const cinerig::GroupId groupId = cameraSystem.createTargetGroup(
        std::make_unique<cinerig::targets::TargetGroup>(world));
    auto* group = static_cast<cinerig::targets::TargetGroup*>(
        cameraSystem.targetGroup(groupId));
    if (group == nullptr)
    {
        std::cerr << "Failed to create target group\n";
        return 1;
    }
    for (const Member& member : members)
        group->addMember(member.entity, 1.0f, member.radius);

    const cinerig::EntityId cameraEntity = world.createEntity({
        {0.0f, 2.5f, 18.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Group Framing Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create group framing camera\n";
        return 1;
    }

    camera->ownerEntity = cameraEntity;
    camera->targets.lookAt = cinerig::TargetRef::group(groupId);
    camera->targets.customLookAt = true;
    camera->lens.fieldOfView = 40.0f;
    camera->lens.orthographicSize = 6.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 300.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto ownedFraming = std::make_unique<cinerig::targets::GroupFraming>();
    auto* framing = ownedFraming.get();
    applyDefaults(*framing);
    camera->addExtension(std::move(ownedFraming));
    framing->reset(*camera);

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle sphere = app.renderer().createSphere(1.0f);
    const cinerig::examples::GlRenderer::MeshHandle cube = app.renderer().createCube(1.0f);

    bool paused = false;
    bool orthographic = false;
    float pathTime = 0.0f;
    bool previousSpace = false;
    bool previousMode = false;
    bool previousSize = false;
    bool previousLateral = false;
    bool previousDamping = false;
    bool previousOrtho = false;
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
        const bool modeDown = app.input().keyDown(SDL_SCANCODE_M);
        const bool sizeDown = app.input().keyDown(SDL_SCANCODE_S);
        const bool lateralDown = app.input().keyDown(SDL_SCANCODE_L);
        const bool dampingDown = app.input().keyDown(SDL_SCANCODE_D);
        const bool orthoDown = app.input().keyDown(SDL_SCANCODE_O);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (spaceDown && !previousSpace)
            paused = !paused;
        if (modeDown && !previousMode)
        {
            const int mode = (static_cast<int>(framing->framingMode) + 1) % 3;
            framing->framingMode = static_cast<cinerig::targets::GroupFraming::FramingMode>(mode);
        }
        if (sizeDown && !previousSize)
        {
            const int mode = (static_cast<int>(framing->sizeAdjustment) + 1) % 3;
            framing->sizeAdjustment = static_cast<
                cinerig::targets::GroupFraming::SizeAdjustmentMode>(mode);
        }
        if (lateralDown && !previousLateral)
        {
            framing->lateralAdjustment =
                framing->lateralAdjustment
                    == cinerig::targets::GroupFraming::LateralAdjustmentMode::ChangePosition
                ? cinerig::targets::GroupFraming::LateralAdjustmentMode::ChangeRotation
                : cinerig::targets::GroupFraming::LateralAdjustmentMode::ChangePosition;
        }
        if (dampingDown && !previousDamping)
        {
            framing->damping = framing->damping > 0.0f ? 0.0f : 2.0f;
        }
        if (orthoDown && !previousOrtho)
        {
            orthographic = !orthographic;
            camera->lens.orthographic = orthographic;
            framing->reset(*camera);
            cameraSystem.resetCameraState();
            output.reset();
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            orthographic = false;
            pathTime = 0.0f;
            camera->lens.orthographic = false;
            applyDefaults(*framing);
            framing->reset(*camera);
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousSpace = spaceDown;
        previousMode = modeDown;
        previousSize = sizeDown;
        previousLateral = lateralDown;
        previousDamping = dampingDown;
        previousOrtho = orthoDown;
        previousReset = resetDown;

        const float sizeSpeed = 0.35f * deltaTime;
        if (app.input().keyDown(SDL_SCANCODE_UP))
            framing->framingSize += sizeSpeed;
        if (app.input().keyDown(SDL_SCANCODE_DOWN))
            framing->framingSize -= sizeSpeed;
        framing->framingSize = cinerig::math::clamp(framing->framingSize, 0.15f, 1.5f);

        if (!paused)
            pathTime += deltaTime;

        for (int index = 0; index < 4; ++index)
        {
            const Member& member = members[static_cast<std::size_t>(index)];
            world.setTransform(member.entity, {
                memberPosition(index, pathTime),
                cinerig::math::quatFromAxisAngle(
                    cinerig::math::Vec3::up,
                    pathTime * (0.4f + 0.15f * static_cast<float>(index))),
                cinerig::math::Vec3::one,
            });
        }

        cameraSystem.lateUpdate(deltaTime);

        const cinerig::math::Bounds bounds = group->boundingBox();
        const cinerig::math::Vec3 groupCenter = group->groupPosition();
        const float cameraDistance =
            (groupCenter - camera->state().correctedPosition()).length();
        const cinerig::math::Vec3 boundsSize = bounds.size();

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.01f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.15f, 0.18f, 0.22f});
        app.renderer().drawGrid(30.0f, 1.0f);
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {0.0f, 3.0f, -20.0f},
                cinerig::math::Quat::identity(),
                {34.0f, 6.0f, 0.3f}},
            {0.20f, 0.24f, 0.30f});
        app.renderer().setLineWidth(2.0f);
        drawBounds(app.renderer(), bounds, {0.98f, 0.68f, 0.24f});
        for (const Member& member : members)
        {
            const cinerig::Transform transform =
                world.transform(member.entity);
            app.renderer().drawMesh(
                sphere,
                cinerig::Transform{
                    transform.position,
                    transform.rotation,
                    {member.radius, member.radius, member.radius}},
                member.color);
            app.renderer().drawTargetMarker(transform.position, member.radius + 0.25f, member.color);
        }
        app.renderer().drawTargetMarker(groupCenter, 0.9f, {1.0f, 0.8f, 0.25f});
        app.renderer().drawCameraFrustum(output, {0.8f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P3-02 group framing");
        app.hud().addLine("Members", "4 spheres + weighted group bounds");
        std::ostringstream centerLine;
        centerLine << groupCenter.x << ", " << groupCenter.y << ", " << groupCenter.z;
        app.hud().addLine("Group center", centerLine.str());
        std::ostringstream boundsLine;
        boundsLine << boundsSize.x << " x " << boundsSize.y << " x " << boundsSize.z;
        app.hud().addLine("Bounds size", boundsLine.str());
        std::ostringstream distanceLine;
        distanceLine << cameraDistance << " world units";
        app.hud().addLine("Camera distance", distanceLine.str());
        app.hud().addLine("Framing mode", framingModeName(framing->framingMode));
        app.hud().addLine("Size adjustment", sizeModeName(framing->sizeAdjustment));
        app.hud().addLine(
            "Lateral adjustment",
            framing->lateralAdjustment
                    == cinerig::targets::GroupFraming::LateralAdjustmentMode::ChangePosition
                ? "change position"
                : "change rotation");
        std::ostringstream framingLine;
        framingLine << "size " << framing->framingSize
                    << "  damping " << framing->damping;
        app.hud().addLine("Framing", framingLine.str());
        std::ostringstream lensLine;
        if (orthographic)
            lensLine << "orthographic size " << output.state().lens.orthographicSize;
        else
            lensLine << "perspective fov " << output.state().lens.fieldOfView;
        app.hud().addLine("Lens", lensLine.str());
        app.hud().addLine("Motion", paused ? "paused" : "members moving");
        app.hud().addLine(
            "Controls",
            "M mode  S size mode  L lateral  D damping  O ortho  Up/Down framing  Space pause  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}


