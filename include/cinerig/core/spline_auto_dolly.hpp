#pragma once

#include <memory>
#include <optional>

#include <cinerig/core/spline.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

// Selects a position on a spline for automatic dolly movement.
struct SplineAutoDolly
{
    class ISplineAutoDolly
    {
    public:
        virtual ~ISplineAutoDolly() = default;

        virtual void validate() {}
        virtual void reset() {}

        virtual bool requiresTrackingTarget() const = 0;

        virtual float getSplinePosition(
            const ISpline& spline,
            const std::optional<math::Vec3>& targetPosition,
            float currentPosition,
            PathIndexUnit positionUnits,
            float deltaTime) = 0;
    };

    // Moves at a constant speed expressed in the current position units per
    // second.
    class FixedSpeed final : public ISplineAutoDolly
    {
    public:
        float speed = 0.0f;

        void validate() override {}
        void reset() override {}
        bool requiresTrackingTarget() const override { return false; }
        float getSplinePosition(
            const ISpline& spline,
            const std::optional<math::Vec3>& targetPosition,
            float currentPosition,
            PathIndexUnit positionUnits,
            float deltaTime) override;
    };

    // Chooses the point on the spline closest to the tracking target.
    class NearestPointToTarget final : public ISplineAutoDolly
    {
    public:
        float positionOffset = 0.0f;
        int searchResolution = 4;
        int searchIteration = 2;

        void validate() override;
        void reset() override {}
        bool requiresTrackingTarget() const override { return true; }
        float getSplinePosition(
            const ISpline& spline,
            const std::optional<math::Vec3>& targetPosition,
            float currentPosition,
            PathIndexUnit positionUnits,
            float deltaTime) override;
    };

    bool enabled = false;
    std::shared_ptr<ISplineAutoDolly> method;

    void validate();
    void reset();
    bool requiresTrackingTarget() const;

    float getSplinePosition(
        const ISpline& spline,
        const std::optional<math::Vec3>& targetPosition,
        float currentPosition,
        PathIndexUnit positionUnits,
        float deltaTime) const;
};

} // namespace cinerig
