#pragma once

#include "function_ref.h"

#include <cstdint>
#include <optional>

namespace headsup
{
    constexpr double kPassSeconds = 2.0; // from one whole walk's end to the next's start

    // One region as VirtualQuery gives it.
    struct Region
    {
        uint64_t base = 0, size = 0;
        bool free = false;
    };

    struct FreeSpace
    {
        uint64_t freeBytes = 0, ceiling = 0; // ceiling: where the space VirtualQuery answers for ends
        bool operator==(const FreeSpace&) const = default;
    };

    // The 32-bit space's free bytes, summed a few regions a call so no frame pays for the whole walk.
    class FreeSpaceWalk
    {
    public:
        // query answers for the region at an address, nothing past the space's end.
        void Step(double now, FunctionRef<std::optional<Region>(uint64_t at)> query, int budget);
        // As of the last whole walk; nothing before one.
        std::optional<FreeSpace> Last() const { return m_Last; }

    private:
        uint64_t m_At = 0, m_Free = 0;
        bool m_Walking = false;
        std::optional<double> m_EndedAt;
        std::optional<FreeSpace> m_Last;
    };

    struct MemoryReport
    {
        uint32_t usedMb = 0, leftMb = 0, ceilingMb = 0;
        bool operator==(const MemoryReport&) const = default;
    };

    // What is used and left of the space, mapped being the process's VirtualSize. Under Wine, VirtualQuery calls free
    // only the unused part of Wine's own reservations (which VirtualSize counts) and calls what Linux has not mapped,
    // with what native libraries have, reserved; Wine places memory in both, so both are left.
    MemoryReport MemoryOf(const FreeSpace& space, uint64_t mapped, bool wine);
}
