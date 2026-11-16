#pragma once

#include <cinerig/core/bezier_spline.hpp>

namespace cinerig
{

// Applies second-order smooth control points to a BezierSpline. This is a
// caller-invoked helper, not a camera component; onSplineModified() maps the
// original editor-only automatic smoothing hook to an explicit edit event.
class SplineSmoother
{
public:
    BezierSpline* spline = nullptr;
    bool autoSmooth = true;

    void reset();
    bool isValid() const noexcept { return spline != nullptr; }

    bool smoothNow();
    bool onSplineModified();
};

} // namespace cinerig
