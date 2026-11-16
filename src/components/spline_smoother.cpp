#include <cinerig/components/spline_smoother.hpp>

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <cinerig/core/spline_helpers.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{
namespace
{

bool finitePosition(math::Vec3 position)
{
    return std::isfinite(position.x)
        && std::isfinite(position.y)
        && std::isfinite(position.z);
}

math::Vec3 fallbackForward(
    const std::vector<math::Vec3>& positions,
    std::size_t index,
    bool closed)
{
    const std::size_t count = positions.size();
    if (closed || index + 1 < count)
    {
        const math::Vec3 next = positions[(index + 1) % count] - positions[index];
        if (!next.isNaN() && !math::almostZero(next))
            return next;
    }
    if (index > 0)
    {
        const math::Vec3 previous = positions[index] - positions[index - 1];
        if (!previous.isNaN() && !math::almostZero(previous))
            return previous;
    }
    return math::Vec3::forward;
}

} // namespace

void SplineSmoother::reset()
{
    spline = nullptr;
    autoSmooth = true;
}

bool SplineSmoother::smoothNow()
{
    if (spline == nullptr)
        return false;

    const std::vector<BezierKnot>& original = spline->knots();
    const std::size_t count = original.size();
    if (count < 3)
        return false;

    std::vector<math::Vec3> positions(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        positions[index] = original[index].position;
        if (!finitePosition(positions[index]))
            return false;
    }

    const bool closed = spline->closed();
    std::vector<math::Vec3> ctrl1;
    std::vector<math::Vec3> ctrl2;
    if (closed)
        computeSmoothControlPointsLooped(positions, ctrl1, ctrl2);
    else
        computeSmoothControlPoints(positions, ctrl1, ctrl2);

    std::vector<BezierKnot> smoothed = original;
    for (std::size_t index = 0; index < count; ++index)
    {
        const bool openStart = !closed && index == 0;
        const bool openEnd = !closed && index + 1 == count;

        math::Vec3 forward = openEnd
            ? positions[index] - ctrl2[index - 1]
            : ctrl1[index] - positions[index];
        if (forward.isNaN() || math::almostZero(forward))
            forward = fallbackForward(positions, index, closed);

        float forwardLength = forward.length();
        float backLength = forwardLength;
        if (!openStart && !openEnd)
        {
            const math::Vec3 back =
                ctrl2[(index + count - 1) % count] - positions[index];
            if (!back.isNaN() && !math::almostZero(back))
                backLength = back.length();
        }

        const math::Vec3 direction = math::normalize(forward);
        const float tangentLength = (forwardLength + backLength) * 0.5f;

        math::Vec3 up = math::rotate(original[index].rotation, math::Vec3::up);
        if (up.isNaN() || math::almostZero(up))
            up = math::Vec3::up;

        smoothed[index].position = positions[index];
        smoothed[index].rotation = math::lookRotation(forward, up);
        smoothed[index].tangentOut = openEnd
            ? math::Vec3{}
            : direction * tangentLength;
        smoothed[index].tangentIn = openStart
            ? math::Vec3{}
            : direction * -tangentLength;
    }

    spline->setKnots(std::move(smoothed), closed);
    return true;
}

bool SplineSmoother::onSplineModified()
{
    return autoSmooth && smoothNow();
}

} // namespace cinerig
