#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace cinerig::math
{

inline constexpr float Epsilon = 0.0001f;
inline constexpr float Pi = 3.14159265358979323846f;
inline constexpr float TwoPi = 6.28318530717958647692f;
inline constexpr float HalfPi = 1.57079632679489661923f;
inline constexpr float DegToRad = Pi / 180.0f;
inline constexpr float RadToDeg = 180.0f / Pi;

constexpr float clamp01(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

constexpr float clamp(float value, float minValue, float maxValue)
{
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

constexpr float lerp(float from, float to, float t)
{
    return from + (to - from) * clamp01(t);
}

inline bool approximately(float a, float b, float epsilon = Epsilon)
{
    return std::abs(a - b) <= epsilon;
}

inline bool almostZero(float value)
{
    return std::abs(value) < Epsilon;
}

inline float normalizeAngle(float radians)
{
    return std::remainder(radians, TwoPi);
}

inline float normalizeAngleDegrees(float degrees)
{
    const float wrapped = std::fmod(degrees, 360.0f);
    return wrapped > 180.0f ? wrapped - 360.0f : wrapped;
}

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float xValue, float yValue) : x(xValue), y(yValue) {}
    explicit constexpr Vec2(float value) : x(value), y(value) {}

    float& operator[](int index) { return index == 0 ? x : y; }
    const float& operator[](int index) const { return index == 0 ? x : y; }

    constexpr float sqrMagnitude() const { return x * x + y * y; }
    float length() const { return std::sqrt(sqrMagnitude()); }
    Vec2 normalized() const
    {
        const float len = length();
        return len < Epsilon ? Vec2{} : Vec2{x / len, y / len};
    }

    bool isNaN() const { return std::isnan(x) || std::isnan(y); }

    static const Vec2 zero;
    static const Vec2 one;
    static const Vec2 right;
    static const Vec2 up;
};

inline constexpr Vec2 Vec2::zero{0.0f, 0.0f};
inline constexpr Vec2 Vec2::one{1.0f, 1.0f};
inline constexpr Vec2 Vec2::right{1.0f, 0.0f};
inline constexpr Vec2 Vec2::up{0.0f, 1.0f};

constexpr Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
constexpr Vec2 operator-(Vec2 value) { return {-value.x, -value.y}; }
constexpr Vec2 operator*(Vec2 value, float scalar) { return {value.x * scalar, value.y * scalar}; }
constexpr Vec2 operator*(float scalar, Vec2 value) { return value * scalar; }
constexpr Vec2 operator/(Vec2 value, float scalar) { return {value.x / scalar, value.y / scalar}; }
constexpr Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }
constexpr Vec2& operator-=(Vec2& a, Vec2 b) { a.x -= b.x; a.y -= b.y; return a; }
constexpr Vec2& operator*=(Vec2& value, float scalar) { value.x *= scalar; value.y *= scalar; return value; }

constexpr float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
constexpr float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline float length(Vec2 value) { return value.length(); }
inline float distance(Vec2 a, Vec2 b) { return (a - b).length(); }
inline Vec2 normalize(Vec2 value) { return value.normalized(); }
constexpr Vec2 lerp(Vec2 a, Vec2 b, float t) { const float u = clamp01(t); return a + (b - a) * u; }
inline Vec2 abs(Vec2 value) { return {std::abs(value.x), std::abs(value.y)}; }
inline bool approximately(Vec2 a, Vec2 b, float epsilon = Epsilon)
{
    return approximately(a.x, b.x, epsilon) && approximately(a.y, b.y, epsilon);
}
inline bool almostZero(Vec2 value) { return value.sqrMagnitude() < Epsilon * Epsilon; }

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float xValue, float yValue, float zValue) : x(xValue), y(yValue), z(zValue) {}
    explicit constexpr Vec3(float value) : x(value), y(value), z(value) {}

    float& operator[](int index)
    {
        return index == 0 ? x : (index == 1 ? y : z);
    }
    const float& operator[](int index) const
    {
        return index == 0 ? x : (index == 1 ? y : z);
    }

    constexpr float sqrMagnitude() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(sqrMagnitude()); }
    Vec3 normalized() const
    {
        const float len = length();
        return len < Epsilon ? Vec3{} : Vec3{x / len, y / len, z / len};
    }

    bool isNaN() const { return std::isnan(x) || std::isnan(y) || std::isnan(z); }

    static const Vec3 zero;
    static const Vec3 one;
    static const Vec3 right;
    static const Vec3 left;
    static const Vec3 up;
    static const Vec3 down;
    static const Vec3 forward;
    static const Vec3 back;
};

inline constexpr Vec3 Vec3::zero{0.0f, 0.0f, 0.0f};
inline constexpr Vec3 Vec3::one{1.0f, 1.0f, 1.0f};
inline constexpr Vec3 Vec3::right{1.0f, 0.0f, 0.0f};
inline constexpr Vec3 Vec3::left{-1.0f, 0.0f, 0.0f};
inline constexpr Vec3 Vec3::up{0.0f, 1.0f, 0.0f};
inline constexpr Vec3 Vec3::down{0.0f, -1.0f, 0.0f};
inline constexpr Vec3 Vec3::forward{0.0f, 0.0f, -1.0f};
inline constexpr Vec3 Vec3::back{0.0f, 0.0f, 1.0f};

constexpr Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator-(Vec3 value) { return {-value.x, -value.y, -value.z}; }
constexpr Vec3 operator*(Vec3 value, float scalar) { return {value.x * scalar, value.y * scalar, value.z * scalar}; }
constexpr Vec3 operator*(float scalar, Vec3 value) { return value * scalar; }
constexpr Vec3 operator/(Vec3 value, float scalar) { return {value.x / scalar, value.y / scalar, value.z / scalar}; }
constexpr Vec3& operator+=(Vec3& a, Vec3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
constexpr Vec3& operator-=(Vec3& a, Vec3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
constexpr Vec3& operator*=(Vec3& value, float scalar) { value.x *= scalar; value.y *= scalar; value.z *= scalar; return value; }
constexpr Vec3& operator/=(Vec3& value, float scalar) { value.x /= scalar; value.y /= scalar; value.z /= scalar; return value; }

constexpr float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 cross(Vec3 a, Vec3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}
inline float length(Vec3 value) { return value.length(); }
inline float distance(Vec3 a, Vec3 b) { return (a - b).length(); }
inline Vec3 normalize(Vec3 value) { return value.normalized(); }
constexpr Vec3 lerp(Vec3 a, Vec3 b, float t) { const float u = clamp01(t); return a + (b - a) * u; }
inline Vec3 abs(Vec3 value) { return {std::abs(value.x), std::abs(value.y), std::abs(value.z)}; }
inline Vec3 min(Vec3 a, Vec3 b)
{
    return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z};
}
inline Vec3 max(Vec3 a, Vec3 b)
{
    return {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z};
}
inline Vec3 clamp(Vec3 value, Vec3 minValue, Vec3 maxValue)
{
    return {
        clamp(value.x, minValue.x, maxValue.x),
        clamp(value.y, minValue.y, maxValue.y),
        clamp(value.z, minValue.z, maxValue.z),
    };
}
inline bool approximately(Vec3 a, Vec3 b, float epsilon = Epsilon)
{
    return approximately(a.x, b.x, epsilon)
        && approximately(a.y, b.y, epsilon)
        && approximately(a.z, b.z, epsilon);
}
inline bool almostZero(Vec3 value) { return value.sqrMagnitude() < Epsilon * Epsilon; }

inline Vec3 orthogonal(Vec3 value)
{
    const Vec3 direction = value.normalized();
    if (almostZero(direction))
        return Vec3::right;
    const Vec3 axis = std::abs(direction.x) < 0.9f ? Vec3::right : Vec3::up;
    return normalize(cross(direction, axis));
}

inline float angle(Vec3 from, Vec3 to)
{
    const float fromLength = from.length();
    const float toLength = to.length();
    if (fromLength < Epsilon || toLength < Epsilon)
        return 0.0f;
    const Vec3 a = from / fromLength;
    const Vec3 b = to / toLength;
    return std::atan2(length(a - b), length(a + b)) * 2.0f;
}

inline float angleDegrees(Vec3 from, Vec3 to)
{
    return angle(from, to) * RadToDeg;
}

inline Vec3 slerp(Vec3 from, Vec3 to, float t)
{
    const float u = clamp01(t);
    const float fromLength = from.length();
    const float toLength = to.length();
    if (fromLength < Epsilon || toLength < Epsilon)
        return lerp(from, to, u);

    const Vec3 a = from / fromLength;
    const Vec3 b = to / toLength;
    const float cosine = clamp(dot(a, b), -1.0f, 1.0f);
    Vec3 direction;

    if (cosine > 1.0f - 1e-5f)
    {
        direction = normalize(lerp(a, b, u));
    }
    else if (cosine < -1.0f + 1e-5f)
    {
        const Vec3 axis = orthogonal(a);
        direction = a * std::cos(Pi * u) + cross(axis, a) * std::sin(Pi * u);
    }
    else
    {
        const float theta = std::acos(cosine);
        const float sine = std::sin(theta);
        const float weightFrom = std::sin((1.0f - u) * theta) / sine;
        const float weightTo = std::sin(u * theta) / sine;
        direction = a * weightFrom + b * weightTo;
    }

    return direction * lerp(fromLength, toLength, u);
}

struct Vec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    constexpr Vec4() = default;
    constexpr Vec4(float xValue, float yValue, float zValue, float wValue)
        : x(xValue), y(yValue), z(zValue), w(wValue) {}
    explicit constexpr Vec4(float value) : x(value), y(value), z(value), w(value) {}

    float& operator[](int index)
    {
        return index == 0 ? x : (index == 1 ? y : (index == 2 ? z : w));
    }
    const float& operator[](int index) const
    {
        return index == 0 ? x : (index == 1 ? y : (index == 2 ? z : w));
    }

    constexpr float sqrMagnitude() const { return x * x + y * y + z * z + w * w; }
    float length() const { return std::sqrt(sqrMagnitude()); }
    Vec4 normalized() const
    {
        const float len = length();
        return len < Epsilon ? Vec4{} : Vec4{x / len, y / len, z / len, w / len};
    }

    static const Vec4 zero;
    static const Vec4 one;
};

inline constexpr Vec4 Vec4::zero{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Vec4 Vec4::one{1.0f, 1.0f, 1.0f, 1.0f};

constexpr Vec4 operator+(Vec4 a, Vec4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
constexpr Vec4 operator-(Vec4 a, Vec4 b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
constexpr Vec4 operator-(Vec4 value) { return {-value.x, -value.y, -value.z, -value.w}; }
constexpr Vec4 operator*(Vec4 value, float scalar) { return {value.x * scalar, value.y * scalar, value.z * scalar, value.w * scalar}; }
constexpr Vec4 operator*(float scalar, Vec4 value) { return value * scalar; }
constexpr Vec4 operator/(Vec4 value, float scalar) { return {value.x / scalar, value.y / scalar, value.z / scalar, value.w / scalar}; }
constexpr Vec4& operator+=(Vec4& a, Vec4 b) { a.x += b.x; a.y += b.y; a.z += b.z; a.w += b.w; return a; }
constexpr Vec4& operator-=(Vec4& a, Vec4 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; a.w -= b.w; return a; }

constexpr float dot(Vec4 a, Vec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline float length(Vec4 value) { return value.length(); }
inline Vec4 normalize(Vec4 value) { return value.normalized(); }
constexpr Vec4 lerp(Vec4 a, Vec4 b, float t) { const float u = clamp01(t); return a + (b - a) * u; }
inline bool approximately(Vec4 a, Vec4 b, float epsilon = Epsilon)
{
    return approximately(a.x, b.x, epsilon)
        && approximately(a.y, b.y, epsilon)
        && approximately(a.z, b.z, epsilon)
        && approximately(a.w, b.w, epsilon);
}

}
