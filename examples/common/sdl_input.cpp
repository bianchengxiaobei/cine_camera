#include "sdl_input.hpp"

#include <string>

namespace cinerig::examples
{

void SdlInput::handleEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
        if (event.key.scancode < SDL_SCANCODE_COUNT)
            keys_[static_cast<std::size_t>(event.key.scancode)] = true;
        if (event.key.scancode == SDL_SCANCODE_ESCAPE)
            mouseCaptured_ = false;
        break;
    case SDL_EVENT_KEY_UP:
        if (event.key.scancode < SDL_SCANCODE_COUNT)
            keys_[static_cast<std::size_t>(event.key.scancode)] = false;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button == SDL_BUTTON_RIGHT)
            mouseCaptured_ = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_RIGHT)
            mouseCaptured_ = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (mouseCaptured_)
        {
            mouseDelta_.x += event.motion.xrel;
            mouseDelta_.y += event.motion.yrel;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        scroll_ += event.wheel.y;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        keys_.fill(false);
        mouseCaptured_ = false;
        break;
    default:
        break;
    }
}

void SdlInput::newFrame()
{
    mouseDelta_ = math::Vec2::zero;
    scroll_ = 0.0f;
}

float SdlInput::axis(std::string_view name) const
{
    if (name == "Mouse X" || name == "MouseX")
        return mouseDelta_.x;
    if (name == "Mouse Y" || name == "MouseY")
        return mouseDelta_.y;
    if (name == "Horizontal")
    {
        const bool right = keyDown(SDL_SCANCODE_D) || keyDown(SDL_SCANCODE_RIGHT);
        const bool left = keyDown(SDL_SCANCODE_A) || keyDown(SDL_SCANCODE_LEFT);
        return (right ? 1.0f : 0.0f) - (left ? 1.0f : 0.0f);
    }
    if (name == "Vertical")
    {
        const bool up = keyDown(SDL_SCANCODE_W) || keyDown(SDL_SCANCODE_UP);
        const bool down = keyDown(SDL_SCANCODE_S) || keyDown(SDL_SCANCODE_DOWN);
        return (up ? 1.0f : 0.0f) - (down ? 1.0f : 0.0f);
    }
    if (name == "Scroll")
        return scroll_;
    return 0.0f;
}

math::Vec2 SdlInput::axis2D(std::string_view name) const
{
    if (name == "Move" || name == "Movement")
        return {axis("Horizontal"), axis("Vertical")};
    if (name == "Mouse" || name == "Look")
        return {axis("Mouse X"), axis("Mouse Y")};
    if (name == "Scroll")
        return {scroll_, 0.0f};

    const std::string horizontalName(name);
    return {axis(name), axis(horizontalName + " Y")};
}

bool SdlInput::keyDown(SDL_Scancode scancode) const
{
    return scancode < SDL_SCANCODE_COUNT && keys_[static_cast<std::size_t>(scancode)];
}

bool SdlInput::mouseCaptured() const noexcept
{
    return mouseCaptured_;
}

void SdlInput::setMouseCaptured(bool captured)
{
    mouseCaptured_ = captured;
}

math::Vec2 SdlInput::mouseDelta() const noexcept
{
    return mouseDelta_;
}

float SdlInput::scroll() const noexcept
{
    return scroll_;
}

}
