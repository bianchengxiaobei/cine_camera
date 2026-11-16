#include "brain_test_support.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

BrainConfig linearConfig(float duration)
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = BlendDefinition{BlendStyle::Linear, duration};
    return config;
}

} // namespace

TEST_CASE("Nested blend interruption matches the reference event counts", "[parity][blend]")
{
    BrainTestRig rig(linearConfig(1.0f));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    const CameraId c = rig.addCamera("C", 30, 20.0f);
    rig.system.setCameraEnabled(b, false);
    rig.system.setCameraEnabled(c, false);

    int activated = 0;
    int deactivated = 0;
    int created = 0;
    int finished = 0;
    rig.brain->events().addCameraActivated([&](const ActivationEventParams&) { ++activated; });
    rig.brain->events().addCameraDeactivated([&](ICinemachineMixer*, ICinemachineCamera*) { ++deactivated; });
    rig.brain->events().addBlendCreated([&](const BlendEventParams&) { ++created; });
    rig.brain->events().addBlendFinished([&](ICinemachineMixer*, ICinemachineCamera*) { ++finished; });

    rig.step(0.1f);
    REQUIRE(activated == 1);
    REQUIRE(deactivated == 0);
    REQUIRE(created == 0);
    REQUIRE(finished == 0);
    REQUIRE_FALSE(rig.brain->isBlending());

    rig.step(0.1f);
    REQUIRE(activated == 1);
    REQUIRE(deactivated == 0);
    REQUIRE(created == 0);
    REQUIRE(finished == 0);
    REQUIRE_FALSE(rig.brain->isBlending());

    rig.system.setCameraEnabled(b, true);
    rig.step(0.1f);
    REQUIRE(activated == 2);
    REQUIRE(deactivated == 0);
    REQUIRE(created == 1);
    REQUIRE(finished == 0);
    REQUIRE(rig.brain->isBlending());

    rig.step(0.5f);
    REQUIRE(activated == 2);
    REQUIRE(deactivated == 0);
    REQUIRE(created == 1);
    REQUIRE(finished == 0);
    REQUIRE(rig.brain->isBlending());

    rig.system.setCameraEnabled(c, true);
    rig.step(0.1f);
    REQUIRE(activated == 3);
    REQUIRE(deactivated == 0);
    REQUIRE(created == 2);
    REQUIRE(finished == 0);
    REQUIRE(rig.brain->isBlending());

    rig.step(0.5f);
    REQUIRE(activated == 3);
    REQUIRE(deactivated == 1);
    REQUIRE(created == 2);
    REQUIRE(finished == 0);
    REQUIRE(rig.brain->isBlending());

    rig.step(0.5f);
    REQUIRE(activated == 3);
    REQUIRE(deactivated == 2);
    REQUIRE(created == 2);
    REQUIRE(finished == 1);
    REQUIRE_FALSE(rig.brain->isBlending());
}

TEST_CASE("Blend reversal matches the reference blend positions", "[parity][blend]")
{
    BrainTestRig rig(linearConfig(1.0f));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 1.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.1f);
    REQUIRE_FALSE(rig.brain->isBlending());

    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);
    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.output.applied.state.rawPosition.x == Catch::Approx(0.5f).margin(1e-3f));

    rig.system.setCameraEnabled(b, false);
    rig.step(0.2f);
    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.output.applied.state.rawPosition.x == Catch::Approx(0.3f).margin(1e-3f));

    rig.system.setCameraEnabled(b, true);
    rig.step(0.1f);
    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.output.applied.state.rawPosition.x == Catch::Approx(0.4f).margin(1e-3f));

    rig.step(0.4f);
    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.output.applied.state.rawPosition.x == Catch::Approx(0.8f).margin(1e-3f));

    rig.system.setCameraEnabled(b, false);
    rig.step(0.1f);
    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.output.applied.state.rawPosition.x == Catch::Approx(0.7f).margin(1e-3f));

    rig.system.setCameraEnabled(b, true);
    rig.step(0.1f);
    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.output.applied.state.rawPosition.x == Catch::Approx(0.8f).margin(1e-3f));

    rig.step(0.201f);
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE(rig.output.applied.state.rawPosition.x == Catch::Approx(1.0f).margin(1e-3f));
}

TEST_CASE("A cancelled blend keeps the outgoing camera updating", "[parity][blend]")
{
    BrainTestRig rig(linearConfig(1.0f));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 1.0f);
    rig.system.setCameraEnabled(b, false);

    math::Vec3 positionA{0.0f, 0.0f, 0.0f};
    math::Vec3 positionB{1.0f, 0.0f, 0.0f};
    const auto applyPositions = [&]()
    {
        rig.world.set(rig.camera(a).ownerEntity, Transform{positionA, math::Quat::identity()});
        rig.world.set(rig.camera(b).ownerEntity, Transform{positionB, math::Quat::identity()});
    };
    const auto moveCameras = [&]()
    {
        positionA.z += 1.0f;
        positionB.z += 1.0f;
        applyPositions();
    };

    rig.step(0.1f);
    moveCameras();
    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);
    moveCameras();

    rig.system.setCameraEnabled(b, false);
    rig.step(0.1f);
    REQUIRE(rig.brain->isBlending());

    moveCameras();
    rig.step(0.1f);

    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.output.applied.state.rawPosition.z
        == Catch::Approx(rig.camera(a).state().rawPosition.z).margin(1e-3f));
}

TEST_CASE("An interrupted freeze blend restarts from the current camera position", "[parity][blend]")
{
    BrainTestRig rig(linearConfig(1.0f));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.camera(a).blendHint = BlendHint::FreezeWhenBlendingOut;
    rig.system.setCameraEnabled(b, false);

    rig.step(0.1f);
    rig.system.setCameraEnabled(b, true);
    rig.step(0.1f);
    REQUIRE(rig.brain->isBlending());
    rig.step(0.1f);

    rig.system.setCameraEnabled(b, false);
    rig.step(0.1f);
    REQUIRE(rig.brain->isBlending());

    for (int frame = 0; frame < 100 && rig.brain->isBlending(); ++frame)
        rig.step(0.1f);

    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.world.set(
        rig.camera(a).ownerEntity,
        Transform{{0.0f, 5.0f, 0.0f}, math::Quat::identity()});
    rig.step(0.1f);
    REQUIRE(rig.output.applied.state.rawPosition.y == Catch::Approx(5.0f).margin(1e-3f));

    rig.system.setCameraEnabled(b, true);
    rig.step(0.1f);
    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.output.applied.state.rawPosition.y > 4.0f);
}
