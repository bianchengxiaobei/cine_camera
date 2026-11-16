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
            std::cout << "Usage: cinerig_19_impulse [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

const char* shapeName(cinerig::impulse::ImpulseDefinition::ImpulseShape shape)
{
    using Shape = cinerig::impulse::ImpulseDefinition::ImpulseShape;
    switch (shape)
    {
    case Shape::Recoil:
        return "recoil";
    case Shape::Bump:
        return "bump";
    case Shape::Explosion:
        return "explosion";
    case Shape::Rumble:
        return "rumble";
    default:
        return "custom";
    }
}

void applyShape(
    cinerig::impulse::ImpulseDefinition& definition,
    cinerig::impulse::ImpulseDefinition::ImpulseShape shape)
{
    definition.impulseShape = shape;
    definition.impulseType =
        cinerig::impulse::ImpulseDefinition::ImpulseType::Propagating;
    definition.impulseChannel = 1u;
    definition.dissipationDistance = 26.0f;
    definition.dissipationRate = 0.15f;
    definition.propagationSpeed = 60.0f;
    switch (shape)
    {
    case cinerig::impulse::ImpulseDefinition::ImpulseShape::Recoil:
        definition.impulseDuration = 0.28f;
        break;
    case cinerig::impulse::ImpulseDefinition::ImpulseShape::Explosion:
        definition.impulseDuration = 0.65f;
        break;
    case cinerig::impulse::ImpulseDefinition::ImpulseShape::Rumble:
        definition.impulseDuration = 1.1f;
        break;
    default:
        definition.impulseDuration = 0.4f;
        break;
    }
    definition.validate();
}

cinerig::NoiseProfile makeReactionProfile()
{
    cinerig::NoiseProfile profile;
    profile.positionNoise.push_back({
        {0.55f, 0.25f, false},
        {0.47f, 0.18f, false},
        {0.63f, 0.22f, false},
    });
    profile.orientationNoise.push_back({
        {0.71f, 2.4f, false},
        {0.57f, 1.8f, false},
        {0.83f, 1.4f, false},
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

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P6-05 Impulse Helpers", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 sourcePosition{0.0f, 1.8f, 0.0f};
    const cinerig::EntityId targetEntity = world.createEntity({
        sourcePosition,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::math::Vec3 propBasePosition{4.2f, 0.75f, -3.0f};
    const cinerig::EntityId propEntity = world.createEntity({
        propBasePosition,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::EntityId cameraEntity = world.createEntity({
        {0.0f, 3.4f, 13.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Impulse Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create impulse camera\n";
        return 1;
    }

    camera->ownerEntity = cameraEntity;
    camera->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
    camera->targets.customLookAt = true;
    camera->lens.fieldOfView = 50.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;
    camera->setAim<cinerig::HardLookAt>();

    auto ownedListener = std::make_unique<cinerig::impulse::ImpulseListener>(
        &cameraSystem.impulseManager());
    auto* listener = ownedListener.get();
    listener->applyAfter = cinerig::Stage::Noise;
    listener->channelMask = 1u;
    listener->gain = 0.7f;
    listener->useCameraSpace = true;
    listener->signalCombinationMode =
        cinerig::impulse::ImpulseListener::SignalCombinationMode::Additive;
    listener->reaction.secondaryNoise = makeReactionProfile();
    listener->reaction.amplitudeGain = 0.7f;
    listener->reaction.frequencyGain = 1.1f;
    listener->reaction.duration = 0.8f;
    listener->reaction.seed = 11u;
    listener->reaction.reSeed();
    camera->addExtension(std::move(ownedListener));

    cinerig::impulse::ExternalImpulseListener externalListener(
        &cameraSystem.impulseManager());
    externalListener.targetEntity = propEntity;
    externalListener.channelMask = 1u;
    externalListener.gain = 0.45f;
    externalListener.use2DDistance = false;
    externalListener.useLocalSpace = true;
    externalListener.reaction.secondaryNoise = makeReactionProfile();
    externalListener.reaction.amplitudeGain = 0.35f;
    externalListener.reaction.frequencyGain = 1.6f;
    externalListener.reaction.duration = 0.5f;
    externalListener.reaction.seed = 23u;
    externalListener.reaction.reSeed();

    cinerig::impulse::ImpulseSource source(&cameraSystem.impulseManager());
    source.position = sourcePosition;
    source.defaultVelocity = {0.0f, -0.8f, 0.0f};
    applyShape(
        source.impulseDefinition,
        cinerig::impulse::ImpulseDefinition::ImpulseShape::Recoil);

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(36.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh =
        app.renderer().createSphere(0.7f);
    const cinerig::examples::GlRenderer::MeshHandle cube =
        app.renderer().createCube(1.0f);

    bool autoFire = true;
    float autoTimer = 0.15f;
    float lastFireTime = -100.0f;
    bool previousFire = false;
    bool previousAuto = false;
    bool previousClear = false;
    bool previousReset = false;

    const auto fire = [&]() {
        source.position = sourcePosition;
        if (source.generateImpulse() != nullptr)
            lastFireTime = world.time();
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

        const bool fireDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool autoDown = app.input().keyDown(SDL_SCANCODE_A);
        const bool clearDown = app.input().keyDown(SDL_SCANCODE_C);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);
        const bool recoilDown = app.input().keyDown(SDL_SCANCODE_1);
        const bool bumpDown = app.input().keyDown(SDL_SCANCODE_2);
        const bool explosionDown = app.input().keyDown(SDL_SCANCODE_3);
        const bool rumbleDown = app.input().keyDown(SDL_SCANCODE_4);

        if (fireDown && !previousFire)
            fire();
        if (autoDown && !previousAuto)
            autoFire = !autoFire;
        if (clearDown && !previousClear)
        {
            cameraSystem.impulseManager().clear();
            lastFireTime = -100.0f;
        }
        if (recoilDown)
        {
            applyShape(
                source.impulseDefinition,
                cinerig::impulse::ImpulseDefinition::ImpulseShape::Recoil);
            fire();
        }
        if (bumpDown)
        {
            applyShape(
                source.impulseDefinition,
                cinerig::impulse::ImpulseDefinition::ImpulseShape::Bump);
            fire();
        }
        if (explosionDown)
        {
            applyShape(
                source.impulseDefinition,
                cinerig::impulse::ImpulseDefinition::ImpulseShape::Explosion);
            fire();
        }
        if (rumbleDown)
        {
            applyShape(
                source.impulseDefinition,
                cinerig::impulse::ImpulseDefinition::ImpulseShape::Rumble);
            fire();
        }
        if (resetDown && !previousReset)
        {
            cameraSystem.impulseManager().clear();
            externalListener.reset();
            source.reset();
            applyShape(
                source.impulseDefinition,
                cinerig::impulse::ImpulseDefinition::ImpulseShape::Recoil);
            autoFire = true;
            autoTimer = 0.15f;
            lastFireTime = -100.0f;
            cameraSystem.resetCameraState();
            output.reset();
        }
        previousFire = fireDown;
        previousAuto = autoDown;
        previousClear = clearDown;
        previousReset = resetDown;

        if (autoFire)
        {
            autoTimer -= deltaTime;
            if (autoTimer <= 0.0f)
            {
                fire();
                autoTimer = 0.4f;
            }
        }

        cameraSystem.lateUpdate(deltaTime);
        externalListener.update(deltaTime);

        const cinerig::CameraState& state = camera->state();
        const cinerig::math::Vec3 correction =
            state.correctedPosition() - state.rawPosition;
        const float waveAge = world.time() - lastFireTime;
        const float waveRadius = waveAge * source.impulseDefinition.propagationSpeed;

        app.beginFrame();
        app.renderer().beginFrame(output);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.01f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.15f, 0.18f, 0.22f});
        app.renderer().drawGrid(36.0f, 1.0f);
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                {0.0f, 3.0f, -24.0f},
                cinerig::math::Quat::identity(),
                {42.0f, 6.0f, 0.3f}},
            {0.20f, 0.24f, 0.30f});
        for (int index = -4; index <= 4; ++index)
        {
            const float x = static_cast<float>(index) * 3.5f;
            if (std::abs(x) < 0.5f)
                continue;
            app.renderer().drawMesh(
                cube,
                cinerig::Transform{
                    {x, 1.5f, -8.0f - std::abs(x) * 0.5f},
                    cinerig::math::Quat::identity(),
                    {0.65f, 3.0f, 0.65f}},
                {0.27f, 0.32f, 0.40f});
        }
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{
                sourcePosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.35f, 0.75f, 1.0f});
        app.renderer().drawTargetMarker(sourcePosition, 1.0f, {1.0f, 0.75f, 0.25f});
        const cinerig::Transform propTransform = world.transform(propEntity);
        app.renderer().drawMesh(
            cube,
            cinerig::Transform{
                propTransform.position,
                propTransform.rotation,
                {0.9f, 0.9f, 0.9f}},
            {0.96f, 0.48f, 0.24f});
        app.renderer().drawTargetMarker(
            propBasePosition,
            1.1f,
            {0.96f, 0.48f, 0.24f});
        app.renderer().setLineWidth(2.0f);
        if (waveAge >= 0.0f && waveAge < 1.4f)
        {
            drawGroundRing(
                app.renderer(),
                {sourcePosition.x, 0.04f, sourcePosition.z},
                waveRadius,
                {1.0f, 0.58f, 0.22f});
        }
        app.renderer().drawCameraFrustum(output, {0.8f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P6-05 impulse helpers");
        std::ostringstream shapeLine;
        shapeLine << shapeName(source.impulseDefinition.impulseShape)
                  << "  channel " << source.impulseDefinition.impulseChannel
                  << "  propagating";
        app.hud().addLine("Shape", shapeLine.str());
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
            "Listener",
            "gain 0.7, camera space, additive, secondary reaction");
        std::ostringstream propLine;
        propLine << externalListener.lastPositionOffset().length() << " units, "
                 << correctionAngleDegrees(externalListener.lastRotationOffset())
                 << " deg, "
                 << (externalListener.hasAppliedImpulse() ? "active" : "idle");
        app.hud().addLine("External prop", propLine.str());
        app.hud().addLine("Auto fire", autoFire ? "on" : "off");
        app.hud().addLine(
            "Controls",
            "1/2/3/4 shape+fire  Space fire  A auto  C clear  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}


