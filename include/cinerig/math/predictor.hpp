#pragma once

#include <cinerig/math/damp.hpp>
#include <cinerig/math/quat.hpp>

#include <limits>

namespace cinerig::math
{

struct PositionPredictor
{
    float smoothing = 0.0f;

    void reset()
    {
        havePosition = false;
        smoothDampVelocity = Vec3{};
        velocity = Vec3{};
    }

    bool isEmpty() const { return !havePosition; }
    Vec3 currentPosition() const { return position; }
    Vec3 currentVelocity() const { return velocity; }

    void applyTransformDelta(Vec3 positionDelta)
    {
        position += positionDelta;
    }

    void applyRotationDelta(Quat rotationDelta)
    {
        velocity = rotate(rotationDelta, velocity);
        smoothDampVelocity = rotate(rotationDelta, smoothDampVelocity);
    }

    void addPosition(Vec3 newPosition, float deltaTime)
    {
        if (deltaTime < 0.0f)
            reset();

        if (havePosition && deltaTime > Epsilon)
        {
            const Vec3 targetVelocity = (newPosition - position) / deltaTime;
            const bool slowing = targetVelocity.sqrMagnitude() < velocity.sqrMagnitude();
            const float smoothTime = smoothing / (slowing ? 30.0f : 10.0f);
            velocity = smoothDamp(
                velocity,
                targetVelocity,
                smoothDampVelocity,
                smoothTime,
                std::numeric_limits<float>::infinity(),
                deltaTime);
        }

        position = newPosition;
        havePosition = true;
    }

    Vec3 predictDelta(float lookaheadTime) const
    {
        return velocity * lookaheadTime;
    }

private:
    Vec3 velocity{};
    Vec3 smoothDampVelocity{};
    Vec3 position{};
    bool havePosition = false;
};

}
