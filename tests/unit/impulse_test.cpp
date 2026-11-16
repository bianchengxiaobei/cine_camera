#include "fake_entity_world.hpp"

#include <cmath>
#include <memory>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/impulse/impulse_definition.hpp>
#include <cinerig/impulse/impulse_listener.hpp>
#include <cinerig/impulse/impulse_manager.hpp>
#include <cinerig/impulse/impulse_source.hpp>
#include <cinerig/math/damp.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;
using ImpulseEvent = cinerig::impulse::ImpulseEvent;

namespace
{

class TimeSignal final : public ISignalSource6D
{
public:
    float signalDuration() const override { return 0.0f; }

    void getSignal(
        float timeSinceSignalStart,
        math::Vec3& pos,
        math::Quat& rot) const override
    {
        pos = {timeSinceSignalStart, 0.0f, 0.0f};
        rot = math::Quat::identity();
    }
};

class FixedSignal final : public ISignalSource6D
{
public:
    explicit FixedSignal(
        math::Vec3 position = {1.0f, 0.0f, 0.0f},
        math::Quat rotation = math::Quat::identity(),
        float duration = 0.0f)
        : position_(position),
          rotation_(rotation),
          duration_(duration)
    {
    }

    float signalDuration() const override { return duration_; }

    void getSignal(
        float timeSinceSignalStart,
        math::Vec3& pos,
        math::Quat& rot) const override
    {
        (void)timeSinceSignalStart;
        pos = position_;
        rot = rotation_;
    }

private:
    math::Vec3 position_;
    math::Quat rotation_;
    float duration_;
};

ImpulseEvent* addEvent(
    impulse::ImpulseManager& manager,
    std::unique_ptr<ISignalSource6D> signal,
    math::Vec3 position,
    std::uint32_t channel = 1u,
    float sustain = 2.0f)
{
    std::unique_ptr<impulse::ImpulseEvent> event = manager.newImpulseEvent();
    event->signalSource = std::move(signal);
    event->position = position;
    event->channel = channel;
    event->envelope = {};
    event->envelope.sustainTime = sustain;
    event->dissipationDistance = 100.0f;
    event->propagationSpeed = 9999999.0f;
    ImpulseEvent* result = event.get();
    manager.addImpulseEvent(std::move(event));
    return result;
}

math::Quat yaw(float degrees)
{
    return math::quatFromAxisAngle(math::Vec3::up, degrees * math::DegToRad);
}

} // namespace

TEST_CASE("Impulse envelope preserves attack sustain and decay", "[unit][impulse]")
{
    impulse::EnvelopeDefinition envelope;
    envelope.attackTime = 0.2f;
    envelope.sustainTime = 0.3f;
    envelope.decayTime = 0.5f;

    REQUIRE(math::approximately(envelope.duration(), 1.0f, 1e-6f));
    REQUIRE(envelope.getValueAt(-0.1f) == 0.0f);
    const float attack = envelope.getValueAt(0.1f);
    REQUIRE(attack > 0.0f);
    REQUIRE(attack < 1.0f);
    REQUIRE(envelope.getValueAt(0.2f) == 1.0f);
    REQUIRE(envelope.getValueAt(0.49f) == 1.0f);
    const float decay = envelope.getValueAt(0.75f);
    REQUIRE(decay > 0.0f);
    REQUIRE(decay < 1.0f);
    REQUIRE(envelope.getValueAt(1.0f) == 0.0f);

    envelope.holdForever = true;
    REQUIRE(envelope.duration() == -1.0f);
    REQUIRE(envelope.getValueAt(100.0f) == 1.0f);

    envelope.holdForever = false;
    envelope.changeStopTime(0.1f, true);
    REQUIRE(envelope.attackTime == 0.0f);
    REQUIRE(math::approximately(envelope.sustainTime, 0.1f, 1e-6f));
    REQUIRE(envelope.decayTime == 0.0f);
    REQUIRE(envelope.getValueAt(0.05f) == 1.0f);
    REQUIRE(envelope.getValueAt(0.11f) == 0.0f);

    envelope.attackTime = -1.0f;
    envelope.sustainTime = -1.0f;
    envelope.decayTime = -1.0f;
    envelope.validate();
    REQUIRE(envelope.attackTime == 0.0f);
    REQUIRE(envelope.sustainTime == 0.0f);
    REQUIRE(envelope.decayTime == 0.0f);

    const impulse::EnvelopeDefinition defaults =
        impulse::EnvelopeDefinition::makeDefault();
    REQUIRE(defaults.decayTime == 0.7f);
    REQUIRE(defaults.sustainTime == 0.2f);
    REQUIRE(defaults.scaleWithImpact);
}

TEST_CASE("Impulse manager combines filters and recycles expired events", "[unit][impulse]")
{
    FakeEntityWorld world;
    impulse::ImpulseManager manager(world);
    addEvent(
        manager,
        std::make_unique<FixedSignal>(math::Vec3{1.0f, 0.0f, 0.0f}),
        {},
        1u,
        1.0f);
    addEvent(
        manager,
        std::make_unique<FixedSignal>(math::Vec3{0.0f, 2.0f, 0.0f}),
        {},
        2u,
        1.0f);

    math::Vec3 pos{};
    math::Quat rot{};
    REQUIRE_FALSE(manager.getImpulseAt({}, false, 4u, pos, rot));
    REQUIRE(manager.getImpulseAt({}, false, 1u, pos, rot));
    REQUIRE(math::approximately(pos, {1.0f, 0.0f, 0.0f}, 1e-4f));

    REQUIRE(manager.getImpulseAt({}, false, 1u | 2u, pos, rot));
    REQUIRE(math::approximately(pos, {1.0f, 2.0f, 0.0f}, 1e-4f));

    REQUIRE(manager.getStrongestImpulseAt({}, false, 1u | 2u, pos, rot));
    REQUIRE(math::approximately(pos, {0.0f, 2.0f, 0.0f}, 1e-4f));

    world.setTime(1.1f);
    REQUIRE_FALSE(manager.getImpulseAt({}, false, 1u | 2u, pos, rot));
    REQUIRE(manager.activeEventCount() == 0);
    REQUIRE(manager.expiredEventCount() == 2);

    std::unique_ptr<ImpulseEvent> recycled = manager.newImpulseEvent();
    REQUIRE(recycled != nullptr);
    REQUIRE(manager.expiredEventCount() == 1);
    REQUIRE(manager.activeEventCount() == 0);
    REQUIRE(recycled->channel == 0u);
}

TEST_CASE("Impulse event distance decay supports all modes", "[unit][impulse]")
{
    impulse::ImpulseEvent event;
    event.radius = 1.0f;
    event.dissipationDistance = 9.0f;

    event.dissipationMode = impulse::ImpulseEvent::DissipationMode::LinearDecay;
    REQUIRE(event.distanceDecay(0.5f) == 1.0f);
    REQUIRE(math::approximately(event.distanceDecay(5.0f), 5.0f / 9.0f, 1e-5f));
    REQUIRE(event.distanceDecay(10.0f) == 0.0f);

    event.dissipationMode = impulse::ImpulseEvent::DissipationMode::SoftDecay;
    REQUIRE(math::approximately(
        event.distanceDecay(5.0f),
        0.5f * (1.0f + std::cos(math::Pi * (4.0f / 9.0f))),
        1e-5f));

    event.dissipationMode = impulse::ImpulseEvent::DissipationMode::ExponentialDecay;
    REQUIRE(math::approximately(
        event.distanceDecay(5.0f),
        1.0f - math::standardDamp(1.0f, 9.0f, 4.0f),
        1e-5f));

    event.customDissipation = 0.5f;
    const float custom = event.distanceDecay(5.0f);
    REQUIRE(custom > 0.0f);
    REQUIRE(custom < 1.0f);
}

TEST_CASE("Impulse propagation delays signals and rotates toward source", "[unit][impulse]")
{
    FakeEntityWorld world;
    impulse::ImpulseManager manager(world);
    ImpulseEvent* event = addEvent(
        manager,
        std::make_unique<FixedSignal>(math::Vec3{0.0f, 1.0f, 0.0f}),
        {},
        1u,
        4.0f);
    event->radius = 0.0f;
    event->dissipationDistance = 100.0f;
    event->propagationSpeed = 10.0f;

    math::Vec3 pos{};
    math::Quat rot{};
    world.setTime(1.0f);
    REQUIRE_FALSE(manager.getImpulseAt({0.0f, 0.0f, 20.0f}, false, 1u, pos, rot));

    world.setTime(3.0f);
    REQUIRE(manager.getImpulseAt({0.0f, 0.0f, 20.0f}, false, 1u, pos, rot));
    REQUIRE(math::approximately(pos.normalized(), {0.0f, 1.0f, 0.0f}, 1e-4f));

    event->directionMode = impulse::ImpulseEvent::DirectionMode::RotateTowardSource;
    world.setTime(3.0f);
    REQUIRE(manager.getImpulseAt({10.0f, 0.0f, 0.0f}, false, 1u, pos, rot));
    REQUIRE(math::approximately(pos.normalized(), {1.0f, 0.0f, 0.0f}, 1e-4f));
}

TEST_CASE("Impulse distance can ignore Z and events can be cancelled", "[unit][impulse]")
{
    FakeEntityWorld world;
    impulse::ImpulseManager manager(world);
    ImpulseEvent* event = addEvent(
        manager,
        std::make_unique<FixedSignal>(math::Vec3{1.0f, 0.0f, 0.0f}),
        {},
        1u,
        4.0f);
    event->radius = 0.0f;
    event->dissipationDistance = 100.0f;
    event->propagationSpeed = 10.0f;

    math::Vec3 pos{};
    math::Quat rot{};
    world.setTime(3.0f);
    REQUIRE(manager.getImpulseAt({0.0f, 0.0f, 50.0f}, true, 1u, pos, rot));
    REQUIRE(math::approximately(pos, {1.0f, 0.0f, 0.0f}, 1e-4f));
    REQUIRE_FALSE(manager.getImpulseAt({0.0f, 0.0f, 50.0f}, false, 1u, pos, rot));

    event->cancel(3.0f, true);
    REQUIRE_FALSE(manager.getImpulseAt({}, true, 1u, pos, rot));

    manager.clear();
    REQUIRE(manager.activeEventCount() == 0);
    REQUIRE(manager.expiredEventCount() == 0);
}

TEST_CASE("Legacy impulse randomization is deterministic", "[unit][impulse]")
{
    FakeEntityWorld world;
    impulse::ImpulseManager manager(world);
    TimeSignal raw;

    impulse::ImpulseDefinition definition;
    definition.impulseType = impulse::ImpulseDefinition::ImpulseType::Legacy;
    definition.rawSignal = &raw;
    definition.randomize = true;
    definition.seed = 42u;
    definition.timeEnvelope = {};
    definition.timeEnvelope.sustainTime = 1.0f;

    definition.seed = 42u;
    ImpulseEvent* first = definition.createAndReturnEvent(manager, {}, {0.0f, -1.0f, 0.0f});
    definition.seed = 42u;
    ImpulseEvent* second = definition.createAndReturnEvent(manager, {}, {0.0f, -1.0f, 0.0f});
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    math::Vec3 firstPos{};
    math::Vec3 secondPos{};
    math::Quat rot{};
    first->signalSource->getSignal(0.0f, firstPos, rot);
    second->signalSource->getSignal(0.0f, secondPos, rot);
    REQUIRE(math::approximately(firstPos, secondPos, 1e-5f));
    REQUIRE(std::abs(firstPos.x) > 0.01f);

    definition.seed = 99u;
    ImpulseEvent* third = definition.createAndReturnEvent(manager, {}, {0.0f, -1.0f, 0.0f});
    math::Vec3 thirdPos{};
    third->signalSource->getSignal(0.0f, thirdPos, rot);
    REQUIRE_FALSE(math::approximately(firstPos, thirdPos, 1e-5f));
}

TEST_CASE("Impulse definition creates modern shaped events", "[unit][impulse]")
{
    FakeEntityWorld world;
    impulse::ImpulseManager manager(world);
    impulse::ImpulseDefinition definition;
    definition.impulseChannel = 2u;
    definition.impulseShape = impulse::ImpulseDefinition::ImpulseShape::Recoil;
    definition.impulseDuration = 0.2f;
    definition.impulseType = impulse::ImpulseDefinition::ImpulseType::Uniform;
    definition.dissipationDistance = 10.0f;
    definition.dissipationRate = 0.25f;

    ImpulseEvent* event = definition.createAndReturnEvent(
        manager,
        {1.0f, 2.0f, 3.0f},
        {0.0f, -2.0f, 0.0f});
    REQUIRE(event != nullptr);
    REQUIRE(manager.activeEventCount() == 1);
    REQUIRE(event->channel == 2u);
    REQUIRE(event->position.x == 1.0f);
    REQUIRE(math::approximately(event->envelope.sustainTime, 0.2f, 1e-5f));
    REQUIRE(event->radius > 1000.0f);
    REQUIRE(event->dissipationDistance == 0.0f);
    REQUIRE(event->propagationSpeed > 1000.0f);
    REQUIRE(math::approximately(event->customDissipation, 0.25f, 1e-6f));

    math::Vec3 pos{};
    math::Quat rot{};
    event->signalSource->getSignal(0.0f, pos, rot);
    REQUIRE(math::approximately(pos, {0.0f, -2.0f, 0.0f}, 1e-4f));
    REQUIRE(math::approximately(rot, math::Quat::identity(), 1e-4f));

    definition.impulseType = impulse::ImpulseDefinition::ImpulseType::Propagating;
    definition.impulseShape = impulse::ImpulseDefinition::ImpulseShape::Bump;
    definition.propagationSpeed = 12.0f;
    ImpulseEvent* propagating = definition.createAndReturnEvent(manager, {}, {});
    REQUIRE(propagating != nullptr);
    REQUIRE(propagating->propagationSpeed == 12.0f);

    definition.impulseShape = impulse::ImpulseDefinition::ImpulseShape::Custom;
    definition.customImpulseShape = {};
    REQUIRE(definition.createAndReturnEvent(manager, {}, {}) == nullptr);
    definition.impulseDuration = 0.0f;
    REQUIRE(definition.createAndReturnEvent(manager, {}, {}) == nullptr);
}

TEST_CASE("Impulse definition supports legacy raw signals", "[unit][impulse]")
{
    FakeEntityWorld world;
    impulse::ImpulseManager manager(world);
    FixedSignal raw(
        math::Vec3{0.0f, 1.0f, 0.0f},
        math::Quat::identity(),
        2.0f);

    impulse::ImpulseDefinition definition;
    definition.impulseType = impulse::ImpulseDefinition::ImpulseType::Legacy;
    definition.rawSignal = &raw;
    definition.impulseChannel = 3u;
    definition.amplitudeGain = 2.0f;
    definition.frequencyGain = 1.0f;
    definition.randomize = false;
    definition.timeEnvelope = {};
    definition.timeEnvelope.sustainTime = 2.0f;
    definition.timeEnvelope.scaleWithImpact = true;
    definition.timeEnvelope.decayTime = 0.5f;
    definition.impactRadius = 4.0f;
    definition.dissipationDistance = 20.0f;
    definition.propagationSpeed = 5.0f;

    ImpulseEvent* event = definition.createAndReturnEvent(
        manager,
        {2.0f, 0.0f, 0.0f},
        {0.0f, -3.0f, 0.0f});
    REQUIRE(event != nullptr);
    REQUIRE(event->channel == 3u);
    REQUIRE(event->radius == 4.0f);
    REQUIRE(event->dissipationDistance == 20.0f);
    REQUIRE(event->propagationSpeed == 5.0f);
    REQUIRE(math::approximately(
        event->envelope.decayTime,
        0.5f * std::sqrt(3.0f),
        1e-5f));

    math::Vec3 pos{};
    math::Quat rot{};
    event->signalSource->getSignal(0.0f, pos, rot);
    REQUIRE(math::approximately(pos, {0.0f, 6.0f, 0.0f}, 1e-4f));

    definition.rawSignal = nullptr;
    REQUIRE(definition.createAndReturnEvent(manager, {}, {}) == nullptr);
}

TEST_CASE("Impulse source applies force position and default velocity", "[unit][impulse]")
{
    FakeEntityWorld world;
    impulse::ImpulseManager manager(world);
    impulse::ImpulseSource source(&manager);
    source.impulseDefinition.impulseShape =
        impulse::ImpulseDefinition::ImpulseShape::Recoil;
    source.impulseDefinition.impulseType =
        impulse::ImpulseDefinition::ImpulseType::Uniform;
    source.impulseDefinition.impulseDuration = 1.0f;
    source.position = {1.0f, 2.0f, 3.0f};
    source.defaultVelocity = {0.0f, -1.0f, 0.0f};

    ImpulseEvent* event = source.generateImpulseWithForce(2.0f);
    REQUIRE(event != nullptr);
    REQUIRE(manager.activeEventCount() == 1);

    math::Vec3 pos{};
    math::Quat rot{};
    REQUIRE(manager.getImpulseAt(source.position, false, 1u, pos, rot));
    REQUIRE(math::approximately(pos, {0.0f, -2.0f, 0.0f}, 1e-4f));

    source.impulseManager = nullptr;
    REQUIRE(source.generateImpulse() == nullptr);
}

TEST_CASE("Impulse listener applies gain stage and camera space", "[unit][impulse]")
{
    FakeEntityWorld world;
    world.add(1, {{}, yaw(90.0f), math::Vec3::one});
    CameraSystem system(world);
    const CameraId cameraId = system.createCamera<VirtualCamera>("impulse");
    VirtualCamera* camera = system.camera(cameraId);
    camera->ownerEntity = 1;

    auto ownedListener =
        std::make_unique<impulse::ImpulseListener>(&system.impulseManager());
    auto* listener = ownedListener.get();
    camera->addExtension(std::move(ownedListener));
    listener->gain = 0.5f;

    impulse::ImpulseSource source(&system.impulseManager());
    source.impulseDefinition.impulseShape =
        impulse::ImpulseDefinition::ImpulseShape::Recoil;
    source.impulseDefinition.impulseType =
        impulse::ImpulseDefinition::ImpulseType::Uniform;
    source.impulseDefinition.impulseDuration = 1.0f;
    source.defaultVelocity = {0.0f, 0.0f, -1.0f};

    source.generateImpulse();
    camera->updateCameraState(math::Vec3::up, 1.0f / 60.0f);
    REQUIRE(math::approximately(camera->state().positionCorrection, {-0.5f, 0.0f, 0.0f}, 1e-4f));

    listener->useCameraSpace = false;
    camera->updateCameraState(math::Vec3::up, 1.0f / 60.0f);
    REQUIRE(math::approximately(camera->state().positionCorrection, {0.0f, 0.0f, -0.5f}, 1e-4f));

    listener->applyAfter = Stage::Aim;
    camera->updateCameraState(math::Vec3::up, 1.0f / 60.0f);
    REQUIRE(math::approximately(camera->state().positionCorrection, {0.0f, 0.0f, -0.5f}, 1e-4f));
}

TEST_CASE("Impulse listener reaction samples secondary noise", "[unit][impulse]")
{
    impulse::ImpulseListener::ImpulseReaction reaction;
    reaction.secondaryNoise.emplace();
    NoiseProfile::TransformParams position;
    position.x = {0.0f, 2.0f, true};
    reaction.secondaryNoise->positionNoise.push_back(position);
    reaction.noiseOffsets = {1.0f, 2.0f, 3.0f};
    reaction.duration = 1.0f;

    math::Vec3 pos{};
    math::Quat rot{};
    REQUIRE(reaction.getReaction(0.0f, 1.0f / 60.0f, {0.0f, 1.0f, 0.0f}, pos, rot));
    REQUIRE(math::approximately(pos, {1.0f, 0.0f, 0.0f}, 1e-4f));
    REQUIRE(math::approximately(rot, math::Quat::identity(), 1e-4f));

    REQUIRE(reaction.getReaction(0.1f, 0.01f, {}, pos, rot));
}

TEST_CASE("CameraSystem owns an impulse manager tied to world time", "[unit][impulse]")
{
    FakeEntityWorld world;
    world.setTime(4.0f);
    CameraSystem system(world);
    REQUIRE(system.impulseManager().currentTime() == 4.0f);

    world.setTime(5.0f);
    REQUIRE(system.impulseManager().currentTime() == 5.0f);
}

