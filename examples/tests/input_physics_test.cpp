#include <catch2/catch_all.hpp>

#include <SDL3/SDL_events.h>

#include "sdl_input.hpp"
#include "sdl_physics.hpp"

using namespace cinerig;
using namespace cinerig::examples;

namespace
{

SDL_Event keyEvent(SDL_EventType type, SDL_Scancode scancode)
{
    SDL_Event event{};
    event.type = type;
    event.key.scancode = scancode;
    event.key.down = type == SDL_EVENT_KEY_DOWN;
    return event;
}

}

TEST_CASE("SdlInput maps keyboard axes", "[examples][input]")
{
    SdlInput input;
    REQUIRE(input.axis("Horizontal") == 0.0f);
    input.handleEvent(keyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_D));
    REQUIRE(input.axis("Horizontal") == Catch::Approx(1.0f));
    input.handleEvent(keyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_LEFT));
    REQUIRE(input.axis("Horizontal") == Catch::Approx(0.0f));
    input.handleEvent(keyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_D));
    input.handleEvent(keyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_LEFT));

    input.handleEvent(keyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W));
    REQUIRE(input.axis("Vertical") == Catch::Approx(1.0f));
    REQUIRE(input.axis2D("Move").y == Catch::Approx(1.0f));
    input.handleEvent(keyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_W));
    REQUIRE(input.axis("Vertical") == 0.0f);
}

TEST_CASE("SdlInput captures relative mouse and scroll", "[examples][input]")
{
    SdlInput input;
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.xrel = 4.0f;
    motion.motion.yrel = -2.0f;
    input.handleEvent(motion);
    REQUIRE(input.axis("Mouse X") == 0.0f);

    input.setMouseCaptured(true);
    input.handleEvent(motion);
    REQUIRE(input.axis("Mouse X") == Catch::Approx(4.0f));
    REQUIRE(input.axis("Mouse Y") == Catch::Approx(-2.0f));

    SDL_Event wheel{};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.y = 2.0f;
    input.handleEvent(wheel);
    REQUIRE(input.axis("Scroll") == Catch::Approx(2.0f));

    input.newFrame();
    REQUIRE(input.axis("Mouse X") == 0.0f);
    REQUIRE(input.axis("Scroll") == 0.0f);

    input.handleEvent(keyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_ESCAPE));
    REQUIRE_FALSE(input.mouseCaptured());
}

TEST_CASE("SdlPhysics raycasts boxes and spheres", "[examples][physics]")
{
    SdlPhysics physics;
    physics.addBox(1, {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    physics.addSphere(2, {{3.0f, 0.0f, 0.0f}, 1.0f});

    QueryFilter filter;
    const auto boxHit = physics.raycast(
        {{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}},
        20.0f,
        filter);
    REQUIRE(boxHit.has_value());
    REQUIRE(boxHit->entity == 1);
    REQUIRE(boxHit->distance == Catch::Approx(4.0f));
    REQUIRE(boxHit->point.z == Catch::Approx(1.0f));
    REQUIRE(boxHit->normal.z == Catch::Approx(1.0f));

    const auto sphereHit = physics.raycast(
        {{3.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}},
        20.0f,
        filter);
    REQUIRE(sphereHit.has_value());
    REQUIRE(sphereHit->entity == 2);
    REQUIRE(sphereHit->distance == Catch::Approx(4.0f));

    const auto sphereCast = physics.sphereCast(
        {{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}},
        0.5f,
        20.0f,
        filter);
    REQUIRE(sphereCast.has_value());
    REQUIRE(sphereCast->radius == Catch::Approx(0.5f));

    const auto overlaps = physics.overlapSphere({0.0f, 0.0f, 1.4f}, 0.5f, filter);
    REQUIRE_FALSE(overlaps.empty());
    REQUIRE(physics.bounds(1).has_value());
    REQUIRE(physics.bounds(2)->extents.x == Catch::Approx(1.0f));
    REQUIRE(physics.closestPoint(1, {3.0f, 0.5f, 0.0f}).has_value());
    REQUIRE(physics.closestPoint(1, {3.0f, 0.5f, 0.0f})->x == Catch::Approx(1.0f));
    REQUIRE(physics.closestPoint(2, {5.0f, 0.0f, 0.0f})->x == Catch::Approx(4.0f));
    REQUIRE(physics.closestPoint(2, {3.0f, 0.0f, 0.0f})->x == Catch::Approx(3.0f));

    QueryFilter layerFilter;
    layerFilter.layers.bits = 1u << 1;
    REQUIRE_FALSE(physics.raycast(
        {{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}},
        20.0f,
        layerFilter).has_value());
}

