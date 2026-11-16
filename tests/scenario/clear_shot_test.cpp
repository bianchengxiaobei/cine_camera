#include "fake_entity_world.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/managers/clear_shot.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;
using Catch::Approx;

namespace
{

class QualityCamera final : public VirtualCamera
{
public:
    explicit QualityCamera(std::string name)
        : VirtualCamera(std::move(name))
    {
    }

    void internalUpdateCameraState(math::Vec3 worldUp, float deltaTime) override
    {
        ++updateCount;
        VirtualCamera::internalUpdateCameraState(worldUp, deltaTime);
        state_.shotQuality = quality;
    }

    float quality = 1.0f;
    int updateCount = 0;
};

struct ClearRig
{
    FakeEntityWorld world;
    CameraSystem system{world};

    QualityCamera* addChild(
        const char* name,
        EntityId entity,
        math::Vec3 position,
        float quality,
        int priority)
    {
        world.add(entity, Transform{position, math::Quat::identity()});
        const CameraId id = system.createCamera<QualityCamera>(name);
        auto* camera = static_cast<QualityCamera*>(system.camera(id));
        camera->ownerEntity = entity;
        camera->quality = quality;
        camera->priority = PrioritySettings(priority);
        camera->standbyUpdate = StandbyUpdateMode::Always;
        return camera;
    }

    managers::ClearShot* addManager()
    {
        const CameraId id =
            system.createCamera<managers::ClearShot>("clear-shot");
        auto* manager = static_cast<managers::ClearShot*>(system.camera(id));
        manager->standbyUpdate = StandbyUpdateMode::Always;
        return manager;
    }
};

void updateChildren(const std::vector<QualityCamera*>& children)
{
    for (QualityCamera* child : children)
        child->updateCameraState(math::Vec3::up, 0.0f);
}

} // namespace

TEST_CASE("ClearShot defaults and validation are explicit", "[scenario][manager][clearshot]")
{
    managers::ClearShot clearShot;
    REQUIRE(clearShot.activateAfter == 0.0f);
    REQUIRE(clearShot.minDuration == 0.0f);
    REQUIRE_FALSE(clearShot.randomizeChoice);
    REQUIRE(clearShot.randomSeed == 1u);
    REQUIRE(clearShot.randomizedChildOrder().empty());

    clearShot.activateAfter = -2.0f;
    clearShot.minDuration = std::numeric_limits<float>::infinity();
    clearShot.validate();
    REQUIRE(clearShot.activateAfter == 0.0f);
    REQUIRE(clearShot.minDuration == 0.0f);

    clearShot.activateAfter = 2.0f;
    clearShot.minDuration = 3.0f;
    clearShot.randomizeChoice = true;
    clearShot.randomSeed = 99;
    clearShot.reset();
    REQUIRE(clearShot.activateAfter == 0.0f);
    REQUIRE(clearShot.minDuration == 0.0f);
    REQUIRE_FALSE(clearShot.randomizeChoice);
    REQUIRE(clearShot.randomSeed == 1u);
    REQUIRE(clearShot.defaultBlend.style == BlendStyle::EaseInOut);
    REQUIRE(clearShot.defaultBlend.time == Approx(0.5f));
    REQUIRE(clearShot.customBlends.rules().empty());
    REQUIRE_FALSE(clearShot.previousStateIsValid);
    REQUIRE(clearShot.liveChild() == nullptr);
}

TEST_CASE("ClearShot picks best quality then priority", "[scenario][manager][clearshot]")
{
    ClearRig rig;
    managers::ClearShot* manager = rig.addManager();
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};
    QualityCamera* a = rig.addChild("A", 1, {0.0f, 0.0f, 0.0f}, 0.4f, 100);
    QualityCamera* b = rig.addChild("B", 2, {1.0f, 0.0f, 0.0f}, 0.9f, 10);
    QualityCamera* c = rig.addChild("C", 3, {2.0f, 0.0f, 0.0f}, 0.9f, 20);
    manager->addChild(*a);
    manager->addChild(*b);
    manager->addChild(*c);
    updateChildren({a, b, c});

    rig.world.setTime(1.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->liveChild() == c);
    REQUIRE(manager->isLiveChild(c, true));

    c->quality = 0.3f;
    c->updateCameraState(math::Vec3::up, 0.0f);
    rig.world.setTime(2.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->isLiveChild(b, true));
}

TEST_CASE("ClearShot skips disabled children and handles no children", "[scenario][manager][clearshot]")
{
    ClearRig rig;
    managers::ClearShot* manager = rig.addManager();
    rig.world.setTime(1.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->liveChild() == nullptr);

    QualityCamera* child = rig.addChild("A", 10, {0.0f, 0.0f, 0.0f}, 1.0f, 0);
    child->setEnabled(false);
    manager->addChild(*child);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->liveChild() == nullptr);

    child->setEnabled(true);
    child->updateCameraState(math::Vec3::up, 0.0f);
    rig.world.setTime(2.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->liveChild() == child);
}

TEST_CASE("ClearShot honors activateAfter and minDuration", "[scenario][manager][clearshot]")
{
    ClearRig rig;
    managers::ClearShot* manager = rig.addManager();
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};
    QualityCamera* a = rig.addChild("A", 20, {0.0f, 0.0f, 0.0f}, 1.0f, 0);
    QualityCamera* b = rig.addChild("B", 21, {1.0f, 0.0f, 0.0f}, 0.5f, 0);
    manager->addChild(*a);
    manager->addChild(*b);
    updateChildren({a, b});

    rig.world.setTime(1.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->liveChild() == a);

    b->quality = 2.0f;
    b->updateCameraState(math::Vec3::up, 0.0f);
    manager->activateAfter = 1.0f;
    manager->minDuration = 0.0f;

    rig.world.setTime(1.2f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.2f);
    REQUIRE(manager->liveChild() == a);

    rig.world.setTime(2.1f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.9f);
    REQUIRE(manager->liveChild() == a);

    rig.world.setTime(2.3f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.2f);
    REQUIRE(manager->liveChild() == b);

    a->quality = 3.0f;
    a->updateCameraState(math::Vec3::up, 0.0f);
    manager->activateAfter = 0.0f;
    manager->minDuration = 1.0f;

    rig.world.setTime(3.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.7f);
    REQUIRE(manager->liveChild() == b);

    rig.world.setTime(3.4f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.4f);
    REQUIRE(manager->liveChild() == a);
}

TEST_CASE("ClearShot cancels a pending switch when the live child recovers", "[scenario][manager][clearshot]")
{
    ClearRig rig;
    managers::ClearShot* manager = rig.addManager();
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};
    manager->activateAfter = 1.0f;
    QualityCamera* a = rig.addChild("A", 30, {0.0f, 0.0f, 0.0f}, 1.0f, 0);
    QualityCamera* b = rig.addChild("B", 31, {1.0f, 0.0f, 0.0f}, 0.5f, 0);
    manager->addChild(*a);
    manager->addChild(*b);
    updateChildren({a, b});

    rig.world.setTime(1.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->liveChild() == a);

    b->quality = 2.0f;
    b->updateCameraState(math::Vec3::up, 0.0f);
    rig.world.setTime(1.2f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.2f);
    REQUIRE(manager->liveChild() == a);

    a->quality = 3.0f;
    a->updateCameraState(math::Vec3::up, 0.0f);
    rig.world.setTime(1.4f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.2f);
    REQUIRE(manager->liveChild() == a);

    rig.world.setTime(3.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 1.6f);
    REQUIRE(manager->liveChild() == a);
}

TEST_CASE("ClearShot randomized choice can refresh equal-quality shots", "[scenario][manager][clearshot]")
{
    ClearRig rig;
    managers::ClearShot* manager = rig.addManager();
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};
    manager->randomizeChoice = true;
    manager->randomSeed = 7;
    QualityCamera* a = rig.addChild("A", 40, {0.0f, 0.0f, 0.0f}, 1.0f, 0);
    QualityCamera* b = rig.addChild("B", 41, {1.0f, 0.0f, 0.0f}, 1.0f, 0);
    manager->addChild(*a);
    manager->addChild(*b);
    updateChildren({a, b});

    manager->resetRandomization();
    rig.world.setTime(1.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    VirtualCamera* firstLive = manager->liveChild();
    REQUIRE((firstLive == a || firstLive == b));

    manager->resetRandomization();
    rig.world.setTime(2.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->liveChild() != firstLive);
    REQUIRE(manager->liveChild() == (firstLive == a ? b : a));
}

TEST_CASE("ClearShot uses the manager blend path on selection changes", "[scenario][manager][clearshot]")
{
    ClearRig rig;
    managers::ClearShot* manager = rig.addManager();
    manager->defaultBlend = {BlendStyle::Linear, 0.5f};
    QualityCamera* a = rig.addChild("A", 50, {0.0f, 0.0f, 0.0f}, 1.0f, 0);
    QualityCamera* b = rig.addChild("B", 51, {10.0f, 0.0f, 0.0f}, 0.5f, 0);
    manager->addChild(*a);
    manager->addChild(*b);
    updateChildren({a, b});

    rig.world.setTime(1.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->liveChild() == a);

    b->quality = 2.0f;
    b->updateCameraState(math::Vec3::up, 0.0f);
    rig.world.setTime(2.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.1f);

    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->isBlending());
    REQUIRE(manager->activeBlend() != nullptr);
    REQUIRE(manager->activeBlend()->duration == Approx(0.5f));
    REQUIRE(manager->isLiveChild(a));
    REQUIRE(manager->isLiveChild(b));
    REQUIRE_FALSE(manager->isLiveChild(a, true));
    REQUIRE(manager->isLiveChild(b, true));
}
