#pragma once

#include "function_ref.h"
#include "native_locate.h"
#include "process_memory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace headsup
{
    // The game's top strip as the bar shows it.
    struct StripText
    {
        std::string caption; // the title window's, UTF-8
        std::string help;    // the help window's, UTF-8
        bool operator==(const StripText&) const = default;
    };

    // One text object's shown string (kStripObjectBytes of it), in the game's CP932: the shown level's buffer to its
    // NUL or kStripBufferBytes, byte 0x04 (drawn with no width) dropped and the spaces at either end trimmed, as the
    // game pads a scrolling line with two; empty while nothing is shown or the level is one the game never draws.
    // The game clears a level by writing a NUL to its first byte, so whatever follows a NUL is never read.
    std::string ShownCp932(const uint8_t* object, size_t size);
    // ASCII as it is; anything else through convert, the plugin's CP932 to UTF-8.
    std::string Cp932ToUtf8(const std::string& cp932, FunctionRef<std::string(const std::string&)> convert);

    using StripObject = std::array<uint8_t, kStripObjectBytes>;

    // One strip window's text object, read each frame through the chain to it: the owner the game's slot points at,
    // the owner's window record (MCD), which points back at it, and the record's descriptor, which names the window.
    // The chain is checked with memory.readable when its owner, record or descriptor differs from the last that
    // passed; otherwise only memory found readable then is copied. Every check fails closed.
    class StripWindowReader
    {
    public:
        // Copies the text object at textAt in the owner into into: nullptr when it did, else why not. slot is in the
        // game's image, checked once by the caller.
        const char* Read(uint32_t slot, uint32_t textAt, StripWindow window, const ProcessMemory& memory,
                         StripObject& into);
        uint32_t Owner() const { return m_Owner; } // as the slot last held it

    private:
        uint32_t m_Owner = 0;
        uint32_t m_Mcd = 0, m_Descriptor = 0; // m_Owner's, as last checked; 0 when its chain did not pass
    };
}
