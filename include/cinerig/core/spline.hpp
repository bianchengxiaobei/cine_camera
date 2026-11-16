#pragma once

#include <cstddef>
#include <cstdint>

#include <cinerig/math/vec.hpp>

namespace cinerig
{

// How a position value along a spline is interpreted.
enum class PathIndexUnit : std::uint8_t
{
    // World distance from the start of the spline.
    Distance = 0,

    // Global arc-length fraction in [0, 1].
    Normalized,

    // Knot index plus a fraction in the segment's Bezier parameter.
    Knot,
};

// Non-owning spline sample interface. Parameter values are normalized
// arc-length fractions in [0, 1], and positions are in the core coordinate
// convention. The host owns the spline and must keep it alive while referenced.
class ISpline
{
public:
    virtual ~ISpline() = default;

    virtual float length() const = 0;
    virtual math::Vec3 evaluatePosition(float t) const = 0;
    virtual math::Vec3 evaluateTangent(float t) const = 0;
    virtual math::Vec3 evaluateUp(float t) const = 0;
    virtual float closestPoint(math::Vec3 worldPosition) const = 0;

    // Closest normalized position using a configurable number of samples per
    // curve and local refinement iterations. The default implementation
    // samples the normalized parameter; concrete splines may override it.
    virtual float closestPoint(
        math::Vec3 worldPosition,
        int searchResolution,
        int searchIterations) const;

    // Metadata used by position-unit conversion. Implementations should
    // override these when they contain more than one knot or form a loop.
    virtual std::size_t knotCount() const { return 1; }
    virtual bool closed() const { return false; }

    // Convert a spline position between units. The default implementation is
    // exact for distance/normalized and treats knot units as a linear
    // interpolation across the curve count. Splines with non-uniform segment
    // lengths should override this.
    virtual float convertPosition(
        float position,
        PathIndexUnit from,
        PathIndexUnit to) const;
};

}
