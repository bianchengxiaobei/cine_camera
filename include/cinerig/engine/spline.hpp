#pragma once

#include <cinerig/core/spline.hpp>
#include <cinerig/engine/entity_world.hpp>

namespace cinerig
{

// The sampling contract lives in core so core::SplineSettings can use it
// without depending on the engine layer. Include this header for the engine
// adapter surface and the provider below.

// Non-owning registry of splines. get() returns nullptr for unknown ids.
class ISplineProvider
{
public:
    virtual ~ISplineProvider() = default;
    virtual const ISpline* get(SplineId id) const = 0;
};

}
