#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <typeindex>
#include <utility>
#include <vector>

#include <cinerig/core/lens_settings.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

enum class BlendHint : std::uint32_t
{
    Nothing = 0,
    SphericalPositionBlend = 1u << 0,
    CylindricalPositionBlend = 1u << 1,
    ScreenSpaceAimWhenTargetsDiffer = 1u << 2,
    InheritPosition = 1u << 3,
    IgnoreLookAtTarget = 1u << 4,
    FreezeWhenBlendingOut = 1u << 5,

    NoPosition = 1u << 16,
    NoOrientation = 1u << 17,
    NoTransform = (1u << 16) | (1u << 17),
    NoLens = 1u << 18,
};

constexpr BlendHint operator|(BlendHint a, BlendHint b) noexcept
{
    return static_cast<BlendHint>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

constexpr BlendHint operator&(BlendHint a, BlendHint b) noexcept
{
    return static_cast<BlendHint>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

constexpr BlendHint operator~(BlendHint value) noexcept
{
    return static_cast<BlendHint>(~static_cast<std::uint32_t>(value));
}

constexpr BlendHint& operator|=(BlendHint& a, BlendHint b) noexcept
{
    a = a | b;
    return a;
}

constexpr BlendHint& operator&=(BlendHint& a, BlendHint b) noexcept
{
    a = a & b;
    return a;
}

constexpr bool hasBlendHint(BlendHint value, BlendHint hint) noexcept
{
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(hint)) != 0u;
}

struct CustomBlendable
{
    std::shared_ptr<const void> data;
    std::type_index type = std::type_index(typeid(void));
    float weight = 0.0f;

    CustomBlendable() = default;

    template <typename T>
    CustomBlendable(std::shared_ptr<T> value, float blendWeight)
        : data(std::static_pointer_cast<const void>(std::move(value))),
          type(typeid(T)),
          weight(blendWeight)
    {
    }

    bool sameIdentity(const CustomBlendable& other) const noexcept
    {
        return data.get() == other.data.get() && type == other.type;
    }
};

struct CustomBlendableItems
{
    static constexpr std::size_t kFixedSlotCount = 4;

    std::array<CustomBlendable, kFixedSlotCount> slots;
    std::vector<CustomBlendable> overflow;
    std::size_t count = 0;

    std::size_t size() const noexcept
    {
        return count;
    }

    void add(CustomBlendable item);
    const CustomBlendable& at(std::size_t index) const;
    void clear() noexcept;

private:
    std::size_t findIndex(const CustomBlendable& item) const noexcept;
};

struct CameraState
{
    using BlendHints = BlendHint;

    LensSettings lens;
    math::Vec3 referenceUp{0.0f, 1.0f, 0.0f};
    math::Vec3 referenceLookAt{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(),
    };
    math::Vec3 rawPosition{};
    math::Quat rawOrientation{};
    math::Quat rotationDampingBypass{};
    float shotQuality = 1.0f;
    math::Vec3 positionCorrection{};
    math::Quat orientationCorrection{};
    BlendHint blendHint = BlendHint::Nothing;
    CustomBlendableItems customBlendables;

    static const math::Vec3 kNoPoint;
    static const CameraState Default;

    static CameraState makeDefault();
    static CameraState lerp(const CameraState& a, const CameraState& b, float t);
    bool hasLookAt() const noexcept;
    math::Vec3 correctedPosition() const noexcept;
    math::Quat correctedOrientation() const noexcept;
    math::Vec3 finalPosition() const noexcept;
    math::Quat finalOrientation() const noexcept;
    bool isTargetOffscreen() const;

    std::size_t numCustomBlendables() const noexcept;
    const CustomBlendable& customBlendable(std::size_t index) const;
    void addCustomBlendable(CustomBlendable blendable);
};

inline void CustomBlendableItems::add(CustomBlendable item)
{
    const std::size_t index = findIndex(item);
    if (index < count)
    {
        if (index < kFixedSlotCount)
            slots[index].weight += item.weight;
        else
            overflow[index - kFixedSlotCount].weight += item.weight;
        return;
    }

    if (count < kFixedSlotCount)
        slots[count] = std::move(item);
    else
        overflow.push_back(std::move(item));
    ++count;
}

inline const CustomBlendable& CustomBlendableItems::at(std::size_t index) const
{
    if (index < count)
    {
        if (index < kFixedSlotCount)
            return slots[index];
        return overflow[index - kFixedSlotCount];
    }

    static const CustomBlendable empty;
    return empty;
}

inline void CustomBlendableItems::clear() noexcept
{
    count = 0;
    overflow.clear();
}

inline std::size_t CustomBlendableItems::findIndex(const CustomBlendable& item) const noexcept
{
    const std::size_t slotCount = count < kFixedSlotCount ? count : kFixedSlotCount;
    for (std::size_t index = 0; index < slotCount; ++index)
    {
        if (slots[index].sameIdentity(item))
            return index;
    }

    for (std::size_t index = 0; index < overflow.size(); ++index)
    {
        if (overflow[index].sameIdentity(item))
            return kFixedSlotCount + index;
    }

    return count;
}

inline const math::Vec3 CameraState::kNoPoint{
    std::numeric_limits<float>::quiet_NaN(),
    std::numeric_limits<float>::quiet_NaN(),
    std::numeric_limits<float>::quiet_NaN(),
};

inline CameraState CameraState::makeDefault()
{
    CameraState state;
    state.lens = LensSettings::Default;
    state.referenceUp = math::Vec3::up;
    state.referenceLookAt = kNoPoint;
    state.rawPosition = math::Vec3::zero;
    state.rawOrientation = math::Quat::identity();
    state.rotationDampingBypass = math::Quat::identity();
    state.shotQuality = 1.0f;
    state.positionCorrection = math::Vec3::zero;
    state.orientationCorrection = math::Quat::identity();
    state.blendHint = BlendHint::Nothing;
    return state;
}

inline const CameraState CameraState::Default = CameraState::makeDefault();

inline bool CameraState::hasLookAt() const noexcept
{
    return !std::isnan(referenceLookAt.x)
        && !std::isnan(referenceLookAt.y)
        && !std::isnan(referenceLookAt.z);
}

inline math::Vec3 CameraState::correctedPosition() const noexcept
{
    return rawPosition + positionCorrection;
}

inline math::Quat CameraState::correctedOrientation() const noexcept
{
    return rawOrientation * orientationCorrection;
}

inline math::Vec3 CameraState::finalPosition() const noexcept
{
    return correctedPosition();
}

inline math::Quat CameraState::finalOrientation() const noexcept
{
    const math::Quat corrected = correctedOrientation();
    if (std::abs(lens.dutch) > math::Epsilon)
        return corrected * math::quatFromAxisAngle(math::Vec3::forward, lens.dutch * math::DegToRad);
    return corrected;
}

inline std::size_t CameraState::numCustomBlendables() const noexcept
{
    return customBlendables.size();
}

inline const CustomBlendable& CameraState::customBlendable(std::size_t index) const
{
    return customBlendables.at(index);
}

inline void CameraState::addCustomBlendable(CustomBlendable blendable)
{
    customBlendables.add(std::move(blendable));
}

}
