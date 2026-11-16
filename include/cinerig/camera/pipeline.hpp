#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include <cinerig/camera/camera_component.hpp>

namespace cinerig
{

class VirtualCamera;

class Pipeline
{
public:
    Pipeline() = default;
    explicit Pipeline(VirtualCamera* owner) noexcept;

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) noexcept;
    Pipeline& operator=(Pipeline&&) noexcept;

    void setOwner(VirtualCamera* owner) noexcept;
    VirtualCamera* owner() const noexcept;

    CameraComponent* set(Stage stage, std::unique_ptr<CameraComponent> component);
    CameraComponent* component(Stage stage) const noexcept;
    CameraComponent* component(int stageIndex) const noexcept;

    void prePipelineMutate(CameraState& state, float deltaTime);
    void clear() noexcept;

    bool empty() const noexcept;
    std::size_t size() const noexcept;

private:
    friend class VirtualCamera;

    std::array<std::unique_ptr<CameraComponent>, kStageCount> components_{};
    VirtualCamera* owner_ = nullptr;
};

} // namespace cinerig
