#include <cinerig/camera/camera_component.hpp>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig
{

VirtualCamera& CameraComponent::virtualCamera() const
{
    return *owner_;
}

VirtualCamera* CameraComponent::virtualCameraOrNull() const noexcept
{
    return owner_;
}

bool CameraComponent::isAttached() const noexcept
{
    return owner_ != nullptr;
}

std::optional<math::Vec3> CameraComponent::followTargetPosition() const
{
    return owner_ == nullptr ? std::nullopt : owner_->followTargetPosition();
}

std::optional<math::Quat> CameraComponent::followTargetRotation() const
{
    return owner_ == nullptr ? std::nullopt : owner_->followTargetRotation();
}

std::optional<math::Vec3> CameraComponent::lookAtTargetPosition() const
{
    return owner_ == nullptr ? std::nullopt : owner_->lookAtTargetPosition();
}

std::optional<math::Quat> CameraComponent::lookAtTargetRotation() const
{
    return owner_ == nullptr ? std::nullopt : owner_->lookAtTargetRotation();
}

} // namespace cinerig
