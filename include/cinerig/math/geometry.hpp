#pragma once

#include <cinerig/math/quat.hpp>

#include <algorithm>
#include <cmath>

namespace cinerig::math
{

struct Rect
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    constexpr float xMin() const { return x; }
    constexpr float yMin() const { return y; }
    constexpr float xMax() const { return x + width; }
    constexpr float yMax() const { return y + height; }
    constexpr Vec2 center() const { return {x + width * 0.5f, y + height * 0.5f}; }
    constexpr Vec2 size() const { return {width, height}; }
    constexpr bool contains(Vec2 point) const
    {
        return point.x >= xMin() && point.x <= xMax() && point.y >= yMin() && point.y <= yMax();
    }

    Rect inflated(Vec2 delta) const
    {
        Vec2 applied = delta;
        if (width + applied.x * 2.0f < 0.0f)
            applied.x = -width * 0.5f;
        if (height + applied.y * 2.0f < 0.0f)
            applied.y = -height * 0.5f;
        return {
            xMin() - applied.x,
            yMin() - applied.y,
            width + applied.x * 2.0f,
            height + applied.y * 2.0f,
        };
    }
};

struct Bounds
{
    Vec3 center{};
    Vec3 extents{};

    constexpr Vec3 min() const { return center - extents; }
    constexpr Vec3 max() const { return center + extents; }
    constexpr Vec3 size() const { return extents * 2.0f; }
    constexpr bool contains(Vec3 point) const
    {
        return point.x >= min().x && point.x <= max().x
            && point.y >= min().y && point.y <= max().y
            && point.z >= min().z && point.z <= max().z;
    }

    void encapsulate(Vec3 point)
    {
        const Vec3 currentMin = min();
        const Vec3 currentMax = max();
        const Vec3 newMin = cinerig::math::min(currentMin, point);
        const Vec3 newMax = cinerig::math::max(currentMax, point);
        center = (newMin + newMax) * 0.5f;
        extents = (newMax - newMin) * 0.5f;
    }
};

struct BoundingSphere
{
    Vec3 center{};
    float radius = 0.0f;

    constexpr bool contains(Vec3 point) const
    {
        return (point - center).sqrMagnitude() <= radius * radius;
    }
};

struct Ray
{
    Vec3 origin{};
    Vec3 direction{0.0f, 0.0f, -1.0f};

    constexpr Vec3 pointAt(float distance) const { return origin + direction * distance; }
};

struct Plane
{
    Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;

    constexpr float signedDistance(Vec3 point) const { return dot(normal, point) + distance; }
    Vec3 project(Vec3 point) const { return point - normal * signedDistance(point); }

    bool raycast(const Ray& ray, float& enterDistance) const
    {
        const float denominator = dot(normal, ray.direction);
        if (std::abs(denominator) < Epsilon)
            return false;
        const float result = -signedDistance(ray.origin) / denominator;
        if (result < 0.0f)
            return false;
        enterDistance = result;
        return true;
    }
};

inline Vec3 projectOnPlane(Vec3 value, Vec3 planeNormal)
{
    return value - planeNormal * dot(value, planeNormal);
}

inline float signedAngle(Vec3 from, Vec3 to, Vec3 axis)
{
    const float result = angle(from, to);
    return dot(axis, cross(from, to)) < 0.0f ? -result : result;
}

inline float signedAngleDegrees(Vec3 from, Vec3 to, Vec3 axis)
{
    return signedAngle(from, to, axis) * RadToDeg;
}

inline float closestPointOnSegment(Vec3 point, Vec3 start, Vec3 end)
{
    const Vec3 segment = end - start;
    const float lengthSquared = segment.sqrMagnitude();
    if (lengthSquared < Epsilon)
        return 0.0f;
    return clamp01(dot(point - start, segment) / lengthSquared);
}

inline float closestPointOnSegment(Vec2 point, Vec2 start, Vec2 end)
{
    const Vec2 segment = end - start;
    const float lengthSquared = segment.sqrMagnitude();
    if (lengthSquared < Epsilon)
        return 0.0f;
    return clamp01(dot(point - start, segment) / lengthSquared);
}

inline Quat safeFromToRotation(Vec3 from, Vec3 to, Vec3 up)
{
    if (almostZero(from) || almostZero(to))
        return Quat{};

    const Vec3 fromProjected = projectOnPlane(from, up);
    const Vec3 toProjected = projectOnPlane(to, up);
    if (almostZero(fromProjected) || almostZero(toProjected))
    {
        Vec3 axis = cross(from, to);
        if (almostZero(axis))
            axis = orthogonal(from);
        return quatFromAxisAngle(axis, angle(from, to));
    }

    const float pitchChange = angle(to, up) - angle(from, up);
    const Vec3 pitchAxis = normalize(cross(up, from));
    return quatFromAxisAngle(up, signedAngle(fromProjected, toProjected, up))
        * quatFromAxisAngle(pitchAxis, pitchChange);
}

inline Vec2 getCameraRotationToTarget(Quat orientation, Vec3 lookAtDirection, Vec3 worldUp)
{
    if (almostZero(lookAtDirection))
        return {};

    const Quat inverseOrientation = inverse(orientation);
    const Vec3 up = rotate(inverseOrientation, worldUp);
    const Vec3 direction = rotate(inverseOrientation, lookAtDirection);

    float horizontalAngle = 0.0f;
    const Vec3 targetHorizontal = projectOnPlane(direction, up);
    if (!almostZero(targetHorizontal))
    {
        Vec3 currentHorizontal = projectOnPlane(Vec3::forward, up);
        if (almostZero(currentHorizontal))
            currentHorizontal = projectOnPlane(Vec3::up, up);
        horizontalAngle = signedAngle(currentHorizontal, targetHorizontal, up);
    }

    const Quat horizontalRotation = quatFromAxisAngle(up, horizontalAngle);
    const float verticalAngle = signedAngle(
        rotate(horizontalRotation, Vec3::forward),
        direction,
        rotate(horizontalRotation, Vec3::right));

    return {verticalAngle, horizontalAngle};
}

inline Quat applyCameraRotation(Quat orientation, Vec2 rotation, Vec3 worldUp)
{
    if (rotation.sqrMagnitude() < Epsilon * Epsilon)
        return orientation;
    const Quat vertical = quatFromAxisAngle(Vec3::right, rotation.x);
    return (quatFromAxisAngle(worldUp, rotation.y) * orientation) * vertical;
}

}
