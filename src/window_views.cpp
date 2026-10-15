#include "window_views.h"

#include "bytes.h"

#include <algorithm>
#include <array>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kLinkNext = 0x00, kLinkWindow = 0x10, kLinkSkip = 0x14, kLinkBytes = kLinkSkip + 1;
        constexpr uint32_t kRecordBytes = kMcdVirtual + 1;

        bool Copy(const ProcessMemory& memory, uint32_t at, void* into, size_t bytes)
        {
            if (at == 0 || !memory.readable(at, bytes)) return false;
            memory.copy(at, into, bytes);
            return true;
        }

        bool ReadWindow(const ProcessMemory& memory, uint32_t mcd, OpenWindow& out)
        {
            std::array<uint8_t, kRecordBytes> record;
            std::array<char, kWindowNameLength> name;
            if (!Copy(memory, mcd, record.data(), record.size()) ||
                !Copy(memory, ReadAt<uint32_t>(record.data(), kWindowDescriptor) + kDescriptorName, name.data(),
                    name.size()))
                return false;
            const std::optional<WindowKey> key = WindowKeyOf(name.data());
            if (!key) return false;
            const auto edge = [&](int side) { return static_cast<int>(ReadAt<int16_t>(record.data(), kMcdRect + 2 * side)); };
            out = OpenWindow{mcd, *key, edge(0), edge(1), edge(2), edge(3),
                ReadAt<uint32_t>(record.data(), kMcdType) == kWindowOpening};
            return true;
        }
    }

    size_t ReadWindowList(uint32_t list, const ProcessMemory& memory, std::span<OpenWindow> out)
    {
        uint32_t link = 0;
        if (!Copy(memory, list, &link, sizeof(link))) return 0;
        size_t count = 0;
        for (size_t links = 0; link != 0 && count < out.size() && links < kMaxListLinks; ++links)
        {
            std::array<uint8_t, kLinkBytes> raw;
            if (!Copy(memory, link, raw.data(), raw.size())) break;
            if (raw[kLinkSkip] == 0 && ReadWindow(memory, ReadAt<uint32_t>(raw.data(), kLinkWindow), out[count])) ++count;
            link = ReadAt<uint32_t>(raw.data(), kLinkNext);
        }
        return count;
    }

    std::optional<size_t> ViewOf(std::span<const WindowView> views, std::span<const FrameWindow> frame)
    {
        std::optional<size_t> best;
        size_t bestHeld = 0;
        for (size_t v = 0; v < views.size(); ++v)
        {
            const size_t held = static_cast<size_t>(std::ranges::count_if(views[v].windows, [&](const WindowKey& key) {
                return std::ranges::any_of(frame, [&](const FrameWindow& w) { return !w.hidden && w.key == key; });
            }));
            if (held > bestHeld || (held == bestHeld && held > 0 && views[v].windows.size() < views[*best].windows.size()))
            {
                best     = v;
                bestHeld = held;
            }
        }
        return best;
    }

    std::optional<int> FrameTop(std::span<const FrameWindow> frame, int screenBottom)
    {
        std::optional<int> top;
        for (const FrameWindow& w : frame)
            if (!w.hidden && MovableWindow(w.key) && w.top < screenBottom / 2) top = std::min(top.value_or(w.top), w.top);
        return top;
    }

    int FrameShift(int frameTop, std::optional<int> chosen, int placedUnder, int band, bool keepClear)
    {
        const int shift = chosen.value_or(placedUnder - frameTop);
        return keepClear ? std::max(shift, band - frameTop) : shift;
    }
}
