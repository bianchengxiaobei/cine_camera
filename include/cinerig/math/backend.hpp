#pragma once

namespace cinerig::math
{

enum class Backend
{
    Native,
    Glm,
};

inline constexpr Backend backend = Backend::Native;

const char* backendName() noexcept;

}
