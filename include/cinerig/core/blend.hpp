#pragma once

#include <cinerig/core/blend_definition.hpp>
#include <cinerig/core/camera_interface.hpp>
#include <cinerig/core/camera_state.hpp>
#include <cinerig/math/curve.hpp>

namespace cinerig
{

class Blend;

class IBlender
{
public:
    virtual ~IBlender() = default;

    virtual CameraState getIntermediateState(
        const ICinemachineCamera& camA,
        const ICinemachineCamera& camB,
        float t) = 0;
};

class Blend
{
public:
    ICinemachineCamera* camA = nullptr;
    ICinemachineCamera* camB = nullptr;
    math::Curve blendCurve{};
    float timeInBlend = 0.0f;
    float duration = 0.0f;
    IBlender* customBlender = nullptr;

    float blendWeight() const;
    bool isComplete() const;
    bool isValid() const;
    bool uses(const ICinemachineCamera* cam) const;
    CameraState state() const;
    void updateCameraState(math::Vec3 worldUp, float deltaTime);
    void clearBlend();
    void copyFrom(const Blend& src);
};

class NestedBlendSource : public ICinemachineCamera
{
public:
    explicit NestedBlendSource(Blend* blend = nullptr);

    Blend* blend() const noexcept;
    void setBlend(Blend* blend) noexcept;

    std::string_view name() const override;
    const CameraState& state() const override;
    bool isValid() const override;
    ICinemachineMixer* parentCamera() const override;
    void updateCameraState(math::Vec3 worldUp, float deltaTime) override;

private:
    Blend* blend_ = nullptr;
    mutable CameraState state_{};
};

}
