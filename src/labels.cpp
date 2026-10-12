#include "labels.h"

#include <cstdio>
#include <cstring>

namespace headsup
{
    namespace
    {
        constexpr LabelShade kConShades[kConCount] = {LabelShade::TooWeak, LabelShade::EasyPrey, LabelShade::EasyPrey,
            LabelShade::DecentChallenge, LabelShade::EvenMatch, LabelShade::Tough, LabelShade::VeryTough,
            LabelShade::VeryTough};
    }

    std::string IdText(uint32_t serverId)
    {
        char text[16];
        std::snprintf(text, sizeof(text), "[%03X]", static_cast<unsigned>(serverId & kTargetIndexBits));
        return text;
    }

    std::string ServerIdText(uint32_t serverId)
    {
        char text[16];
        std::snprintf(text, sizeof(text), "[%u]", static_cast<unsigned>(serverId));
        return text;
    }

    std::string MobIdText(uint32_t serverId, bool placeholder, MobIdFormat format, bool markPlaceholders)
    {
        const bool marked = placeholder && markPlaceholders;
        char text[40];
        switch (format)
        {
            case MobIdFormat::Off: return "";
            case MobIdFormat::LastThree: return marked ? kPlaceholderMark : IdText(serverId);
            case MobIdFormat::Decimal:
                return ServerIdText(serverId) + (marked ? std::string(kLabelSeparator) + kPlaceholderMark : "");
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

    std::string LevelText(const MobRecord* mob, const CheckResult* checked)
    {
        if (checked != nullptr && checked->level > 0) return std::to_string(checked->level);
        if (mob == nullptr || mob->maxLevel == 0) return "?";
        if (mob->minLevel == mob->maxLevel) return std::to_string(mob->maxLevel);
        return std::to_string(mob->minLevel) + "-" + std::to_string(mob->maxLevel);
    }

    std::string ConText(const MobRecord* mob, const CheckResult* checked, int playerLevel)
    {
        if (checked != nullptr && checked->level > 0) return Abbrev(checked->con);
        if (mob == nullptr || mob->maxLevel == 0 || playerLevel <= 0) return "??";
        const Con easiest = Difficulty(playerLevel, mob->minLevel + mob->expLevelMod);
        const Con hardest = Difficulty(playerLevel, mob->maxLevel + mob->expLevelMod);
        return easiest == hardest ? Abbrev(hardest) : std::string(Abbrev(easiest)) + "-" + Abbrev(hardest);
    }

    Label MakeLabel(const MobRecord* mob, const CheckResult* checked, int playerLevel)
    {
        Label label{};
        label.shade = LabelShade::Unknown;
        const std::string text = "Lv " + LevelText(mob, checked) + kLabelSeparator + ConText(mob, checked, playerLevel);
        std::snprintf(label.text, sizeof(label.text), "%s", text.c_str());
        if (checked != nullptr && checked->level > 0)
            label.shade = ShadeFor(checked->con);
        else if (mob != nullptr && mob->maxLevel != 0 && playerLevel > 0)
            label.shade = ShadeFor(Difficulty(playerLevel, mob->maxLevel + mob->expLevelMod));
        return label;
    }
}
