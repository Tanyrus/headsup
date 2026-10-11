#include "strip_text.h"

#include "bytes.h"
#include "native_locate.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        constexpr uint8_t kNoWidth = 0x04; // never a CP932 trail byte (0x40 and up), so safe to drop
        constexpr uint8_t kFirstNonAscii = 0x80;

        bool IsLeadByte(uint8_t b) { return (b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xFC); }

        // A buffer cut mid-character ends on a lead byte whose trail byte never fit.
        void DropCutLead(std::string& text)
        {
            size_t i = 0;
            while (i < text.size())
                i += IsLeadByte(static_cast<uint8_t>(text[i])) ? 2 : 1;
            if (i > text.size()) text.pop_back();
        }
    }

    std::string ShownCp932(const uint8_t* object, size_t size)
    {
        if (object == nullptr || size < kStripObjectBytes) return "";
        const uint32_t level = ReadAt<uint32_t>(object, kStripLevel);
        if (ReadAt<uint32_t>(object, kStripShown) == 0 || level >= kStripLevels) return "";
        const uint8_t* buffer = object + kStripBuffers + level * kStripStride;
        std::string text;
        for (uint32_t i = 0; i < kStripBufferBytes && buffer[i] != 0; ++i)
            if (buffer[i] != kNoWidth) text.push_back(static_cast<char>(buffer[i]));
        DropCutLead(text);
        const size_t first = text.find_first_not_of(' ');
        if (first == std::string::npos) return "";
        return text.substr(first, text.find_last_not_of(' ') - first + 1);
    }

    std::string Cp932ToUtf8(const std::string& cp932, FunctionRef<std::string(const std::string&)> convert)
    {
        const bool ascii = std::all_of(cp932.begin(), cp932.end(),
            [](char c) { return static_cast<uint8_t>(c) < kFirstNonAscii; });
        return ascii ? cp932 : convert(cp932);
    }

    const char* StripWindowReader::Read(uint32_t slot, uint32_t textAt, StripWindow window, const ProcessMemory& memory,
                                        StripObject& into)
    {
        const auto word = [&](uint32_t at) {
            uint32_t value = 0;
            memory.copy(at, &value, sizeof(value));
            return value;
        };
        if (const uint32_t owner = word(slot); owner != m_Owner)
        {
            m_Owner = owner;
            m_Mcd = m_Descriptor = 0;
        }
        if (m_Owner == 0) return "no window (not in game)";
        // A chain that passed is read again only where it was found readable; anything else in it is checked anew.
        if (m_Mcd == 0 || word(m_Owner + kOwnerMcd) != m_Mcd || word(m_Mcd + kMcdOwner) != m_Owner ||
            word(m_Mcd + kWindowDescriptor) != m_Descriptor)
        {
            m_Mcd = m_Descriptor = 0;
            if (!memory.readable(m_Owner, kOwnerMcd + sizeof(uint32_t)) ||
                !memory.readable(m_Owner + textAt, kStripObjectBytes))
                return "its window cannot be read";
            const uint32_t mcd = word(m_Owner + kOwnerMcd);
            if (mcd == 0 || !memory.readable(mcd, kMcdOwner + sizeof(uint32_t)) || word(mcd + kMcdOwner) != m_Owner)
                return "its window record does not point back at it";
            const uint32_t descriptor = word(mcd + kWindowDescriptor);
            std::array<char, kWindowNameLength> name{};
            if (descriptor == 0 || !memory.readable(descriptor + kDescriptorName, name.size()))
                return "its window record names another window";
            memory.copy(descriptor + kDescriptorName, name.data(), name.size());
            if (!IsStripWindow(name.data(), window)) return "its window record names another window";
            m_Mcd        = mcd;
            m_Descriptor = descriptor;
        }
        memory.copy(m_Owner + textAt, into.data(), into.size());
        return nullptr;
    }
}
