#pragma once

#include <cstdint>
#include <string>

#include "fake_camera_output.hpp"
#include "fake_entity_world.hpp"

#include <cinerig/brain/camera_brain.hpp>

namespace cinerig::testing
{

struct BrainTestRig
{
    FakeEntityWorld world;
    CameraSystem system;
    FakeCameraOutput output;

    explicit BrainTestRig(const BrainConfig& config = BrainConfig{})
        : system(world)
    {
        system.setUnitTestMode(true);
        brainId = system.createBrain(output, config);
        brain = system.brain(brainId);
    }

    CameraId addCamera(const char* name, int priority, float x)
    {
        const EntityId entity = nextEntity_++;
        world.add(entity, Transform{{x, 0.0f, 0.0f}, math::Quat::identity()});
        const CameraId id = system.createCamera<VirtualCamera>(name);
        VirtualCamera* camera = system.camera(id);
        camera->priority = PrioritySettings(priority);
        camera->ownerEntity = entity;
        return id;
    }

    VirtualCamera& camera(CameraId id)
    {
        return *system.camera(id);
    }

    const VirtualCamera& camera(CameraId id) const
    {
        return *system.camera(id);
    }

    void step(float deltaTime)
    {
        ++frame;
        world.setFrame(UpdateClock::Late, frame);
        brain->manualUpdate(deltaTime, frame);
    }

    BrainId brainId = 0;
    CameraBrain* brain = nullptr;
    std::uint64_t frame = 0;

private:
    EntityId nextEntity_ = 100;
};

} // namespace cinerig::testing
