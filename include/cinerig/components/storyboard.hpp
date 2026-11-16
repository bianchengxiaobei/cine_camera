#pragma once

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

enum class StoryboardFillStrategy
{
    BestFit,
    CropImageToFit,
    StretchToFit,
};

struct StoryboardImage
{
    std::shared_ptr<const void> data;
    std::type_index type = std::type_index(typeid(void));
    math::Vec2 size{};

    StoryboardImage() = default;

    StoryboardImage(
        std::shared_ptr<const void> value,
        std::type_index valueType,
        math::Vec2 pixelSize)
        : data(std::move(value)), type(valueType), size(pixelSize)
    {
    }

    template <typename T>
    StoryboardImage(std::shared_ptr<T> value, math::Vec2 pixelSize)
        : data(std::static_pointer_cast<const void>(std::move(value))),
          type(typeid(T)),
          size(pixelSize)
    {
    }

    bool isValid() const noexcept
    {
        return data != nullptr;
    }

    template <typename T>
    const T* get() const noexcept
    {
        if (type != typeid(T) || data == nullptr)
            return nullptr;
        return static_cast<const T*>(data.get());
    }
};

struct StoryboardSettings
{
    StoryboardImage image;
    StoryboardFillStrategy aspect = StoryboardFillStrategy::BestFit;
    float alpha = 1.0f;
    math::Vec2 center{};
    math::Vec3 rotation{};
    math::Vec2 scale = math::Vec2::one;
    bool syncScale = true;
    float splitView = 0.0f;
};

struct StoryboardLayout
{
    math::Vec2 screenSize{};
    math::Vec2 viewportCenter{};
    math::Vec2 viewportSize{};
    math::Vec2 imageCenter{};
    math::Vec2 imageSize{};
    math::Vec2 imageScale{};
    math::Vec3 rotation{};
    float alpha = 0.0f;
    bool valid = false;
    bool hasImage = false;
};

StoryboardLayout calculateStoryboardLayout(
    const StoryboardSettings& settings,
    float blendWeight,
    math::Vec2 screenSize) noexcept;

class StoryboardExtension : public CameraExtension
{
public:
    using FillStrategy = StoryboardFillStrategy;

    bool showImage = true;
    StoryboardImage image;
    FillStrategy aspect = FillStrategy::BestFit;
    float alpha = 1.0f;
    math::Vec2 center{};
    math::Vec3 rotation{};
    math::Vec2 scale = math::Vec2::one;
    bool syncScale = true;
    bool muteCamera = false;
    float splitView = 0.0f;

    StoryboardExtension();

    void validate();
    std::shared_ptr<const StoryboardSettings> settings() const noexcept;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    void syncSettings();

    std::shared_ptr<StoryboardSettings> settings_;
};

} // namespace cinerig
