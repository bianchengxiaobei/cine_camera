#include <catch2/catch_all.hpp>

#include "sdl_world.hpp"

using namespace cinerig;
using namespace cinerig::examples;

TEST_CASE("SdlWorld creates and updates entities", "[examples][world]")
{
    SdlWorld world;
    const EntityId entity = world.createEntity({{1.0f, 2.0f, 3.0f}, math::Quat::identity(), math::Vec3::one});
    REQUIRE(entity != 0);
    REQUIRE(world.exists(entity));
    REQUIRE(world.entityCount() == 1);
    REQUIRE(math::approximately(world.transform(entity).position, math::Vec3(1.0f, 2.0f, 3.0f)));

    Transform moved;
    moved.position = {4.0f, 5.0f, 6.0f};
    world.setTransform(entity, moved);
    REQUIRE(math::approximately(world.transform(entity).position, math::Vec3(4.0f, 5.0f, 6.0f)));

    REQUIRE_FALSE(world.trySetTransform(999, moved));
    REQUIRE_FALSE(world.exists(999));
    REQUIRE(math::approximately(world.transform(999).position, math::Vec3::zero));

    world.destroyEntity(entity);
    REQUIRE_FALSE(world.exists(entity));
    REQUIRE(world.entityCount() == 0);
}

TEST_CASE("SdlWorld tracks clocks, frames and viewport", "[examples][world]")
{
    SdlWorld world;
    world.setDeltaTime(UpdateClock::Fixed, 1.0f / 50.0f);
    world.setDeltaTime(UpdateClock::Late, 0.02f);
    REQUIRE(world.fixedDeltaTime() == Catch::Approx(1.0f / 50.0f));
    REQUIRE(world.deltaTime(UpdateClock::Fixed) == Catch::Approx(1.0f / 50.0f));
    REQUIRE(world.deltaTime(UpdateClock::Late) == Catch::Approx(0.02f));

    world.setFrame(UpdateClock::Fixed, 7);
    world.setFrame(UpdateClock::Late, 11);
    world.advanceFrame(UpdateClock::Fixed);
    world.advanceFrame(UpdateClock::Late);
    REQUIRE(world.frame(UpdateClock::Fixed) == 8);
    REQUIRE(world.frame(UpdateClock::Late) == 12);

    world.setTime(3.5f);
    world.advanceTime(0.5f);
    REQUIRE(world.time() == Catch::Approx(4.0f));

    world.setWorldUp({0.0f, 2.0f, 0.0f});
    REQUIRE(world.worldUp().y == Catch::Approx(1.0f));
    world.setViewportSize({800.0f, 600.0f});
    REQUIRE(world.viewportSize().x == Catch::Approx(800.0f));
    REQUIRE(world.viewportSize().y == Catch::Approx(600.0f));
}

TEST_CASE("SdlWorld records the current update clock for SmartUpdate", "[examples][world]")
{
    SdlWorld world;
    const EntityId entity = world.createEntity();
    REQUIRE(world.lastChangeClock(entity) == UpdateClock::Late);

    world.setCurrentClock(UpdateClock::Fixed);
    world.setTransform(entity, {{1.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    REQUIRE(world.lastChangeClock(entity) == UpdateClock::Fixed);

    world.setDeltaTime(UpdateClock::Late, 0.016f);
    world.setTransform(entity, {{2.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    REQUIRE(world.lastChangeClock(entity) == UpdateClock::Late);
}

