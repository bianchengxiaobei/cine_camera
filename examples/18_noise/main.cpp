#include <algorithm>
#include <cmath>
#include <cstdint>
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
            std::cout << "Usage: cinerig_18_noise [--frames N] [--hidden] [--no-vsync]\n";
            std::exit(0);
        }
    }
    return options;
}

cinerig::NoiseProfile::Params channel(float frequency, float amplitude)
{
    return {frequency, amplitude, false};
}

cinerig::NoiseProfile makeNoiseProfile()
{
    cinerig::NoiseProfile profile;
    profile.positionNoise.push_back({
        channel(0.17f, 0.45f),
        channel(0.29f, 0.32f),
        channel(0.21f, 0.38f),
    });
    profile.positionNoise.push_back({
        channel(0.53f, 0.22f),
        channel(0.61f, 0.16f),
        channel(0.47f, 0.20f),
    });
    profile.orientationNoise.push_back({
        channel(0.41f, 1.8f),
        channel(0.36f, 1.35f),
        channel(0.49f, 1.2f),
    });
    profile.orientationNoise.push_back({
        channel(0.83f, 0.85f),
        channel(0.91f, 0.65f),
        channel(0.77f, 0.75f),
    });
    return profile;
}

void applyDefaults(cinerig::BasicMultiChannelPerlin& noise)
{
    noise.noiseProfile = makeNoiseProfile();
    noise.amplitudeGain = 1.2f;
    noise.frequencyGain = 0.9f;
    noise.pivotOffset = {};
    noise.seed = 7u;
    noise.reSeed();
}

float correctionAngleDegrees(const cinerig::math::Quat& rotation)
{
    const float cosine = cinerig::math::clamp(std::abs(rotation.w), 0.0f, 1.0f);
    return 2.0f * std::acos(cosine) * cinerig::math::RadToDeg;
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app("CineRig P3-03 Basic Multi Channel Perlin", 1280, 720, appOptions);
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

    const cinerig::math::Vec3 targetPosition{0.0f, 1.8f, 0.0f};
    const cinerig::EntityId targetEntity = world.createEntity({
        targetPosition,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::EntityId cameraEntity = world.createEntity({
        {0.0f, 3.2f, 13.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Noise Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create noise camera\n";
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
    auto& noise = camera->setNoise<cinerig::BasicMultiChannelPerlin>();
    applyDefaults(noise);

    output.setDefaultLens(camera->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::examples::GlRenderer::MeshHandle ground = app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh = app.renderer().createSphere(0.55f);
    const cinerig::examples::GlRenderer::MeshHandle cube = app.renderer().createCube(1.0f);

    bool paused = false;
    bool pivotEnabled = false;
    bool previousAmplitudeDown = false;
    bool previousAmplitudeUp = false;
    bool previousFrequencyDown = false;
    bool previousFrequencyUp = false;
    bool previousPivot = false;
    bool previousPause = false;
    bool previousReset = false;

    while (app.running())
    {
        app.pollEvents();
        if (!app.running())
            break;

        const cinerig::math::Vec2 viewport = app.viewportSize();
        const float deltaTime = app.deltaTime();
        const float aspect = viewport.y > 0.0f ? viewport.x / viewport.y : 1.0f;
        const float simulationDelta = paused ? 0.0f : deltaTime;

        world.setViewportSize(viewport);
        world.setDeltaTime(cinerig::UpdateClock::Late, simulationDelta);
        world.setTime(app.elapsedTime());
        world.setFrame(cinerig::UpdateClock::Late, app.frameCount());
        output.setViewportSize(viewport);
        camera->lens.aspect = aspect;

        const bool amplitudeDownDown = app.input().keyDown(SDL_SCANCODE_A);
        const bool amplitudeUpDown = app.input().keyDown(SDL_SCANCODE_D);
        const bool frequencyDownDown = app.input().keyDown(SDL_SCANCODE_F);
        const bool frequencyUpDown = app.input().keyDown(SDL_SCANCODE_G);
        const bool pivotDown = app.input().keyDown(SDL_SCANCODE_P);
        const bool pauseDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (amplitudeDownDown && !previousAmplitudeDown)
            noise.amplitudeGain = std::max(0.0f, noise.amplitudeGain - 0.1f);
        if (amplitudeUpDown && !previousAmplitudeUp)
            noise.amplitudeGain = std::min(2.0f, noise.amplitudeGain + 0.1f);
        if (frequencyDownDown && !previousFrequencyDown)
            noise.frequencyGain = std::max(0.0f, noise.frequencyGain - 0.1f);
        if (frequencyUpDown && !previousFrequencyUp)
            noise.frequencyGain = std::min(3.0f, noise.frequencyGain + 0.1f);
        if (pivotDown && !previousPivot)
        {
            pivotEnabled = !pivotEnabled;
            noise.pivotOffset = pivotEnabled
                ? cinerig::math::Vec3{0.0f, 0.0f, -2.0f}
                : cinerig::math::Vec3{};
        }
        if (pauseDown && !previousPause)
            paused = !paused;
        if (resetDown && !previousReset)
        {
            paused = false;
            pivotEnabled = false;
            applyDefaults(noise);
            cameraSystem.resetCameraState();
            output.reset();
        }

        previousAmplitudeDown = amplitudeDownDown;
        previousAmplitudeUp = amplitudeUpDown;
        previousFrequencyDown = frequencyDownDown;
        previousFrequencyUp = frequencyUpDown;
        previousPivot = pivotDown;
        previousPause = pauseDown;
        previousReset = resetDown;

        cameraSystem.lateUpdate(simulationDelta);

        const cinerig::CameraState& state = camera->state();
        const cinerig::math::Vec3 rawPosition = state.rawPosition;
        const cinerig::math::Vec3 correctedPosition = state.correctedPosition();

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
                {0.0f, 3.0f, -22.0f},
                cinerig::math::Quat::identity(),
                {38.0f, 6.0f, 0.3f}},
            {0.20f, 0.24f, 0.30f});
        for (int index = -3; index <= 3; ++index)
        {
            const float x = static_cast<float>(index) * 3.8f;
            if (std::abs(x) < 0.5f)
                continue;
            app.renderer().drawMesh(
                cube,
                cinerig::Transform{
                    {x, 1.5f, -7.0f - std::abs(x) * 0.7f},
                    cinerig::math::Quat::identity(),
                    {0.7f, 3.0f, 0.7f}},
                {0.27f, 0.32f, 0.40f});
        }
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{
                targetPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.35f, 0.75f, 1.0f});
        app.renderer().drawTargetMarker(targetPosition, 0.9f);
        app.renderer().drawCameraFrustum(output, {0.8f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P3-03 basic multi channel perlin");
        std::ostringstream amplitudeLine;
        amplitudeLine << noise.amplitudeGain << "  (position + orientation)";
        app.hud().addLine("Amplitude", amplitudeLine.str());
        std::ostringstream frequencyLine;
        frequencyLine << noise.frequencyGain << "  time offset samples";
        app.hud().addLine("Frequency", frequencyLine.str());
        std::ostringstream correctionLine;
        correctionLine << (correctedPosition - rawPosition).length()
                       << " units, " << correctionAngleDegrees(state.orientationCorrection)
                       << " deg";
        app.hud().addLine("Correction", correctionLine.str());
        std::ostringstream timeLine;
        timeLine << noise.noiseTime() << " s, seed " << noise.seed;
        app.hud().addLine("Noise time", timeLine.str());
        app.hud().addLine("Pivot", pivotEnabled ? "2 units forward" : "camera origin");
        app.hud().addLine("Motion", paused ? "frozen" : "sampling");
        app.hud().addLine(
            "Controls",
            "A/D amplitude  F/G frequency  P pivot  Space freeze  R reseed");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
        app.hud().render(static_cast<int>(viewport.x), static_cast<int>(viewport.y));

        app.endFrame();
        if (options.frames > 0 && app.frameCount() >= static_cast<std::uint64_t>(options.frames))
            break;
    }

    return 0;
}
