#pragma once

#include "process_memory.h"
#include "test.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>
#include <vector>

namespace headsup
{
    // The game's memory as the reader sees it: regions that can be read, each VirtualQuery counted. A copy of anything
    // outside them, which in the game would read freed memory and crash, fails the test.
    struct FakeMemory
    {
        std::vector<std::pair<uint32_t, std::vector<uint8_t>>> regions;
        int queries = 0;

        std::vector<uint8_t>* RegionAt(uint32_t at, size_t bytes)
        {
            for (auto& [start, data] : regions)
                if (at >= start && at + bytes <= start + data.size()) return &data;
            return nullptr;
        }
        void Map(uint32_t at, size_t bytes) { regions.push_back({at, std::vector<uint8_t>(bytes, 0)}); }
        void Free(uint32_t at)
        {
            std::erase_if(regions, [&](const auto& r) { return r.first == at; });
        }
        void Put(uint32_t at, const void* bytes, size_t count)
        {
            for (auto& [start, data] : regions)
                if (at >= start && at + count <= start + data.size()) std::memcpy(data.data() + (at - start), bytes, count);
        }
        void Put32(uint32_t at, uint32_t value) { Put(at, &value, sizeof(value)); }
        ProcessMemory Access()
        {
            return ProcessMemory{m_Readable, m_Copy};
        }

    private:
        std::function<bool(uint32_t, size_t)> m_Readable = [this](uint32_t at, size_t bytes) {
            ++queries;
            return RegionAt(at, bytes) != nullptr;
        };
        std::function<void(uint32_t, void*, size_t)> m_Copy = [this](uint32_t at, void* into, size_t bytes) {
            for (auto& [start, data] : regions)
                if (at >= start && at + bytes <= start + data.size())
                {
                    std::memcpy(into, data.data() + (at - start), bytes);
                    return;
                }
            CHECK(!"read memory that is not there");
        };
    };
}
