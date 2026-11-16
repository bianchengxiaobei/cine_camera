#pragma once

#include <array>

#include <cinerig/math/vec.hpp>

namespace cinerig
{

struct OrbitRigOrbit
{
    float radius = 0.0f;
    float height = 0.0f;

    bool operator==(const OrbitRigOrbit& other) const noexcept = default;
};

struct OrbitRigSettings
{
    OrbitRigOrbit top{2.0f, 5.0f};
    OrbitRigOrbit center{4.0f, 2.25f};
    OrbitRigOrbit bottom{2.5f, 0.1f};
    float splineCurvature = 0.5f;

    static const OrbitRigSettings Default;

    bool operator==(const OrbitRigSettings& other) const noexcept = default;
};

inline const OrbitRigSettings OrbitRigSettings::Default{};

class OrbitSplineCache
{
public:
    bool settingsChanged(const OrbitRigSettings& settings) const;
    void update(const OrbitRigSettings& settings);
    math::Vec4 splineValue(float t) const;

    const OrbitRigSettings& settings() const noexcept;
    bool isValid() const noexcept;

private:
    static math::Vec4 bezier3(
        float t,
        math::Vec4 p0,
        math::Vec4 p1,
        math::Vec4 p2,
        math::Vec4 p3);
    static float bezier1(float t, float p0, float p1, float p2, float p3);

    OrbitRigSettings settings_{};
    std::array<math::Vec4, 5> knots_{};
    std::array<math::Vec4, 5> control1_{};
    std::array<math::Vec4, 5> control2_{};
    bool valid_ = false;
};

} // namespace cinerig
