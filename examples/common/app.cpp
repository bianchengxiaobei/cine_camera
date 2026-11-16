#include "app.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <vector>

#include <SDL3/SDL_main.h>
#include <glad/gl.h>

namespace cinerig::examples
{

App::App(const char* title, int width, int height, const AppOptions& options)
{
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        error_ = std::string("SDL_Init failed: ") + SDL_GetError();
        return;
    }
    initialized_ = true;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
    if (options.resizable)
        flags |= SDL_WINDOW_RESIZABLE;
    if (!options.visible)
        flags |= SDL_WINDOW_HIDDEN;

    window_ = SDL_CreateWindow(title, std::max(width, 1), std::max(height, 1), flags);
    if (window_ == nullptr)
    {
        error_ = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        return;
    }

    context_ = SDL_GL_CreateContext(window_);
    if (context_ == nullptr)
    {
        error_ = std::string("SDL_GL_CreateContext failed: ") + SDL_GetError();
        return;
    }

    if (!SDL_GL_MakeCurrent(window_, context_))
    {
        error_ = std::string("SDL_GL_MakeCurrent failed: ") + SDL_GetError();
        return;
    }

    SDL_GL_SetSwapInterval(options.vsync ? 1 : 0);
    updateViewport();

    std::string rendererError;
    if (!renderer_.initialize(&rendererError))
    {
        error_ = std::string("GlRenderer initialization failed: ") + rendererError;
        return;
    }

    std::string hudError;
    if (!hud_.initialize(&hudError))
    {
        error_ = std::string("Hud initialization failed: ") + hudError;
        return;
    }

    lastFrameTime_ = std::chrono::steady_clock::now();
    running_ = true;
}

App::~App()
{
    if (context_ != nullptr && window_ != nullptr)
        SDL_GL_MakeCurrent(window_, context_);
    hud_.shutdown();
    renderer_.shutdown();

    if (context_ != nullptr)
        SDL_GL_DestroyContext(context_);
    if (window_ != nullptr)
        SDL_DestroyWindow(window_);
    if (initialized_)
        SDL_Quit();
}

bool App::running() const noexcept
{
    return running_;
}

bool App::ok() const noexcept
{
    return running_ && error_.empty();
}

const std::string& App::error() const noexcept
{
    return error_;
}

void App::pollEvents()
{
    if (!initialized_)
        return;

    const auto now = std::chrono::steady_clock::now();
    const float frameDelta =
        std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;
    deltaTime_ = std::max(0.0f, frameDelta);
    elapsedTime_ += deltaTime_;
    input_.newFrame();

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        const bool wasCaptured = input_.mouseCaptured();
        input_.handleEvent(event);

        switch (event.type)
        {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            running_ = false;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == SDL_SCANCODE_ESCAPE && !wasCaptured)
                running_ = false;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            updateViewport();
            break;
        default:
            break;
        }
    }

    syncMouseCapture();
}

void App::beginFrame()
{
    if (!ok())
        return;

    ++frameCount_;
    updateViewport();
    glViewport(
        0,
        0,
        static_cast<GLsizei>(viewport_.x),
        static_cast<GLsizei>(viewport_.y));
}

void App::endFrame()
{
    if (ok())
        SDL_GL_SwapWindow(window_);
}

bool App::captureFrame(const std::string& path, std::string* error) const
{
    const auto fail = [error](const char* message) {
        if (error != nullptr)
            *error = message;
        return false;
    };

    if (!ok())
        return fail("application is not running");
    if (path.empty())
        return fail("capture path is empty");

    const int width = static_cast<int>(viewport_.x);
    const int height = static_cast<int>(viewport_.y);
    if (width <= 0 || height <= 0)
        return fail("invalid viewport size");

    std::vector<unsigned char> rgba(
        static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height)
        * 4u);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    const int rowSize = (width * 3 + 3) & ~3;
    const std::uint32_t imageSize =
        static_cast<std::uint32_t>(rowSize) * static_cast<std::uint32_t>(height);
    std::vector<unsigned char> file(
        54u + static_cast<std::size_t>(imageSize),
        0u);

    const auto write16 = [&file](std::size_t offset, std::uint16_t value) {
        file[offset] = static_cast<unsigned char>(value & 0xFFu);
        file[offset + 1] = static_cast<unsigned char>((value >> 8u) & 0xFFu);
    };
    const auto write32 = [&file](std::size_t offset, std::uint32_t value) {
        file[offset] = static_cast<unsigned char>(value & 0xFFu);
        file[offset + 1] = static_cast<unsigned char>((value >> 8u) & 0xFFu);
        file[offset + 2] = static_cast<unsigned char>((value >> 16u) & 0xFFu);
        file[offset + 3] = static_cast<unsigned char>((value >> 24u) & 0xFFu);
    };

    file[0] = 'B';
    file[1] = 'M';
    write32(2, static_cast<std::uint32_t>(file.size()));
    write32(10, 54u);
    write32(14, 40u);
    write32(18, static_cast<std::uint32_t>(width));
    write32(22, static_cast<std::uint32_t>(height));
    write16(26, 1u);
    write16(28, 24u);
    write32(34, imageSize);
    write32(38, 2835u);
    write32(42, 2835u);

    for (int y = 0; y < height; ++y)
    {
        const std::size_t row =
            54u + static_cast<std::size_t>(y)
            * static_cast<std::size_t>(rowSize);
        const std::size_t sourceRow =
            static_cast<std::size_t>(y)
            * static_cast<std::size_t>(width)
            * 4u;
        for (int x = 0; x < width; ++x)
        {
            const std::size_t source =
                sourceRow + static_cast<std::size_t>(x) * 4u;
            const std::size_t destination =
                row + static_cast<std::size_t>(x) * 3u;
            file[destination] = rgba[source + 2u];
            file[destination + 1] = rgba[source + 1u];
            file[destination + 2] = rgba[source];
        }
    }

    std::ofstream output(path, std::ios::binary);
    if (!output)
        return fail("failed to open capture path");
    output.write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    if (!output)
        return fail("failed to write capture file");
    return true;
}

void App::requestQuit()
{
    running_ = false;
}

float App::deltaTime() const noexcept
{
    return deltaTime_;
}

float App::elapsedTime() const noexcept
{
    return elapsedTime_;
}

std::uint64_t App::frameCount() const noexcept
{
    return frameCount_;
}

math::Vec2 App::viewportSize() const noexcept
{
    return viewport_;
}

SdlInput& App::input() noexcept
{
    return input_;
}

const SdlInput& App::input() const noexcept
{
    return input_;
}

GlRenderer& App::renderer() noexcept
{
    return renderer_;
}

const GlRenderer& App::renderer() const noexcept
{
    return renderer_;
}

Hud& App::hud() noexcept
{
    return hud_;
}

const Hud& App::hud() const noexcept
{
    return hud_;
}

SDL_Window* App::window() const noexcept
{
    return window_;
}

SDL_GLContext App::glContext() const noexcept
{
    return context_;
}

void App::updateViewport()
{
    if (window_ == nullptr)
        return;

    int width = 1;
    int height = 1;
    if (!SDL_GetWindowSizeInPixels(window_, &width, &height)
        || width <= 0
        || height <= 0)
    {
        width = 1;
        height = 1;
    }
    viewport_ = {
        static_cast<float>(width),
        static_cast<float>(height),
    };
}

void App::syncMouseCapture()
{
    if (window_ == nullptr)
        return;

    const bool captured = input_.mouseCaptured();
    if (captured == mouseCaptureApplied_)
        return;

    SDL_SetWindowRelativeMouseMode(window_, captured);
    mouseCaptureApplied_ = captured;
}

}
