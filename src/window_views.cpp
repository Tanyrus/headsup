#include "window_views.h"

#include "bytes.h"

#include <algorithm>
#include <array>
#include <charconv>

namespace headsup
{
#include "generated/views.inc"

    namespace
    {
        constexpr char kShiftSeparator = '|', kShiftColon = ':';
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
            const auto edge = [&](int side) {
                return static_cast<int>(ReadAt<int16_t>(record.data(), kMcdRect + 2 * side));
            };
            out = OpenWindow{mcd, *key, edge(0), edge(1), edge(2), edge(3),
                ReadAt<uint32_t>(record.data(), kMcdType) == kWindowOpening};
            return true;
        }

        // A shown window hanging from the top that moves with its view: not the strip, nor above its row.
        bool HangsInView(const FrameWindow& w, int screenBottom)
        {
            return !w.hidden && MovableWindow(w.key) && w.top >= kGameTopRow && HangsFromTop(w.top, screenBottom);
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
            if (raw[kLinkSkip] == 0 && ReadWindow(memory, ReadAt<uint32_t>(raw.data(), kLinkWindow), out[count]))
                ++count;
            link = ReadAt<uint32_t>(raw.data(), kLinkNext);
        }
        return count;
    }

    bool HangsFromTop(int top, int screenBottom) { return top < screenBottom / 2; }

    std::optional<size_t> ViewOf(std::span<const WindowView> views, std::span<const FrameWindow> frame)
    {
        std::optional<size_t> best;
        size_t bestHeld = 0;
        for (size_t v = 0; v < views.size(); ++v)
        {
            const size_t held = static_cast<size_t>(std::ranges::count_if(views[v].windows, [&](const WindowKey& key) {
                return std::ranges::any_of(frame, [&](const FrameWindow& w) { return !w.hidden && w.key == key; });
            }));
            const bool smaller = held == bestHeld && held > 0 && views[v].windows.size() < views[*best].windows.size();
            if (held > bestHeld || smaller)
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
            if (HangsInView(w, screenBottom)) top = std::min(top.value_or(w.top), w.top);
        return top;
    }

    FrameShifts ShiftsOf(std::span<const FrameWindow> frame, std::optional<int> chosen, int placedUnder, int band,
        bool keepClear, int screenRight, int screenBottom)
    {
        std::vector<FrameWindow> left, right;
        for (const FrameWindow& w : frame)
            (OnLeftSide(w.left, screenRight) ? left : right).push_back(w);
        const std::optional<int> leftTop = FrameTop(left, screenBottom), rightTop = FrameTop(right, screenBottom);
        const std::optional<int> top = FrameTop(frame, screenBottom);
        if (!top) return {};
        const int viewShift = FrameShift(*top, chosen, placedUnder, band, keepClear);
        const auto own = [&](std::optional<int> sideTop) {
            if (!sideTop || *sideTop == *top) return viewShift;
            return FrameShift(*sideTop, std::nullopt, placedUnder, band, keepClear);
        };
        const auto side = [&](std::span<const FrameWindow> windows, std::optional<int> sideTop) {
            SideShift shift{own(sideTop), std::nullopt};
            for (const FrameWindow& w : windows)
                if (HangsInView(w, screenBottom) && w.top < kGameUnderStripRow)
                    shift.topRow = TopRow{std::min(shift.topRow ? shift.topRow->left : w.left, w.left),
                        std::max(shift.topRow ? shift.topRow->right : w.right, w.right)};
            return shift;
        };
        return {side(left, leftTop), side(right, rightTop)};
    }

    int FrameShifts::For(int windowLeft, int windowRight, int screenRight) const
    {
        const SideShift& side = OnLeftSide(windowLeft, screenRight) ? left : right;
        const bool beside = side.topRow && (windowRight <= side.topRow->left || windowLeft >= side.topRow->right);
        return beside ? side.shift - (kGameUnderStripRow - kGameTopRow) : side.shift;
    }

    int FrameShift(int frameTop, std::optional<int> chosen, int placedUnder, int band, bool keepClear)
    {
        const int shift = chosen.value_or(placedUnder - frameTop);
        return keepClear ? std::max(shift, band - frameTop) : shift;
    }

    HelpMode ModeOf(bool panelHelp, bool hideGameHelp)
    {
        return panelHelp || !hideGameHelp ? HelpMode::On : HelpMode::Off;
    }

    std::span<const WindowView> WindowViews() { return kWindowViews; }

    int ReferenceBand(HelpMode mode) { return mode == HelpMode::On ? kReferenceBandHelpOn : kReferenceBandHelpOff; }

    std::vector<ViewShift> ViewShiftsFrom(std::string_view text, std::span<const WindowView> views)
    {
        std::vector<ViewShift> shifts;
        while (!text.empty())
        {
            const size_t end       = std::min(text.find(kShiftSeparator), text.size());
            const std::string_view entry = text.substr(0, end);
            text.remove_prefix(std::min(end + 1, text.size()));
            const size_t colon = entry.rfind(kShiftColon);
            if (colon == std::string_view::npos) continue;
            const std::string_view id = entry.substr(0, colon), number = entry.substr(colon + 1);
            int dy = 0;
            const auto [rest, error] = std::from_chars(number.data(), number.data() + number.size(), dy);
            if (error != std::errc{} || rest != number.data() + number.size() ||
                std::ranges::none_of(views, [&](const WindowView& v) { return v.id == id; }) ||
                std::ranges::any_of(shifts, [&](const ViewShift& s) { return s.id == id; }))
                continue;
            shifts.push_back(ViewShift{std::string(id), std::clamp(dy, -kMaxWindowOffset, kMaxWindowOffset)});
        }
        return shifts;
    }

    std::string ViewShiftsText(const std::vector<ViewShift>& shifts)
    {
        std::string text;
        for (const ViewShift& s : shifts)
        {
            if (!text.empty()) text += kShiftSeparator;
            text += s.id + kShiftColon + std::to_string(s.dy);
        }
        return text;
    }

    StripPlace PlaceStrip(int panelBand, std::optional<StripRows> strip, bool moveGameHelp, int clearance)
    {
        if (panelBand == 0) return {};
        if (!strip) return {0, panelBand};
        if (!moveGameHelp) return {0, std::max(panelBand, strip->bottom + clearance)};
        const int shift = panelBand - strip->top;
        return {shift, strip->bottom + shift + clearance};
    }

    void LabelPlace(std::span<int16_t, 4> rect, int frameShift, int titleMoved)
    {
        const int by = frameShift - titleMoved;
        rect[1]      = static_cast<int16_t>(rect[1] + by);
        rect[3]      = static_cast<int16_t>(rect[3] + by);
    }
}
