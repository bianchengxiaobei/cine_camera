#pragma once

#include <algorithm>

#include <cinerig/math/vec.hpp>

namespace cinerig
{

struct LensSettings
{
    static constexpr float kMinAperture = 0.7f;
    static constexpr float kMaxAperture = 32.0f;
    static constexpr int kMinBladeCount = 3;
    static constexpr int kMaxBladeCount = 9;

    enum class OverrideMode
    {
        None = 0,
        Orthographic,
        Perspective,
        Physical,
    };

    struct PhysicalSettings
    {
        enum class GateFit
        {
            None = 0,
            Horizontal,
            Vertical,
            Fill,
            Overscan,
        };

        GateFit gateFit = GateFit::Horizontal;
        math::Vec2 sensorSize{21.946f, 16.002f};
        math::Vec2 lensShift{0.0f, 0.0f};
        float focusDistance = 10.0f;
        int iso = 200;
        float shutterSpeed = 0.005f;
        float aperture = 16.0f;
        int bladeCount = 5;
        math::Vec2 curvature{2.0f, 11.0f};
        float barrelClipping = 0.25f;
        float anamorphism = 0.0f;
    };

    float fieldOfView = 40.0f;
    float orthographicSize = 10.0f;
    float nearClipPlane = 0.1f;
    float farClipPlane = 5000.0f;
    float dutch = 0.0f;
    OverrideMode modeOverride = OverrideMode::None;
    PhysicalSettings physical;
    bool orthographic = false;
    bool physicalCamera = false;
    float aspect = 1.0f;

    static const LensSettings Default;

    static LensSettings lerp(const LensSettings& a, const LensSettings& b, float t);
    void validate();
    static bool areEqual(const LensSettings& a, const LensSettings& b);
};

inline const LensSettings LensSettings::Default{};

inline void LensSettings::validate()
{
    farClipPlane = std::max(farClipPlane, nearClipPlane + 0.001f);
    fieldOfView = math::clamp(fieldOfView, 0.01f, 179.0f);
    physical.sensorSize.x = std::max(physical.sensorSize.x, 0.1f);
    physical.sensorSize.y = std::max(physical.sensorSize.y, 0.1f);
    physical.focusDistance = std::max(physical.focusDistance, 0.01f);
    if (aspect == 0.0f)
        aspect = 1.0f;
    physical.shutterSpeed = std::max(0.0f, physical.shutterSpeed);
    physical.aperture = math::clamp(physical.aperture, kMinAperture, kMaxAperture);
    physical.bladeCount = std::clamp(physical.bladeCount, kMinBladeCount, kMaxBladeCount);
    physical.barrelClipping = math::clamp01(physical.barrelClipping);
    physical.curvature.x = math::clamp(physical.curvature.x, kMinAperture, kMaxAperture);
    physical.curvature.y = math::clamp(physical.curvature.y, physical.curvature.x, kMaxAperture);
    physical.anamorphism = math::clamp(physical.anamorphism, -1.0f, 1.0f);
}

inline bool LensSettings::areEqual(const LensSettings& a, const LensSettings& b)
{
    return math::approximately(a.nearClipPlane, b.nearClipPlane)
        && math::approximately(a.farClipPlane, b.farClipPlane)
        && math::approximately(a.orthographicSize, b.orthographicSize)
        && math::approximately(a.fieldOfView, b.fieldOfView)
        && math::approximately(a.dutch, b.dutch)
        && math::approximately(a.physical.lensShift.x, b.physical.lensShift.x)
        && math::approximately(a.physical.lensShift.y, b.physical.lensShift.y)
        && math::approximately(a.physical.sensorSize.x, b.physical.sensorSize.x)
        && math::approximately(a.physical.sensorSize.y, b.physical.sensorSize.y)
        && a.physical.gateFit == b.physical.gateFit
        && math::approximately(a.physical.focusDistance, b.physical.focusDistance)
        && a.physical.iso == b.physical.iso
        && math::approximately(a.physical.shutterSpeed, b.physical.shutterSpeed)
        && math::approximately(a.physical.aperture, b.physical.aperture)
        && a.physical.bladeCount == b.physical.bladeCount
        && math::approximately(a.physical.curvature.x, b.physical.curvature.x)
        && math::approximately(a.physical.curvature.y, b.physical.curvature.y)
        && math::approximately(a.physical.barrelClipping, b.physical.barrelClipping)
        && math::approximately(a.physical.anamorphism, b.physical.anamorphism);
}

}
