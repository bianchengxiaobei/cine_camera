#pragma once

#include <utility>

#include <cinerig/core/screen_composer_settings.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

class IModifierValueSource
{
public:
    virtual ~IModifierValueSource() = default;
    virtual float getNormalizedModifierValue() = 0;
};

class IModifiablePositionDamping
{
public:
    virtual ~IModifiablePositionDamping() = default;
    virtual math::Vec3 getPositionDamping() const = 0;
    virtual void setPositionDamping(math::Vec3 damping) = 0;
};

class IModifiableComposition
{
public:
    virtual ~IModifiableComposition() = default;
    virtual ScreenComposerSettings getComposition() const = 0;
    virtual void setComposition(const ScreenComposerSettings& composition) = 0;
};

class IModifiableDistance
{
public:
    virtual ~IModifiableDistance() = default;
    virtual float getDistance() const = 0;
    virtual void setDistance(float distance) = 0;
};

class IModifiableNoise
{
public:
    virtual ~IModifiableNoise() = default;
    virtual std::pair<float, float> getNoiseAmplitudeFrequency() const = 0;
    virtual void setNoiseAmplitudeFrequency(std::pair<float, float> value) = 0;
};

} // namespace cinerig
