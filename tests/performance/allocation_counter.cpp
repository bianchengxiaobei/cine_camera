#include "allocation_counter.hpp"

#include <algorithm>
#include <cstdlib>
#include <new>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace
{

bool g_tracking = false;
std::size_t g_allocationCount = 0;

void countAllocation() noexcept
{
    if (g_tracking)
        ++g_allocationCount;
}

void* allocate(std::size_t size, std::size_t alignment)
{
    if (size == 0)
        size = 1;

    if (alignment == 0)
    {
        if (void* memory = std::malloc(size))
            return memory;
        throw std::bad_alloc();
    }

#ifdef _WIN32
    if (void* memory = _aligned_malloc(size, alignment))
        return memory;
#else
    const std::size_t effectiveAlignment = std::max(alignment, alignof(std::max_align_t));
    const std::size_t roundedSize =
        ((size + effectiveAlignment - 1) / effectiveAlignment) * effectiveAlignment;
    if (void* memory = std::aligned_alloc(effectiveAlignment, roundedSize))
        return memory;
#endif
    throw std::bad_alloc();
}

void deallocate(void* memory, std::size_t alignment) noexcept
{
    if (memory == nullptr)
        return;
#ifdef _WIN32
    if (alignment != 0)
    {
        _aligned_free(memory);
        return;
    }
#endif
    std::free(memory);
}

} // namespace

void* operator new(std::size_t size)
{
    countAllocation();
    return allocate(size, 0);
}

void* operator new[](std::size_t size)
{
    countAllocation();
    return allocate(size, 0);
}

void* operator new(std::size_t size, std::align_val_t alignment)
{
    countAllocation();
    return allocate(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
    countAllocation();
    return allocate(size, static_cast<std::size_t>(alignment));
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        countAllocation();
        return allocate(size, 0);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        countAllocation();
        return allocate(size, 0);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new(
    std::size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept
{
    try
    {
        countAllocation();
        return allocate(size, static_cast<std::size_t>(alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](
    std::size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept
{
    try
    {
        countAllocation();
        return allocate(size, static_cast<std::size_t>(alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete(void* memory) noexcept
{
    deallocate(memory, 0);
}

void operator delete[](void* memory) noexcept
{
    deallocate(memory, 0);
}

void operator delete(void* memory, std::size_t) noexcept
{
    deallocate(memory, 0);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    deallocate(memory, 0);
}

void operator delete(void* memory, std::align_val_t alignment) noexcept
{
    deallocate(memory, static_cast<std::size_t>(alignment));
}

void operator delete[](void* memory, std::align_val_t alignment) noexcept
{
    deallocate(memory, static_cast<std::size_t>(alignment));
}

void operator delete(void* memory, std::size_t, std::align_val_t alignment) noexcept
{
    deallocate(memory, static_cast<std::size_t>(alignment));
}

void operator delete[](void* memory, std::size_t, std::align_val_t alignment) noexcept
{
    deallocate(memory, static_cast<std::size_t>(alignment));
}

void operator delete(void* memory, const std::nothrow_t&) noexcept
{
    deallocate(memory, 0);
}

void operator delete[](void* memory, const std::nothrow_t&) noexcept
{
    deallocate(memory, 0);
}

void operator delete(
    void* memory,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept
{
    deallocate(memory, static_cast<std::size_t>(alignment));
}

void operator delete[](
    void* memory,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept
{
    deallocate(memory, static_cast<std::size_t>(alignment));
}

namespace cinerig::bench
{

AllocationScope::AllocationScope() noexcept
{
    g_allocationCount = 0;
    g_tracking = true;
}

AllocationScope::~AllocationScope() noexcept
{
    (void)finish();
}

std::size_t AllocationScope::finish() noexcept
{
    if (finished_)
        return allocations_;

    g_tracking = false;
    allocations_ = g_allocationCount;
    finished_ = true;
    return allocations_;
}

} // namespace cinerig::bench
