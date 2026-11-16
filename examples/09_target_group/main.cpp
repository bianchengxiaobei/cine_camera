#include <algorithm>
#include <array>
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
            std::cout << "Usage: cinerig_09_target_group [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

std::array<cinerig::math::Vec3, 3> memberPositionsAt(float time)
{
    return {
        cinerig::math::Vec3{
            3.2f * std::sin(time * 0.50f),
            1.0f,
            2.0f * std::cos(time * 0.40f),
        },
        cinerig::math::Vec3{
            -3.0f * std::sin(time * 0.32f),
            2.0f + 0.5f * std::sin(time * 0.9f),
            -2.2f * std::cos(time * 0.50f),
        },
        cinerig::math::Vec3{
            0.0f,
            1.5f + 1.0f * std::sin(time * 0.70f),
            0.0f,
        },
    };
}

std::array<cinerig::math::Quat, 3> memberRotationsAt(float time)
{
    return {
        cinerig::math::quatFromAxisAngle(
            cinerig::math::Vec3::up, time * 0.35f),
        cinerig::math::quatFromAxisAngle(
            cinerig::math::Vec3::up, -time * 0.22f),
        cinerig::math::quatFromAxisAngle(
            cinerig::math::Vec3::up, time * 0.50f),
    };
}

const char* positionModeName(cinerig::targets::TargetGroup::PositionMode mode)
{
    return mode == cinerig::targets::TargetGroup::PositionMode::GroupCenter
        ? "GroupCenter"
        : "GroupAverage";
}

const char* rotationModeName(cinerig::targets::TargetGroup::RotationMode mode)
{
    return mode == cinerig::targets::TargetGroup::RotationMode::GroupAverage
        ? "GroupAverage"
        : "Manual";
}

void drawBox(
    cinerig::examples::GlRenderer& renderer,
    const cinerig::math::Bounds& bounds,
    cinerig::math::Vec3 color)
{
    const cinerig::math::Vec3 minimum = bounds.min();
    const cinerig::math::Vec3 maximum = bounds.max();
    const cinerig::math::Vec3 corners[8] = {
        {minimum.x, minimum.y, minimum.z},
        {maximum.x, minimum.y, minimum.z},
        {maximum.x, maximum.y, minimum.z},
        {minimum.x, maximum.y, minimum.z},
        {minimum.x, minimum.y, maximum.z},
        {maximum.x, minimum.y, maximum.z},
        {maximum.x, maximum.y, maximum.z},
        {minimum.x, maximum.y, maximum.z},
    };
    const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };
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
    cinerig::examples::App app("CineRig P2-09 TargetGroup", 1280, 720, appOptions);
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

    const auto initialPositions = memberPositionsAt(0.0f);
    const auto initialRotations = memberRotationsAt(0.0f);
    std::array<cinerig::EntityId, 3> memberEntities{};
    std::array<cinerig::examples::GlRenderer::MeshHandle, 3> memberMeshes{};
    for (std::size_t index = 0; index < memberEntities.size(); ++index)
    {
        memberEntities[index] = world.createEntity({
            initialPositions[index],
            initialRotations[index],
            cinerig::math::Vec3::one,
        });
    }

    auto ownedGroup = std::make_unique<cinerig::targets::TargetGroup>(world);
    auto* group = ownedGroup.get();
    group->addMember(memberEntities[0], 1.0f, 0.45f);
    group->addMember(memberEntities[1], 2.0f, 0.75f);
    group->addMember(memberEntities[2], 1.0f, 0.35f);
    const cinerig::GroupId groupId = cameraSystem.createTargetGroup(std::move(ownedGroup));

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Group Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create target-group camera\n";
        return 1;
    }

    camera->targets.tracking = cinerig::TargetRef::group(groupId);
    camera->lens.fieldOfView = 55.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& follow = camera->setBody<cinerig::Follow>();
    follow.followOffset = {0.0f, 1.0f, 12.0f};
    follow.trackerSettings.bindingMode = cinerig::BindingMode::LockToTarget;
    follow.trackerSettings.positionDamping = {0.2f, 0.25f, 0.2f};
    follow.trackerSettings.rotationDamping = {0.3f, 0.3f, 0.3f};

    auto& composer = camera->setAim<cinerig::RotationComposer>();
    composer.damping = {0.2f, 0.2f};
    composer.composition.screenPosition = {0.0f, 0.0f};

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle memberMesh = app.renderer().createSphere(0.38f);
    memberMeshes.fill(memberMesh);

    bool paused = false;
    float pathTime = 0.0f;
    bool previousPositionMode = false;
    bool previousRotationMode = false;
    bool previousWeights = false;
    bool previousSpace = false;
    bool previousReset = false;
    int weightPreset = 1;

    auto applyWeights = [&](int preset)
    {
        group->clear();
        if (preset == 0)
        {
            group->addMember(memberEntities[0], 1.0f, 0.45f);
            group->addMember(memberEntities[1], 1.0f, 0.75f);
            group->addMember(memberEntities[2], 1.0f, 0.35f);
        }
        else if (preset == 2)
        {
            group->addMember(memberEntities[0], 0.25f, 0.45f);
            group->addMember(memberEntities[1], 4.0f, 0.75f);
            group->addMember(memberEntities[2], 0.25f, 0.35f);
        }
        else
        {
            group->addMember(memberEntities[0], 1.0f, 0.45f);
            group->addMember(memberEntities[1], 2.0f, 0.75f);
            group->addMember(memberEntities[2], 1.0f, 0.35f);
        }
    };
    applyWeights(weightPreset);

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

        const bool positionDown = app.input().keyDown(SDL_SCANCODE_P);
        const bool rotationDown = app.input().keyDown(SDL_SCANCODE_O);
        const bool weightsDown = app.input().keyDown(SDL_SCANCODE_W);
        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (positionDown && !previousPositionMode)
        {
            group->positionMode = group->positionMode == cinerig::targets::TargetGroup::PositionMode::GroupCenter
                ? cinerig::targets::TargetGroup::PositionMode::GroupAverage
                : cinerig::targets::TargetGroup::PositionMode::GroupCenter;
        }
        if (rotationDown && !previousRotationMode)
        {
            group->rotationMode = group->rotationMode == cinerig::targets::TargetGroup::RotationMode::Manual
                ? cinerig::targets::TargetGroup::RotationMode::GroupAverage
                : cinerig::targets::TargetGroup::RotationMode::Manual;
        }
        if (weightsDown && !previousWeights)
        {
            weightPreset = (weightPreset + 1) % 3;
            applyWeights(weightPreset);
        }
        if (spaceDown && !previousSpace)
            paused = !paused;
        if (resetDown && !previousReset)
        {
            paused = false;
            pathTime = 0.0f;
            weightPreset = 1;
            group->positionMode = cinerig::targets::TargetGroup::PositionMode::GroupCenter;
            group->rotationMode = cinerig::targets::TargetGroup::RotationMode::Manual;
            group->manualRotation = cinerig::math::Quat::identity();
            applyWeights(weightPreset);
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousPositionMode = positionDown;
        previousRotationMode = rotationDown;
        previousWeights = weightsDown;
        previousSpace = spaceDown;
        previousReset = resetDown;

        if (!paused)
            pathTime += deltaTime;
        const auto positions = memberPositionsAt(pathTime);
        const auto rotations = memberRotationsAt(pathTime);
        for (std::size_t index = 0; index < memberEntities.size(); ++index)
        {
            world.setTransform(memberEntities[index], {
                positions[index],
                rotations[index],
                cinerig::math::Vec3::one,
            });
        }
        group->manualRotation = rotations[1];

        cameraSystem.lateUpdate(deltaTime);

        const cinerig::math::Vec3 groupPosition = group->groupPosition();
        const cinerig::math::Bounds groupBounds = group->boundingBox();
        const cinerig::math::BoundingSphere groupSphere = group->boundingSphere();

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
        for (std::size_t index = 0; index < memberEntities.size(); ++index)
        {
            app.renderer().drawMesh(
                memberMeshes[index],
                cinerig::Transform{
                    positions[index],
                    rotations[index],
                    cinerig::math::Vec3::one},
                index == 0
                    ? cinerig::math::Vec3{0.95f, 0.55f, 0.25f}
                    : (index == 1
                        ? cinerig::math::Vec3{0.35f, 0.75f, 1.0f}
                        : cinerig::math::Vec3{0.55f, 0.95f, 0.45f}));
            app.renderer().drawLine(positions[index], groupPosition, {0.6f, 0.6f, 0.7f});
        }
        drawBox(app.renderer(), groupBounds, {0.75f, 0.8f, 0.95f});
        app.renderer().drawTargetMarker(groupPosition, 0.55f);
        app.renderer().drawLine(
            output.state().finalPosition(),
            groupPosition,
            {0.4f, 0.9f, 0.7f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P2-09 target group");
        app.hud().addLine(
            "Active",
            cameraSystem.brain(brainId)->activeVirtualCamera() != nullptr
                ? std::string(cameraSystem.brain(brainId)->activeVirtualCamera()->name())
                : "none");

        std::ostringstream groupLine;
        groupLine << "(" << groupPosition.x << ", " << groupPosition.y << ", " << groupPosition.z << ")";
        app.hud().addLine("Group position", groupLine.str());
        app.hud().addLine("Position mode", positionModeName(group->positionMode));
        app.hud().addLine("Rotation mode", rotationModeName(group->rotationMode));

        std::ostringstream memberLine;
        memberLine << memberEntities.size() << " members";
        app.hud().addLine("Members", memberLine.str());

        std::ostringstream sphereLine;
        sphereLine << "center " << groupSphere.center.x << "," << groupSphere.center.y << "," << groupSphere.center.z
                   << "  radius " << groupSphere.radius;
        app.hud().addLine("Sphere", sphereLine.str());

        std::ostringstream boxLine;
        boxLine << "size " << groupBounds.size().x << "," << groupBounds.size().y << "," << groupBounds.size().z;
        app.hud().addLine("Bounds", boxLine.str());
        app.hud().addLine("Weights", weightPreset == 0 ? "balanced" : (weightPreset == 2 ? "heavy B" : "default"));
        app.hud().addLine("Target state", paused ? "paused" : "moving");
        app.hud().addLine("Controls", "P position  O rotation  W weights  Space pause  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}
