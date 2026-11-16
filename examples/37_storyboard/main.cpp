#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>
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
    std::string capturePath;
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
        else if (argument == "--capture" && index + 1 < argc)
            options.capturePath = argv[++index];
        else if (argument == "--help")
        {
            std::cout << "Usage: cinerig_37_storyboard [--frames N] "
                         "[--hidden] [--no-vsync] [--capture file.bmp]\n";
            std::exit(0);
        }
    }
    return options;
}

struct StoryboardTexture
{
    std::string name;
    cinerig::examples::GlRenderer::TextureHandle texture = 0;
    cinerig::math::Vec2 size{};
};

cinerig::math::Vec3 mix(
    cinerig::math::Vec3 from,
    cinerig::math::Vec3 to,
    float t)
{
    return cinerig::math::lerp(from, to, t);
}

void writePixel(
    std::vector<unsigned char>& pixels,
    int width,
    int height,
    int x,
    int y,
    cinerig::math::Vec3 color)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;

    const auto channel = [](float value) {
        return static_cast<unsigned char>(
            std::lround(cinerig::math::clamp01(value) * 255.0f));
    };
    const std::size_t index =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
         + static_cast<std::size_t>(x))
        * 4u;
    pixels[index] = channel(color.x);
    pixels[index + 1] = channel(color.y);
    pixels[index + 2] = channel(color.z);
    pixels[index + 3] = 255;
}

std::shared_ptr<StoryboardTexture> createStoryboardTexture(
    cinerig::examples::GlRenderer& renderer,
    std::string name,
    int width,
    int height,
    int style)
{
    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height)
        * 4u,
        0u);

    for (int y = 0; y < height; ++y)
    {
        const float v = (static_cast<float>(y) + 0.5f)
            / static_cast<float>(height);
        for (int x = 0; x < width; ++x)
        {
            const float u = (static_cast<float>(x) + 0.5f)
                / static_cast<float>(width);
            cinerig::math::Vec3 color;

            if (style == 0)
            {
                color = mix(
                    {0.075f, 0.045f, 0.060f},
                    {0.88f, 0.40f, 0.14f},
                    0.15f + v * 0.80f);
                const float sunX = u - 0.72f;
                const float sunY = v - 0.64f;
                if (sunX * sunX + sunY * sunY < 0.030f)
                    color = {1.0f, 0.80f, 0.36f};
                if (std::abs(v - 0.37f) < 0.005f)
                    color = {0.99f, 0.74f, 0.40f};
                if (v < 0.37f
                    && (static_cast<int>(u * 28.0f + v * 19.0f) % 2 == 0))
                {
                    color = mix(color, {0.16f, 0.09f, 0.07f}, 0.45f);
                }
                if (v > 0.72f)
                    color = mix(color, {0.18f, 0.08f, 0.16f}, (v - 0.72f) * 1.8f);
            }
            else
            {
                color = mix(
                    {0.025f, 0.060f, 0.150f},
                    {0.045f, 0.240f, 0.310f},
                    v);
                const float ringX = u - 0.36f;
                const float ringY = v - 0.58f;
                const float radius = std::sqrt(
                    ringX * ringX + ringY * ringY);
                if (std::abs(radius - 0.24f) < 0.010f)
                    color = {0.35f, 0.94f, 1.0f};
                if (std::abs(u - 0.58f) < 0.16f
                    && std::abs(v - 0.56f) < 0.10f)
                {
                    color = mix(color, {0.20f, 0.82f, 0.92f}, 0.55f);
                }
                if ((static_cast<int>((u + v) * 24.0f) % 4) == 0)
                    color = mix(color, {0.55f, 0.92f, 1.0f}, 0.10f);
            }

            if ((x % 82 == 0) || (y % 82 == 0))
                color = mix(color, {0.92f, 0.94f, 0.96f}, 0.12f);

            if (x < 7 || x >= width - 7 || y < 7 || y >= height - 7)
                color = {0.93f, 0.90f, 0.80f};

            writePixel(pixels, width, height, x, y, color);
        }
    }

    StoryboardTexture result;
    result.name = std::move(name);
    result.texture = renderer.createTexture(width, height, pixels.data());
    result.size = {
        static_cast<float>(width),
        static_cast<float>(height),
    };

    auto shared = std::make_shared<StoryboardTexture>(std::move(result));
    return shared;
}

struct OverlayResult
{
    int count = 0;
    int drawn = 0;
    float totalWeight = 0.0f;
    std::string summary = "(none)";
    std::string primaryName;
    cinerig::StoryboardLayout primaryLayout{};
    bool hasPrimary = false;
};

class ExampleStoryboardAdapter
{
public:
    OverlayResult present(
        const cinerig::AppliedCamera& camera,
        cinerig::math::Vec2 screenSize,
        cinerig::examples::GlRenderer& renderer,
        bool globalMute)
    {
        OverlayResult result;
        if (!camera.hasCamera)
            return result;

        std::ostringstream summary;
        for (std::size_t index = 0;
             index < camera.state.numCustomBlendables();
             ++index)
        {
            const cinerig::CustomBlendable& blendable =
                camera.state.customBlendable(index);
            if (blendable.type != typeid(cinerig::StoryboardSettings)
                || blendable.data == nullptr
                || blendable.weight <= 0.0f)
            {
                continue;
            }

            const auto* settings =
                static_cast<const cinerig::StoryboardSettings*>(
                    blendable.data.get());
            if (settings == nullptr)
                continue;

            const auto* image =
                settings->image.get<StoryboardTexture>();
            if (image == nullptr)
                continue;

            const cinerig::StoryboardLayout layout =
                cinerig::calculateStoryboardLayout(
                    *settings, blendable.weight, screenSize);
            if (!layout.valid)
                continue;

            if (result.count > 0)
                summary << " + ";
            summary << image->name << " "
                    << static_cast<int>(
                        std::lround(blendable.weight * 100.0f))
                    << "%";
            result.totalWeight += blendable.weight;

            if (!result.hasPrimary)
            {
                result.hasPrimary = true;
                result.primaryName = image->name;
                result.primaryLayout = layout;
            }

            if (!globalMute
                && image->texture != 0
                && layout.hasImage
                && layout.alpha > 0.0f)
            {
                renderer.drawScreenTexture(
                    image->texture,
                    screenSize,
                    layout.imageCenter,
                    layout.imageSize,
                    layout.rotation.z,
                    layout.alpha,
                    layout.viewportCenter,
                    layout.viewportSize);
                ++result.drawn;
            }

            ++result.count;
        }

        if (result.count > 0)
            result.summary = summary.str();
        return result;
    }
};

std::string fillName(cinerig::StoryboardFillStrategy fill)
{
    switch (fill)
    {
    case cinerig::StoryboardFillStrategy::BestFit:
        return "BestFit";
    case cinerig::StoryboardFillStrategy::CropImageToFit:
        return "Crop";
    case cinerig::StoryboardFillStrategy::StretchToFit:
        return "Stretch";
    }
    return "?";
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    cinerig::examples::AppOptions appOptions;
    appOptions.visible = !options.hidden;
    appOptions.vsync = options.vsync;
    cinerig::examples::App app(
        "CineRig P5-03 Storyboard", 1280, 720, appOptions);
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
    brainConfig.defaultBlend = {cinerig::BlendStyle::EaseInOut, 1.1f};
    const cinerig::BrainId brainId =
        cameraSystem.createBrain(output, brainConfig);
    cinerig::CameraBrain* brain = cameraSystem.brain(brainId);
    if (brain == nullptr)
    {
        std::cerr << "Failed to create brain\n";
        return 1;
    }

    const cinerig::EntityId targetEntity = world.createEntity({
        {0.0f, 1.4f, 0.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::math::Vec3 cameraAPosition{-5.4f, 3.2f, 9.4f};
    const cinerig::math::Vec3 cameraBPosition{7.4f, 2.6f, 5.4f};
    const cinerig::EntityId cameraAEntity = world.createEntity({
        cameraAPosition,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });
    const cinerig::EntityId cameraBEntity = world.createEntity({
        cameraBPosition,
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraAId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Shot A");
    const cinerig::CameraId cameraBId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Shot B");
    auto* cameraA = cameraSystem.camera(cameraAId);
    auto* cameraB = cameraSystem.camera(cameraBId);
    if (cameraA == nullptr || cameraB == nullptr)
    {
        std::cerr << "Failed to create storyboard cameras\n";
        return 1;
    }

    cameraA->ownerEntity = cameraAEntity;
    cameraA->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
    cameraA->targets.customLookAt = true;
    cameraA->priority = cinerig::PrioritySettings(10);
    cameraA->lens.fieldOfView = 42.0f;
    cameraA->lens.nearClipPlane = 0.1f;
    cameraA->lens.farClipPlane = 100.0f;
    cameraA->standbyUpdate = cinerig::StandbyUpdateMode::Always;
    cameraA->setAim<cinerig::HardLookAt>();

    cameraB->ownerEntity = cameraBEntity;
    cameraB->targets.lookAt = cinerig::TargetRef::entity(targetEntity);
    cameraB->targets.customLookAt = true;
    cameraB->priority = cinerig::PrioritySettings(20);
    cameraB->lens.fieldOfView = 48.0f;
    cameraB->lens.nearClipPlane = 0.1f;
    cameraB->lens.farClipPlane = 100.0f;
    cameraB->standbyUpdate = cinerig::StandbyUpdateMode::Always;
    cameraB->setAim<cinerig::HardLookAt>();

    cameraSystem.setCameraEnabled(cameraBId, false);

    const auto imageA = createStoryboardTexture(
        app.renderer(), "Storyboard A", 800, 600, 0);
    const auto imageB = createStoryboardTexture(
        app.renderer(), "Storyboard B", 512, 768, 1);

    auto storyboardA = std::make_unique<cinerig::StoryboardExtension>();
    storyboardA->image = cinerig::StoryboardImage(imageA, imageA->size);
    storyboardA->aspect = cinerig::StoryboardFillStrategy::BestFit;
    storyboardA->alpha = 0.90f;
    storyboardA->center = {0.0f, 0.0f};
    storyboardA->rotation = {0.0f, 0.0f, -2.5f};
    storyboardA->scale = {0.96f, 0.96f};
    storyboardA->syncScale = true;
    storyboardA->validate();
    cinerig::StoryboardExtension* storyboardAPtr = storyboardA.get();
    cameraA->addExtension(std::move(storyboardA));

    auto storyboardB = std::make_unique<cinerig::StoryboardExtension>();
    storyboardB->image = cinerig::StoryboardImage(imageB, imageB->size);
    storyboardB->aspect = cinerig::StoryboardFillStrategy::CropImageToFit;
    storyboardB->alpha = 0.90f;
    storyboardB->center = {0.06f, 0.0f};
    storyboardB->rotation = {0.0f, 0.0f, 2.0f};
    storyboardB->scale = {0.95f, 0.95f};
    storyboardB->syncScale = true;
    storyboardB->splitView = -0.35f;
    storyboardB->validate();
    cinerig::StoryboardExtension* storyboardBPtr = storyboardB.get();
    cameraB->addExtension(std::move(storyboardB));

    output.setDefaultLens(cameraA->lens);
    output.setAspect(app.viewportSize().x / app.viewportSize().y);

    const cinerig::StoryboardFillStrategy fills[] = {
        cinerig::StoryboardFillStrategy::BestFit,
        cinerig::StoryboardFillStrategy::CropImageToFit,
        cinerig::StoryboardFillStrategy::StretchToFit,
    };
    const float splitValues[] = {0.0f, -0.35f, 0.35f};
    const float alphaValues[] = {0.90f, 0.65f, 0.35f};

    bool paused = false;
    bool shotB = false;
    bool showImage = true;
    bool muteA = false;
    bool globalMute = false;
    std::size_t fillIndex = 0;
    std::size_t splitIndex = 0;
    std::size_t alphaIndex = 0;
    bool previousSpace = false;
    bool previousShot = false;
    bool previousShow = false;
    bool previousMute = false;
    bool previousFill = false;
    bool previousSplit = false;
    bool previousAlpha = false;
    bool previousGlobal = false;
    bool previousReset = false;
    float simulationTime = 0.0f;

    ExampleStoryboardAdapter storyboardAdapter;

    app.hud().setTextScale(1.35f);

    const cinerig::examples::GlRenderer::MeshHandle ground =
        app.renderer().createPlane(30.0f);
    const cinerig::examples::GlRenderer::MeshHandle targetMesh =
        app.renderer().createSphere(0.42f);
    const cinerig::examples::GlRenderer::MeshHandle pedestal =
        app.renderer().createCube(1.0f);
    const cinerig::examples::GlRenderer::MeshHandle cameraMesh =
        app.renderer().createSphere(0.30f);
    const cinerig::examples::GlRenderer::MeshHandle pillar =
        app.renderer().createCube(1.0f);

    const cinerig::math::Vec3 cameraAColor{1.0f, 0.55f, 0.18f};
    const cinerig::math::Vec3 cameraBColor{0.22f, 0.84f, 1.0f};

    while (app.running())
    {
        app.pollEvents();
        if (!app.running())
            break;

        const cinerig::math::Vec2 viewport = app.viewportSize();
        const float deltaTime = app.deltaTime();
        const float aspect =
            viewport.y > 0.0f ? viewport.x / viewport.y : 1.0f;
        const float frameDelta =
            options.frames > 0 ? 1.0f / 60.0f : deltaTime;
        const float simDelta = paused ? 0.0f : frameDelta;

        world.setViewportSize(viewport);
        world.setDeltaTime(cinerig::UpdateClock::Late, simDelta);
        world.setFrame(cinerig::UpdateClock::Late, app.frameCount());
        output.setViewportSize(viewport);
        output.setAspect(aspect);
        cameraA->lens.aspect = aspect;
        cameraB->lens.aspect = aspect;

        const bool spaceDown = app.input().keyDown(SDL_SCANCODE_SPACE);
        const bool shotDown = app.input().keyDown(SDL_SCANCODE_B);
        const bool showDown = app.input().keyDown(SDL_SCANCODE_S);
        const bool muteDown = app.input().keyDown(SDL_SCANCODE_M);
        const bool fillDown = app.input().keyDown(SDL_SCANCODE_F);
        const bool splitDown = app.input().keyDown(SDL_SCANCODE_V);
        const bool alphaDown = app.input().keyDown(SDL_SCANCODE_A);
        const bool globalDown = app.input().keyDown(SDL_SCANCODE_G);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (spaceDown && !previousSpace)
            paused = !paused;
        if (shotDown && !previousShot)
        {
            shotB = !shotB;
            cameraSystem.setCameraEnabled(cameraBId, shotB);
        }
        if (showDown && !previousShow)
        {
            showImage = !showImage;
            storyboardAPtr->showImage = showImage;
            storyboardBPtr->showImage = showImage;
        }
        if (muteDown && !previousMute)
        {
            muteA = !muteA;
            storyboardAPtr->muteCamera = muteA;
        }
        if (fillDown && !previousFill)
        {
            fillIndex = (fillIndex + 1) % 3;
            storyboardAPtr->aspect = fills[fillIndex];
            storyboardBPtr->aspect = fills[fillIndex];
        }
        if (splitDown && !previousSplit)
        {
            splitIndex = (splitIndex + 1) % 3;
            storyboardAPtr->splitView = splitValues[splitIndex];
            storyboardBPtr->splitView = splitValues[splitIndex];
        }
        if (alphaDown && !previousAlpha)
        {
            alphaIndex = (alphaIndex + 1) % 3;
            storyboardAPtr->alpha = alphaValues[alphaIndex];
            storyboardBPtr->alpha = alphaValues[alphaIndex];
        }
        if (globalDown && !previousGlobal)
            globalMute = !globalMute;
        if (resetDown && !previousReset)
        {
            paused = false;
            shotB = false;
            showImage = true;
            muteA = false;
            globalMute = false;
            fillIndex = 0;
            splitIndex = 0;
            alphaIndex = 0;
            simulationTime = 0.0f;
            storyboardAPtr->showImage = true;
            storyboardAPtr->muteCamera = false;
            storyboardAPtr->aspect = fills[0];
            storyboardAPtr->splitView = splitValues[0];
            storyboardAPtr->alpha = alphaValues[0];
            storyboardAPtr->validate();
            storyboardBPtr->showImage = true;
            storyboardBPtr->aspect =
                cinerig::StoryboardFillStrategy::CropImageToFit;
            storyboardBPtr->splitView = -0.35f;
            storyboardBPtr->alpha = alphaValues[0];
            storyboardBPtr->validate();
            cameraSystem.setCameraEnabled(cameraBId, false);
            cameraSystem.setCameraEnabled(cameraAId, true);
            cameraSystem.resetCameraState();
            world.setTime(0.0f);
            output.reset();
            output.setDefaultLens(cameraA->lens);
        }
        previousSpace = spaceDown;
        previousShot = shotDown;
        previousShow = showDown;
        previousMute = muteDown;
        previousFill = fillDown;
        previousSplit = splitDown;
        previousAlpha = alphaDown;
        previousGlobal = globalDown;
        previousReset = resetDown;

        if (!paused)
            simulationTime += simDelta;
        world.setTime(simulationTime);

        const cinerig::math::Vec3 targetPosition{
            2.4f * std::sin(simulationTime * 0.55f),
            1.4f + 0.25f * std::sin(simulationTime * 1.1f),
            0.8f * std::cos(simulationTime * 0.4f),
        };
        world.setTransform(
            targetEntity,
            cinerig::Transform{
                targetPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one,
            });

        const cinerig::math::Vec3 movingA{
            -5.4f + 0.9f * std::sin(simulationTime * 0.45f),
            3.2f + 0.35f * std::sin(simulationTime * 0.85f),
            9.4f + 0.45f * std::cos(simulationTime * 0.35f),
        };
        world.setTransform(
            cameraAEntity,
            cinerig::Transform{
                movingA,
                cinerig::math::lookRotation(
                    targetPosition - movingA,
                    cinerig::math::Vec3::up),
                cinerig::math::Vec3::one,
            });

        cameraSystem.lateUpdate(simDelta);

        const cinerig::math::Vec3 clearColor{
            0.045f + (shotB ? 0.02f : 0.0f),
            0.055f + (shotB ? 0.04f : 0.0f),
            0.075f + (shotB ? 0.05f : 0.0f),
        };

        app.beginFrame();
        app.renderer().beginFrame(output, clearColor);
        app.renderer().drawMesh(
            ground,
            cinerig::Transform{
                {0.0f, -0.02f, 0.0f},
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.11f, 0.14f, 0.18f});
        app.renderer().drawGrid(
            26.0f, 1.0f, {0.20f, 0.24f, 0.30f});
        app.renderer().drawMesh(
            pedestal,
            cinerig::Transform{
                {0.0f, 0.25f, 0.0f},
                cinerig::math::Quat::identity(),
                {1.2f, 0.5f, 1.2f}},
            {0.28f, 0.31f, 0.36f});
        app.renderer().drawMesh(
            targetMesh,
            cinerig::Transform{
                targetPosition,
                cinerig::math::Quat::identity(),
                cinerig::math::Vec3::one},
            {0.35f, 0.95f, 0.55f});
        app.renderer().drawTargetMarker(
            targetPosition, 0.85f, {0.35f, 0.95f, 0.55f});

        const cinerig::math::Vec3 pillarPositions[] = {
            {-3.2f, 0.0f, -2.4f},
            {3.4f, 0.0f, -3.0f},
            {-3.0f, 0.0f, 3.4f},
        };
        const cinerig::math::Vec3 pillarColors[] = {
            {0.36f, 0.22f, 0.48f},
            {0.20f, 0.34f, 0.50f},
            {0.50f, 0.30f, 0.18f},
        };
        for (std::size_t index = 0; index < 3; ++index)
        {
            app.renderer().drawMesh(
                pillar,
                cinerig::Transform{
                    pillarPositions[index]
                        + cinerig::math::Vec3{0.0f, 0.7f, 0.0f},
                    cinerig::math::Quat::identity(),
                    {0.7f, 1.4f, 0.7f}},
                pillarColors[index]);
        }

        const cinerig::VirtualCamera* activeCamera =
            brain->activeVirtualCamera() == cameraA
            ? cameraA
            : (brain->activeVirtualCamera() == cameraB ? cameraB : nullptr);
        for (const auto* camera : {cameraA, cameraB})
        {
            const cinerig::CameraState& state = camera->state();
            const bool live = camera == activeCamera;
            const cinerig::math::Vec3 color =
                camera == cameraA ? cameraAColor : cameraBColor;
            app.renderer().setLineWidth(live ? 2.4f : 1.2f);
            cinerig::LensSettings lens = state.lens;
            lens.farClipPlane = 2.2f;
            app.renderer().drawFrustum(
                state.finalPosition(),
                state.finalOrientation(),
                lens,
                aspect,
                live ? color : color * 0.55f);
            if (!live)
            {
                app.renderer().drawMesh(
                    cameraMesh,
                    cinerig::Transform{
                        state.finalPosition(),
                        cinerig::math::Quat::identity(),
                        cinerig::math::Vec3::one},
                    color);
            }
            app.renderer().setLineWidth(1.0f);
            app.renderer().drawLine(
                state.finalPosition(),
                targetPosition,
                color * 0.35f);
        }

        if (output.hasCamera())
        {
            app.renderer().drawAxes(
                cinerig::Transform{
                    output.position(),
                    output.orientation(),
                    cinerig::math::Vec3::one},
                1.2f);
        }
        app.renderer().endFrame();

        const OverlayResult overlay = storyboardAdapter.present(
            output.appliedCamera(), viewport, app.renderer(), globalMute);

        app.hud().begin();
        app.hud().addLine(
            "CineRig P5-03 storyboard   frame "
            + std::to_string(app.frameCount()));
        std::ostringstream cameraLine;
        cameraLine << "shot " << (shotB ? "B" : "A")
                   << "  live "
                   << (activeCamera != nullptr
                           ? std::string(activeCamera->name())
                           : std::string("(none)"));
        if (brain->isBlending() && brain->activeBlend() != nullptr)
        {
            cameraLine << "  blend "
                       << static_cast<int>(std::lround(
                              brain->activeBlend()->blendWeight() * 100.0f))
                       << "%";
        }
        app.hud().addLine("Camera", cameraLine.str());

        std::ostringstream overlayLine;
        overlayLine << overlay.summary
                    << "  drawn " << overlay.drawn
                    << "  total " << overlay.totalWeight;
        app.hud().addLine("Overlays", overlayLine.str());

        std::ostringstream settingsLine;
        settingsLine << (showImage ? "show" : "hidden")
                     << "  mute A " << (muteA ? "on" : "off")
                     << "  global mute " << (globalMute ? "on" : "off");
        app.hud().addLine("Settings", settingsLine.str());

        std::ostringstream storyboardALine;
        storyboardALine << "fill " << fillName(storyboardAPtr->aspect)
                        << "  split " << storyboardAPtr->splitView
                        << "  alpha " << storyboardAPtr->alpha;
        app.hud().addLine("Story A", storyboardALine.str());
        std::ostringstream storyboardBLine;
        storyboardBLine << "fill " << fillName(storyboardBPtr->aspect)
                        << "  split " << storyboardBPtr->splitView
                        << "  alpha " << storyboardBPtr->alpha;
        app.hud().addLine("Story B", storyboardBLine.str());

        if (overlay.hasPrimary)
        {
            std::ostringstream layoutLine;
            layoutLine << overlay.primaryName
                       << "  image "
                       << static_cast<int>(std::lround(
                              std::abs(overlay.primaryLayout.imageSize.x)))
                       << "x"
                       << static_cast<int>(std::lround(
                              std::abs(overlay.primaryLayout.imageSize.y)))
                       << "  clip "
                       << static_cast<int>(std::lround(
                              overlay.primaryLayout.viewportSize.x))
                       << "x"
                       << static_cast<int>(std::lround(
                              overlay.primaryLayout.viewportSize.y))
                       << " at ("
                       << static_cast<int>(std::lround(
                              overlay.primaryLayout.viewportCenter.x))
                       << ","
                       << static_cast<int>(std::lround(
                              overlay.primaryLayout.viewportCenter.y))
                       << ")";
            app.hud().addLine("Layout", layoutLine.str());
        }
        app.hud().addLine(
            "Controls",
            "Space pause  B shot  S show  M mute A  F fill  "
            "V split  A alpha  G global mute  R reset");
        app.hud().addLine(
            "Legend",
            "orange A  cyan B  textured screen-space overlay");
        app.hud().render(
            static_cast<int>(viewport.x),
            static_cast<int>(viewport.y));

        const bool finalFrame = options.frames > 0
            && app.frameCount() >= static_cast<std::uint64_t>(options.frames);
        if (finalFrame && !options.capturePath.empty())
        {
            std::string captureError;
            if (!app.captureFrame(options.capturePath, &captureError))
                std::cerr << "Capture failed: " << captureError << '\n';
        }

        app.endFrame();
        if (finalFrame)
            break;
    }

    return 0;
}
