#pragma once

#include <vector>

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/components/spline_dolly.hpp>
#include <cinerig/engine/entity_world.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

class VirtualCamera;

// Aim-stage component that looks at points keyed along a SplineDolly's spline.
class SplineDollyLookAtTargets : public CameraComponent
{
public:
    struct Item
    {
        EntityId lookAt = 0;
        math::Vec3 offset{};
        float easing = 1.0f;
    };

    struct Key
    {
        float position = 0.0f;
        Item item{};
    };

    std::vector<Key> targets;
    Item defaultItem{};

    void reset();
    void validate();

    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;

private:
    SplineDolly* associatedDolly() const;
    math::Vec3 worldLookAt(
        const Item& item,
        const VirtualCamera& camera) const;
    math::Vec3 evaluate(
        float normalizedPosition,
        const VirtualCamera& camera) const;
};

} // namespace cinerig
