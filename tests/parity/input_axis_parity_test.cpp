#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/core/input_axis.hpp>
#include <cinerig/core/input_axis_controller.hpp>

using namespace cinerig;

namespace
{

struct AxisResponseCase
{
    float inputValue;
    float expected[3];
};

const AxisResponseCase kAxisResponseCases[] = {
    {0.1f, {0.006018929f, 0.01443404f, 0.02380308f}},
    {0.5f, {0.03009464f, 0.07217018f, 0.1190154f}},
    {1.0f, {0.06018928f, 0.1443404f, 0.2380308f}},
    {100.0f, {6.018928f, 14.43403f, 23.80308f}},
};

} // namespace

TEST_CASE("InputAxis driver matches the reference response sequence", "[parity][input-axis]")
{
    for (const AxisResponseCase& testCase : kAxisResponseCases)
    {
        InputAxis axis;
        axis.range = {-100.0f, 100.0f};
        axis.center = 0.0f;
        axis.wrap = false;
        axis.validate();

        DefaultInputAxisDriver driver;
        driver.accelTime = 0.5f;
        driver.decelTime = 0.5f;
        driver.validate();

        for (float expected : testCase.expected)
        {
            driver.processInput(axis, testCase.inputValue, 0.1f);
            REQUIRE(axis.value == Catch::Approx(expected).margin(1e-5f));
        }
    }
}

TEST_CASE("InputAxis driver accelerates and decelerates over the reference timing grid", "[parity][input-axis]")
{
    const float accelTimes[] = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    for (float accelTime : accelTimes)
    {
        InputAxis axis;
        axis.range = {-100.0f, 100.0f};
        axis.center = 0.0f;
        axis.wrap = false;
        axis.validate();

        DefaultInputAxisDriver driver;
        driver.accelTime = accelTime;
        driver.decelTime = accelTime;
        driver.validate();

        float previousValue = axis.value;
        float previousDelta = 0.0f;
        float inputValue = 1.0f;

        for (int frame = 0; frame < 20; ++frame)
        {
            driver.processInput(axis, inputValue, 0.1f);
            const float delta = axis.value - previousValue;
            REQUIRE(delta > previousDelta);
            previousValue = axis.value;
            previousDelta = delta;
            if (0.1f * static_cast<float>(frame) >= accelTime)
                break;
            REQUIRE(delta < inputValue * 0.1f);
        }
        REQUIRE(previousDelta == Catch::Approx(inputValue * 0.1f).margin(1e-3f));

        inputValue = 0.0f;
        for (int frame = 0; frame < 20; ++frame)
        {
            driver.processInput(axis, inputValue, 0.1f);
            const float delta = axis.value - previousValue;
            REQUIRE(delta < previousDelta);
            previousValue = axis.value;
            previousDelta = delta;
            if (0.1f * static_cast<float>(frame) >= accelTime)
                break;
            REQUIRE(delta > 0.0f);
        }
        REQUIRE(previousDelta == Catch::Approx(0.0f).margin(1e-3f));
    }
}
