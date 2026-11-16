#include <catch2/catch_test_macros.hpp>

#include <cinerig/core/tracker.hpp>
#include <cinerig/engine/entity_world.hpp>

using namespace cinerig;

namespace
{

constexpr float kDeltaTime = 1.0f / 60.0f;

TrackerTarget makeTarget(
    math::Vec3 position,
    math::Quat rotation = math::Quat::identity(),
    EntityId id = 1)
{
    return TrackerTarget{TargetRef::entity(id), position, rotation, true};
}

math::Quat yaw(float degrees)
{
    return math::quatFromAxisAngle(math::Vec3::up, degrees * math::DegToRad);
}

TrackerSettings settingsFor(BindingMode mode)
{
    TrackerSettings settings;
    settings.bindingMode = mode;
    settings.positionDamping = math::Vec3::zero;
    settings.rotationDamping = math::Vec3::zero;
    settings.quaternionDamping = 0.0f;
    return settings;
}

CameraState makeState()
{
    CameraState state = CameraState::makeDefault();
    state.rawPosition = {0.0f, 0.0f, 5.0f};
    return state;
}

} // namespace

TEST_CASE("TrackerSettings expose defaults, validation and effective damping", "[unit][tracker]")
{
    TrackerSettings settings;
    REQUIRE(settings.bindingMode == BindingMode::WorldSpace);
    REQUIRE(math::approximately(settings.positionDamping, math::Vec3::one));
    REQUIRE(settings.angularDampingMode == AngularDampingMode::Euler);
    REQUIRE(math::approximately(settings.rotationDamping, math::Vec3::one));
    REQUIRE(settings.quaternionDamping == 1.0f);
    REQUIRE(settings.maxDampTime() == 1.0f);

    settings.positionDamping = {-1.0f, 2.0f, -3.0f};
    settings.rotationDamping = {-4.0f, 5.0f, -6.0f};
    settings.quaternionDamping = -7.0f;
    settings.validate();
    REQUIRE(math::approximately(settings.positionDamping, {0.0f, 2.0f, 0.0f}));
    REQUIRE(math::approximately(settings.rotationDamping, {0.0f, 5.0f, 0.0f}));
    REQUIRE(settings.quaternionDamping == 0.0f);

    settings.bindingMode = BindingMode::LazyFollow;
    REQUIRE(math::approximately(settings.effectivePositionDamping(), {0.0f, 2.0f, 0.0f}));
    REQUIRE(math::approximately(settings.effectiveRotationDamping(), math::Vec3::zero));

    settings.bindingMode = BindingMode::LockToTargetWithWorldUp;
    REQUIRE(math::approximately(settings.effectiveRotationDamping(), {0.0f, 5.0f, 0.0f}));

    settings.bindingMode = BindingMode::LockToTargetNoRoll;
    REQUIRE(math::approximately(settings.effectiveRotationDamping(), {0.0f, 5.0f, 0.0f}));

    settings.bindingMode = BindingMode::LockToTarget;
    settings.angularDampingMode = AngularDampingMode::Quaternion;
    settings.quaternionDamping = 2.0f;
    REQUIRE(settings.maxDampTime() == 2.0f);
}

TEST_CASE("Tracker WorldSpace ignores target rotation", "[unit][tracker]")
{
    const TrackerSettings settings = settingsFor(BindingMode::WorldSpace);
    Tracker tracker;
    CameraState state = makeState();
    const TrackerTarget target = makeTarget({1.0f, 2.0f, 3.0f}, yaw(90.0f));

    tracker.initStateInfo(
        target, kDeltaTime, false, settings, math::Vec3::zero, math::Vec3::up, state);

    const math::Quat reference = tracker.getReferenceOrientation(
        target,
        BindingMode::WorldSpace,
        math::Vec3::zero,
        math::Vec3::up,
        state);
    REQUIRE(math::approximately(reference, math::Quat::identity()));

    math::Vec3 trackedPosition;
    math::Quat trackedOrientation;
    tracker.trackTarget(
        target,
        kDeltaTime,
        false,
        math::Vec3::up,
        math::Vec3{0.0f, 0.0f, -6.0f},
        settings,
        math::Vec3::zero,
        state,
        1.0f,
        trackedPosition,
        trackedOrientation);

    REQUIRE(math::approximately(trackedPosition, target.position));
    REQUIRE(math::approximately(trackedOrientation, math::Quat::identity()));
}

TEST_CASE("Tracker LockToTarget and OnAssign use the expected target orientation", "[unit][tracker]")
{
    CameraState state = makeState();
    Tracker tracker;
    const TrackerSettings lockSettings = settingsFor(BindingMode::LockToTarget);
    const TrackerTarget first = makeTarget({0.0f, 0.0f, 0.0f}, yaw(90.0f));

    tracker.initStateInfo(
        first, kDeltaTime, false, lockSettings, math::Vec3::zero, math::Vec3::up, state);

    const math::Quat locked = tracker.getReferenceOrientation(
        first,
        BindingMode::LockToTarget,
        math::Vec3::zero,
        math::Vec3::up,
        state);
    REQUIRE(math::approximately(locked, first.rotation));

    const TrackerSettings assignSettings = settingsFor(BindingMode::LockToTargetOnAssign);
    Tracker assignTracker;
    assignTracker.initStateInfo(
        first, kDeltaTime, false, assignSettings, math::Vec3::zero, math::Vec3::up, state);

    TrackerTarget moved = first;
    moved.rotation = yaw(180.0f);
    assignTracker.initStateInfo(
        moved, kDeltaTime, true, assignSettings, math::Vec3::zero, math::Vec3::up, state);

    const math::Quat assigned = assignTracker.getReferenceOrientation(
        moved,
        BindingMode::LockToTargetOnAssign,
        math::Vec3::zero,
        math::Vec3::up,
        state);
    REQUIRE(math::approximately(assigned, yaw(90.0f)));
}

TEST_CASE("Tracker world-up modes remove roll and pitch", "[unit][tracker]")
{
    CameraState state = makeState();
    Tracker tracker;
    const math::Quat rotation =
        yaw(35.0f)
        * math::quatFromAxisAngle(math::Vec3::right, 25.0f * math::DegToRad)
        * math::quatFromAxisAngle(math::Vec3::forward, 40.0f * math::DegToRad);
    const TrackerTarget target = makeTarget({1.0f, 2.0f, 3.0f}, rotation);

    const math::Quat worldUpReference = tracker.getReferenceOrientation(
        target,
        BindingMode::LockToTargetWithWorldUp,
        math::Vec3::zero,
        math::Vec3::up,
        state);
    REQUIRE(math::dot(
                math::rotate(worldUpReference, math::Vec3::up),
                math::Vec3::up)
            > 0.999f);

    const math::Quat rollOnly =
        math::quatFromAxisAngle(math::Vec3::forward, 40.0f * math::DegToRad);
    const TrackerTarget rollTarget = makeTarget({1.0f, 2.0f, 3.0f}, rollOnly);

    const math::Quat noRollReference = tracker.getReferenceOrientation(
        rollTarget,
        BindingMode::LockToTargetNoRoll,
        math::Vec3::zero,
        math::Vec3::up,
        state);
    REQUIRE(math::dot(
                math::rotate(noRollReference, math::Vec3::up),
                math::Vec3::up)
            > 0.999f);

    const math::Quat fullReference = tracker.getReferenceOrientation(
        rollTarget,
        BindingMode::LockToTarget,
        math::Vec3::zero,
        math::Vec3::up,
        state);
    REQUIRE(math::dot(
                math::rotate(fullReference, math::Vec3::up),
                math::Vec3::up)
            < 0.999f);
}

TEST_CASE("Tracker LazyFollow uses the target-local offset", "[unit][tracker]")
{
    const TrackerSettings settings = settingsFor(BindingMode::LazyFollow);
    Tracker tracker;
    CameraState state = makeState();
    const TrackerTarget target = makeTarget({1.0f, 2.0f, 3.0f}, yaw(90.0f));

    const math::Vec3 targetOffset{0.0f, 0.0f, 2.0f};
    tracker.initStateInfo(
        target,
        kDeltaTime,
        false,
        settings,
        targetOffset,
        math::Vec3::up,
        state);

    math::Vec3 trackedPosition;
    math::Quat trackedOrientation;
    tracker.trackTarget(
        target,
        kDeltaTime,
        false,
        math::Vec3::up,
        math::Vec3{0.0f, 0.0f, -5.0f},
        settings,
        targetOffset,
        state,
        1.0f,
        trackedPosition,
        trackedOrientation);

    const math::Vec3 expectedPosition =
        target.position + math::rotate(target.rotation, targetOffset);
    REQUIRE(math::approximately(trackedPosition, expectedPosition));
    REQUIRE(settings.effectivePositionDamping().x == 0.0f);
}

TEST_CASE("Tracker applies damping and snaps on negative time", "[unit][tracker]")
{
    TrackerSettings settings = settingsFor(BindingMode::WorldSpace);
    settings.positionDamping = {1.0f, 1.0f, 1.0f};
    Tracker tracker;
    CameraState state = makeState();
    const TrackerTarget target = makeTarget({0.0f, 0.0f, 0.0f});

    math::Vec3 trackedPosition;
    math::Quat trackedOrientation;
    tracker.initStateInfo(
        target, kDeltaTime, false, settings, math::Vec3::zero, math::Vec3::up, state);
    tracker.trackTarget(
        target,
        kDeltaTime,
        false,
        math::Vec3::up,
        math::Vec3::zero,
        settings,
        math::Vec3::zero,
        state,
        1.0f,
        trackedPosition,
        trackedOrientation);
    REQUIRE(math::approximately(trackedPosition, math::Vec3::zero));

    const TrackerTarget moved = makeTarget({10.0f, 0.0f, 0.0f});
    tracker.initStateInfo(
        moved, kDeltaTime, true, settings, math::Vec3::zero, math::Vec3::up, state);
    tracker.trackTarget(
        moved,
        kDeltaTime,
        true,
        math::Vec3::up,
        math::Vec3::zero,
        settings,
        math::Vec3::zero,
        state,
        1.0f,
        trackedPosition,
        trackedOrientation);
    REQUIRE(trackedPosition.x > 0.0f);
    REQUIRE(trackedPosition.x < 10.0f);

    tracker.initStateInfo(
        moved, -1.0f, true, settings, math::Vec3::zero, math::Vec3::up, state);
    tracker.trackTarget(
        moved,
        -1.0f,
        true,
        math::Vec3::up,
        math::Vec3::zero,
        settings,
        math::Vec3::zero,
        state,
        1.0f,
        trackedPosition,
        trackedOrientation);
    REQUIRE(math::approximately(trackedPosition, {10.0f, 0.0f, 0.0f}));
}

TEST_CASE("Tracker damps target rotation in Euler and quaternion modes", "[unit][tracker]")
{
    TrackerSettings settings = settingsFor(BindingMode::LockToTarget);
    settings.rotationDamping = {1.0f, 1.0f, 1.0f};
    settings.quaternionDamping = 1.0f;

    CameraState state = makeState();
    Tracker eulerTracker;
    const TrackerTarget still = makeTarget({0.0f, 0.0f, 0.0f});
    math::Vec3 trackedPosition;
    math::Quat trackedOrientation;

    eulerTracker.initStateInfo(
        still, kDeltaTime, false, settings, math::Vec3::zero, math::Vec3::up, state);
    eulerTracker.trackTarget(
        still,
        kDeltaTime,
        false,
        math::Vec3::up,
        math::Vec3::zero,
        settings,
        math::Vec3::zero,
        state,
        1.0f,
        trackedPosition,
        trackedOrientation);

    const TrackerTarget rotated = makeTarget({0.0f, 0.0f, 0.0f}, yaw(90.0f));
    eulerTracker.initStateInfo(
        rotated, kDeltaTime, true, settings, math::Vec3::zero, math::Vec3::up, state);
    eulerTracker.trackTarget(
        rotated,
        kDeltaTime,
        true,
        math::Vec3::up,
        math::Vec3::zero,
        settings,
        math::Vec3::zero,
        state,
        1.0f,
        trackedPosition,
        trackedOrientation);
    const float eulerAngle = math::angle(trackedOrientation, math::Quat::identity());
    REQUIRE(eulerAngle > 0.0f);
    REQUIRE(eulerAngle < math::HalfPi);

    settings.angularDampingMode = AngularDampingMode::Quaternion;
    Tracker quaternionTracker;
    quaternionTracker.initStateInfo(
        still, kDeltaTime, false, settings, math::Vec3::zero, math::Vec3::up, state);
    quaternionTracker.trackTarget(
        still,
        kDeltaTime,
        false,
        math::Vec3::up,
        math::Vec3::zero,
        settings,
        math::Vec3::zero,
        state,
        1.0f,
        trackedPosition,
        trackedOrientation);
    quaternionTracker.initStateInfo(
        rotated, kDeltaTime, true, settings, math::Vec3::zero, math::Vec3::up, state);
    quaternionTracker.trackTarget(
        rotated,
        kDeltaTime,
        true,
        math::Vec3::up,
        math::Vec3::zero,
        settings,
        math::Vec3::zero,
        state,
        1.0f,
        trackedPosition,
        trackedOrientation);
    const float quaternionAngle = math::angle(trackedOrientation, math::Quat::identity());
    REQUIRE(quaternionAngle > 0.0f);
    REQUIRE(quaternionAngle < math::HalfPi);
}

TEST_CASE("Tracker minimum target distance pushes the camera back", "[unit][tracker]")
{
    Tracker tracker;
    const math::Vec3 offset = tracker.getOffsetForMinimumTargetDistance(
        1.0f,
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -10.0f},
        math::Vec3::forward,
        math::Vec3::up,
        {0.0f, 0.0f, -9.0f});

    REQUIRE(math::approximately(offset, {0.0f, 0.0f, -1.0f}));
    REQUIRE(math::approximately(tracker.previousTargetPosition(), {0.0f, 0.0f, -1.0f}));

    const math::Vec3 disabled = tracker.getOffsetForMinimumTargetDistance(
        0.0f,
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -10.0f},
        math::Vec3::forward,
        math::Vec3::up,
        {0.0f, 0.0f, -9.0f});
    REQUIRE(math::approximately(disabled, math::Vec3::zero));
}

TEST_CASE("Tracker warps its previous target position", "[unit][tracker]")
{
    const TrackerSettings settings = settingsFor(BindingMode::WorldSpace);
    Tracker tracker;
    CameraState state = makeState();
    const TrackerTarget target = makeTarget({1.0f, 2.0f, 3.0f});

    tracker.initStateInfo(
        target, kDeltaTime, false, settings, math::Vec3::zero, math::Vec3::up, state);
    tracker.onTargetObjectWarped({4.0f, 5.0f, 6.0f});
    REQUIRE(math::approximately(tracker.previousTargetPosition(), {5.0f, 7.0f, 9.0f}));
}
