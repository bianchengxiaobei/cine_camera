#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/confiner_oven.hpp>

using namespace cinerig;

namespace
{

std::vector<std::vector<math::Vec2>> lRoom()
{
    return {{
        {-11.0f, -7.0f},
        {11.0f, -7.0f},
        {11.0f, 7.0f},
        {0.0f, 7.0f},
        {0.0f, 0.0f},
        {-11.0f, 0.0f},
    }};
}

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

TEST_CASE("ConfinerOven bakes incrementally and exposes usable progress",
    "[unit][confineroven]")
{
    ConfinerOven oven(lRoom(), 1.0f, 0.0f, 0.0f);
    REQUIRE(oven.state() == ConfinerOven::BakingState::Baking);
    REQUIRE(oven.bakeProgress() == Catch::Approx(0.0f));

    const ConfinerOven::BakedSolution whileBaking = oven.getBakedSolution(1.0f);
    REQUIRE(whileBaking.isValid());
    REQUIRE_FALSE(whileBaking.isEmpty());

    oven.bakeConfiner(-1.0f);
    REQUIRE(oven.state() == ConfinerOven::BakingState::Baking);
    REQUIRE(oven.bakeProgress() >= 0.0f);
    REQUIRE(oven.bakeProgress() <= 1.0f);

    int bakeCalls = 0;
    while (oven.state() == ConfinerOven::BakingState::Baking && bakeCalls < 500)
    {
        oven.bakeConfiner(0.01f);
        ++bakeCalls;
    }

    REQUIRE(oven.state() == ConfinerOven::BakingState::Baked);
    REQUIRE(oven.bakeProgress() == Catch::Approx(1.0f));
    REQUIRE(oven.getBakedSolution(1.0f).isValid());
}

TEST_CASE("ConfinerOven bake budget does not change the final solution",
    "[unit][confineroven]")
{
    ConfinerOven incremental(lRoom(), 1.0f, 0.0f, 3.0f);
    ConfinerOven immediate(lRoom(), 1.0f, 0.0f, 3.0f);
    for (int call = 0;
         call < 500
         && incremental.state() == ConfinerOven::BakingState::Baking;
         ++call)
    {
        incremental.bakeConfiner(0.005f);
    }
    immediate.bakeConfiner(10.0f);

    REQUIRE(incremental.state() == ConfinerOven::BakingState::Baked);
    REQUIRE(immediate.state() == ConfinerOven::BakingState::Baked);
    for (const float height : {5.5f, 8.8f, 10.5f})
    {
        const ConfinerOven::BakedSolution a =
            incremental.getBakedSolution(height);
        const ConfinerOven::BakedSolution b = immediate.getBakedSolution(height);
        REQUIRE(a.paths().size() == b.paths().size());
        for (const math::Vec2 probe : {
                 math::Vec2{5.0f, 5.0f},
                 math::Vec2{-10.0f, 6.0f},
                 math::Vec2{0.0f, -5.0f}})
        {
            REQUIRE(math::approximately(
                a.confinePoint(probe),
                b.confinePoint(probe),
                1e-5f));
        }
    }
}

TEST_CASE("ConfinerOven skeleton padding changes oversize solutions",
    "[unit][confineroven]")
{
    ConfinerOven unpadded(lRoom(), 1.0f, 0.0f, 0.0f);
    ConfinerOven padded(lRoom(), 1.0f, 0.0f, 10.0f);
    for (int call = 0;
         call < 500
         && (unpadded.state() == ConfinerOven::BakingState::Baking
             || padded.state() == ConfinerOven::BakingState::Baking);
         ++call)
    {
        if (unpadded.state() == ConfinerOven::BakingState::Baking)
            unpadded.bakeConfiner(0.01f);
        if (padded.state() == ConfinerOven::BakingState::Baking)
            padded.bakeConfiner(0.01f);
    }
    REQUIRE(unpadded.state() == ConfinerOven::BakingState::Baked);
    REQUIRE(padded.state() == ConfinerOven::BakingState::Baked);

    const float height = 5.5f;
    const ConfinerOven::BakedSolution raw = unpadded.getBakedSolution(height);
    const ConfinerOven::BakedSolution wide = padded.getBakedSolution(height);
    REQUIRE(raw.isValid());
    REQUIRE(wide.isValid());
    REQUIRE_FALSE(raw.isEmpty());
    REQUIRE_FALSE(wide.isEmpty());
    REQUIRE((raw.paths().size() > 1 || raw.paths().front().size() > 1));

    const math::Vec2 rawPoint = raw.confinePoint({5.0f, 5.0f});
    const math::Vec2 widePoint = wide.confinePoint({5.0f, 5.0f});
    REQUIRE((rawPoint - widePoint).length() > 1e-3f);
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
        REQUIRE(math::approximately(solution.confinePoint({4.0f, 4.0f}), {0.0f, 0.0f}));
    }
}

TEST_CASE("ConfinerOven clamps to the maximum window size", "[unit][confineroven]")
{
    ConfinerOven oven(square(5.0f), 1.0f, 2.0f);
    REQUIRE(oven.state() == ConfinerOven::BakingState::Baking);
    REQUIRE(oven.bakeProgress() == Catch::Approx(0.0f));
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

