#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <cinerig/math/vec.hpp>

namespace cinerig::examples
{

// Small immediate-mode text overlay using stb_easy_font. Text coordinates are
// in framebuffer pixels with the origin at the top-left.
class Hud
{
public:
    Hud();
    ~Hud();

    Hud(const Hud&) = delete;
    Hud& operator=(const Hud&) = delete;

    bool initialize(std::string* error = nullptr);
    void shutdown();
    bool isReady() const noexcept;

    void begin();
    void addLine(std::string line);
    void addLine(std::string_view label, std::string_view value);
    const std::vector<std::string>& lines() const noexcept;

    void setTextScale(float scale);
    float textScale() const noexcept;
    void setColor(math::Vec3 color);
    void render(int framebufferWidth, int framebufferHeight);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<std::string> lines_;
    math::Vec3 color_{0.95f, 0.97f, 1.0f};
    float textScale_ = 2.0f;
};

}
