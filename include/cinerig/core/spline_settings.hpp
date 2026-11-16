#pragma once

#include <cinerig/core/spline.hpp>

namespace cinerig
{

// Maximum valid position for the given unit. The minimum is always 0.
float splineMaxPosition(const ISpline& spline, PathIndexUnit unit);

// Clamp open splines and wrap closed splines into their valid range. NaN
// positions become 0. When maxPosition is non-null it receives the maximum
// position for the selected unit.
float splineStandardizePosition(
    const ISpline& spline,
    float position,
    PathIndexUnit unit,
    float* maxPosition = nullptr);

// Non-owning reference to a spline plus a position expressed in the selected
// units. The caller must keep the spline alive for the lifetime of this value.
struct SplineSettings
{
    const ISpline* spline = nullptr;
    float position = 0.0f;
    PathIndexUnit units = PathIndexUnit::Distance;

    bool isValid() const noexcept { return spline != nullptr; }

    float maxPosition() const;
    float standardizedPosition(float* maxPosition = nullptr) const;
    float normalizedPosition() const;

    // Change the unit while preserving the point on the spline. The numeric
    // position field is converted to the new unit.
    void changeUnitPreservePosition(PathIndexUnit newUnits);
};

}
