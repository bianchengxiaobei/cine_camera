#pragma once

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <utility>

#include <cinerig/engine/camera_output.hpp>

namespace cinerig
{

// Opaque, shared reference to an engine post-process profile. CineRig never
// interprets the profile; adapters recover their concrete type with get<T>().
struct PostProcessProfile
{
    std::shared_ptr<const void> data;
    std::type_index type = typeid(void);

    PostProcessProfile() = default;

    PostProcessProfile(
        std::shared_ptr<const void> value,
        std::type_index valueType)
        : data(std::move(value)),
          type(valueType)
    {
    }

    template <typename T>
    explicit PostProcessProfile(std::shared_ptr<T> value)
        : data(std::static_pointer_cast<const void>(std::move(value))),
          type(typeid(T))
    {
    }

    bool isValid() const noexcept
    {
        return data != nullptr;
    }

    template <typename T>
    const T* get() const noexcept
    {
        return type == typeid(T) ? static_cast<const T*>(data.get()) : nullptr;
    }

    bool sameIdentity(const PostProcessProfile& other) const noexcept
    {
        return data.get() == other.data.get() && type == other.type;
    }
};

// Optional non-owning post-processing hook. It runs after ICameraOutput has
// applied the final camera and does not own the camera, profiles, or resources.
class IPostProcessor
{
public:
    virtual ~IPostProcessor() = default;
    virtual void apply(const AppliedCamera& camera) = 0;
    virtual void resetHistory() {}
};

}
