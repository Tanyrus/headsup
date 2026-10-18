#include "address_space.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        constexpr int kMbShift = 20;
        uint32_t Mb(uint64_t bytes) { return static_cast<uint32_t>(bytes >> kMbShift); }
    }

    void FreeSpaceWalk::Step(double now, FunctionRef<std::optional<Region>(uint64_t at)> query, int budget)
    {
        if (!m_Walking)
        {
            if (m_EndedAt && now - *m_EndedAt < kPassSeconds) return;
            m_Walking = true;
            m_At      = 0;
            m_Free    = 0;
        }
        for (int asked = 0; asked < budget; ++asked)
        {
            const std::optional<Region> region = query(m_At);
            const uint64_t end                 = region ? region->base + region->size : 0;
            if (!region || end <= m_At)
            {
                m_Last    = FreeSpace{m_Free, m_At};
                m_EndedAt = now;
                m_Walking = false;
                return;
            }
            if (region->free) m_Free += end - m_At;
            m_At = end;
        }
    }

    MemoryReport MemoryOf(const FreeSpace& space, uint64_t mapped, bool wine)
    {
        const uint64_t unmapped = wine && mapped < space.ceiling ? space.ceiling - mapped : 0;
        const uint64_t left     = std::min(space.ceiling, space.freeBytes + unmapped);
        return MemoryReport{Mb(space.ceiling - left), Mb(left), Mb(space.ceiling)};
    }
}
