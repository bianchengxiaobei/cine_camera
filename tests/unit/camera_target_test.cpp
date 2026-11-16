#include <cinerig/core/camera_target.hpp>

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

namespace
{

class StubTargetGroup final : public TargetGroup
{
public:
    bool isValid() const override { return true; }
    bool isEmpty() const override { return false; }
    std::size_t targetCount() const override { return 1; }
    math::Vec3 targetPosition(std::size_t) const override { return {1.0f, 2.0f, 3.0f}; }
    math::Quat targetRotation(std::size_t) const override { return math::Quat::identity(); }
    float targetWeight(std::size_t) const override { return 2.0f; }
    float targetRadius(std::size_t) const override { return 0.5f; }
};

}

TEST_CASE("Target references retain kind and id", "[unit][core][target]")
{
    const TargetRef none;
    REQUIRE_FALSE(none.isValid());
    REQUIRE(none.kind == TargetRef::Kind::None);
    REQUIRE(none.id == 0);

    const TargetRef entity = TargetRef::entity(11);
    REQUIRE(entity.isValid());
    REQUIRE(entity.kind == TargetRef::Kind::Entity);
    REQUIRE(entity.id == 11);

    const TargetRef camera = TargetRef::camera(12);
    REQUIRE(camera.kind == TargetRef::Kind::Camera);
    REQUIRE(camera.id == 12);

    const TargetRef group = TargetRef::group(13);
    REQUIRE(group.kind == TargetRef::Kind::Group);
    REQUIRE(group.id == 13);

    REQUIRE(entity == TargetRef::entity(11));
    REQUIRE(entity != TargetRef::entity(12));
    REQUIRE(entity != none);
}

TEST_CASE("Camera target uses tracking unless a custom look-at is requested", "[unit][core][target]")
{
    CameraTarget target;
    REQUIRE_FALSE(target.customLookAt);
    REQUIRE_FALSE(target.tracking.isValid());
    REQUIRE_FALSE(target.lookAt.isValid());
    REQUIRE_FALSE(target.effectiveLookAt().isValid());

    target.tracking = TargetRef::entity(1);
    target.lookAt = TargetRef::entity(2);
    REQUIRE(target.effectiveLookAt() == target.tracking);

    target.customLookAt = true;
    REQUIRE(target.effectiveLookAt() == target.lookAt);
}

TEST_CASE("Target group is exposed as a member interface", "[unit][core][target]")
{
    const StubTargetGroup group;

    REQUIRE(group.isValid());
    REQUIRE_FALSE(group.isEmpty());
    REQUIRE(group.targetCount() == 1);
    REQUIRE(group.targetPosition(0).y == 2.0f);
    REQUIRE(group.targetWeight(0) == 2.0f);
    REQUIRE(group.targetRadius(0) == 0.5f);
    REQUIRE(math::angle(group.targetRotation(0), math::Quat::identity()) == 0.0f);
}
