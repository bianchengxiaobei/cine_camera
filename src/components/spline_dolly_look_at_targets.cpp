#include <cinerig/components/spline_dolly_look_at_targets.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig
{

void SplineDollyLookAtTargets::reset()
{
    targets.clear();
    defaultItem = {};
}

void SplineDollyLookAtTargets::validate()
{
    defaultItem.easing = std::clamp(defaultItem.easing, 0.0f, 1.0f);
    for (Key& key : targets)
    {
        if (!std::isfinite(key.position))
            key.position = 0.0f;
        key.position = std::clamp(key.position, 0.0f, 1.0f);
        key.item.easing = std::clamp(key.item.easing, 0.0f, 1.0f);
    }
    std::stable_sort(
        targets.begin(),
        targets.end(),
        [](const Key& a, const Key& b) {
            return a.position < b.position;
        });
}

SplineDolly* SplineDollyLookAtTargets::associatedDolly() const
{
    if (!isAttached())
        return nullptr;
    return dynamic_cast<SplineDolly*>(
        virtualCamera().component(Stage::Body));
}

bool SplineDollyLookAtTargets::isValid() const
{
    const SplineDolly* dolly = associatedDolly();
    return dolly != nullptr && dolly->spline.spline != nullptr;
}

math::Vec3 SplineDollyLookAtTargets::worldLookAt(
    const Item& item,
    const VirtualCamera& camera) const
{
    if (item.lookAt != 0)
    {
        const TargetRef target = TargetRef::entity(item.lookAt);
        if (const auto position = camera.targetPosition(target))
        {
            if (const auto rotation = camera.targetRotation(target))
                return *position + math::rotate(*rotation, item.offset);
            return *position + item.offset;
        }
    }
    return item.offset;
}

math::Vec3 SplineDollyLookAtTargets::evaluate(
    float normalizedPosition,
    const VirtualCamera& camera) const
{
    if (targets.empty())
        return worldLookAt(defaultItem, camera);

    normalizedPosition = std::isnan(normalizedPosition)
        ? 0.0f
        : std::clamp(normalizedPosition, 0.0f, 1.0f);
    if (normalizedPosition <= targets.front().position)
        return worldLookAt(targets.front().item, camera);

    for (std::size_t index = 1; index < targets.size(); ++index)
    {
        const Key& before = targets[index - 1];
        const Key& after = targets[index];
        if (normalizedPosition > after.position)
            continue;

        const float span = after.position - before.position;
        if (span <= math::Epsilon)
            return worldLookAt(after.item, camera);

        const float t = (normalizedPosition - before.position) / span;
        const float t2 = t * t;
        const float d = 1.0f - t;
        const float easingA = std::clamp(before.item.easing, 0.0f, 1.0f);
        const float easingB = std::clamp(after.item.easing, 0.0f, 1.0f);
        const float controlA = 0.3333f * (1.0f - easingA);
        const float controlB = 0.6666f + 0.3334f * easingB;
        const float eased =
            3.0f * d * d * t * controlA
            + 3.0f * d * t2 * controlB
            + t * t2;
        return math::lerp(
            worldLookAt(before.item, camera),
            worldLookAt(after.item, camera),
            eased);
    }
    return worldLookAt(targets.back().item, camera);
}

void SplineDollyLookAtTargets::mutateCameraState(
    CameraState& state,
    float deltaTime)
{
    (void)deltaTime;
    if (!isAttached())
        return;

    SplineDolly* dolly = associatedDolly();
    if (dolly == nullptr || dolly->spline.spline == nullptr)
        return;

    const math::Vec3 lookAt = evaluate(
        dolly->spline.normalizedPosition(),
        virtualCamera());
    const math::Vec3 direction = lookAt - state.rawPosition;
    if (direction.sqrMagnitude() > math::Epsilon)
    {
        math::Vec3 up = state.referenceUp;
        if (math::cross(direction, up).sqrMagnitude() < math::Epsilon)
        {
            up = math::rotate(state.rawOrientation, math::Vec3::back);
            if (math::cross(direction, up).sqrMagnitude() < math::Epsilon)
                up = math::rotate(state.rawOrientation, math::Vec3::left);
        }
        state.rawOrientation = math::lookRotation(direction, up);
    }
    state.referenceLookAt = lookAt;
}

} // namespace cinerig
