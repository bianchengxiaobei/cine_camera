#pragma once

#include <string_view>

#include <cinerig/core/input_axis_source.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

// Non-owning input adapter. Names are stable bindings chosen by the example or
// game. Implementations return 0 for unbound axes.
class IInputSource : public IInputAxisSource
{
public:
    virtual math::Vec2 axis2D(std::string_view name) const = 0;
};

}
