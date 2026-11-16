#include "fake_entity_world.hpp"
#include "fake_physics_world.hpp"

#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/hard_look_at.hpp>
#include <cinerig/components/shot_quality_evaluator.hpp>
#include <cinerig/managers/clear_shot.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct ClearShotQualityRig
{
    FakeEntityWorld world;
    FakePhysicsWorld physics;
    CameraSystem system{world, &physics};
    managers::ClearShot* manager = nullptr;
    EntityId target = 100;

    ClearShotQualityRig()
    {
        world.add(
            target,
            Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id =
            system.createCamera<managers::ClearShot>("clear-shot");
        manager = static_cast<managers::ClearShot*>(system.camera(id));
        manager->reset();
        manager->defaultBlend = {BlendStyle::Cut, 0.0f};
        manager->standbyUpdate = StandbyUpdateMode::Always;
    }

    VirtualCamera* addShot(
        const char* name,
        EntityId entity,
        math::Vec3 position)
    {
        world.add(
            entity,
            Transform{position, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>(name);
        auto* camera = system.camera(id);
        camera->ownerEntity = entity;
        camera->targets.lookAt = TargetRef::entity(target);
        camera->targets.customLookAt = true;
        camera->standbyUpdate = StandbyUpdateMode::Always;
        camera->setAim<HardLookAt>();

        auto owned = std::make_unique<ShotQualityEvaluator>();
        owned->reset();
        owned->ignoreEntity = target;
        owned->validate();
        camera->addExtension(std::move(owned));
        return camera;
    }
};

SphereCastHit occludingHit()
{
    SphereCastHit hit;
    hit.entity = 5;
    hit.point = {0.0f, 0.0f, 5.0f};
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.distance = 5.0f;
    hit.radius = 0.0f;
    return hit;
}

} // namespace

TEST_CASE(
    "ClearShot selects the child with the better evaluated shot quality",
    "[scenario][manager][clearshot][shot-quality]")
{
    ClearShotQualityRig rig;
    VirtualCamera* shotA = rig.addShot("A", 10, {0.0f, 0.0f, 10.0f});
    VirtualCamera* shotB = rig.addShot("B", 11, {4.0f, 0.0f, 10.0f});
    REQUIRE(rig.manager->addChild(*shotA));
    REQUIRE(rig.manager->addChild(*shotB));

    rig.physics.sphereCastResults = {occludingHit()};

    rig.manager->internalUpdateCameraState(math::Vec3::up, 0.0f);

    REQUIRE(rig.manager->liveChild() == shotB);
    REQUIRE(shotA->state().shotQuality == Catch::Approx(0.2f));
    REQUIRE(shotB->state().shotQuality == Catch::Approx(1.0f));
    REQUIRE(rig.physics.sphereCastCalls >= 2);
    REQUIRE(rig.physics.lastFilter.ignoreEntity == rig.target);
}
