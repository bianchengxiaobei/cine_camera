#include <cinerig/core/blend_definition.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

TEST_CASE("Blend definition defaults and cut have the expected timing", "[unit][core][blend]")
{
    const BlendDefinition definition;
    REQUIRE(definition.style == BlendStyle::EaseInOut);
    REQUIRE(definition.time == 2.0f);
    REQUIRE(definition.blendTime() == 2.0f);
    REQUIRE_FALSE(definition.curve().empty());

    const BlendDefinition cut{BlendStyle::Cut, 3.0f};
    REQUIRE(cut.blendTime() == 0.0f);
    REQUIRE(cut.curve().empty());
}

TEST_CASE("Blend definition maps each style to its standard curve", "[unit][core][blend]")
{
    const BlendStyle styles[] = {
        BlendStyle::EaseInOut,
        BlendStyle::EaseIn,
        BlendStyle::EaseOut,
        BlendStyle::HardIn,
        BlendStyle::HardOut,
        BlendStyle::Linear,
    };

    for (BlendStyle style : styles)
    {
        const BlendDefinition definition{style, 1.0f};
        const math::Curve curve = definition.curve();
        REQUIRE(curve.size() == 2);
        REQUIRE(curve.evaluate(0.0f) == 0.0f);
        REQUIRE(curve.evaluate(1.0f) == 1.0f);
        REQUIRE(curve.evaluate(0.5f) > 0.0f);
        REQUIRE(curve.evaluate(0.5f) < 1.0f);
    }

    REQUIRE(BlendDefinition{BlendStyle::EaseInOut, 1.0f}.curve().evaluate(0.5f) == 0.5f);

    const float hardInMid = BlendDefinition{BlendStyle::HardIn, 1.0f}.curve().evaluate(0.5f);
    const float hardOutMid = BlendDefinition{BlendStyle::HardOut, 1.0f}.curve().evaluate(0.5f);
    REQUIRE(hardInMid < 0.5f);
    REQUIRE(hardOutMid > 0.5f);
}

TEST_CASE("Custom blend curves fall back to ease-in-out when empty", "[unit][core][blend]")
{
    BlendDefinition definition;
    definition.style = BlendStyle::Custom;
    REQUIRE(definition.curve().evaluate(0.5f) == 0.5f);

    definition.customCurve = math::Curve::linear();
    REQUIRE(definition.curve().evaluate(0.25f) == Catch::Approx(0.25f));
}
