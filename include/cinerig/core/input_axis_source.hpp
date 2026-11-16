#pragma once

#include <string_view>

namespace cinerig
{

class IInputAxisSource
{
public:
    virtual ~IInputAxisSource() = default;
    virtual float axis(std::string_view name) const = 0;
};

} // namespace cinerig
