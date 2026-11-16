#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/confiner_oven.hpp>

using namespace cinerig;

namespace
{

std::vector<std::vector<math::Vec2>> square(float halfExtent, bool clockwise = false)
{
    if (clockwise)
    {
        return {{
            {-halfExtent, -halfExtent},
            {-halfExtent, halfExtent},
            {halfExtent, halfExtent},
            {halfExtent, -halfExtent},
        }};
    }
    return {{
        {-halfExtent, -halfExtent},
        {halfExtent, -halfExtent},
        {halfExtent, halfExtent},
        {-halfExtent, halfExtent},
    }};
}

} // namespace

TEST_CASE("ConfinerOven has a synchronous baked lifecycle", "[unit][confineroven]")
{
    ConfinerOven empty;
    REQUIRE_FALSE(empty.isValid());
    REQUIRE(empty.state() == ConfinerOven::BakingState::Baked);
    REQUIRE(empty.bakeProgress() == 1.0f);
    REQUIRE_FALSE(empty.getBakedSolution(0.0f).isValid());

    ConfinerOven oven(square(2.0f), 1.0f);
    REQUIRE(oven.isValid());
    REQUIRE(oven.state() == ConfinerOven::BakingState::Baked);
    REQUIRE(oven.bakeProgress() == 1.0f);
    oven.bakeConfiner(0.01f);
    REQUIRE(oven.state() == ConfinerOven::BakingState::Baked);
    REQUIRE(oven.getBakedSolution(0.0f).isValid());
}

TEST_CASE("ConfinerOven confines independently of winding", "[unit][confineroven]")
{
    for (const bool clockwise : {false, true})
    {
        ConfinerOven oven(square(1.0f, clockwise), 1.0f);
        const ConfinerOven::BakedSolution solution = oven.getBakedSolution(0.0f);

        const math::Vec2 inside = solution.confinePoint({0.25f, -0.5f});
        REQUIRE(math::approximately(inside, {0.25f, -0.5f}));

        const math::Vec2 outside = solution.confinePoint({2.0f, 0.0f});
        REQUIRE(outside.x == Catch::Approx(1.0f).margin(1e-4f));
        REQUIRE(outside.y == Catch::Approx(0.0f).margin(1e-4f));

        const math::Vec2 boundary = solution.confinePoint({1.0f, 0.5f});
        REQUIRE(math::approximately(boundary, {1.0f, 0.5f}));
    }
}

TEST_CASE("ConfinerOven accounts for aspect ratio", "[unit][confineroven]")
{
    const std::vector<std::vector<math::Vec2>> rectangle{{
        {-4.0f, -2.0f},
        {4.0f, -2.0f},
        {4.0f, 2.0f},
        {-4.0f, 2.0f},
    }};
    ConfinerOven oven(rectangle, 2.0f);
    const ConfinerOven::BakedSolution solution = oven.getBakedSolution(0.0f);

    const math::Vec2 horizontal = solution.confinePoint({10.0f, 0.0f});
    REQUIRE(horizontal.x == Catch::Approx(4.0f).margin(1e-4f));
    REQUIRE(horizontal.y == Catch::Approx(0.0f).margin(1e-4f));

    const math::Vec2 vertical = solution.confinePoint({0.0f, 5.0f});
    REQUIRE(vertical.x == Catch::Approx(0.0f).margin(1e-4f));
    REQUIRE(vertical.y == Catch::Approx(2.0f).margin(1e-4f));
}

TEST_CASE("ConfinerOven erodes for the requested frustum", "[unit][confineroven]")
{
    ConfinerOven oven(square(5.0f), 1.0f);
    const ConfinerOven::BakedSolution solution = oven.getBakedSolution(1.0f);
    REQUIRE(solution.frustumHeight() == Catch::Approx(1.0f));

    const math::Vec2 inside = solution.confinePoint({3.0f, 0.0f});
    REQUIRE(math::approximately(inside, {3.0f, 0.0f}));

    const math::Vec2 outside = solution.confinePoint({8.0f, 0.0f});
    REQUIRE(outside.x == Catch::Approx(4.0f).margin(1e-4f));
    REQUIRE(outside.y == Catch::Approx(0.0f).margin(1e-4f));
}

TEST_CASE("ConfinerOven collapses oversize frustums to the midpoint", "[unit][confineroven]")
{
    ConfinerOven oven(square(5.0f), 1.0f);
    for (const float height : {5.0f, 20.0f})
    {
        const ConfinerOven::BakedSolution solution = oven.getBakedSolution(height);
        REQUIRE(solution.isValid());
        REQUIRE(solution.paths().size() == 1);
        REQUIRE(solution.paths().front().size() == 1);
        REQUIRE(math::approximately(solution.paths().front().front(), {0.0f, 0.0f}));
        REQUIRE(math::approximately(solution.confinePoint({100.0f, -50.0f}), {0.0f, 0.0f}));
    }
}

TEST_CASE("ConfinerOven clamps to the maximum window size", "[unit][confineroven]")
{
    ConfinerOven oven(square(5.0f), 1.0f, 2.0f);
    const ConfinerOven::BakedSolution solution = oven.getBakedSolution(4.0f);
    REQUIRE(solution.frustumHeight() == Catch::Approx(2.0f));
    REQUIRE(solution.confinePoint({8.0f, 0.0f}).x
        == Catch::Approx(3.0f).margin(1e-4f));
}

TEST_CASE("ConfinerOven handles concave paths", "[unit][confineroven]")
{
    const std::vector<std::vector<math::Vec2>> shape{{
        {-10.0f, -6.0f},
        {10.0f, -6.0f},
        {10.0f, 6.0f},
        {0.0f, 6.0f},
        {0.0f, 0.0f},
        {-10.0f, 0.0f},
    }};
    ConfinerOven oven(shape, 1.0f, 2.0f);
    const ConfinerOven::BakedSolution solution = oven.getBakedSolution(1.0f);
    REQUIRE(solution.isValid());
    REQUIRE_FALSE(solution.paths().empty());

    const math::Vec2 inside = solution.confinePoint({-5.0f, -3.0f});
    REQUIRE(math::approximately(inside, {-5.0f, -3.0f}));

    const math::Vec2 outside = solution.confinePoint({100.0f, 100.0f});
    REQUIRE(std::isfinite(outside.x));
    REQUIRE(std::isfinite(outside.y));
    REQUIRE((outside - math::Vec2{100.0f, 100.0f}).sqrMagnitude() > 0.0f);
}

TEST_CASE("ConfinerOven treats inner paths as holes", "[unit][confineroven]")
{
    std::vector<std::vector<math::Vec2>> shape = square(5.0f);
    shape.push_back({
        {-2.0f, -2.0f},
        {-2.0f, 2.0f},
        {2.0f, 2.0f},
        {2.0f, -2.0f},
    });
    ConfinerOven oven(shape, 1.0f);
    const ConfinerOven::BakedSolution solution = oven.getBakedSolution(0.0f);

    REQUIRE(math::approximately(solution.confinePoint({3.0f, 0.0f}), {3.0f, 0.0f}));

    const math::Vec2 holePoint = solution.confinePoint({0.0f, 0.0f});
    REQUIRE(std::abs(std::abs(holePoint.x) - 2.0f) < 1e-4f);
    REQUIRE(std::abs(holePoint.y) < 1e-4f);
}

TEST_CASE("ConfinerOven expands holes while eroding", "[unit][confineroven]")
{
    std::vector<std::vector<math::Vec2>> shape = square(5.0f);
    shape.push_back({
        {-2.0f, -2.0f},
        {-2.0f, 2.0f},
        {2.0f, 2.0f},
        {2.0f, -2.0f},
    });
    ConfinerOven oven(shape, 1.0f);
    const ConfinerOven::BakedSolution solution = oven.getBakedSolution(1.0f);

    REQUIRE(math::approximately(solution.confinePoint({3.5f, 0.0f}), {3.5f, 0.0f}));

    const math::Vec2 outer = solution.confinePoint({4.5f, 0.0f});
    REQUIRE(outer.x == Catch::Approx(4.0f).margin(1e-4f));

    const math::Vec2 hole = solution.confinePoint({0.0f, 0.0f});
    REQUIRE(std::abs(std::abs(hole.x) - 3.0f) < 1e-4f);
    REQUIRE(std::abs(hole.y) < 1e-4f);
}

TEST_CASE("ConfinerOven collapses when a hole consumes the eroded region",
    "[unit][confineroven]")
{
    std::vector<std::vector<math::Vec2>> shape = square(5.0f);
    shape.push_back({
        {-2.0f, -2.0f},
        {-2.0f, 2.0f},
        {2.0f, 2.0f},
        {2.0f, -2.0f},
    });
    ConfinerOven oven(shape, 1.0f);
    const ConfinerOven::BakedSolution solution = oven.getBakedSolution(2.0f);

    REQUIRE(solution.paths().size() == 1);
    REQUIRE(solution.paths().front().size() == 1);
    REQUIRE(math::approximately(solution.paths().front().front(), {0.0f, 0.0f}));
}


