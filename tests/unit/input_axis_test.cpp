#include <algorithm>
#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/core/input_axis.hpp>

using namespace cinerig;

namespace
{

float wrappedDistance(float a, float b, float size)
{
    const float distance = std::abs(a - b);
    return std::min(distance, std::abs(size - distance));
}

} // namespace

TEST_CASE("InputAxis clamps, wraps, validates, and normalizes values", "[unit][input-axis]")
{
    InputAxis axis;
    axis.range = {-100.0f, 100.0f};
    axis.wrap = false;
    axis.value = 150.0f;
    axis.center = 250.0f;
    REQUIRE(axis.getClampedValue() == 100.0f);
    REQUIRE(axis.getNormalizedValue() == 1.0f);

    axis.wrap = true;
    axis.value = 150.0f;
    REQUIRE(axis.clampValue(150.0f) == Catch::Approx(-50.0f));
    REQUIRE(axis.clampValue(-250.0f) == Catch::Approx(-50.0f));

    axis.value = 0.0f;
    axis.center = 0.0f;
    axis.range = {100.0f, 50.0f};
    axis.validate();
    REQUIRE(axis.range.x == 100.0f);
    REQUIRE(axis.range.y == 100.0f);
    REQUIRE(axis.value == 100.0f);
    REQUIRE(axis.center == 100.0f);

    axis.range = {0.0f, 10.0f};
    axis.wrap = false;
    axis.value = 5.0f;
    REQUIRE(axis.getNormalizedValue() == Catch::Approx(0.5f));

    axis.range = {3.0f, 3.0f};
    axis.value = 3.0f;
    REQUIRE(axis.getNormalizedValue() == Catch::Approx(0.0f));
}

TEST_CASE("InputAxis momentary defaults and reset follow restrictions", "[unit][input-axis]")
{
    InputAxis momentary = InputAxis::defaultMomentary();
    REQUIRE(momentary.range.x == -1.0f);
    REQUIRE(momentary.range.y == 1.0f);
    REQUIRE(hasInputAxisRestriction(momentary.restrictions, InputAxisRestriction::Momentary));
    REQUIRE(hasInputAxisRestriction(momentary.restrictions, InputAxisRestriction::NoRecentering));

    InputAxis axis;
    axis.range = {-10.0f, 10.0f};
    axis.center = 2.0f;
    axis.value = 8.0f;
    axis.recentering.enabled = true;
    axis.reset();
    REQUIRE(axis.value == 2.0f);

    axis.restrictions = InputAxisRestriction::NoRecentering;
    axis.value = 8.0f;
    axis.reset();
    REQUIRE(axis.value == 8.0f);
}

TEST_CASE("InputAxis tracks changes and recenters after the wait time", "[unit][input-axis]")
{
    InputAxis axis;
    axis.range = {-100.0f, 100.0f};
    axis.value = 50.0f;
    axis.center = 0.0f;
    axis.recentering.enabled = true;
    axis.recentering.wait = 0.2f;
    axis.recentering.time = 0.5f;
    axis.validate();

    REQUIRE(axis.trackValueChange(0.0f));
    axis.updateRecentering(0.1f, false, 0.0f);
    REQUIRE(axis.value == 50.0f);

    axis.updateRecentering(0.1f, false, 0.2f);
    REQUIRE(axis.value < 50.0f);
    REQUIRE(axis.value > 0.0f);

    axis.value = 60.0f;
    REQUIRE(axis.trackValueChange(0.3f));
    axis.updateRecentering(0.1f, false, 0.3f);
    REQUIRE(axis.value == 60.0f);

    axis.triggerRecentering();
    axis.updateRecentering(0.1f, false, 0.3f);
    REQUIRE(axis.value < 60.0f);

    axis.cancelRecentering();
    const float canceled = axis.value;
    axis.updateRecentering(0.1f, false, 0.4f);
    REQUIRE(axis.value == canceled);

    axis.updateRecentering(0.1f, true, 0.5f);
    REQUIRE(axis.value == canceled);
}

TEST_CASE("InputAxis recentering uses the shortest wrap path and honors restrictions", "[unit][input-axis]")
{
    InputAxis axis;
    axis.range = {-180.0f, 180.0f};
    axis.wrap = true;
    axis.value = 170.0f;
    axis.center = -170.0f;
    axis.recentering.enabled = true;
    axis.recentering.wait = 0.0f;
    axis.recentering.time = 0.5f;
    axis.validate();

    REQUIRE(axis.trackValueChange(0.0f));
    axis.updateRecentering(0.1f, false, 0.0f);
    REQUIRE(wrappedDistance(axis.value, -170.0f, 360.0f) < 20.0f);

    axis.restrictions = InputAxisRestriction::NoRecentering;
    axis.value = 50.0f;
    axis.triggerRecentering();
    axis.updateRecentering(0.1f, false, 0.2f);
    REQUIRE(axis.value == 50.0f);

    axis.restrictions = InputAxisRestriction::Momentary;
    axis.value = 50.0f;
    axis.updateRecentering(0.1f, true, 0.3f);
    REQUIRE(axis.value == 50.0f);
}

TEST_CASE("InputAxis snaps immediately for negative time", "[unit][input-axis]")
{
    InputAxis axis;
    axis.range = {-100.0f, 100.0f};
    axis.value = 50.0f;
    axis.center = 10.0f;
    axis.recentering.enabled = true;
    axis.validate();

    axis.trackValueChange(0.0f);
    axis.updateRecentering(-1.0f, false, 0.0f);
    REQUIRE(axis.value == 10.0f);
}
