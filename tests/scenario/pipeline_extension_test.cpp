#include "fake_entity_world.hpp"
#include "pipeline_test_components.hpp"

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

TEST_CASE("Extensions run before components and after every stage", "[scenario][pipeline][extension]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    VirtualCamera& camera = makeCamera(system, "extensions");
    std::vector<std::string> log;

    camera.setBody<RecordingComponent>(Stage::Body, "body", &log);
    camera.setAim<RecordingComponent>(Stage::Aim, "aim", &log);

    auto first = std::make_unique<RecordingExtension>("first", &log);
    auto second = std::make_unique<RecordingExtension>("second", &log);
    RecordingExtension* firstPtr = first.get();
    RecordingExtension* secondPtr = second.get();
    second->setEnabled(false);

    camera.addExtension(std::move(first));
    camera.addExtension(std::move(second));

    REQUIRE(camera.extensionCount() == 2);
    REQUIRE(firstPtr->isAttached());
    REQUIRE(firstPtr->virtualCamera() == &camera);
    REQUIRE_FALSE(secondPtr->isEnabled());

    camera.internalUpdateCameraState(math::Vec3::up, 1.0f / 60.0f);

    const std::vector<std::string> expected{
        "ext-pre:first",
        "pre:body",
        "pre:aim",
        "mutate:body",
        "ext-post:first:Body",
        "mutate:aim",
        "ext-post:first:Aim",
        "ext-post:first:Noise",
        "ext-post:first:Finalize",
    };
    REQUIRE(log == expected);
    REQUIRE(firstPtr->preCount == 1);
    REQUIRE(firstPtr->postCount == 4);
    REQUIRE(secondPtr->preCount == 0);
    REQUIRE(secondPtr->postCount == 0);
}

TEST_CASE("Parent camera extensions receive child pipeline callbacks", "[scenario][pipeline][extension]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    VirtualCamera& parent = makeCamera(system, "parent");
    VirtualCamera& child = makeCamera(system, "child");
    child.setParent(&parent);

    std::vector<std::string> log;
    auto childExtension = std::make_unique<RecordingExtension>("child", &log);
    auto parentExtension = std::make_unique<RecordingExtension>("parent", &log);
    RecordingExtension* childPtr = childExtension.get();
    RecordingExtension* parentPtr = parentExtension.get();
    child.addExtension(std::move(childExtension));
    parent.addExtension(std::move(parentExtension));

    child.internalUpdateCameraState(math::Vec3::up, 1.0f / 60.0f);

    const std::vector<std::string> expected{
        "ext-pre:child",
        "ext-pre:parent",
        "ext-post:child:Body",
        "ext-post:parent:Body",
        "ext-post:child:Aim",
        "ext-post:parent:Aim",
        "ext-post:child:Noise",
        "ext-post:parent:Noise",
        "ext-post:child:Finalize",
        "ext-post:parent:Finalize",
    };
    REQUIRE(log == expected);
    REQUIRE(childPtr->callbackCamera == &child);
    REQUIRE(parentPtr->callbackCamera == &child);
}

TEST_CASE("Extensions detach cleanly and report damping", "[scenario][pipeline][extension]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    VirtualCamera& camera = makeCamera(system, "detach");

    RecordingComponent& body = camera.setBody<RecordingComponent>(Stage::Body, "body");
    body.dampTime = 0.5f;

    auto extension = std::make_unique<RecordingExtension>("damped");
    extension->dampTime = 1.25f;
    RecordingExtension* extensionPtr = extension.get();
    camera.addExtension(std::move(extension));

    REQUIRE(camera.maxDampTime() == 1.25f);
    REQUIRE(extensionPtr->virtualCamera() == &camera);

    camera.removeExtension(extensionPtr);
    REQUIRE(camera.extensionCount() == 0);
    REQUIRE(camera.maxDampTime() == 0.5f);
}

TEST_CASE("Transition notifications reach components and extensions", "[scenario][pipeline][extension]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    VirtualCamera& camera = makeCamera(system, "transition");
    std::vector<std::string> log;

    RecordingComponent& body = camera.setBody<RecordingComponent>(Stage::Body, "body", &log);
    body.transitionResult = true;
    RecordingComponent* bodyPtr = &body;
    auto extension = std::make_unique<RecordingExtension>("ext", &log, true);
    RecordingExtension* extensionPtr = extension.get();
    camera.addExtension(std::move(extension));

    camera.onTransitionFromCamera(nullptr, math::Vec3::up, 1.0f / 60.0f);

    REQUIRE(bodyPtr->transitionCount == 1);
    REQUIRE(extensionPtr->transitionCount == 1);
    REQUIRE(camera.previousStateIsValid);
    REQUIRE(log.front() == "ext-transition:ext");
    REQUIRE(log[1] == "transition:body");
}
