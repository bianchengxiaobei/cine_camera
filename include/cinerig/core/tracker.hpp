#pragma once

#include <cinerig/core/camera_state.hpp>
#include <cinerig/core/camera_target.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

enum class BindingMode
{
    LockToTargetOnAssign = 0,
    LockToTargetWithWorldUp,
    LockToTargetNoRoll,
    LockToTarget,
    WorldSpace,
    LazyFollow,
};

enum class AngularDampingMode
{
    Euler = 0,
    Quaternion,
};

struct TrackerSettings
{
    BindingMode bindingMode = BindingMode::WorldSpace;
    math::Vec3 positionDamping = math::Vec3::one;
    AngularDampingMode angularDampingMode = AngularDampingMode::Euler;
    math::Vec3 rotationDamping = math::Vec3::one;
    float quaternionDamping = 1.0f;

    static const TrackerSettings Default;

    void validate();
    math::Vec3 effectivePositionDamping() const;
    math::Vec3 effectiveRotationDamping() const;
    float maxDampTime() const;
};

inline const TrackerSettings TrackerSettings::Default{};

struct TrackerTarget
{
    TargetRef ref{};
    math::Vec3 position{};
    math::Quat rotation{};
    bool valid = false;
};

class Tracker
{
public:
    void initStateInfo(
        const TrackerTarget& target,
        float deltaTime,
        bool previousStateIsValid,
        const TrackerSettings& settings,
        math::Vec3 targetOffset,
        math::Vec3 worldUp,
        const CameraState& cameraState);

    math::Quat getReferenceOrientation(
        const TrackerTarget& target,
        BindingMode bindingMode,
        math::Vec3 targetOffset,
        math::Vec3 worldUp,
        const CameraState& cameraState) const;

    void trackTarget(
        const TrackerTarget& target,
        float deltaTime,
        bool previousStateIsValid,
        math::Vec3 worldUp,
        math::Vec3 desiredCameraOffset,
        const TrackerSettings& settings,
        math::Vec3 targetOffset,
        CameraState& cameraState,
        float followTargetAttachment,
        math::Vec3& outTargetPosition,
        math::Quat& outTargetOrient);

    math::Vec3 getOffsetForMinimumTargetDistance(
        float followTargetAttachment,
        math::Vec3 dampedTargetPos,
        math::Vec3 cameraOffset,
        math::Vec3 cameraForward,
        math::Vec3 worldUp,
        math::Vec3 actualTargetPos);

    void onTargetObjectWarped(math::Vec3 positionDelta);
    void setPreviousTargetPosition(math::Vec3 position);
    void onForceCameraPosition(
        const TrackerTarget& target,
        const TrackerSettings& settings,
        math::Vec3 targetOffset,
        const CameraState& oldState,
        const CameraState& newState);

    void reset() noexcept;

    const math::Vec3& previousTargetPosition() const noexcept;
    const math::Quat& previousReferenceOrientation() const noexcept;
    const math::Vec3& previousOffset() const noexcept;
    bool hasPreviousReferenceOrientation() const noexcept;

private:
    TargetRef previousTarget_{};
    math::Vec3 previousTargetPosition_{};
    math::Quat previousReferenceOrientation_{};
    math::Vec3 previousOffset_{};
    math::Quat targetOrientationOnAssign_{};
    bool hasPreviousReferenceOrientation_ = false;
};

} // namespace cinerig
