#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace headsup
{
    // A font compiled into the plugin: family is the name GDI matches on once the bytes are loaded.
    struct BundledFont
    {
        const char* family;
        const unsigned char* data;
        size_t bytes;
    };

    const BundledFont* BundledFonts(size_t& count);
    std::vector<std::string> BundledFamilies();
}
