#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
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
            std::cout << "Usage: cinerig_38_pixel_perfect [--frames N] "
                         "[--hidden] [--no-vsync] [--capture file.bmp]\n";
            std::exit(0);
        }
    }
    return options;
}

const char* cropName(cinerig::PixelPerfectCropFrame cropFrame)
{
    switch (cropFrame)
    {
    case cinerig::PixelPerfectCropFrame::None:
        return "none";
    case cinerig::PixelPerfectCropFrame::Pillarbox:
        return "pillarbox";
    case cinerig::PixelPerfectCropFrame::Letterbox:
        return "letterbox";
    case cinerig::PixelPerfectCropFrame::Windowbox:
        return "windowbox";
    case cinerig::PixelPerfectCropFrame::StretchFill:
        return "stretch-fill";
    }
    return "unknown";
}

const char* gridName(cinerig::PixelPerfectGridSnapping gridSnapping)
{
    switch (gridSnapping)
    {
    case cinerig::PixelPerfectGridSnapping::None:
        return "none";
    case cinerig::PixelPerfectGridSnapping::PixelSnapping:
        return "pixel-snap";
    case cinerig::PixelPerfectGridSnapping::UpscaleRenderTexture:
        return "upscale-rt";
    }
    return "unknown";
}

void drawPixelBlock(
    cinerig::examples::GlRenderer& renderer,
    cinerig::examples::GlRenderer::MeshHandle cube,
    cinerig::math::Vec2 center,
    cinerig::math::Vec2 size,
    cinerig::math::Vec3 color)
{
    renderer.drawMesh(
        cube,
        cinerig::Transform{
            {center.x, center.y, 0.02f},
            cinerig::math::Quat::identity(),
            {size.x, size.y, 0.01f}},
        color);
}

void drawReferenceGrid(
    cinerig::examples::GlRenderer& renderer,
    float halfWidth,
    float halfHeight,
    float pixelSize)
{
    const cinerig::math::Vec3 minor{0.11f, 0.16f, 0.22f};
    const cinerig::math::Vec3 medium{0.18f, 0.25f, 0.34f};
    const cinerig::math::Vec3 major{0.32f, 0.44f, 0.56f};

    const int columns = static_cast<int>(std::lround(halfWidth * 2.0f / pixelSize));
    const int rows = static_cast<int>(std::lround(halfHeight * 2.0f / pixelSize));
    for (int index = 0; index <= columns; ++index)
    {
        const float x = -halfWidth + static_cast<float>(index) * pixelSize;
        const cinerig::math::Vec3 color = index % 32 == 0
            ? major
            : (index % 8 == 0 ? medium : minor);
        renderer.drawLine(
            {x, -halfHeight, 0.005f},
            {x, halfHeight, 0.005f},
            color);
    }
    for (int index = 0; index <= rows; ++index)
    {
        const float y = -halfHeight + static_cast<float>(index) * pixelSize;
        const cinerig::math::Vec3 color = index % 32 == 0
            ? major
            : (index % 8 == 0 ? medium : minor);
        renderer.drawLine(
            {-halfWidth, y, 0.005f},
            {halfWidth, y, 0.005f},
            color);
    }
}

void drawBorder(
    cinerig::examples::GlRenderer& renderer,
    cinerig::math::Vec2 center,
    float halfWidth,
    float halfHeight,
    cinerig::math::Vec3 color)
{
    const cinerig::math::Vec3 topLeft{
        center.x - halfWidth, center.y + halfHeight, 0.04f};
    const cinerig::math::Vec3 topRight{
        center.x + halfWidth, center.y + halfHeight, 0.04f};
    const cinerig::math::Vec3 bottomRight{
        center.x + halfWidth, center.y - halfHeight, 0.04f};
    const cinerig::math::Vec3 bottomLeft{
        center.x - halfWidth, center.y - halfHeight, 0.04f};
    renderer.drawLine(topLeft, topRight, color);
    renderer.drawLine(topRight, bottomRight, color);
    renderer.drawLine(bottomRight, bottomLeft, color);
    renderer.drawLine(bottomLeft, topLeft, color);
}

void drawHeart(
    cinerig::examples::GlRenderer& renderer,
    cinerig::examples::GlRenderer::MeshHandle cube,
    cinerig::math::Vec2 origin,
    float pixelSize)
{
    static constexpr const char* kRows[] = {
        "................",
        "...##.....##....",
        "..####...####...",
        ".######.######..",
        ".#############..",
        ".#############..",
        "..###########...",
        "..###########...",
        "...#########....",
        "....#######.....",
        ".....#####......",
        "......###.......",
        ".......#........",
        "................",
        "................",
        "................",
    };
    constexpr int rowCount = static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
    constexpr int columnCount = 16;

    for (int row = 0; row < rowCount; ++row)
    {
        for (int column = 0; column < columnCount; ++column)
        {
            if (kRows[row][column] != '#')
                continue;
            const cinerig::math::Vec3 color = row < 5
                ? cinerig::math::Vec3{1.0f, 0.42f, 0.55f}
                : cinerig::math::Vec3{0.92f, 0.16f, 0.33f};
            drawPixelBlock(
                renderer,
                cube,
                {
                    origin.x + (static_cast<float>(column) - columnCount * 0.5f + 0.5f)
                        * pixelSize,
                    origin.y + (rowCount * 0.5f - static_cast<float>(row) - 0.5f)
                        * pixelSize,
                },
                {pixelSize, pixelSize},
                color);
        }
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
        "CineRig P5-04 Pixel Perfect", 1280, 720, appOptions);
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

    const cinerig::EntityId cameraEntity = world.createEntity({
        {0.0f, 0.0f, 10.0f},
        cinerig::math::Quat::identity(),
        cinerig::math::Vec3::one,
    });

    const cinerig::CameraId cameraId =
        cameraSystem.createCamera<cinerig::VirtualCamera>("Pixel Perfect Camera");
    auto* camera = cameraSystem.camera(cameraId);
    if (camera == nullptr)
    {
        std::cerr << "Failed to create pixel perfect camera\n";
        return 1;
    }

    constexpr float kTargetOrthoSize = 3.0f;
    constexpr float kPixelSize = 1.0f / 32.0f;

    camera->ownerEntity = cameraEntity;
    camera->priority = cinerig::PrioritySettings(10);
    camera->standbyUpdate = cinerig::StandbyUpdateMode::Always;
    camera->lens.orthographic = true;
    camera->lens.orthographicSize = kTargetOrthoSize;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 50.0f;
    camera->lens.aspect = app.viewportSize().x / app.viewportSize().y;

    auto ownedExtension = std::make_unique<cinerig::PixelPerfectExtension>();
    auto* extension = ownedExtension.get();
    extension->settings.assetsPPU = 32;
    extension->settings.refResolutionX = 320;
    extension->settings.refResolutionY = 180;
    extension->settings.cropFrame = cinerig::PixelPerfectCropFrame::None;
    extension->settings.gridSnapping =
        cinerig::PixelPerfectGridSnapping::PixelSnapping;
    extension->screenSize = app.viewportSize();
    extension->validate();
    camera->addExtension(std::move(ownedExtension));

    output.setDefaultLens(camera->lens);
    output.setViewportSize(app.viewportSize());

    const cinerig::examples::GlRenderer::MeshHandle cube =
        app.renderer().createCube(1.0f);

    cinerig::PixelPerfectCropFrame crops[] = {
        cinerig::PixelPerfectCropFrame::None,
        cinerig::PixelPerfectCropFrame::Pillarbox,
        cinerig::PixelPerfectCropFrame::Letterbox,
        cinerig::PixelPerfectCropFrame::Windowbox,
        cinerig::PixelPerfectCropFrame::StretchFill,
    };
    cinerig::PixelPerfectGridSnapping grids[] = {
        cinerig::PixelPerfectGridSnapping::None,
        cinerig::PixelPerfectGridSnapping::PixelSnapping,
        cinerig::PixelPerfectGridSnapping::UpscaleRenderTexture,
    };
    const float targets[] = {2.6f, 3.0f, 3.4f};

    int cropIndex = 0;
    int gridIndex = 1;
    int targetIndex = 1;
    bool extensionEnabled = true;
    bool previousPixelPerfect = false;
    bool previousGrid = false;
    bool previousCrop = false;
    bool previousTarget = false;
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
        extension->screenSize = viewport;

        const bool pixelPerfectDown =
            app.input().keyDown(SDL_SCANCODE_P);
        const bool gridDown = app.input().keyDown(SDL_SCANCODE_G);
        const bool cropDown = app.input().keyDown(SDL_SCANCODE_C);
        const bool targetDown = app.input().keyDown(SDL_SCANCODE_T);
        const bool resetDown = app.input().keyDown(SDL_SCANCODE_R);

        if (pixelPerfectDown && !previousPixelPerfect)
        {
            extensionEnabled = !extensionEnabled;
            extension->setEnabled(extensionEnabled);
        }
        if (gridDown && !previousGrid)
        {
            gridIndex = (gridIndex + 1) % 3;
            extension->settings.gridSnapping = grids[gridIndex];
            extension->validate();
        }
        if (cropDown && !previousCrop)
        {
            cropIndex = (cropIndex + 1) % 5;
            extension->settings.cropFrame = crops[cropIndex];
            extension->validate();
        }
        if (targetDown && !previousTarget)
        {
            targetIndex = (targetIndex + 1) % 3;
            camera->lens.orthographicSize = targets[targetIndex];
        }
        if (resetDown && !previousReset)
        {
            extensionEnabled = true;
            extension->setEnabled(true);
            cropIndex = 0;
            gridIndex = 1;
            targetIndex = 1;
            extension->settings.assetsPPU = 32;
            extension->settings.refResolutionX = 320;
            extension->settings.refResolutionY = 180;
            extension->settings.cropFrame = crops[cropIndex];
            extension->settings.gridSnapping = grids[gridIndex];
            extension->validate();
            camera->lens.orthographicSize = targets[targetIndex];
        }

        previousPixelPerfect = pixelPerfectDown;
        previousGrid = gridDown;
        previousCrop = cropDown;
        previousTarget = targetDown;
        previousReset = resetDown;

        cameraSystem.lateUpdate(deltaTime);

        const cinerig::PixelPerfectLayout& layout = extension->layout();
        const cinerig::PixelPerfectCorrection& correction =
            extension->correction();
        const float appliedOrtho =
            output.appliedCamera().state.lens.orthographicSize;
        const float referenceWidth =
            extension->settings.refResolutionX / static_cast<float>(
                extension->settings.assetsPPU);
        const float referenceHeight =
            extension->settings.refResolutionY / static_cast<float>(
                extension->settings.assetsPPU);

        app.beginFrame();
        app.renderer().beginFrame(output, {0.025f, 0.035f, 0.055f});
        drawReferenceGrid(
            app.renderer(),
            referenceWidth * 0.5f,
            referenceHeight * 0.5f,
            kPixelSize);
        drawHeart(
            app.renderer(),
            cube,
            {0.0f, -0.45f},
            kPixelSize * 2.0f);
        drawBorder(
            app.renderer(),
            {0.0f, 0.0f},
            referenceWidth * 0.5f - 0.05f,
            referenceHeight * 0.5f - 0.05f,
            {0.20f, 0.85f, 0.95f});
        app.renderer().endFrame();

        app.hud().begin();
        app.hud().addLine("CineRig P5-04 pixel perfect");
        app.hud().addLine(
            "Pixel Perfect",
            extensionEnabled ? "ON  (P toggle)" : "OFF (P toggle)");
        std::ostringstream screenLine;
        screenLine << static_cast<int>(viewport.x) << "x"
                   << static_cast<int>(viewport.y)
                   << "  ref " << extension->settings.refResolutionX << "x"
                   << extension->settings.refResolutionY
                   << "  PPU " << extension->settings.assetsPPU;
        app.hud().addLine("Screen", screenLine.str());
        std::ostringstream modeLine;
        modeLine << "crop " << cropName(extension->settings.cropFrame)
                 << "  grid " << gridName(extension->settings.gridSnapping);
        app.hud().addLine("Mode", modeLine.str());
        std::ostringstream propertyLine;
        propertyLine << "zoom " << layout.zoom
                     << "  vcam " << correction.cinemachineVCamZoom
                     << "  pixel x" << correction.pixelRatio
                     << "  units/pixel "
                     << std::fixed << std::setprecision(5)
                     << correction.unitsPerPixel;
        app.hud().addLine("Properties", propertyLine.str());
        std::ostringstream lensLine;
        lensLine << std::fixed << std::setprecision(3)
                 << "target " << camera->lens.orthographicSize
                 << " -> applied " << appliedOrtho;
        app.hud().addLine("Lens", lensLine.str());
        std::ostringstream rtLine;
        if (layout.useOffscreenRT)
        {
            const cinerig::PixelPerfectFinalBlit finalBlit =
                cinerig::calculatePixelPerfectFinalBlit(
                    extension->settings, layout);
            rtLine << layout.offscreenRTWidth << "x"
                   << layout.offscreenRTHeight
                   << "  final " << static_cast<int>(finalBlit.pixelRect.width)
                   << "x" << static_cast<int>(finalBlit.pixelRect.height)
                   << "  upscale "
                   << (finalBlit.requiresUpscaling ? "yes" : "no");
        }
        else
        {
            rtLine << "none";
        }
        app.hud().addLine("Offscreen", rtLine.str());
        app.hud().addLine(
            "Controls",
            "P on/off  G grid  C crop  T target  R reset");
        app.hud().addLine("Frame", std::to_string(app.frameCount()));
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
