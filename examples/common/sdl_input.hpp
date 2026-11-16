#pragma once

#include <array>
#include <string_view>

#include <SDL3/SDL_events.h>

#include <cinerig/engine/input_source.hpp>

namespace cinerig::examples
{

// SDL keyboard/mouse adapter with the axis names used by the examples.
class SdlInput final : public IInputSource
{
public:
    void handleEvent(const SDL_Event& event);
    void newFrame();

    float axis(std::string_view name) const override;
    math::Vec2 axis2D(std::string_view name) const override;

    bool keyDown(SDL_Scancode scancode) const;
    bool mouseCaptured() const noexcept;
    void setMouseCaptured(bool captured);
    math::Vec2 mouseDelta() const noexcept;
    float scroll() const noexcept;

private:
    std::array<bool, SDL_SCANCODE_COUNT> keys_{};
    math::Vec2 mouseDelta_{};
    float scroll_ = 0.0f;
    bool mouseCaptured_ = false;
};

}
