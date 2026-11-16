#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig::testing
{

// VirtualCamera test double that records update calls and optional global
// execution order. It deliberately adds no pipeline behavior.
class TestCamera final : public VirtualCamera
{
public:
    explicit TestCamera(
        std::string cameraName,
        std::vector<std::string>* executionOrder = nullptr)
        : VirtualCamera(std::move(cameraName)), order(executionOrder)
    {
    }

    void internalUpdateCameraState(math::Vec3 worldUp, float deltaTime) override
    {
        ++updateCount;
        lastDeltaTime = deltaTime;
        lastWorldUp = worldUp;
        if (order != nullptr)
            order->emplace_back(name());
        VirtualCamera::internalUpdateCameraState(worldUp, deltaTime);
    }

    int updateCount = 0;
    float lastDeltaTime = 0.0f;
    math::Vec3 lastWorldUp{};
    std::vector<std::string>* order = nullptr;
};

}
