#include <cinerig/camera/external_camera.hpp>

#include <utility>

namespace cinerig
{

ExternalCamera::ExternalCamera(std::string name)
    : VirtualCamera(std::move(name))
{
}

ExternalCamera::~ExternalCamera() = default;

TargetRef ExternalCamera::effectiveLookAtTarget() const
{
    if (lookAtTarget.isValid())
        return lookAtTarget;
    return VirtualCamera::effectiveLookAtTarget();
}

void ExternalCamera::internalUpdateCameraState(math::Vec3 worldUp, float deltaTime)
{
    (void)worldUp;

    CameraState state = CameraState::makeDefault();

    IEntityWorld* world = entityWorld();
    const EntityId sourceEntity =
        (cameraEntity != 0 && world != nullptr && world->exists(cameraEntity))
        ? cameraEntity
        : ownerEntity;
    if (world != nullptr && sourceEntity != 0 && world->exists(sourceEntity))
    {
        const Transform transform = world->transform(sourceEntity);
        state.rawPosition = transform.position;
        state.rawOrientation = transform.rotation;
    }

    state.referenceUp = state.rawOrientation * math::Vec3::up;
    state.lens = output != nullptr ? output->outputLens() : lens;
    state.blendHint = blendHint;

    const auto target = targetPosition(lookAtTarget);
    if (target.has_value())
    {
        const math::Vec3 direction = *target - state.rawPosition;
        if (!math::almostZero(direction))
        {
            const math::Vec3 forward = state.rawOrientation * math::Vec3::forward;
            state.referenceLookAt =
                state.rawPosition + forward * math::dot(direction, forward);
        }
        else
        {
            state.referenceLookAt = *target;
        }
    }

    state_ = state;
    invokePostPipelineStage(Stage::Finalize, state_, deltaTime);
    previousStateIsValid = true;
}

} // namespace cinerig
