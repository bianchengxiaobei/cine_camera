#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/components/follow.hpp>
#include <cinerig/components/orbital_follow.hpp>
#include <cinerig/core/input_axis_controller.hpp>
#include <cinerig/engine/input_source.hpp>

using namespace cinerig;

namespace
{

struct FakeInputSource final : IInputSource
{
    float horizontal = 0.0f;
    float vertical = 0.0f;
    float scroll = 0.0f;

    float axis(std::string_view name) const override
    {
        if (name == "Horizontal")
            return horizontal;
        if (name == "Vertical")
            return vertical;
        if (name == "Scroll")
            return scroll;
        return 0.0f;
    }

    math::Vec2 axis2D(std::string_view name) const override
    {
        if (name == "Move")
            return {horizontal, vertical};
        return {};
    }
};

} // namespace

TEST_CASE("DefaultInputAxisDriver accelerates and decelerates", "[unit][input-controller]")
{
    InputAxis axis;
    axis.range = {-100.0f, 100.0f};
    axis.validate();

    DefaultInputAxisDriver driver;
    driver.accelTime = 0.5f;
    driver.decelTime = 0.5f;

    const float expected[] = {0.006018929f, 0.01443404f, 0.02380308f};
    for (float value : expected)
    {
        driver.processInput(axis, 0.1f, 0.1f);
        REQUIRE(axis.value == Catch::Approx(value).margin(1e-5f));
    }

    for (int frame = 0; frame < 20; ++frame)
        driver.processInput(axis, 0.0f, 0.1f);

    REQUIRE(std::abs(driver.currentSpeed()) < 1e-4f);
    REQUIRE(axis.value > 0.0f);
}

TEST_CASE("DefaultInputAxisDriver limits speed at endpoints and wraps", "[unit][input-controller]")
{
    InputAxis clamped;
    clamped.range = {0.0f, 10.0f};
    DefaultInputAxisDriver clampedDriver;
    clampedDriver.accelTime = 0.0f;
    clampedDriver.decelTime = 0.5f;
    for (int frame = 0; frame < 200; ++frame)
        clampedDriver.processInput(clamped, 1.0f, 0.1f);
    REQUIRE(clamped.value <= clamped.range.y);
    REQUIRE(clamped.value > 9.0f);

    InputAxis wrapped;
    wrapped.range = {-180.0f, 180.0f};
    wrapped.wrap = true;
    wrapped.value = 170.0f;
    DefaultInputAxisDriver wrappedDriver;
    wrappedDriver.accelTime = 0.1f;
    wrappedDriver.decelTime = 0.1f;
    for (int frame = 0; frame < 30; ++frame)
        wrappedDriver.processInput(wrapped, 1.0f, 1.0f);
    REQUIRE(wrapped.value >= wrapped.range.x);
    REQUIRE(wrapped.value <= wrapped.range.y);
    REQUIRE(wrapped.value < 0.0f);
}

TEST_CASE("DefaultInputAxisDriver handles momentary axes and negative time", "[unit][input-controller]")
{
    InputAxis axis = InputAxis::defaultMomentary();
    axis.center = 0.25f;
    axis.value = 0.25f;
    axis.validate();

    DefaultInputAxisDriver driver;
    driver.accelTime = 0.2f;
    driver.decelTime = 0.2f;
    driver.processInput(axis, 1.0f, 0.1f);
    REQUIRE(axis.value > 0.25f);

    driver.processInput(axis, 1.0f, -1.0f);
    REQUIRE(axis.value == 0.25f);

    InputAxis normal;
    normal.range = {-10.0f, 10.0f};
    normal.value = 3.0f;
    driver.processInput(normal, 1.0f, 0.1f);
    const float before = normal.value;
    driver.processInput(normal, 1.0f, -1.0f);
    REQUIRE(normal.value == before);
    REQUIRE(driver.currentSpeed() == 0.0f);
}

TEST_CASE("InputAxisController discovers OrbitalFollow axes", "[unit][input-controller]")
{
    VirtualCamera camera("controller");
    auto& orbital = camera.setBody<OrbitalFollow>();

    InputAxisController controller;
    REQUIRE(controller.synchronize(camera) == 3);
    REQUIRE(controller.size() == 3);

    auto* horizontal = controller.find("Look Orbit X");
    REQUIRE(horizontal != nullptr);
    REQUIRE(horizontal->axis == &orbital.horizontalAxis);
    REQUIRE(horizontal->hint == InputAxisHint::X);
    REQUIRE(horizontal->inputAxis == "Look Orbit X");

    auto* vertical = controller.find("Look Orbit Y");
    REQUIRE(vertical != nullptr);
    REQUIRE(vertical->axis == &orbital.verticalAxis);
    REQUIRE(vertical->hint == InputAxisHint::Y);

    auto* radial = controller.find("Orbit Scale");
    REQUIRE(radial != nullptr);
    REQUIRE(radial->axis == &orbital.radialAxis);

    REQUIRE(controller.bind("Look Orbit X", "Horizontal"));
    REQUIRE(controller.bind("Look Orbit Y", "Vertical"));
    horizontal->gain = 80.0f;
    vertical->gain = 55.0f;

    FakeInputSource input;
    input.horizontal = 1.0f;
    input.vertical = -1.0f;
    REQUIRE(controller.update(input, 0.1f));
    REQUIRE(orbital.horizontalAxis.value > 0.0f);
    REQUIRE(orbital.verticalAxis.value < 17.5f);

    REQUIRE(controller.synchronize(camera) == 3);
    REQUIRE(controller.size() == 3);
    auto* rediscovered = controller.find("Look Orbit X");
    REQUIRE(rediscovered != nullptr);
    REQUIRE(rediscovered->gain == Catch::Approx(80.0f));
    REQUIRE(rediscovered->inputAxis == "Horizontal");
}

TEST_CASE("InputAxisController synchronize removes stale owner bindings", "[unit][input-controller]")
{
    VirtualCamera camera("controller");
    camera.setBody<OrbitalFollow>();

    InputAxisController controller;
    REQUIRE(controller.synchronize(camera) == 3);

    InputAxis manualAxis;
    manualAxis.range = {-1.0f, 1.0f};
    controller.addAxis("manual", manualAxis);
    REQUIRE(controller.size() == 4);
    REQUIRE_FALSE(controller.bind("missing", "Horizontal"));

    camera.setBody<Follow>();
    REQUIRE(controller.synchronize(camera) == 0);
    REQUIRE(controller.size() == 1);
    REQUIRE(controller.find("manual") != nullptr);
    REQUIRE(controller.find("Look Orbit X") == nullptr);
}

TEST_CASE("InputAxisController supports gain, delta cancellation, and disabling", "[unit][input-controller]")
{
    InputAxis horizontal;
    horizontal.range = {-180.0f, 180.0f};
    InputAxis vertical;
    vertical.range = {-180.0f, 180.0f};

    InputAxisController controller;
    controller.addAxis("x", horizontal);
    controller.addAxis("y", vertical);
    auto* x = controller.find("x");
    auto* y = controller.find("y");
    REQUIRE(x != nullptr);
    REQUIRE(y != nullptr);
    REQUIRE(controller.bind("x", "Horizontal"));
    REQUIRE(controller.bind("y", "Vertical"));
    x->gain = 3.0f;
    x->cancelDeltaTime = true;
    x->driver.accelTime = 0.0f;
    x->driver.decelTime = 0.0f;

    FakeInputSource input;
    input.horizontal = 2.0f;
    REQUIRE(controller.update(input, 0.5f));
    REQUIRE(x->inputValue == Catch::Approx(12.0f));
    REQUIRE(horizontal.value == Catch::Approx(6.0f));

    const float before = horizontal.value;
    x->enabled = false;
    input.horizontal = 100.0f;
    controller.update(input, 0.5f);
    REQUIRE(horizontal.value == before);

    x->enabled = true;
    controller.cancelInput();
    controller.update(input, 0.5f);
    REQUIRE(horizontal.value > before);
    REQUIRE(x->driver.currentSpeed() > 0.0f);
}

TEST_CASE("InputAxisController triggers recentering and resets bindings", "[unit][input-controller]")
{
    InputAxis axis;
    axis.range = {-180.0f, 180.0f};
    axis.center = 0.0f;
    axis.value = 40.0f;
    axis.recentering.enabled = true;
    axis.recentering.wait = 0.0f;
    axis.recentering.time = 0.5f;

    InputAxisController controller;
    auto* binding = controller.addAxis("look", axis);
    REQUIRE(binding != nullptr);
    REQUIRE(controller.triggerRecentering("look"));
    axis.updateRecentering(0.1f, false, 1.0f);
    REQUIRE(axis.value < 40.0f);

    controller.reset();
    REQUIRE(controller.size() == 1);
    controller.clear();
    REQUIRE(controller.size() == 0);
    REQUIRE_FALSE(controller.triggerRecentering("look"));
    REQUIRE(controller.bindings().empty());
}
