#pragma once

namespace cinerig
{

class ICameraLensCacheInvalidator
{
public:
    virtual ~ICameraLensCacheInvalidator() = default;
    virtual void invalidateLensCache() = 0;
};

} // namespace cinerig
