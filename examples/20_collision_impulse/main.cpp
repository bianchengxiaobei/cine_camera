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
            std::cout << "Usage: cinerig_20_collision_impulse [--frames N] "
                         "[--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

void applyDefinition(cinerig::impulse::ImpulseDefinition& definition)
{
    definition.impulseType =
        cinerig::impulse::ImpulseDefinition::ImpulseType::Propagating;
    definition.impulseShape =
        cinerig::impulse::ImpulseDefinition::ImpulseShape::Explosion;
    definition.impulseChannel = 1u;
    definition.impulseDuration = 0.55f;
    definition.dissipationDistance = 40.0f;
    definition.dissipationRate = 0.12f;
    definition.propagationSpeed = 60.0f;
    definition.validate();
}

cinerig::NoiseProfile makeReactionProfile()
{
    cinerig::NoiseProfile profile;
    profile.positionNoise.push_back({
        {0.55f, 0.22f, false},
        {0.47f, 0.18f, false},
        {0.63f, 0.20f, false},
    });
    profile.orientationNoise.push_back({
        {0.71f, 2.2f, false},
        {0.57f, 1.6f, false},
        {0.83f, 1.3f, false},
    });
    return profile;
}

float correctionAngleDegrees(const cinerig::math::Quat& rotation)
{
    const float cosine = cinerig::math::clamp(std::abs(rotation.w), 0.0f, 1.0f);
    return 2.0f * std::acos(cosine) * cinerig::math::RadToDeg;
}

void drawGroundRing(
    cinerig::examples::GlRenderer& renderer,
    cinerig::math::Vec3 center,
    float radius,
    cinerig::math::Vec3 color)
{
    if (radius <= 0.05f)
        return;
    constexpr int segments = 48;
    cinerig::math::Vec3 previous = center + cinerig::math::Vec3{radius, 0.0f, 0.0f};
    for (int index = 1; index <= segments; ++index)
    {
        const float angle = cinerig::math::TwoPi * static_cast<float>(index)
            / static_cast<float>(segments);
        const cinerig::math::Vec3 current{
            center.x + radius * std::cos(angle),
            center.y,
            center.z + radius * std::sin(angle),
        };
        renderer.drawLine(previous, current, color);
        previous = current;
    }
}

void drawWireBox(
    cinerig::examples::GlRenderer& renderer,
    cinerig::math::Vec3 min,
    cinerig::math::Vec3 max,
    cinerig::math::Vec3 color)
{
    const cinerig::math::Vec3 corners[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {max.x, min.y, max.z},
        {min.x, min.y, max.z},
        {min.x, max.y, min.z},
        {max.x, max.y, min.z},
        {max.x, max.y, max.z},
        {min.x, max.y, max.z},
    };
    constexpr int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };
    for (const auto& edge : edges)
    {
        renderer.drawLine(corners[edge[0]], corners[edge[1]], color);
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
        "CineRig P3-05 Collision Impulse", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 roomCenter{0.0f, 2.0f, 0.0f};
    const cinerig::EntityId targetEntity = world.createEntity({
        roomCenter,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::EntityId cameraEntity = world.createEntity({
        {0.0f, 7.0f, 20.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Collision Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create collision impulse camera\n";
        return 1;
    }

    camera->ownerEntity = cameraEntity;
    camera->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
    camera->targets.customLookAt = true;
    camera->lens.fieldOfView = 55.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;
    camera->setAim<cinerig::HardLookAt>();

    auto ownedListener = std::make_unique<cinerig::impulse::ImpulseListener>(
        &cameraSystem.impulseManager());
    auto* listener = ownedListener.get();
    listener->applyAfter = cinerig::Stage::Noise;
    listener->channelMask = 1u;
    listener->gain = 0.65f;
    listener->useCameraSpace = true;
    listener->signalCombinationMode =
        cinerig::impulse::ImpulseListener::SignalCombinationMode::Additive;
    listener->reaction.secondaryNoise = makeReactionProfile();
    listener->reaction.amplitudeGain = 0.55f;
    listener->reaction.frequencyGain = 1.0f;
    listener->reaction.duration = 0.7f;
    listener->reaction.seed = 23u;
    listener->reaction.reSeed();
    camera->addExtension(std::move(ownedListener));

    cinerig::impulse::CollisionImpulseSource source(&cameraSystem.impulseManager());
    source.hasRigidbody = true;
    source.defaultVelocity = {0.0f, -1.0f, 0.0f};
    applyDefinition(source.impulseDefinition);

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(40.0f);
    const cinerig::examples::GlRenderer::MeshHandle ballMesh =
        app.renderer().createSphere(0.5f);
    const cinerig::examples::GlRenderer::MeshHandle cube =
        app.renderer().createCube(1.0f);

    constexpr float wallX = 7.2f;
    constexpr float ballSpeed = 9.0f;
    const cinerig::math::Vec3 triggerMin{-1.2f, 0.02f, -1.2f};
    const cinerig::math::Vec3 triggerMax{1.2f, 3.8f, 1.2f};

    cinerig::math::Vec3 ballPosition{-wallX, 2.0f, 0.0f};
    cinerig::math::Vec3 ballVelocity{ballSpeed, 0.0f, 0.0f};
    float lastCollisionTime = -100.0f;
    float lastCollisionX = 0.0f;
    float lastTriggerTime = -100.0f;
    bool lastCollisionValid = false;
    bool lastCollisionAccepted = false;
    std::uint32_t lastCollisionLayer = 0;
    bool lastTriggerValid = false;
    bool lastTriggerAccepted = false;
    int acceptedCollisions = 0;
    int rejectedCollisions = 0;
    int acceptedTriggers = 0;
    int rejectedTriggers = 0;

    bool previousLayer = false;
    bool previousTag = false;
    bool previousDirection = false;
    bool previousMass = false;
    bool previousSpeed = false;
    bool previousReset = false;

    const auto reset = [&]() {
        cameraSystem.impulseManager().clear();
        source.reset();
        source.hasRigidbody = true;
        source.defaultVelocity = {0.0f, -1.0f, 0.0f};
        applyDefinition(source.impulseDefinition);
        ballPosition = {-wallX, 2.0f, 0.0f};
        ballVelocity = {ballSpeed, 0.0f, 0.0f};
        lastCollisionTime = -100.0f;
        lastTriggerTime = -100.0f;
        lastCollisionValid = false;
        lastTriggerValid = false;
        acceptedCollisions = 0;
        rejectedCollisions = 0;
        acceptedTriggers = 0;
        rejectedTriggers = 0;
        cameraSystem.resetCameraState();
        output.reset();
    };

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

        const bool layerDown = app.input().keyDown(SDL_SCANCODE_L);
        const bool tagDown = app.input().keyDown(SDL_SCANCODE_T);
        const bool directionDown = app.input().keyDown(SDL_SCANCODE_D);
        const bool massDown = app.input().keyDown(SDL_SCANCODE_M);
        const bool speedDown = app.input().keyDown(SDL_SCANCODE_S);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (layerDown && !previousLayer)
        {
            source.layerMask.bits =
                source.layerMask.bits == 1u ? 0xFFFFFFFFu : 1u;
        }
        if (tagDown && !previousTag)
            source.ignoreTag = source.ignoreTag.empty() ? "NoImpulse" : "";
        if (directionDown && !previousDirection)
            source.useImpactDirection = !source.useImpactDirection;
        if (massDown && !previousMass)
            source.scaleImpactWithMass = !source.scaleImpactWithMass;
        if (speedDown && !previousSpeed)
            source.scaleImpactWithSpeed = !source.scaleImpactWithSpeed;
        if (resetDown && !previousReset)
            reset();

        previousLayer = layerDown;
        previousTag = tagDown;
        previousDirection = directionDown;
        previousMass = massDown;
        previousSpeed = speedDown;
        previousReset = resetDown;

        const float previousX = ballPosition.x;
        ballPosition.x += ballVelocity.x * deltaTime;

        if (previousX < 0.0f && ballPosition.x >= 0.0f)
        {
            source.position = ballPosition;
            source.rigidbodyVelocity = ballVelocity;
            cinerig::impulse::CollisionImpulseSource::ImpactInfo trigger;
            trigger.layer = 0u;
            trigger.tag = "NoImpulse";
            trigger.otherHasRigidbody = false;
            trigger.relativeVelocity = ballVelocity;
            const cinerig::impulse::ImpulseEvent* event = source.onTrigger(trigger);
            lastTriggerValid = true;
            lastTriggerAccepted = event != nullptr;
            if (event != nullptr)
            {
                lastTriggerTime = world.time();
                ++acceptedTriggers;
            }
            else
            {
                ++rejectedTriggers;
            }
        }

        bool hitWall = false;
        std::uint32_t wallLayer = 0;
        bool rightWall = false;
        if (ballPosition.x >= wallX)
        {
            ballPosition.x = wallX;
            hitWall = true;
            wallLayer = 1u;
            rightWall = true;
        }
        else if (ballPosition.x <= -wallX)
        {
            ballPosition.x = -wallX;
            hitWall = true;
            wallLayer = 0u;
        }

        if (hitWall && ballVelocity.x != 0.0f)
        {
            source.position = ballPosition;
            source.rigidbodyVelocity = ballVelocity;
            cinerig::impulse::CollisionImpulseSource::ImpactInfo collision;
            collision.layer = wallLayer;
            collision.tag = rightWall ? "NoImpulse" : "Wall";
            collision.otherHasRigidbody = true;
            collision.otherMass = rightWall ? 4.0f : 2.0f;
            collision.relativeVelocity = ballVelocity;
            const cinerig::impulse::ImpulseEvent* event =
                source.onCollision(collision);
            lastCollisionValid = true;
            lastCollisionAccepted = event != nullptr;
            lastCollisionLayer = wallLayer;
            if (event != nullptr)
            {
                lastCollisionTime = world.time();
                lastCollisionX = ballPosition.x;
                ++acceptedCollisions;
            }
            else
            {
                ++rejectedCollisions;
            }
            ballVelocity.x = -ballVelocity.x;
        }

        cameraSystem.lateUpdate(deltaTime);

        const cinerig::CameraState& state = camera->state();
        const cinerig::math::Vec3 correction =
            state.correctedPosition() - state.rawPosition;
        const float collisionAge = world.time() - lastCollisionTime;
        const float triggerAge = world.time() - lastTriggerTime;

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.01f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.14f, 0.17f, 0.21f});
        app.renderer().drawGrid(40.0f, 1.0f);
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {-8.0f, 2.0f, 0.0f},
                cinerig::math::Quat::identity(),
                {0.5f, 4.0f, 6.0f}},
            {0.20f, 0.48f, 0.68f});
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {8.0f, 2.0f, 0.0f},
                cinerig::math::Quat::identity(),
                {0.5f, 4.0f, 6.0f}},
            {0.62f, 0.34f, 0.24f});
        app.renderer().drawMesh(
            ballMesh,
            cinerig::Transform{
                ballPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {1.0f, 0.72f, 0.25f});
        app.renderer().drawTargetMarker(roomCenter, 0.8f, {0.5f, 0.8f, 1.0f});
        app.renderer().setLineWidth(2.0f);
        drawWireBox(
            app.renderer(),
            triggerMin,
            triggerMax,
            {0.30f, 0.90f, 0.92f});
        if (collisionAge >= 0.0f && collisionAge < 0.6f)
        {
            drawGroundRing(
                app.renderer(),
                {lastCollisionX, 0.04f, ballPosition.z},
                collisionAge * source.impulseDefinition.propagationSpeed,
                {1.0f, 0.48f, 0.20f});
        }
        if (triggerAge >= 0.0f && triggerAge < 0.6f)
        {
            drawGroundRing(
                app.renderer(),
                {0.0f, 0.05f, 0.0f},
                triggerAge * source.impulseDefinition.propagationSpeed,
                {0.30f, 0.95f, 0.95f});
        }
        app.renderer().drawCameraFrustum(output, {0.8f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P3-05 collision impulse source");
        std::ostringstream ballLine;
        ballLine << "x " << ballPosition.x << "  speed " << std::abs(ballVelocity.x);
        app.hud().addLine("Ball", ballLine.str());
        std::ostringstream filterLine;
        filterLine << "mask " << source.layerMask.bits
                   << (source.layerMask.bits == 1u ? " (layer 0)" : " (all layers)")
                   << "  ignore '" << source.ignoreTag << "'";
        app.hud().addLine("Filter", filterLine.str());
        std::ostringstream flagsLine;
        flagsLine << "direction " << (source.useImpactDirection ? "on" : "off")
                  << "  mass " << (source.scaleImpactWithMass ? "on" : "off")
                  << "  speed " << (source.scaleImpactWithSpeed ? "on" : "off");
        app.hud().addLine("Scaling", flagsLine.str());
        std::ostringstream collisionLine;
        if (!lastCollisionValid)
            collisionLine << "none";
        else
        {
            collisionLine << (lastCollisionAccepted ? "accepted" : "rejected")
                          << "  layer " << lastCollisionLayer
                          << "  " << acceptedCollisions << " / "
                          << (acceptedCollisions + rejectedCollisions);
        }
        app.hud().addLine("Last collision", collisionLine.str());
        std::ostringstream triggerLine;
        if (!lastTriggerValid)
            triggerLine << "none";
        else
        {
            triggerLine << (lastTriggerAccepted ? "accepted" : "rejected")
                        << "  " << acceptedTriggers << " / "
                        << (acceptedTriggers + rejectedTriggers);
        }
        app.hud().addLine("Last trigger", triggerLine.str());
        std::ostringstream eventLine;
        eventLine << cameraSystem.impulseManager().activeEventCount()
                  << " active, " << cameraSystem.impulseManager().expiredEventCount()
                  << " recycled";
        app.hud().addLine("Events", eventLine.str());
        std::ostringstream correctionLine;
        correctionLine << correction.length() << " units, "
                       << correctionAngleDegrees(state.orientationCorrection)
                       << " deg";
        app.hud().addLine("Impulse correction", correctionLine.str());
        app.hud().addLine(
            "Walls",
            "left layer 0 mass 2; right layer 1 mass 4; trigger layer 0");
        app.hud().addLine(
            "Controls",
            "L layers  T ignore tag  D direction  M mass  S speed  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}

