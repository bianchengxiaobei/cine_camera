#include <cinerig/camera/pipeline.hpp>

#include <utility>

namespace cinerig
{

Pipeline::Pipeline(VirtualCamera* owner) noexcept
    : owner_(owner)
{
}

Pipeline::Pipeline(Pipeline&& other) noexcept
    : components_(std::move(other.components_)), owner_(other.owner_)
{
    other.owner_ = nullptr;
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept
{
    if (this != &other)
    {
        clear();
        components_ = std::move(other.components_);
        owner_ = other.owner_;
        other.owner_ = nullptr;
    }
    return *this;
}

void Pipeline::setOwner(VirtualCamera* owner) noexcept
{
    owner_ = owner;
    for (auto& component : components_)
    {
        if (component)
            component->setVirtualCamera(owner);
    }
}

VirtualCamera* Pipeline::owner() const noexcept
{
    return owner_;
}

CameraComponent* Pipeline::set(Stage stage, std::unique_ptr<CameraComponent> component)
{
    if (component == nullptr)
        return nullptr;

    const auto index = static_cast<std::size_t>(stage);
    if (index >= components_.size())
        return nullptr;

    if (components_[index])
        components_[index]->setVirtualCamera(nullptr);
    component->setVirtualCamera(owner_);
    CameraComponent* raw = component.get();
    components_[index] = std::move(component);
    return raw;
}

CameraComponent* Pipeline::component(Stage stage) const noexcept
{
    return component(static_cast<int>(stage));
}

CameraComponent* Pipeline::component(int stageIndex) const noexcept
{
    if (stageIndex < 0 || stageIndex >= kStageCount)
        return nullptr;
    return components_[static_cast<std::size_t>(stageIndex)].get();
}

void Pipeline::prePipelineMutate(CameraState& state, float deltaTime)
{
    for (auto& component : components_)
    {
        if (component && component->isValid())
            component->prePipelineMutate(state, deltaTime);
    }
}

void Pipeline::clear() noexcept
{
    for (auto& component : components_)
    {
        if (component)
            component->setVirtualCamera(nullptr);
        component.reset();
    }
}

bool Pipeline::empty() const noexcept
{
    return size() == 0;
}

std::size_t Pipeline::size() const noexcept
{
    std::size_t count = 0;
    for (const auto& component : components_)
    {
        if (component)
            ++count;
    }
    return count;
}

} // namespace cinerig
