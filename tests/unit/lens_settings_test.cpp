#include <cinerig/core/lens_settings.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

TEST_CASE("Lens settings defaults match the Unity lens contract", "[unit][core][lens]")
{
    const LensSettings& lens = LensSettings::Default;

    REQUIRE(lens.fieldOfView == 40.0f);
    REQUIRE(lens.orthographicSize == 10.0f);
    REQUIRE(lens.nearClipPlane == 0.1f);
    REQUIRE(lens.farClipPlane == 5000.0f);
    REQUIRE(lens.dutch == 0.0f);
    REQUIRE(lens.modeOverride == LensSettings::OverrideMode::None);

    REQUIRE(lens.physical.gateFit == LensSettings::PhysicalSettings::GateFit::Horizontal);
    REQUIRE(lens.physical.sensorSize.x == 21.946f);
    REQUIRE(lens.physical.sensorSize.y == 16.002f);
    REQUIRE(lens.physical.lensShift.x == 0.0f);
    REQUIRE(lens.physical.lensShift.y == 0.0f);
    REQUIRE(lens.physical.focusDistance == 10.0f);
    REQUIRE(lens.physical.iso == 200);
    REQUIRE(lens.physical.shutterSpeed == 0.005f);
    REQUIRE(lens.physical.aperture == 16.0f);
    REQUIRE(lens.physical.bladeCount == 5);
    REQUIRE(lens.physical.curvature.x == 2.0f);
    REQUIRE(lens.physical.curvature.y == 11.0f);
    REQUIRE(lens.physical.barrelClipping == 0.25f);
    REQUIRE(lens.physical.anamorphism == 0.0f);

    REQUIRE_FALSE(lens.orthographic);
    REQUIRE_FALSE(lens.physicalCamera);
    REQUIRE(lens.aspect == 1.0f);
}

TEST_CASE("Lens validation clamps physical camera settings", "[unit][core][lens]")
{
    LensSettings lens;
    lens.fieldOfView = 200.0f;
    lens.nearClipPlane = 1.0f;
    lens.farClipPlane = 0.0f;
    lens.physical.sensorSize = {0.0f, 0.0f};
    lens.physical.focusDistance = 0.0f;
    lens.physical.shutterSpeed = -1.0f;
    lens.physical.aperture = 100.0f;
    lens.physical.bladeCount = 100;
    lens.physical.barrelClipping = 2.0f;
    lens.physical.curvature = {0.0f, 100.0f};
    lens.physical.anamorphism = -3.0f;
    lens.aspect = 0.0f;

    lens.validate();

    REQUIRE(lens.fieldOfView == 179.0f);
    REQUIRE(lens.farClipPlane == Catch::Approx(1.001f));
    REQUIRE(lens.physical.sensorSize.x == 0.1f);
    REQUIRE(lens.physical.sensorSize.y == 0.1f);
    REQUIRE(lens.physical.focusDistance == 0.01f);
    REQUIRE(lens.physical.shutterSpeed == 0.0f);
    REQUIRE(lens.physical.aperture == LensSettings::kMaxAperture);
    REQUIRE(lens.physical.bladeCount == LensSettings::kMaxBladeCount);
    REQUIRE(lens.physical.barrelClipping == 1.0f);
    REQUIRE(lens.physical.curvature.x == LensSettings::kMinAperture);
    REQUIRE(lens.physical.curvature.y == LensSettings::kMaxAperture);
    REQUIRE(lens.physical.anamorphism == -1.0f);
    REQUIRE(lens.aspect == 1.0f);

    lens.fieldOfView = -10.0f;
    lens.physical.aperture = 0.0f;
    lens.physical.bladeCount = 0;
    lens.physical.barrelClipping = -1.0f;
    lens.physical.anamorphism = 3.0f;
    lens.physical.curvature = {10.0f, 5.0f};
    lens.validate();

    REQUIRE(lens.fieldOfView == 0.01f);
    REQUIRE(lens.physical.aperture == LensSettings::kMinAperture);
    REQUIRE(lens.physical.bladeCount == LensSettings::kMinBladeCount);
    REQUIRE(lens.physical.barrelClipping == 0.0f);
    REQUIRE(lens.physical.anamorphism == 1.0f);
    REQUIRE(lens.physical.curvature.x == 10.0f);
    REQUIRE(lens.physical.curvature.y == 10.0f);
}

TEST_CASE("Lens settings approximate comparison", "[unit][core][lens]")
{
    LensSettings a = LensSettings::Default;
    LensSettings b = LensSettings::Default;

    REQUIRE(LensSettings::areEqual(a, b));

    b.fieldOfView += 1.0f;
    REQUIRE_FALSE(LensSettings::areEqual(a, b));

    b = LensSettings::Default;
    b.physical.iso += 1;
    REQUIRE_FALSE(LensSettings::areEqual(a, b));

    b = LensSettings::Default;
    b.modeOverride = LensSettings::OverrideMode::Physical;
    REQUIRE(LensSettings::areEqual(a, b));
}
