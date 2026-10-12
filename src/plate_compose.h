#pragma once

#include "argb.h"
#include "generated/icon_ids.h"
#include "plate_settings.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <string>
#include <vector>

namespace headsup
{
    struct IconDraw
    {
        Icon icon          = Icon::AggroNQ;
        uint32_t tint      = kWhite;
        std::string badge;
        uint32_t badgeColor = 0;
        bool operator==(const IconDraw&) const = default;
    };
    enum class ItemKind : uint8_t { Text, Icons, Space };
    // Text: a run of letters drawn as one texture. Icons: a row of icons, badges hanging off them. Space: a gap of this
    // many spaces between an icon run and its neighbor.
    // name: a Text run that holds the entity's name, which the debug report and the developer's watch follow.
    struct PlateItem
    {
        ItemKind kind = ItemKind::Text;
        std::string text;
        std::vector<IconDraw> icons;
        int spaces = 0;
        bool name  = false;
        bool operator==(const PlateItem&) const = default;
    };
    // A line's style with every pick resolved and every percent applied to the base size: sizes in pixels before the
    // distance scale.
    struct LineLook
    {
        std::string font;
        bool bold        = false;
        float textSize   = 0.0f, iconSize = 0.0f;
        uint32_t color   = 0;
        float shadow     = 0.0f;
        uint32_t shadowColor = 0;
        float glow       = 0.0f, glowSize = 0.0f;
        uint32_t glowColor = 0;
        Align align      = Align::Center;
        bool operator==(const LineLook&) const = default;
    };
    struct ComposedLine
    {
        int source = 0; // the design line, 0 to 2
        std::vector<PlateItem> items;
        LineLook look;
    };
    constexpr int kNoOrnament = -1;
    // ornamentAbove: the index into lines of the lower of the two lines the kind's ornament sits between, or none.
    struct ComposedPlate
    {
        std::vector<ComposedLine> lines;
        int ornamentAbove   = kNoOrnament;
        uint32_t ornamentColor = 0;
    };

    // Screenshot mode hides names first, even after /hu off; then the game's names stay while HeadsUp is off or its
    // nameplates are, and the entity's row decides the rest.
    HookAction HookActionFor(const EntityState& state, const Settings& settings, bool screenshot);
    // The lines this entity shows now, with every tag rendered; empty for a mode that shows nothing.
    ComposedPlate ComposePlate(const ActorInfo& info, const PlayerState& player, const Settings& settings, uint32_t gameArgb);
    // The percent of the base size as pixels.
    float BasePixels(const Settings& settings, int percent);
}
