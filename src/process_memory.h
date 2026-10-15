#pragma once

#include "function_ref.h"

#include <cstddef>
#include <cstdint>

namespace headsup
{
    // How a reader reaches the game's memory: whether bytes there can be read (VirtualQuery, in the plugin), and a
    // copy of bytes it found readable.
    struct ProcessMemory
    {
        FunctionRef<bool(uint32_t at, size_t bytes)> readable;
        FunctionRef<void(uint32_t at, void* into, size_t bytes)> copy;
    };
}
