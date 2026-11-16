#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/core/modifier_interfaces.hpp>

namespace cinerig
{

namespace detail
{

template <typename T>
T* findModifierInterface(VirtualCamera& vcam)
{
    for (int index = 0; index < kStageCount; ++index)
    {
        if (auto* component = dynamic_cast<T*>(vcam.component(static_cast<Stage>(index))))
            return component;
    }
    for (std::size_t index = 0; index < vcam.extensionCount(); ++index)
    {
        if (auto* extension = dynamic_cast<T*>(vcam.extension(index)))
            return extension;
    }
    return nullptr;
}

} // namespace detail

class FreeLookModifier : public CameraExtension
{
public:
    template <typename T>
    struct TopBottomRigs
    {
        T top{};
        T bottom{};
    };

    class Modifier
    {
    public:
        virtual ~Modifier() = default;

        virtual void validate() {}
        virtual void reset(VirtualCamera& vcam) { (void)vcam; }
        virtual bool hasRequiredComponent() const { return true; }
        virtual void refreshCache(VirtualCamera& vcam) { (void)vcam; }

        virtual void beforePipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue)
        {
            (void)vcam;
            (void)state;
            (void)deltaTime;
            (void)modifierValue;
        }

        virtual void afterPipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue)
        {
            (void)vcam;
            (void)state;
            (void)deltaTime;
            (void)modifierValue;
        }
    };

    template <typename T>
    class ComponentModifier : public Modifier
    {
    public:
        bool hasRequiredComponent() const override { return cachedComponent_ != nullptr; }

        void refreshCache(VirtualCamera& vcam) override
        {
            cachedComponent_ = detail::findModifierInterface<T>(vcam);
        }

    protected:
        T* cachedComponent_ = nullptr;
    };

    class TiltModifier : public Modifier
    {
    public:
        TopBottomRigs<float> tilt{-5.0f, 5.0f};

        void validate() override;
        void reset(VirtualCamera& vcam) override;
        void afterPipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue) override;
    };

    class LensModifier : public Modifier
    {
    public:
        TopBottomRigs<LensSettings> lens{};

        void validate() override;
        void reset(VirtualCamera& vcam) override;
        void beforePipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue) override;
    };

    class PositionDampingModifier
        : public ComponentModifier<IModifiablePositionDamping>
    {
    public:
        TopBottomRigs<math::Vec3> damping{};

        void validate() override;
        void reset(VirtualCamera& vcam) override;
        void beforePipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue) override;
        void afterPipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue) override;

    private:
        math::Vec3 centerDamping_{};
    };

    class CompositionModifier : public ComponentModifier<IModifiableComposition>
    {
    public:
        TopBottomRigs<ScreenComposerSettings> composition{};

        void validate() override;
        void reset(VirtualCamera& vcam) override;
        void beforePipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue) override;
        void afterPipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue) override;

    private:
        ScreenComposerSettings centerComposition_{};
    };

    class DistanceModifier : public ComponentModifier<IModifiableDistance>
    {
    public:
        TopBottomRigs<float> distance{};

        void validate() override;
        void reset(VirtualCamera& vcam) override;
        void beforePipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue) override;
        void afterPipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue) override;

    private:
        float centerDistance_ = 0.0f;
    };

    class NoiseModifier : public ComponentModifier<IModifiableNoise>
    {
    public:
        struct NoiseSettings
        {
            float amplitude = 0.0f;
            float frequency = 0.0f;
        };

        TopBottomRigs<NoiseSettings> noise{};

        void reset(VirtualCamera& vcam) override;
        void beforePipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue) override;
        void afterPipeline(
            VirtualCamera& vcam,
            CameraState& state,
            float deltaTime,
            float modifierValue) override;

    private:
        std::pair<float, float> centerNoise_{};
    };

    std::vector<std::unique_ptr<Modifier>> modifiers;
    float easing = 0.0f;

    float currentValue() const noexcept { return currentValue_; }

    void validate();
    void refreshCache(VirtualCamera& vcam);
    void reset(VirtualCamera& vcam);

    void prePipelineMutateCameraStateCallback(
        VirtualCamera& vcam,
        CameraState& state,
        float deltaTime) override;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    float applyEasing(float value) const;

    IModifierValueSource* valueSource_ = nullptr;
    float currentValue_ = 0.0f;
};

} // namespace cinerig
