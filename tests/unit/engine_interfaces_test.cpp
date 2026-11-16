#include <cinerig/cinerig.hpp>

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

static_assert(std::is_same_v<EntityId, std::uint64_t>);
static_assert(std::is_same_v<CameraId, std::uint64_t>);
static_assert(std::is_same_v<SplineId, std::uint64_t>);
static_assert(std::is_same_v<GroupId, std::uint64_t>);

namespace
{

class FakeEntityWorld final : public IEntityWorld
{
public:
    bool exists(EntityId id) const override
    {
        return transforms.find(id) != transforms.end();
    }

    Transform transform(EntityId id) const override
    {
        const auto it = transforms.find(id);
        return it == transforms.end() ? Transform{} : it->second;
    }

    bool trySetTransform(EntityId id, const Transform& value) override
    {
        const auto it = transforms.find(id);
        if (it == transforms.end())
            return false;
        it->second = value;
        return true;
    }

    UpdateClock lastChangeClock(EntityId id) const override
    {
        const auto it = changedClock.find(id);
        return it == changedClock.end() ? UpdateClock::Late : it->second;
    }

    float deltaTime(UpdateClock clock) const override
    {
        return clock == UpdateClock::Fixed ? fixedDelta : lateDelta;
    }

    float fixedDeltaTime() const override { return fixedDelta; }
    float time() const override { return elapsed; }

    std::uint64_t frame(UpdateClock clock) const override
    {
        return clock == UpdateClock::Fixed ? fixedFrame : lateFrame;
    }

    math::Vec3 worldUp() const override { return {0.0f, 1.0f, 0.0f}; }
    math::Vec2 viewportSize() const override { return {1920.0f, 1080.0f}; }

    std::unordered_map<EntityId, Transform> transforms;
    std::unordered_map<EntityId, UpdateClock> changedClock;
    float fixedDelta = 0.02f;
    float lateDelta = 0.016f;
    float elapsed = 12.5f;
    std::uint64_t fixedFrame = 50;
    std::uint64_t lateFrame = 100;
};

class ReadOnlyEntityWorld final : public IEntityWorld
{
public:
    bool exists(EntityId) const override { return true; }
    Transform transform(EntityId) const override { return {}; }
    float deltaTime(UpdateClock) const override { return 0.0f; }
    float fixedDeltaTime() const override { return 0.0f; }
    float time() const override { return 0.0f; }
    std::uint64_t frame(UpdateClock) const override { return 0; }
    math::Vec3 worldUp() const override { return math::Vec3::up; }
    math::Vec2 viewportSize() const override { return {1.0f, 1.0f}; }
};

class FakeCameraOutput final : public ICameraOutput
{
public:
    void apply(const AppliedCamera& camera) override
    {
        applied = camera;
        ++applyCount;
    }

    LensSettings outputLens() const override { return inheritedLens; }
    float aspect() const override { return outputAspect; }

    AppliedCamera applied;
    int applyCount = 0;
    LensSettings inheritedLens = LensSettings::Default;
    float outputAspect = 16.0f / 9.0f;
};

class FakeSpline final : public ISpline
{
public:
    float length() const override { return lengthValue; }
    math::Vec3 evaluatePosition(float t) const override { return {t * lengthValue, 0.0f, 0.0f}; }
    math::Vec3 evaluateTangent(float) const override { return math::Vec3::right; }
    math::Vec3 evaluateUp(float) const override { return math::Vec3::up; }
    float closestPoint(math::Vec3 worldPosition) const override
    {
        return math::clamp01(worldPosition.x / lengthValue);
    }

    float lengthValue = 10.0f;
};

class FakeSplineProvider final : public ISplineProvider
{
public:
    const ISpline* get(SplineId id) const override
    {
        return id == 1 ? &spline : nullptr;
    }

    FakeSpline spline;
};

class FakeAnimationState final : public IAnimationStateProvider
{
public:
    int currentStateHash(EntityId entity, int layer) const override
    {
        (void)entity;
        return layer == 0 ? 11 : 0;
    }

    int nextStateHash(EntityId entity, int layer) const override
    {
        (void)entity;
        return layer == 0 ? 22 : 0;
    }

    float normalizedTime(EntityId entity, int layer) const override
    {
        (void)entity;
        return layer == 0 ? 0.5f : 0.0f;
    }
};

}

TEST_CASE("Engine ids, transforms, rays and layer masks have stable defaults", "[unit][engine]")
{
    REQUIRE(static_cast<std::uint8_t>(UpdateClock::Fixed) != static_cast<std::uint8_t>(UpdateClock::Late));

    const Transform transform;
    REQUIRE(transform.position.x == 0.0f);
    REQUIRE(transform.position.y == 0.0f);
    REQUIRE(transform.position.z == 0.0f);
    REQUIRE(transform.scale.x == 1.0f);
    REQUIRE(transform.scale.y == 1.0f);
    REQUIRE(transform.scale.z == 1.0f);

    const Ray ray;
    REQUIRE(ray.origin.x == 0.0f);
    REQUIRE(ray.direction.x == 0.0f);
    REQUIRE(ray.direction.y == 0.0f);
    REQUIRE(ray.direction.z == -1.0f);

    const Bounds bounds;
    REQUIRE(bounds.center.x == 0.0f);
    REQUIRE(bounds.extents.y == 0.0f);

    const LayerMask all;
    REQUIRE(all.contains(0));
    REQUIRE(all.contains(31));
    REQUIRE_FALSE(all.contains(32));

    const LayerMask selected{0b101u};
    REQUIRE(selected.contains(0));
    REQUIRE_FALSE(selected.contains(1));
    REQUIRE(selected.contains(2));

    const QueryFilter filter;
    REQUIRE(filter.layers.contains(0));
    REQUIRE(filter.ignoreEntity == 0);
}

TEST_CASE("Entity world exposes time, frame clocks and writable transforms", "[unit][engine]")
{
    FakeEntityWorld world;
    world.transforms[7] = Transform{{1.0f, 2.0f, 3.0f}, math::Quat::identity(), {1.0f, 1.0f, 1.0f}};
    world.changedClock[7] = UpdateClock::Fixed;

    REQUIRE(world.exists(7));
    REQUIRE_FALSE(world.exists(8));
    REQUIRE(world.transform(7).position.x == 1.0f);
    REQUIRE(world.transform(8).position.x == 0.0f);
    REQUIRE(world.lastChangeClock(7) == UpdateClock::Fixed);
    REQUIRE(world.lastChangeClock(8) == UpdateClock::Late);
    REQUIRE(world.deltaTime(UpdateClock::Fixed) == Catch::Approx(0.02f));
    REQUIRE(world.deltaTime(UpdateClock::Late) == Catch::Approx(0.016f));
    REQUIRE(world.fixedDeltaTime() == Catch::Approx(0.02f));
    REQUIRE(world.time() == Catch::Approx(12.5f));
    REQUIRE(world.frame(UpdateClock::Fixed) == 50);
    REQUIRE(world.frame(UpdateClock::Late) == 100);
    REQUIRE(world.viewportSize().x == 1920.0f);
    REQUIRE(world.viewportSize().y == 1080.0f);

    const Transform moved{{4.0f, 5.0f, 6.0f}, math::Quat::identity(), {2.0f, 2.0f, 2.0f}};
    REQUIRE(world.trySetTransform(7, moved));
    REQUIRE(world.transform(7).position.z == 6.0f);
    REQUIRE_FALSE(world.trySetTransform(8, moved));
}

TEST_CASE("Entity world optional hooks default to read-only and late", "[unit][engine]")
{
    ReadOnlyEntityWorld world;
    REQUIRE_FALSE(world.trySetTransform(1, Transform{}));
    REQUIRE_FALSE(world.trySetTransform(1, Transform{{1.0f, 0.0f, 0.0f}, {}, {}}));
    REQUIRE(world.lastChangeClock(1) == UpdateClock::Late);
}

TEST_CASE("Camera output receives final position, orientation and lens", "[unit][engine]")
{
    CameraState state = CameraState::Default;
    state.rawPosition = {1.0f, 2.0f, 3.0f};
    state.rawOrientation = math::quatFromAxisAngle(math::Vec3::up, math::HalfPi);
    state.positionCorrection = {0.5f, -1.0f, 2.0f};
    state.orientationCorrection = math::quatFromAxisAngle(math::Vec3::right, 0.25f);
    state.lens.fieldOfView = 70.0f;
    state.lens.dutch = 12.0f;

    FakeCameraOutput output;
    output.inheritedLens.fieldOfView = 35.0f;
    output.outputAspect = 2.0f;

    AppliedCamera applied;
    applied.state = state;
    applied.aspect = 2.0f;
    applied.hasCamera = true;
    output.apply(applied);

    REQUIRE(output.applyCount == 1);
    REQUIRE(output.applied.hasCamera);
    REQUIRE(output.applied.aspect == Catch::Approx(2.0f));
    REQUIRE(output.applied.state.lens.fieldOfView == Catch::Approx(70.0f));
    REQUIRE(output.applied.state.finalPosition().x == Catch::Approx(state.finalPosition().x));
    REQUIRE(output.applied.state.finalPosition().y == Catch::Approx(state.finalPosition().y));
    REQUIRE(output.applied.state.finalPosition().z == Catch::Approx(state.finalPosition().z));
    REQUIRE(math::angle(output.applied.state.finalOrientation(), state.finalOrientation())
        == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(output.outputLens().fieldOfView == Catch::Approx(35.0f));
    REQUIRE(output.aspect() == Catch::Approx(2.0f));

    const AppliedCamera empty;
    REQUIRE_FALSE(empty.hasCamera);
    REQUIRE(empty.aspect == 1.0f);
}

TEST_CASE("Null adapters are safe and return neutral values", "[unit][engine]")
{
    const NullPhysicsWorld physics;
    const QueryFilter filter;
    REQUIRE_FALSE(physics.raycast(Ray{}, 10.0f, filter).has_value());
    REQUIRE_FALSE(physics.sphereCast(Ray{}, 1.0f, 10.0f, filter).has_value());
    REQUIRE(physics.overlapSphere({}, 1.0f, filter).empty());
    REQUIRE_FALSE(physics.bounds(1).has_value());

    const NullInputSource input;
    REQUIRE(input.axis("Move X") == 0.0f);
    REQUIRE(input.axis2D("Move").x == 0.0f);
    REQUIRE(input.axis2D("Move").y == 0.0f);

    NullPostProcessor post;
    post.apply(AppliedCamera{});
}

TEST_CASE("Spline, spline provider and animation state interfaces are usable", "[unit][engine]")
{
    FakeSplineProvider provider;
    const ISpline* spline = provider.get(1);
    REQUIRE(spline != nullptr);
    REQUIRE(provider.get(2) == nullptr);
    REQUIRE(spline->length() == Catch::Approx(10.0f));
    REQUIRE(spline->evaluatePosition(0.5f).x == Catch::Approx(5.0f));
    REQUIRE(spline->evaluateTangent(0.5f).x == 1.0f);
    REQUIRE(spline->evaluateUp(0.5f).y == 1.0f);
    REQUIRE(spline->closestPoint({4.0f, 0.0f, 0.0f}) == Catch::Approx(0.4f));

    const FakeAnimationState animation;
    REQUIRE(animation.currentStateHash(7, 0) == 11);
    REQUIRE(animation.nextStateHash(7, 0) == 22);
    REQUIRE(animation.normalizedTime(7, 0) == Catch::Approx(0.5f));
    REQUIRE(animation.isInTransition(7, 0));
}
