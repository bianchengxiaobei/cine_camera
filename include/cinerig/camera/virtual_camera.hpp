#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/camera/pipeline.hpp>
#include <cinerig/core/camera_interface.hpp>
#include <cinerig/core/camera_state.hpp>
#include <cinerig/core/camera_target.hpp>
#include <cinerig/core/lens_settings.hpp>
#include <cinerig/core/input_axis_owner.hpp>
#include <cinerig/core/output_channels.hpp>
#include <cinerig/core/priority.hpp>
#include <cinerig/engine/entity_world.hpp>

namespace cinerig
{

class CameraSystem;
class IPhysicsWorld;

enum class StandbyUpdateMode
{
    Never,
    Always,
    RoundRobin,
};

class VirtualCamera : public virtual ICinemachineCamera, public IInputAxisOwnerProvider
{
public:
    explicit VirtualCamera(std::string name = "VirtualCamera");
    ~VirtualCamera() override;

    VirtualCamera(const VirtualCamera&) = delete;
    VirtualCamera& operator=(const VirtualCamera&) = delete;

    CameraTarget targets{};
    LensSettings lens{};
    PrioritySettings priority{};
    OutputChannels outputChannel = OutputChannels::Default;
    StandbyUpdateMode standbyUpdate = StandbyUpdateMode::RoundRobin;
    BlendHint blendHint = BlendHint::Nothing;
    EntityId ownerEntity = 0;

    bool previousStateIsValid = false;
    float followTargetAttachment = 1.0f;
    float lookAtTargetAttachment = 1.0f;

    EntityId updateTargetEntity = 0;

    std::string_view name() const override;
    const CameraState& state() const override;
    bool isValid() const override;
    ICinemachineMixer* parentCamera() const override;
    void updateCameraState(math::Vec3 worldUp, float deltaTime) override;
    void onCameraActivated(const ActivationEventParams& event) override;

    virtual void internalUpdateCameraState(math::Vec3 worldUp, float deltaTime);
    virtual void onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime);
    virtual void forceCameraPosition(math::Vec3 pos, math::Quat rot);
    virtual void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta);
    float detachedFollowTargetDamp(float initial, float dampTime, float deltaTime) const;
    math::Vec3 detachedFollowTargetDamp(
        math::Vec3 initial,
        float dampTime,
        float deltaTime) const;
    math::Vec3 detachedFollowTargetDamp(
        math::Vec3 initial,
        math::Vec3 dampTime,
        float deltaTime) const;
    float detachedLookAtTargetDamp(float initial, float dampTime, float deltaTime) const;
    math::Vec3 detachedLookAtTargetDamp(
        math::Vec3 initial,
        float dampTime,
        float deltaTime) const;
    math::Vec3 detachedLookAtTargetDamp(
        math::Vec3 initial,
        math::Vec3 dampTime,
        float deltaTime) const;
    virtual float maxDampTime() const;

    bool isEnabled() const noexcept;
    void setEnabled(bool enabled);

    void setLive(bool live) noexcept;
    bool isLive() const noexcept;
    float currentTime() const noexcept;

    VirtualCamera* parentVirtualCamera() const noexcept;
    void setParent(VirtualCamera* parent);

    int activationId() const noexcept;
    CameraId cameraId() const noexcept;

    virtual TargetRef effectiveFollowTarget() const;
    virtual TargetRef effectiveLookAtTarget() const;
    const TargetGroup* lookAtTargetAsGroup() const;
    const TargetGroup* followTargetAsGroup() const;
    bool isEntityTarget(TargetRef target, EntityId entity) const;

    std::optional<math::Vec3> targetPosition(TargetRef target) const;
    std::optional<math::Quat> targetRotation(TargetRef target) const;
    std::optional<math::Vec3> followTargetPosition() const;
    std::optional<math::Quat> followTargetRotation() const;
    std::optional<math::Vec3> lookAtTargetPosition() const;
    std::optional<math::Quat> lookAtTargetRotation() const;

    CameraComponent* component(Stage stage) const noexcept;
    Pipeline& pipeline() noexcept;
    const Pipeline& pipeline() const noexcept;
    void getInputAxisOwners(std::vector<IInputAxisOwner*>& owners) const override;
    const IPhysicsWorld* physicsWorld() const noexcept;

    template <typename T, typename... Args>
    T& setBody(Args&&... args)
    {
        return setComponent<T>(Stage::Body, std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    T& setAim(Args&&... args)
    {
        return setComponent<T>(Stage::Aim, std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    T& setNoise(Args&&... args)
    {
        return setComponent<T>(Stage::Noise, std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    T& setFinalize(Args&&... args)
    {
        return setComponent<T>(Stage::Finalize, std::forward<Args>(args)...);
    }

    void addExtension(std::unique_ptr<CameraExtension> extension);
    void removeExtension(CameraExtension* extension);
    std::size_t extensionCount() const noexcept;
    CameraExtension* extension(std::size_t index) const noexcept;

protected:
    CameraState state_ = CameraState::makeDefault();

    virtual void updateTargetCache();
    virtual void invokePrePipelineMutate(CameraState& state, float deltaTime);
    virtual void invokePostPipelineStage(Stage stage, CameraState& state, float deltaTime);
    virtual     CameraState pullStateFromVirtualCamera(math::Vec3 worldUp);
    void pushStateToOwner(const CameraState& state);
    IEntityWorld* entityWorld() const noexcept;

private:
    friend class CameraSystem;
    friend class VirtualCameraRegistry;

    template <typename T, typename... Args>
    T& setComponent(Stage stage, Args&&... args)
    {
        static_assert(std::is_base_of_v<CameraComponent, T>, "component must derive from CameraComponent");
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* componentPtr = owned.get();
        pipeline_.set(stage, std::move(owned));
        return *componentPtr;
    }

    void invokePipeline(CameraState& state, float deltaTime);
    void invokePrePipelineMutateFor(
        VirtualCamera& vcam,
        CameraState& state,
        float deltaTime);
    void invokePostPipelineStageFor(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime);
    bool invokeOnTransitionExtensions(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime);
    void propagateForceCameraPosition(math::Vec3 pos, math::Quat rot);
    void propagateTargetWarped(EntityId target, math::Vec3 positionDelta);

    std::optional<math::Vec3> groupPosition(const TargetGroup& group) const;
    std::optional<math::Quat> groupRotation(const TargetGroup& group) const;

    std::string name_;
    CameraSystem* system_ = nullptr;
    VirtualCamera* parent_ = nullptr;
    CameraId cameraId_ = 0;
    int activationId_ = 0;
    bool valid_ = true;
    bool enabled_ = true;
    bool live_ = false;

    Pipeline pipeline_;
    std::vector<std::unique_ptr<CameraExtension>> extensions_;
};

} // namespace cinerig



