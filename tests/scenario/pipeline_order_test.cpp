#include "fake_entity_world.hpp"
#include "pipeline_test_components.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

VirtualCamera& makeCamera(CameraSystem& system, const char* name)
{
    const CameraId id = system.createCamera<VirtualCamera>(name);
    return *system.camera(id);
}

} // namespace

TEST_CASE("Component pipeline runs stages in Body Aim Noise Finalize order", "[scenario][pipeline][order]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    VirtualCamera& camera = makeCamera(system, "pipeline");

    std::vector<std::string> log;
    camera.setBody<RecordingComponent>(Stage::Body, "body", &log);
    camera.setAim<RecordingComponent>(Stage::Aim, "aim", &log);
    camera.setNoise<RecordingComponent>(Stage::Noise, "noise", &log);
    camera.setFinalize<RecordingComponent>(Stage::Finalize, "finalize", &log);

    REQUIRE(camera.pipeline().size() == 4);
    REQUIRE(camera.component(Stage::Body) != nullptr);
    REQUIRE(camera.component(Stage::Aim) != nullptr);
    REQUIRE(camera.component(Stage::Noise) != nullptr);
    REQUIRE(camera.component(Stage::Finalize) != nullptr);

    camera.internalUpdateCameraState(math::Vec3::up, 1.0f / 60.0f);

    const std::vector<std::string> expected{
        "pre:body",
        "pre:aim",
        "pre:noise",
        "pre:finalize",
        "mutate:body",
        "mutate:aim",
        "mutate:noise",
        "mutate:finalize",
    };
    REQUIRE(log == expected);
}

TEST_CASE("Post-aim Body runs after Aim and invokes the Body callback twice", "[scenario][pipeline][order]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    VirtualCamera& camera = makeCamera(system, "post-aim");

    std::vector<std::string> log;
    camera.setBody<RecordingComponent>(Stage::Body, "body", &log, true);
    camera.setAim<RecordingComponent>(Stage::Aim, "aim", &log);
    camera.addExtension(std::make_unique<RecordingExtension>("ext", &log));

    camera.internalUpdateCameraState(math::Vec3::up, 1.0f / 60.0f);

    const std::vector<std::string> expected{
        "ext-pre:ext",
        "pre:body",
        "pre:aim",
        "ext-post:ext:Body",
        "mutate:aim",
        "ext-post:ext:Aim",
        "mutate:body",
        "ext-post:ext:Body",
        "ext-post:ext:Noise",
        "ext-post:ext:Finalize",
    };
    REQUIRE(log == expected);

    const auto aimIt = std::find(log.begin(), log.end(), "mutate:aim");
    const auto bodyIt = std::find(log.begin(), log.end(), "mutate:body");
    REQUIRE(aimIt != log.end());
    REQUIRE(bodyIt != log.end());
    REQUIRE(aimIt < bodyIt);
    REQUIRE(std::count(log.begin(), log.end(), "ext-post:ext:Body") == 2);
}

TEST_CASE("Pipeline keeps one component per stage and replaces duplicates", "[scenario][pipeline][order]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    VirtualCamera& camera = makeCamera(system, "replace");

    std::vector<std::string> log;
    camera.setBody<RecordingComponent>(Stage::Body, "first", &log);
    camera.setBody<RecordingComponent>(Stage::Body, "second", &log);

    REQUIRE(camera.pipeline().size() == 1);
    REQUIRE(camera.pipeline().component(Stage::Body) != nullptr);

    camera.internalUpdateCameraState(math::Vec3::up, 1.0f / 60.0f);

    REQUIRE(log == std::vector<std::string>{"pre:second", "mutate:second"});
}
