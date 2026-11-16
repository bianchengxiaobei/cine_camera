#include <cinerig/core/screen_composer_settings.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

TEST_CASE("Screen composer defaults and effective sizes", "[unit][core][screen]")
{
    const ScreenComposerSettings& settings = ScreenComposerSettings::Default;

    REQUIRE(settings.screenPosition.x == 0.0f);
    REQUIRE(settings.screenPosition.y == 0.0f);

    REQUIRE_FALSE(settings.deadZone.enabled);
    REQUIRE(settings.deadZone.size.x == 0.2f);
    REQUIRE(settings.deadZone.size.y == 0.2f);

    REQUIRE_FALSE(settings.hardLimits.enabled);
    REQUIRE(settings.hardLimits.size.x == 0.8f);
    REQUIRE(settings.hardLimits.size.y == 0.8f);
    REQUIRE(settings.hardLimits.offset.x == 0.0f);
    REQUIRE(settings.hardLimits.offset.y == 0.0f);

    REQUIRE(settings.effectiveDeadZoneSize().x == 0.0f);
    REQUIRE(settings.effectiveDeadZoneSize().y == 0.0f);
    REQUIRE(settings.effectiveHardLimitSize().x == 3.0f);
    REQUIRE(settings.effectiveHardLimitSize().y == 3.0f);

    ScreenComposerSettings enabled = settings;
    enabled.deadZone.enabled = true;
    enabled.hardLimits.enabled = true;
    enabled.deadZone.size = {1.0f, 1.5f};
    enabled.hardLimits.size = {2.0f, 2.5f};
    REQUIRE(enabled.effectiveDeadZoneSize().x == 1.0f);
    REQUIRE(enabled.effectiveDeadZoneSize().y == 1.5f);
    REQUIRE(enabled.effectiveHardLimitSize().x == 2.0f);
    REQUIRE(enabled.effectiveHardLimitSize().y == 2.5f);
}

TEST_CASE("Screen composer validation enforces the hard limit relationship", "[unit][core][screen]")
{
    ScreenComposerSettings settings;
    settings.screenPosition = {2.0f, -2.0f};
    settings.deadZone.size = {3.0f, -1.0f};
    settings.hardLimits.size = {0.1f, 5.0f};
    settings.hardLimits.offset = {2.0f, -2.0f};

    settings.validate();

    REQUIRE(settings.screenPosition.x == 1.5f);
    REQUIRE(settings.screenPosition.y == -1.5f);
    REQUIRE(settings.deadZone.size.x == 2.0f);
    REQUIRE(settings.deadZone.size.y == 0.0f);
    REQUIRE(settings.hardLimits.size.x == 2.0f);
    REQUIRE(settings.hardLimits.size.y == 3.0f);
    REQUIRE(settings.hardLimits.offset.x == 1.0f);
    REQUIRE(settings.hardLimits.offset.y == -1.0f);
}

TEST_CASE("Screen composer builds dead-zone and hard-limit rectangles", "[unit][core][screen]")
{
    ScreenComposerSettings settings;

    const math::Rect emptyDeadZone = settings.deadZoneRect();
    REQUIRE(emptyDeadZone.x == 0.5f);
    REQUIRE(emptyDeadZone.y == 0.5f);
    REQUIRE(emptyDeadZone.width == 0.0f);
    REQUIRE(emptyDeadZone.height == 0.0f);

    const math::Rect defaultHardLimits = settings.hardLimitsRect();
    REQUIRE(defaultHardLimits.x == -1.5f);
    REQUIRE(defaultHardLimits.y == -1.5f);
    REQUIRE(defaultHardLimits.width == 3.0f);
    REQUIRE(defaultHardLimits.height == 3.0f);

    settings.screenPosition = {0.1f, -0.2f};
    settings.deadZone.enabled = true;
    settings.deadZone.size = {1.0f, 1.5f};
    const math::Rect deadZone = settings.deadZoneRect();
    REQUIRE(deadZone.x == Catch::Approx(0.1f));
    REQUIRE(deadZone.y == Catch::Approx(-0.45f));
    REQUIRE(deadZone.width == Catch::Approx(1.0f));
    REQUIRE(deadZone.height == Catch::Approx(1.5f));

    settings.screenPosition = {0.0f, 0.0f};
    settings.hardLimits.enabled = true;
    settings.hardLimits.size = {2.0f, 2.5f};
    settings.hardLimits.offset = {0.5f, -0.5f};
    const math::Rect hardLimits = settings.hardLimitsRect();
    REQUIRE(hardLimits.x == Catch::Approx(-0.25f));
    REQUIRE(hardLimits.y == Catch::Approx(-1.0f));
    REQUIRE(hardLimits.width == Catch::Approx(2.0f));
    REQUIRE(hardLimits.height == Catch::Approx(2.5f));
}

TEST_CASE("Screen composer approximate comparison uses effective sizes", "[unit][core][screen]")
{
    ScreenComposerSettings a = ScreenComposerSettings::Default;
    ScreenComposerSettings b = ScreenComposerSettings::Default;

    REQUIRE(ScreenComposerSettings::approximately(a, b));
    a.deadZone.size = {1.0f, 1.0f};
    REQUIRE(ScreenComposerSettings::approximately(a, b));

    a.deadZone.enabled = true;
    REQUIRE_FALSE(ScreenComposerSettings::approximately(a, b));

    b.deadZone.enabled = true;
    b.deadZone.size = {1.0f, 1.0f};
    REQUIRE(ScreenComposerSettings::approximately(a, b));

    b.hardLimits.offset.x = 0.5f;
    REQUIRE_FALSE(ScreenComposerSettings::approximately(a, b));
}
