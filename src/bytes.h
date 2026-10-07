#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace headsup
{
    // A value at an offset in a packet, which need not be aligned.
    template <typename T>
    T ReadAt(const uint8_t* data, size_t offset)
    {
        T value;
        std::memcpy(&value, data + offset, sizeof(value));
        return value;
    }
}
