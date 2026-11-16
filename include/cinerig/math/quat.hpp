#pragma once

#include <cinerig/math/vec.hpp>

#include <algorithm>
#include <cmath>

namespace cinerig::math
{

struct Quat
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    constexpr Quat() = default;
    constexpr Quat(float xValue, float yValue, float zValue, float wValue)
        : x(xValue), y(yValue), z(zValue), w(wValue) {}

    static constexpr Quat identity() { return Quat{}; }

    Vec3 xyz() const { return {x, y, z}; }
    constexpr float sqrMagnitude() const { return x * x + y * y + z * z + w * w; }
    float length() const { return std::sqrt(sqrMagnitude()); }
    Quat normalized() const
    {
        const float len = length();
        return len < Epsilon ? Quat{} : Quat{x / len, y / len, z / len, w / len};
    }

    bool isNaN() const
    {
        return std::isnan(x) || std::isnan(y) || std::isnan(z) || std::isnan(w);
    }

};

constexpr Quat operator*(Quat a, Quat b)
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

constexpr Quat operator*(Quat q, float scalar) { return {q.x * scalar, q.y * scalar, q.z * scalar, q.w * scalar}; }
constexpr Quat operator*(float scalar, Quat q) { return q * scalar; }
constexpr Quat operator/(Quat q, float scalar) { return {q.x / scalar, q.y / scalar, q.z / scalar, q.w / scalar}; }
constexpr Quat operator+(Quat a, Quat b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
constexpr Quat operator-(Quat a, Quat b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
constexpr Quat operator-(Quat q) { return {-q.x, -q.y, -q.z, -q.w}; }

constexpr float dot(Quat a, Quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline float length(Quat q) { return q.length(); }
constexpr Quat conjugate(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }
inline Quat inverse(Quat q)
{
    const float magnitude = q.sqrMagnitude();
    if (magnitude < Epsilon)
        return Quat{};
    return conjugate(q) / magnitude;
}
inline Quat normalize(Quat q) { return q.normalized(); }

inline Vec3 rotate(Quat q, Vec3 value)
{
    const Vec3 axis = q.xyz();
    const Vec3 firstCross = cross(axis, value);
    const Vec3 secondCross = cross(axis, firstCross);
    return value + (firstCross * q.w + secondCross) * 2.0f;
}

inline Vec3 operator*(Quat q, Vec3 value) { return rotate(q, value); }

inline Quat quatFromAxisAngle(Vec3 axis, float radians)
{
    const float axisLength = axis.length();
    if (axisLength < Epsilon)
        return Quat{};
    const Vec3 normalizedAxis = axis / axisLength;
    const float halfAngle = radians * 0.5f;
    const float sine = std::sin(halfAngle);
    return {
        normalizedAxis.x * sine,
        normalizedAxis.y * sine,
        normalizedAxis.z * sine,
        std::cos(halfAngle),
    };
}

inline Quat fromToRotation(Vec3 from, Vec3 to)
{
    const float fromLength = from.length();
    const float toLength = to.length();
    if (fromLength < Epsilon || toLength < Epsilon)
        return Quat{};

    const Vec3 a = from / fromLength;
    const Vec3 b = to / toLength;
    const float cosine = clamp(dot(a, b), -1.0f, 1.0f);
    if (cosine > 1.0f - 1e-6f)
        return Quat{};
    if (cosine < -1.0f + 1e-6f)
        return quatFromAxisAngle(orthogonal(a), Pi);
    return quatFromAxisAngle(normalize(cross(a, b)), std::acos(cosine));
}

inline Quat quatFromBasis(Vec3 xAxis, Vec3 yAxis, Vec3 zAxis)
{
    const float m00 = xAxis.x;
    const float m01 = yAxis.x;
    const float m02 = zAxis.x;
    const float m10 = xAxis.y;
    const float m11 = yAxis.y;
    const float m12 = zAxis.y;
    const float m20 = xAxis.z;
    const float m21 = yAxis.z;
    const float m22 = zAxis.z;

    const float trace = m00 + m11 + m22;
    Quat result;
    if (trace > 0.0f)
    {
        const float scale = std::sqrt(trace + 1.0f) * 2.0f;
        result.w = 0.25f * scale;
        result.x = (m21 - m12) / scale;
        result.y = (m02 - m20) / scale;
        result.z = (m10 - m01) / scale;
    }
    else if (m00 > m11 && m00 > m22)
    {
        const float scale = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        result.w = (m21 - m12) / scale;
        result.x = 0.25f * scale;
        result.y = (m01 + m10) / scale;
        result.z = (m02 + m20) / scale;
    }
    else if (m11 > m22)
    {
        const float scale = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        result.w = (m02 - m20) / scale;
        result.x = (m01 + m10) / scale;
        result.y = 0.25f * scale;
        result.z = (m12 + m21) / scale;
    }
    else
    {
        const float scale = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        result.w = (m10 - m01) / scale;
        result.x = (m02 + m20) / scale;
        result.y = (m12 + m21) / scale;
        result.z = 0.25f * scale;
    }
    return normalize(result);
}

inline Quat lookRotation(Vec3 forward, Vec3 up)
{
    const float forwardLength = forward.length();
    if (forwardLength < Epsilon)
        return Quat{};

    const Vec3 forwardDirection = forward / forwardLength;
    const Vec3 zAxis = -forwardDirection;
    Vec3 upDirection = up.normalized();
    if (almostZero(upDirection))
        upDirection = Vec3::up;

    Vec3 xAxis = cross(upDirection, zAxis);
    if (almostZero(xAxis))
    {
        const Vec3 fallbackUp = std::abs(forwardDirection.y) < 0.999f ? Vec3::up : Vec3::right;
        xAxis = cross(fallbackUp, zAxis);
    }
    xAxis = normalize(xAxis);
    const Vec3 yAxis = cross(zAxis, xAxis);
    return quatFromBasis(xAxis, yAxis, zAxis);
}

inline Quat quatFromEuler(Vec3 radians)
{
    const Quat rotationX = quatFromAxisAngle(Vec3::right, radians.x);
    const Quat rotationY = quatFromAxisAngle(Vec3::up, radians.y);
    const Quat rotationZ = quatFromAxisAngle(Vec3::back, radians.z);
    return rotationY * rotationX * rotationZ;
}

inline Quat quatFromEulerXYZ(Vec3 radians) { return quatFromEuler(radians); }

inline Quat quatFromYawPitchRoll(float yaw, float pitch, float roll)
{
    const Quat rotationX = quatFromAxisAngle(Vec3::right, pitch);
    const Quat rotationY = quatFromAxisAngle(Vec3::up, yaw);
    const Quat rotationZ = quatFromAxisAngle(Vec3::back, roll);
    return rotationZ * rotationX * rotationY;
}

inline Vec3 eulerAngles(Quat q)
{
    const Quat normalized = normalize(q);
    const float xx = normalized.x * normalized.x;
    const float yy = normalized.y * normalized.y;
    const float zz = normalized.z * normalized.z;
    const float xy = normalized.x * normalized.y;
    const float xz = normalized.x * normalized.z;
    const float yz = normalized.y * normalized.z;
    const float wx = normalized.w * normalized.x;
    const float wy = normalized.w * normalized.y;
    const float wz = normalized.w * normalized.z;

    const float m00 = 1.0f - 2.0f * (yy + zz);
    const float m01 = 2.0f * (xy - wz);
    const float m02 = 2.0f * (xz + wy);
    const float m10 = 2.0f * (xy + wz);
    const float m11 = 1.0f - 2.0f * (xx + zz);
    const float m12 = 2.0f * (yz - wx);
    const float m22 = 1.0f - 2.0f * (xx + yy);

    const float x = std::asin(clamp(-m12, -1.0f, 1.0f));
    float y = 0.0f;
    float z = 0.0f;
    if (std::abs(m12) < 1.0f - 1e-5f)
    {
        y = std::atan2(m02, m22);
        z = std::atan2(m10, m11);
    }
    else if (m12 < 0.0f)
    {
        y = std::atan2(m01, m00);
    }
    else
    {
        y = std::atan2(-m01, m00);
    }

    return {x, normalizeAngle(y), normalizeAngle(z)};
}

inline Vec3 eulerAnglesDegrees(Quat q)
{
    const Vec3 radians = eulerAngles(q);
    return {
        std::fmod(radians.x * RadToDeg + 360.0f, 360.0f),
        std::fmod(radians.y * RadToDeg + 360.0f, 360.0f),
        std::fmod(radians.z * RadToDeg + 360.0f, 360.0f),
    };
}

inline Quat slerpUnclamped(Quat from, Quat to, float t)
{
    Quat a = normalize(from);
    Quat b = normalize(to);
    float cosine = dot(a, b);
    if (cosine < 0.0f)
    {
        b = -b;
        cosine = -cosine;
    }

    if (cosine > 0.9995f)
        return normalize(a + (b - a) * t);

    const float theta = std::acos(clamp(cosine, -1.0f, 1.0f));
    const float sine = std::sin(theta);
    if (std::abs(sine) < Epsilon)
        return normalize(a + (b - a) * t);
    const float weightFrom = std::sin((1.0f - t) * theta) / sine;
    const float weightTo = std::sin(t * theta) / sine;
    return normalize(a * weightFrom + b * weightTo);
}

inline Quat slerp(Quat from, Quat to, float t)
{
    return slerpUnclamped(from, to, clamp01(t));
}

inline float angle(Quat a, Quat b)
{
    const Quat first = normalize(a);
    Quat second = normalize(b);
    if (dot(first, second) < 0.0f)
        second = -second;
    return 4.0f * std::atan2(length(first - second), length(first + second));
}

inline bool approximately(Quat a, Quat b, float epsilon = Epsilon)
{
    return approximately(a.x, b.x, epsilon)
        && approximately(a.y, b.y, epsilon)
        && approximately(a.z, b.z, epsilon)
        && approximately(a.w, b.w, epsilon);
}

}
