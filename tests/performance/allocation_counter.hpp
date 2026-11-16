#pragma once

#include <cstddef>

namespace cinerig::bench
{

class AllocationScope
{
public:
    AllocationScope() noexcept;
    ~AllocationScope() noexcept;

    AllocationScope(const AllocationScope&) = delete;
    AllocationScope& operator=(const AllocationScope&) = delete;

    std::size_t finish() noexcept;

private:
    std::size_t allocations_ = 0;
    bool finished_ = false;
};

} // namespace cinerig::bench
