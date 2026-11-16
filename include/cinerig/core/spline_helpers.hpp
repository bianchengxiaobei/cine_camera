#pragma once

#include <vector>

#include <cinerig/math/vec.hpp>

namespace cinerig
{

// Cubic Bezier utilities used by spline evaluation and smoothing.
math::Vec3 bezier3(
    float t,
    math::Vec3 p0,
    math::Vec3 p1,
    math::Vec3 p2,
    math::Vec3 p3);

math::Vec3 bezierTangent3(
    float t,
    math::Vec3 p0,
    math::Vec3 p1,
    math::Vec3 p2,
    math::Vec3 p3);

void bezierTangentWeights3(
    math::Vec3 p0,
    math::Vec3 p1,
    math::Vec3 p2,
    math::Vec3 p3,
    math::Vec3& w0,
    math::Vec3& w1,
    math::Vec3& w2);

float bezier1(float t, float p0, float p1, float p2, float p3);
float bezierTangent1(float t, float p0, float p1, float p2, float p3);

// Compute second-order smooth cubic control points for an open knot sequence.
// The outputs receive one control point per segment; the final entry is unused.
void computeSmoothControlPoints(
    const std::vector<math::Vec3>& knots,
    std::vector<math::Vec3>& ctrl1,
    std::vector<math::Vec3>& ctrl2);

// Same as above, treating the knot sequence as a loop.
void computeSmoothControlPointsLooped(
    const std::vector<math::Vec3>& knots,
    std::vector<math::Vec3>& ctrl1,
    std::vector<math::Vec3>& ctrl2);

}
