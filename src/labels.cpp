#include "labels.h"

#include <cstdio>
#include <cstring>

namespace headsup
{
    namespace
    {
        constexpr const char* kPlaceholderMark = "[PH]";

        constexpr LabelShade kConShades[kConCount] = {LabelShade::TooWeak, LabelShade::EasyPrey, LabelShade::EasyPrey,
            LabelShade::DecentChallenge, LabelShade::EvenMatch, LabelShade::Tough, LabelShade::VeryTough,
            LabelShade::VeryTough};
    }

    std::string MobIdText(uint32_t serverId, bool placeholder, MobIdFormat format, bool markPlaceholders)
    {
        const bool marked = placeholder && markPlaceholders;
        char text[40];
        switch (format)
        {
            case MobIdFormat::Off: return "";
            case MobIdFormat::LastThree:
                if (marked) return kPlaceholderMark;
                std::snprintf(text, sizeof(text), "[%03X]", static_cast<unsigned>(serverId & kTargetIndexBits));
                return text;
            case MobIdFormat::Full:
                std::snprintf(text, sizeof(text), "[%u (0x%X)]%s%s", static_cast<unsigned>(serverId), static_cast<unsigned>(serverId),
                    marked ? kLabelSeparator : "", marked ? kPlaceholderMark : "");
                return text;
        }
        return "";
    }

    Label LevelLine(const Label& level, bool showLevel, const std::string& id)
    {
        const bool both        = showLevel && !id.empty();
        const std::string text = std::string(showLevel ? level.text : "") + (both ? kLabelSeparator : "") + id;
        Label line             = level;
        std::snprintf(line.text, sizeof(line.text), "%s", text.c_str());
        line.markAt = id.ends_with(kPlaceholderMark) ? text.size() - std::strlen(kPlaceholderMark) : Label::kNoMark;
        return line;
    }

    LabelShade ShadeFor(Con con)
    {
        const auto i = static_cast<unsigned>(con);
        return i < kConCount ? kConShades[i] : LabelShade::Unknown;
    }

    Label MakeLabel(const MobRecord* mob, const CheckResult* checked, int playerLevel)
    {
        Label label{};
        label.shade = LabelShade::Unknown;
        if (checked != nullptr && checked->level > 0)
        {
            std::snprintf(label.text, sizeof(label.text), "Lv %d%s%s", checked->level, kLabelSeparator, Abbrev(checked->con));
            label.shade = ShadeFor(checked->con);
            return label;
        }
        if (mob == nullptr || mob->maxLevel == 0)
        {
            std::snprintf(label.text, sizeof(label.text), "Lv ?%s??", kLabelSeparator);
            return label;
        }

        const int low = mob->minLevel, high = mob->maxLevel;
        char levels[12];
        if (low == high)
            std::snprintf(levels, sizeof(levels), "%d", high);
        else
            std::snprintf(levels, sizeof(levels), "%d-%d", low, high);
        if (playerLevel <= 0)
        {
            std::snprintf(label.text, sizeof(label.text), "Lv %s%s??", levels, kLabelSeparator);
            return label;
        }
        const Con easiest = Difficulty(playerLevel, low + mob->expLevelMod);
        const Con hardest = Difficulty(playerLevel, high + mob->expLevelMod);
        if (easiest == hardest)
            std::snprintf(label.text, sizeof(label.text), "Lv %s%s%s", levels, kLabelSeparator, Abbrev(hardest));
        else
            std::snprintf(label.text, sizeof(label.text), "Lv %s%s%s-%s", levels, kLabelSeparator, Abbrev(easiest), Abbrev(hardest));
        label.shade = ShadeFor(hardest);
        return label;
    }
}
