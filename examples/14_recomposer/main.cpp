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
            std::cout << "Usage: cinerig_14_recomposer [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::math::Vec3 targetPositionAt(float time)
{
    return {
        1.5f * std::sin(time * 0.35f),
        1.0f + 0.25f * std::sin(time * 0.75f),
        0.6f * std::cos(time * 0.3f),
    };
}

cinerig::math::Quat targetRotationAt(float time)
{
    return cinerig::math::quatFromAxisAngle(
        cinerig::math::Vec3::up,
        time * 0.25f);
}

void applyDefaults(cinerig::Recomposer& recomposer)
{
    recomposer.applyAfter = cinerig::Stage::Finalize;
    recomposer.tilt = 6.0f;
    recomposer.pan = 10.0f;
    recomposer.dutch = 8.0f;
    recomposer.zoomScale = 1.12f;
    recomposer.followAttachment = 1.0f;
    recomposer.lookAtAttachment = 1.0f;
    recomposer.validate();
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P2-14 Recomposer", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 initialTargetPosition = targetPositionAt(0.0f);
    const cinerig::math::Quat initialTargetRotation = targetRotationAt(0.0f);
    const cinerig::EntityId targetEntity = world.createEntity({
        initialTargetPosition,
        initialTargetRotation,
        cinerig::math::Vec3::one,
    });

    const cinerig::math::Vec3 cameraPosition{0.0f, 2.0f, 9.0f};
    const cinerig::EntityId cameraEntity = world.createEntity({
        cameraPosition,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Recomposer Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create recomposer camera\n";
        return 1;
    }

    camera->ownerEntity = cameraEntity;
    camera->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
    camera->targets.customLookAt = true;
    camera->lens.fieldOfView = 55.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto& composer = camera->setAim<cinerig::RotationComposer>();
    composer.damping = {0.15f, 0.15f};
    composer.composition.screenPosition = {0.0f, 0.0f};

    auto ownedRecomposer = std::make_unique<cinerig::Recomposer>();
    auto* recomposer = ownedRecomposer.get();
    applyDefaults(*recomposer);
    camera->addExtension(std::move(ownedRecomposer));

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createSphere(0.5f);
    const cinerig::examples::GlRenderer::MeshHandle cube = app.renderer().createCube(1.0f);

    bool paused = false;
    bool reducedAttachment = false;
    float pathTime = 0.0f;
    bool previousSpace = false;
    bool previousAttachment = false;
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
        const bool attachmentDown = app.input().keyDown(SDL_SCANCODE_A);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);
        if (spaceDown && !previousSpace)
            paused = !paused;
        if (attachmentDown && !previousAttachment)
        {
            reducedAttachment = !reducedAttachment;
            recomposer->followAttachment = reducedAttachment ? 0.55f : 1.0f;
            recomposer->lookAtAttachment = reducedAttachment ? 0.55f : 1.0f;
        }
        if (resetDown && !previousReset)
        {
            paused = false;
            reducedAttachment = false;
            pathTime = 0.0f;
            applyDefaults(*recomposer);
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousSpace = spaceDown;
        previousAttachment = attachmentDown;
        previousReset = resetDown;

        const float adjustSpeed = 24.0f * deltaTime;
        if (app.input().keyDown(SDL_SCANCODE_LEFT))
            recomposer->pan -= adjustSpeed;
        if (app.input().keyDown(SDL_SCANCODE_RIGHT))
            recomposer->pan += adjustSpeed;
        if (app.input().keyDown(SDL_SCANCODE_UP))
            recomposer->tilt += adjustSpeed;
        if (app.input().keyDown(SDL_SCANCODE_DOWN))
            recomposer->tilt -= adjustSpeed;
        if (app.input().keyDown(SDL_SCANCODE_Q))
            recomposer->dutch -= adjustSpeed;
        if (app.input().keyDown(SDL_SCANCODE_E))
            recomposer->dutch += adjustSpeed;
        if (app.input().keyDown(SDL_SCANCODE_Z))
            recomposer->zoomScale -= 0.35f * deltaTime;
        if (app.input().keyDown(SDL_SCANCODE_X))
            recomposer->zoomScale += 0.35f * deltaTime;
        recomposer->pan = cinerig::math::clamp(recomposer->pan, -45.0f, 45.0f);
        recomposer->tilt = cinerig::math::clamp(recomposer->tilt, -45.0f, 45.0f);
        recomposer->dutch = cinerig::math::clamp(recomposer->dutch, -45.0f, 45.0f);
        recomposer->zoomScale = cinerig::math::clamp(recomposer->zoomScale, 0.5f, 2.0f);

        if (!paused)
            pathTime += deltaTime;
        const cinerig::math::Vec3 targetPosition = targetPositionAt(pathTime);
        const cinerig::math::Quat targetRotation = targetRotationAt(pathTime);
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
            cinerig::Transform{
                targetPosition,
                targetRotation,
                cinerig::math::Vec3::one},
            1.5f);
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{targetPosition, targetRotation, cinerig::math::Vec3::one},
            {0.35f, 0.75f, 1.0f});
        app.renderer().drawTargetMarker(targetPosition, 0.65f);
        app.renderer().drawCameraFrustum(output, {0.8f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P2-14 recomposer");
        app.hud().addLine(
            "Active",
            cameraSystem.brain(brainId)->activeVirtualCamera() != nullptr
                ? std::string(cameraSystem.brain(brainId)->activeVirtualCamera()->name())
                : "none");

        std::ostringstream angleLine;
        angleLine << "tilt " << recomposer->tilt
                  << "  pan " << recomposer->pan
                  << "  dutch " << recomposer->dutch;
        app.hud().addLine("Angles", angleLine.str());
        std::ostringstream lensLine;
        lensLine << "scale " << recomposer->zoomScale
                 << "  fov " << output.state().lens.fieldOfView;
        app.hud().addLine("Zoom", lensLine.str());
        std::ostringstream attachmentLine;
        attachmentLine << recomposer->followAttachment << " / "
                       << recomposer->lookAtAttachment;
        app.hud().addLine(
            reducedAttachment ? "Attachment (relaxed)" : "Attachment",
            attachmentLine.str());
        app.hud().addLine("Apply after", "Finalize");
        app.hud().addLine("Target state", paused ? "paused" : "moving/rotating");
        app.hud().addLine(
            "Controls",
            "Arrows pan/tilt  Q/E dutch  Z/X zoom  A attach  Space pause  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}
