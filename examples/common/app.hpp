#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include <SDL3/SDL.h>

#include <cinerig/math/vec.hpp>

#include "gl_renderer.hpp"
#include "hud.hpp"
#include "sdl_input.hpp"

namespace cinerig::examples
{

struct AppOptions
{
    bool visible = true;
    bool vsync = true;
    bool resizable = true;
};

// SDL3 window, OpenGL 3.3 context, timing, events, renderer and HUD.
class App
{
public:
    App(const char* title, int width, int height, const AppOptions& options = {});
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool running() const noexcept;
    bool ok() const noexcept;
    const std::string& error() const noexcept;

    void pollEvents();
    void beginFrame();
    void endFrame();
    void requestQuit();

    bool captureFrame(const std::string& path, std::string* error = nullptr) const;

    float deltaTime() const noexcept;
    float elapsedTime() const noexcept;
    std::uint64_t frameCount() const noexcept;
    math::Vec2 viewportSize() const noexcept;

    SdlInput& input() noexcept;
    const SdlInput& input() const noexcept;
    GlRenderer& renderer() noexcept;
    const GlRenderer& renderer() const noexcept;
    Hud& hud() noexcept;
    const Hud& hud() const noexcept;

    SDL_Window* window() const noexcept;
    SDL_GLContext glContext() const noexcept;

private:
    void updateViewport();
    void syncMouseCapture();

    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
    SdlInput input_;
    GlRenderer renderer_;
    Hud hud_;
    std::string error_;
    math::Vec2 viewport_{1.0f, 1.0f};
    float deltaTime_ = 0.0f;
    float elapsedTime_ = 0.0f;
    std::uint64_t frameCount_ = 0;
    bool running_ = false;
    bool initialized_ = false;
    bool mouseCaptureApplied_ = false;
    std::chrono::steady_clock::time_point lastFrameTime_{};
};

}

