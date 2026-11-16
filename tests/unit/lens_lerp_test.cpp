#include <cinerig/core/camera_state_blend.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

namespace
{

LensSettings makeLensA()
{
    LensSettings lens = LensSettings::Default;
    lens.fieldOfView = 40.0f;
    lens.orthographicSize = 10.0f;
    lens.nearClipPlane = 0.1f;
    lens.farClipPlane = 5000.0f;
    lens.dutch = 0.0f;
    lens.physical.iso = 200;
    lens.physical.bladeCount = 5;
    lens.physical.sensorSize = {20.0f, 10.0f};
    lens.physical.lensShift = {0.0f, 0.0f};
    lens.physical.focusDistance = 10.0f;
    lens.physical.shutterSpeed = 0.005f;
    lens.physical.aperture = 16.0f;
    lens.physical.curvature = {2.0f, 11.0f};
    lens.physical.barrelClipping = 0.25f;
    lens.physical.anamorphism = 0.0f;
    lens.modeOverride = LensSettings::OverrideMode::Orthographic;
    lens.orthographic = true;
    lens.physicalCamera = false;
    lens.aspect = 2.0f;
    return lens;
}

LensSettings makeLensB()
{
    LensSettings lens = makeLensA();
    lens.fieldOfView = 80.0f;
    lens.orthographicSize = 20.0f;
    lens.nearClipPlane = 1.0f;
    lens.farClipPlane = 10000.0f;
    lens.dutch = 10.0f;
    lens.physical.iso = 400;
    lens.physical.bladeCount = 9;
    lens.physical.sensorSize = {40.0f, 20.0f};
    lens.physical.lensShift = {0.5f, -0.5f};
    lens.physical.focusDistance = 20.0f;
    lens.physical.shutterSpeed = 0.02f;
    lens.physical.aperture = 8.0f;
    lens.physical.curvature = {4.0f, 16.0f};
    lens.physical.barrelClipping = 0.5f;
    lens.physical.anamorphism = 0.25f;
    lens.modeOverride = LensSettings::OverrideMode::Physical;
    lens.orthographic = false;
    lens.physicalCamera = true;
    lens.aspect = 4.0f;
    return lens;
}

}

TEST_CASE("Lens lerp selects the nearest base and preserves non-lerpable fields", "[unit][core][blend][lens]")
{
    const LensSettings a = makeLensA();
    const LensSettings b = makeLensB();

    const LensSettings atZero = LensSettings::lerp(a, b, 0.0f);
    REQUIRE(atZero.fieldOfView == 40.0f);
    REQUIRE(atZero.modeOverride == LensSettings::OverrideMode::Orthographic);
    REQUIRE(atZero.orthographic);
    REQUIRE_FALSE(atZero.physicalCamera);
    REQUIRE(atZero.aspect == 2.0f);
    REQUIRE(atZero.physical.gateFit == a.physical.gateFit);

    const LensSettings atOne = LensSettings::lerp(a, b, 1.0f);
    REQUIRE(atOne.fieldOfView == 80.0f);
    REQUIRE(atOne.modeOverride == LensSettings::OverrideMode::Physical);
    REQUIRE_FALSE(atOne.orthographic);
    REQUIRE(atOne.physicalCamera);
    REQUIRE(atOne.aspect == 4.0f);

    const LensSettings atQuarter = LensSettings::lerp(a, b, 0.25f);
    REQUIRE(atQuarter.fieldOfView == Catch::Approx(50.0f));
    REQUIRE(atQuarter.modeOverride == LensSettings::OverrideMode::Orthographic);
    REQUIRE(atQuarter.orthographic);
    REQUIRE(atQuarter.aspect == 2.0f);

    const LensSettings atHalf = LensSettings::lerp(a, b, 0.5f);
    REQUIRE(atHalf.fieldOfView == Catch::Approx(60.0f));
    REQUIRE(atHalf.modeOverride == LensSettings::OverrideMode::Physical);
    REQUIRE(atHalf.physicalCamera);
    REQUIRE(atHalf.aspect == 4.0f);
}

TEST_CASE("Lens lerp interpolates and rounds physical fields", "[unit][core][blend][lens]")
{
    const LensSettings a = makeLensA();
    const LensSettings b = makeLensB();
    const LensSettings result = LensSettings::lerp(a, b, 0.5f);

    REQUIRE(result.nearClipPlane == Catch::Approx(0.55f));
    REQUIRE(result.farClipPlane == Catch::Approx(7500.0f));
    REQUIRE(result.orthographicSize == Catch::Approx(15.0f));
    REQUIRE(result.dutch == Catch::Approx(5.0f));
    REQUIRE(result.physical.sensorSize.x == Catch::Approx(30.0f));
    REQUIRE(result.physical.sensorSize.y == Catch::Approx(15.0f));
    REQUIRE(result.physical.lensShift.x == Catch::Approx(0.25f));
    REQUIRE(result.physical.lensShift.y == Catch::Approx(-0.25f));
    REQUIRE(result.physical.focusDistance == Catch::Approx(15.0f));
    REQUIRE(result.physical.iso == 300);
    REQUIRE(result.physical.shutterSpeed == Catch::Approx(0.0125f));
    REQUIRE(result.physical.aperture == Catch::Approx(12.0f));
    REQUIRE(result.physical.bladeCount == 7);
    REQUIRE(result.physical.curvature.x == Catch::Approx(3.0f));
    REQUIRE(result.physical.curvature.y == Catch::Approx(13.5f));
    REQUIRE(result.physical.barrelClipping == Catch::Approx(0.375f));
    REQUIRE(result.physical.anamorphism == Catch::Approx(0.125f));
}

TEST_CASE("Lens lerp clamps the interpolation factor", "[unit][core][blend][lens]")
{
    const LensSettings a = makeLensA();
    const LensSettings b = makeLensB();

    REQUIRE(LensSettings::lerp(a, b, -1.0f).fieldOfView == a.fieldOfView);
    REQUIRE(LensSettings::lerp(a, b, 2.0f).fieldOfView == b.fieldOfView);
}
