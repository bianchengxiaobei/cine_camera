#pragma once

#include <cinerig/engine/entity_world.hpp>

namespace cinerig
{

// Non-owning adapter to an engine animation state machine. Hashes are the
// host's state identifiers; CineRig treats them as opaque signed integers.
class IAnimationStateProvider
{
public:
    virtual ~IAnimationStateProvider() = default;

    virtual int currentStateHash(EntityId entity, int layer) const = 0;
    virtual int nextStateHash(EntityId entity, int layer) const = 0;
    virtual float normalizedTime(EntityId entity, int layer) const = 0;

    virtual bool isInTransition(EntityId entity, int layer) const
    {
        return nextStateHash(entity, layer) != 0;
    }
};

}
