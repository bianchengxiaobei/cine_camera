#include "fake_entity_world.hpp"

#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/impulse/collision_impulse_source.hpp>
#include <cinerig/math/curve.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct Fixture
{
    FakeEntityWorld world;
    impulse::ImpulseManager manager{world};
    impulse::CollisionImpulseSource source{&manager};

    Fixture()
    {
        source.position = {2.0f, 3.0f, 4.0f};
        source.defaultVelocity = {0.0f, -1.0f, 0.0f};
        source.impulseDefinition.impulseType =
            impulse::ImpulseDefinition::ImpulseType::Uniform;
        source.impulseDefinition.impulseShape =
            impulse::ImpulseDefinition::ImpulseShape::Custom;
        source.impulseDefinition.customImpulseShape = math::Curve({
            {0.0f, 1.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 0.0f, 0.0f},
        });
        source.impulseDefinition.impulseDuration = 1.0f;
        source.impulseDefinition.dissipationDistance = 100.0f;
    }

    math::Vec3 eventVelocity(const impulse::ImpulseEvent& event) const
    {
        math::Vec3 position;
        math::Quat rotation;
        event.signalSource->getSignal(0.0f, position, rotation);
        return position;
    }
};

} // namespace

TEST_CASE("Collision impulse source filters layers and tags", "[unit][impulse-collision]")
{
    Fixture fixture;

    impulse::CollisionImpulseSource::ImpactInfo impact;
    impact.layer = 1u;
    REQUIRE(fixture.source.onCollision(impact) == nullptr);

    impact.layer = 0u;
    impact.tag = "Player";
    fixture.source.ignoreTag = "Player";
    REQUIRE(fixture.source.onCollision(impact) == nullptr);

    impact.tag = "player";
    impulse::ImpulseEvent* event = fixture.source.onCollision(impact);
    REQUIRE(event != nullptr);
    REQUIRE(math::approximately(event->position, fixture.source.position));

    fixture.manager.clear();
    impact.hasOther = false;
    impact.layer = 31u;
    impact.tag = "Player";
    REQUIRE(fixture.source.onCollision(impact) != nullptr);

    fixture.manager.clear();
    fixture.source.ignoreTag.clear();
    fixture.source.layerMask = LayerMask{0b101u};
    impact.hasOther = true;
    impact.layer = 2u;
    REQUIRE(fixture.source.onCollision(impact) != nullptr);
    impact.layer = 1u;
    REQUIRE(fixture.source.onCollision(impact) == nullptr);
}

TEST_CASE("Collision impulse source derives direction from impact", "[unit][impulse-collision]")
{
    Fixture fixture;
    fixture.source.defaultVelocity = {0.0f, -2.0f, 0.0f};
    fixture.source.useImpactDirection = true;

    impulse::CollisionImpulseSource::ImpactInfo impact;
    impact.relativeVelocity = {0.0f, 0.0f, -12.0f};
    impulse::ImpulseEvent* event = fixture.source.onCollision(impact);
    REQUIRE(event != nullptr);
    REQUIRE(math::approximately(
        fixture.eventVelocity(*event),
        math::Vec3{0.0f, 0.0f, 2.0f}));

    fixture.manager.clear();
    impact.relativeVelocity = {};
    event = fixture.source.onCollision(impact);
    REQUIRE(event != nullptr);
    REQUIRE(math::approximately(
        fixture.eventVelocity(*event),
        math::Vec3{0.0f, -2.0f, 0.0f}));
}

TEST_CASE("Collision impulse source scales impact with mass", "[unit][impulse-collision]")
{
    Fixture fixture;
    fixture.source.scaleImpactWithMass = true;
    fixture.source.hasRigidbody = true;
    fixture.source.rigidbodyMass = 2.0f;

    impulse::CollisionImpulseSource::ImpactInfo impact;
    impact.relativeVelocity = {3.0f, 0.0f, 0.0f};
    impact.otherHasRigidbody = true;
    impact.otherMass = 3.0f;
    impulse::ImpulseEvent* event = fixture.source.onCollision(impact);
    REQUIRE(event != nullptr);
    REQUIRE(math::approximately(
        fixture.eventVelocity(*event),
        math::Vec3{0.0f, -6.0f, 0.0f}));

    fixture.manager.clear();
    impact.otherHasRigidbody = false;
    event = fixture.source.onCollision(impact);
    REQUIRE(event != nullptr);
    REQUIRE(math::approximately(
        fixture.eventVelocity(*event),
        math::Vec3{0.0f, -2.0f, 0.0f}));
}

TEST_CASE("Collision impulse source scales speed and combines trigger velocities", "[unit][impulse-collision]")
{
    Fixture fixture;
    fixture.source.useImpactDirection = true;
    fixture.source.scaleImpactWithSpeed = true;
    fixture.source.hasRigidbody = true;
    fixture.source.rigidbodyVelocity = {0.0f, 0.0f, 5.0f};

    impulse::CollisionImpulseSource::ImpactInfo impact;
    impact.relativeVelocity = {9.0f, 9.0f, 9.0f};
    impact.otherHasRigidbody = true;
    impact.otherVelocity = {0.0f, 0.0f, 1.0f};
    impulse::ImpulseEvent* event = fixture.source.onTrigger(impact);
    REQUIRE(event != nullptr);
    REQUIRE(math::approximately(
        fixture.eventVelocity(*event),
        math::Vec3{0.0f, 0.0f, 2.0f}));

    fixture.manager.clear();
    impact.relativeVelocity = {0.0f, 0.0f, -9.0f};
    event = fixture.source.onCollision(impact);
    REQUIRE(event != nullptr);
    REQUIRE(math::approximately(
        fixture.eventVelocity(*event),
        math::Vec3{0.0f, 0.0f, 3.0f}));
}

TEST_CASE("Collision impulse source handles disabled and missing data", "[unit][impulse-collision]")
{
    Fixture fixture;
    fixture.source.enabled = false;
    REQUIRE(fixture.source.onCollision({}) == nullptr);

    fixture.source.enabled = true;
    fixture.source.impulseManager = nullptr;
    REQUIRE(fixture.source.onCollision({}) == nullptr);

    fixture.source.impulseManager = &fixture.manager;
    impulse::CollisionImpulseSource::ImpactInfo impact;
    impact.hasOther = false;
    impact.layer = 31u;
    impact.otherHasRigidbody = true;
    impact.otherMass = 50.0f;
    fixture.source.scaleImpactWithMass = true;
    impulse::ImpulseEvent* event = fixture.source.onCollision(impact);
    REQUIRE(event != nullptr);
    REQUIRE(math::approximately(
        fixture.eventVelocity(*event),
        math::Vec3{0.0f, -1.0f, 0.0f}));
}

TEST_CASE("Collision impulse source reset restores defaults", "[unit][impulse-collision]")
{
    Fixture fixture;
    fixture.source.layerMask = LayerMask{0b100u};
    fixture.source.ignoreTag = "Player";
    fixture.source.useImpactDirection = true;
    fixture.source.scaleImpactWithMass = true;
    fixture.source.scaleImpactWithSpeed = true;
    fixture.source.enabled = false;
    fixture.source.hasRigidbody = true;
    fixture.source.rigidbodyMass = 4.0f;
    fixture.source.rigidbodyVelocity = {1.0f, 2.0f, 3.0f};

    fixture.source.reset();

    REQUIRE(fixture.source.layerMask.bits == 1u);
    REQUIRE(fixture.source.ignoreTag.empty());
    REQUIRE_FALSE(fixture.source.useImpactDirection);
    REQUIRE_FALSE(fixture.source.scaleImpactWithMass);
    REQUIRE_FALSE(fixture.source.scaleImpactWithSpeed);
    REQUIRE(fixture.source.enabled);
    REQUIRE_FALSE(fixture.source.hasRigidbody);
    REQUIRE(fixture.source.rigidbodyMass == 1.0f);
    REQUIRE(math::approximately(fixture.source.rigidbodyVelocity, math::Vec3::zero));
    REQUIRE(math::approximately(fixture.source.defaultVelocity, math::Vec3::down));
    REQUIRE(fixture.source.impulseDefinition.impulseShape
        == impulse::ImpulseDefinition::ImpulseShape::Bump);
}
