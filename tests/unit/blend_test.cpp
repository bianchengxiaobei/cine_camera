#include <cinerig/core/blend.hpp>

#include <string>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

namespace
{

class FakeCamera final : public ICinemachineCamera
{
public:
    explicit FakeCamera(std::string cameraName, float x = 0.0f, bool valid = true)
        : name_(std::move(cameraName)), valid_(valid)
    {
        state_.rawPosition.x = x;
    }

    std::string_view name() const override { return name_; }
    const CameraState& state() const override { return state_; }
    bool isValid() const override { return valid_; }

    void setValid(bool valid) { valid_ = valid; }
    void setX(float x) { state_.rawPosition.x = x; }

private:
    std::string name_;
    CameraState state_;
    bool valid_ = true;
};

class MarkerBlender final : public IBlender
{
public:
    CameraState getIntermediateState(
        const ICinemachineCamera&, const ICinemachineCamera&, float t) override
    {
        called = true;
        lastT = t;
        CameraState state = CameraState::Default;
        state.rawPosition.x = 99.0f;
        return state;
    }

    bool called = false;
    float lastT = 0.0f;
};

}

TEST_CASE("Blend weight follows the curve and completion state", "[unit][core][blend]")
{
    FakeCamera a("A", 0.0f);
    FakeCamera b("B", 1.0f);
    Blend blend;
    blend.camA = &a;
    blend.camB = &b;
    blend.blendCurve = math::Curve::linear();
    blend.duration = 2.0f;

    REQUIRE(blend.isValid());
    REQUIRE_FALSE(blend.isComplete());
    REQUIRE(blend.blendWeight() == 0.0f);

    blend.timeInBlend = 1.0f;
    REQUIRE(blend.blendWeight() == Catch::Approx(0.5f));

    blend.timeInBlend = 2.0f;
    REQUIRE(blend.isComplete());
    REQUIRE(blend.blendWeight() == 1.0f);

    Blend emptyCurve;
    emptyCurve.camA = &a;
    emptyCurve.camB = &b;
    REQUIRE(emptyCurve.blendWeight() == 1.0f);

    Blend shortCurve;
    shortCurve.camA = &a;
    shortCurve.camB = &b;
    shortCurve.blendCurve = math::Curve({{0.0f, 0.0f, 0.0f, 0.0f}});
    REQUIRE(shortCurve.blendWeight() == 1.0f);

    Blend zeroDuration;
    zeroDuration.camA = &a;
    zeroDuration.camB = &b;
    zeroDuration.blendCurve = math::Curve::linear();
    REQUIRE(zeroDuration.isComplete());
    REQUIRE(zeroDuration.blendWeight() == 1.0f);

    Blend invalid;
    REQUIRE_FALSE(invalid.isValid());
    REQUIRE(invalid.isComplete());
    REQUIRE(invalid.state().lens.fieldOfView == CameraState::Default.lens.fieldOfView);
}

TEST_CASE("Blend state selects cameras and interpolates with the blend weight", "[unit][core][blend]")
{
    FakeCamera a("A", 0.0f);
    FakeCamera b("B", 10.0f);
    Blend blend;
    blend.camA = &a;
    blend.camB = &b;
    blend.blendCurve = math::Curve::linear();
    blend.duration = 2.0f;
    blend.timeInBlend = 1.0f;

    const CameraState halfway = blend.state();
    REQUIRE(halfway.rawPosition.x == Catch::Approx(5.0f));

    b.setValid(false);
    REQUIRE(blend.state().rawPosition.x == 0.0f);
    REQUIRE(blend.blendWeight() == Catch::Approx(0.5f));

    b.setValid(true);
    a.setValid(false);
    REQUIRE(blend.state().rawPosition.x == 10.0f);

    a.setValid(false);
    b.setValid(false);
    REQUIRE(blend.state().rawPosition.x == 0.0f);
}

TEST_CASE("Blend custom blender overrides the default state interpolation", "[unit][core][blend]")
{
    FakeCamera a("A", 0.0f);
    FakeCamera b("B", 10.0f);
    MarkerBlender blender;
    Blend blend;
    blend.camA = &a;
    blend.camB = &b;
    blend.blendCurve = math::Curve::linear();
    blend.duration = 1.0f;
    blend.timeInBlend = 0.25f;
    blend.customBlender = &blender;

    const CameraState state = blend.state();
    REQUIRE(blender.called);
    REQUIRE(blender.lastT == 0.25f);
    REQUIRE(state.rawPosition.x == 99.0f);
}

TEST_CASE("Blend clear and copy preserve the documented fields", "[unit][core][blend]")
{
    FakeCamera a("A", 0.0f);
    FakeCamera b("B", 10.0f);
    MarkerBlender blender;
    Blend blend;
    blend.camA = &a;
    blend.camB = &b;
    blend.blendCurve = math::Curve::linear();
    blend.timeInBlend = 0.4f;
    blend.duration = 1.0f;
    blend.customBlender = &blender;

    Blend copied;
    copied.copyFrom(blend);
    REQUIRE(copied.camA == &a);
    REQUIRE(copied.camB == &b);
    REQUIRE(copied.timeInBlend == 0.4f);
    REQUIRE(copied.duration == 1.0f);
    REQUIRE(copied.customBlender == &blender);

    blend.clearBlend();
    REQUIRE(blend.camA == nullptr);
    REQUIRE(blend.camB == &b);
    REQUIRE(blend.timeInBlend == 0.0f);
    REQUIRE(blend.duration == 0.0f);
    REQUIRE(blend.customBlender == nullptr);
    REQUIRE(blend.blendCurve.empty());
}

TEST_CASE("Blend uses traverses nested blend sources", "[unit][core][blend]")
{
    FakeCamera a("A", 0.0f);
    FakeCamera b("B", 10.0f);
    FakeCamera c("C", 20.0f);

    Blend inner;
    inner.camA = &a;
    inner.camB = &b;
    NestedBlendSource nested(&inner);

    Blend outer;
    outer.camA = &nested;
    outer.camB = &c;

    REQUIRE(nested.isValid());
    REQUIRE(nested.name() == "mid-blend");
    REQUIRE(nested.parentCamera() == nullptr);
    REQUIRE(outer.uses(&a));
    REQUIRE(outer.uses(&b));
    REQUIRE(outer.uses(&c));
    REQUIRE_FALSE(outer.uses(nullptr));

    FakeCamera unused("unused", 30.0f);
    REQUIRE_FALSE(outer.uses(&unused));
}
