#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <cinerig/core/spline.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

// Cubic Bezier knot. Tangents are control-point offsets in world units. The
// rotation supplies a preferred up direction and twist between knots.
struct BezierKnot
{
    math::Vec3 position{};
    math::Vec3 tangentIn{};
    math::Vec3 tangentOut{};
    math::Quat rotation = math::Quat::identity();
};

// Engine-independent cubic Bezier spline. Normalized parameters are global
// arc-length fractions, matching SplineSettings semantics.
class BezierSpline final : public ISpline
{
public:
    static constexpr int ArcSamplesPerCurve = 32;

    BezierSpline() = default;
    explicit BezierSpline(std::vector<BezierKnot> knots, bool closed = false);

    // Build a piecewise-linear spline by placing the control points one third
    // of the way along each adjacent segment.
    static BezierSpline linear(
        const std::vector<math::Vec3>& points,
        bool closed = false);

    // Build a second-order smooth spline using SplineHelpers control points.
    static BezierSpline smooth(
        const std::vector<math::Vec3>& points,
        bool closed = false);

    void setKnots(std::vector<BezierKnot> knots, bool closed);
    void clear();

    bool isValid() const noexcept { return !knots_.empty(); }
    const std::vector<BezierKnot>& knots() const noexcept { return knots_; }

    std::size_t knotCount() const override { return knots_.size(); }
    bool closed() const override { return closed_; }
    std::size_t curveCount() const noexcept { return curves_.size(); }

    float length() const override { return length_; }
    math::Vec3 evaluatePosition(float t) const override;
    math::Vec3 evaluateTangent(float t) const override;
    math::Vec3 evaluateUp(float t) const override;
    math::Quat evaluateRotation(float t) const;
    float closestPoint(math::Vec3 worldPosition) const override;
    float closestPoint(
        math::Vec3 worldPosition,
        int searchResolution,
        int searchIterations) const override;
    float convertPosition(
        float position,
        PathIndexUnit from,
        PathIndexUnit to) const override;

private:
    struct Curve
    {
        math::Vec3 p0{};
        math::Vec3 p1{};
        math::Vec3 p2{};
        math::Vec3 p3{};
        float length = 0.0f;
        std::array<float, ArcSamplesPerCurve + 1> sampleDistance{};
    };

    struct CurveLocation
    {
        std::size_t curve = 0;
        float localParameter = 0.0f;
    };

    void rebuild();
    CurveLocation locateNormalized(float t) const;
    float distanceAtKnot(float knot) const;
    float knotAtDistance(float distance) const;
    float normalizedAtKnot(float knot) const;
    float knotAtNormalized(float normalized) const;
    float localParameterAtDistance(const Curve& curve, float distance) const;
    float distanceAtLocalParameter(const Curve& curve, float parameter) const;
    math::Vec3 evaluatePositionAt(const CurveLocation& location) const;
    math::Vec3 evaluateTangentAt(const CurveLocation& location) const;
    math::Quat rotationAt(const CurveLocation& location) const;
    float maxKnotPosition() const;

    std::vector<BezierKnot> knots_;
    std::vector<Curve> curves_;
    std::vector<float> curveStartDistance_;
    bool closed_ = false;
    float length_ = 0.0f;
};

}
