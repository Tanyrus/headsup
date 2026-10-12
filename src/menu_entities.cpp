#include "menu_pages.h"

#include "icons.h"
#include "menu_state.h"
#include "plate_settings.h"
#include "plate_template.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr const char* kDefaultFontChoice = "Default"; // a line's font: the one on the Entities page

        const char* const kMobRowTips[kCategoryCount] = {
            "Would attack you at your level. Too Weak mobs only attack while you sit or rest.",
            "Passive, or aggressive but Too Weak for you.",
            "Not in the Phoenix data.",
            "Notorious monsters that would attack you.",
            "Notorious monsters that would leave you alone.",
            "Lottery placeholders, whose death can pop an NM, while nobody has claimed them.",
            "Claimed by you or your party, whatever else it is.",
            "Claimed by anyone outside your party, whatever else it is."}; // by Category
        const char* const kPlayerRowTips[kPlayerRowCount] = {
            "Every player the rows below do not cover.",
            "In your party.",
            "Named in the Friends card below, and not in your party.",
            "In your linkshell, and neither in your party nor a friend. The game tells a player's linkshell only when "
            "you /check them."}; // by PlayerRow
        constexpr const char* kPlateModesTip =
            "Hidden: no name at all. Shown: HeadsUp's nameplate. When Targeted: the nameplate while it is your target, "
            "and no name otherwise. Expanding: its name alone, and the whole nameplate while it is your target. Vanilla: "
            "the game's own name.";
        constexpr const char* kOutlineModesTip =
            "Disabled: no outline. Shown: an outline. When Targeted: an outline while it is your target.";
        constexpr const char* kByNatureOfClaimTip       = " By Nature: the outline it would have unclaimed.";
        constexpr const char* kByNatureOfPlaceholderTip = " By Nature: the outline its aggression gives it, as any other "
                                                          "mob's.";

        const char* RowTip(PlateKind kind, int row)
        {
            switch (kind)
            {
            case PlateKind::Mobs: return kMobRowTips[row];
            case PlateKind::Players: return kPlayerRowTips[row];
            case PlateKind::Pets: return "Any player's pet.";
            default: return nullptr;
            }
        }

        // A cutoff as its dropdown shows it: the rank as badges read it, or Off past the end.
        std::string CutoffText(int rank, bool ranks)
        {
            return rank == kNoWeakRank || rank == kNoResistRank ? "Off" : RankText(rank, ranks);
        }

        std::string ElementCountText(int count)
        {
            if (count == 0) return "None";
            return count == kElementCount ? "All" : std::to_string(count);
        }

        // entryTip, when given, says for an entry when it applies.
        template <typename Entry>
        void DrawPalette(Ui& ui, const char* title, const char* tip, std::unordered_set<std::string>& collapsed,
            Color* colors, int count, const char* (*label)(Entry), const char* (*entryTip)(Entry) = nullptr)
        {
            if (!ui.BeginCard(title, collapsed, nullptr, tip)) return;
            for (int i = 0; i < count; ++i)
            {
                const auto entry = static_cast<Entry>(i);
                ui.Swatch((std::string(label(entry)) + "##" + title).c_str(), colors[i],
                    entryTip != nullptr ? entryTip(entry) : nullptr);
            }
            ui.EndCard();
        }

        void DrawVisibility(Ui& ui, Settings& s, PlateKind kind, const int* order, int rows,
            std::unordered_set<std::string>& collapsed)
        {
            KindSettings& k    = s.Kind(kind);
            const bool plates  = s.enabled && s.nameplates, outlines = s.enabled && s.outlines;
            ui.Fade(!s.enabled);
            if (!ui.BeginCard("Visibility", collapsed, nullptr,
                    "What each kind of entity shows, and whether it gets an outline."))
                return;
            const float mode = ui.Em(kModeWidth), gap = ui.style.ItemSpacing.x, frame = ui.gui->GetFrameHeight();
            const float width = 2.0f * mode + gap;
            // In a window too narrow for the longest label beside the two dropdowns, they go on a line under each label.
            size_t letters = 0;
            for (int i = 0; i < rows; ++i)
                letters = std::max(letters, std::strlen(RowLabel(kind, order[i])));
            const float beside = static_cast<float>(letters) * ui.Em(kLetterWidth) + width;
            const float down   = beside > ui.cardWidth - 2.0f * ui.Em(kCardPad) ? frame : 0.0f;
            {
                const float y = ui.Row("##visibility heads", nullptr, width);
                const float x = ui.gui->GetCursorPosX();
                ui.Fade(!plates);
                ui.gui->AlignTextToFramePadding();
                ui.Text(kMuted, "Nameplates");
                ui.Fade(!outlines);
                ui.gui->SetCursorPos(ImVec2(x + mode + gap, y));
                ui.gui->AlignTextToFramePadding();
                ui.Text(kMuted, "Outlines");
                ui.EndRowAt(ui.cardX, y, frame);
            }
            for (int i = 0; i < rows; ++i)
            {
                const int r      = order[i];
                RowSettings& row = k.rows[r];
                ui.Fade(!s.enabled);
                const float y = ui.Row(RowLabel(kind, r), RowTip(kind, r), width);
                const float x = ui.gui->GetCursorPosX();
                char id[48];
                ui.Fade(!plates);
                ui.gui->SetCursorPos(ImVec2(x, y + down));
                int plate = static_cast<int>(row.plate);
                std::snprintf(id, sizeof(id), "plate %s", RowKey(kind, r));
                ui.DropdownAt(id, plate, 0, kPlateModeCount - 1,
                    [](int m) { return std::string(PlateModeLabel(static_cast<PlateMode>(m))); }, mode);
                row.plate = static_cast<PlateMode>(plate);
                ui.Tip(kPlateModesTip);
                ui.Fade(!outlines);
                ui.gui->SetCursorPos(ImVec2(x + mode + gap, y + down));
                int outline = static_cast<int>(row.outline);
                std::snprintf(id, sizeof(id), "outline %s", RowKey(kind, r));
                ui.DropdownAt(id, outline, 0, OutlineModesFor(kind, r) - 1,
                    [](int m) { return std::string(OutlineModeLabel(static_cast<OutlineMode>(m))); }, mode);
                row.outline = static_cast<OutlineMode>(outline);
                std::string tip = kOutlineModesTip;
                if (CanOutlineByNature(kind, r))
                    tip += IsClaimRow(static_cast<Category>(r)) ? kByNatureOfClaimTip : kByNatureOfPlaceholderTip;
                ui.Tip(tip.c_str());
                ui.EndRowAt(ui.cardX, y, frame + down);
            }
            ui.EndCard();
        }

        void DrawOutlineDesign(Ui& ui, Settings& s, PlateKind kind, const int* order, int rows,
            std::unordered_set<std::string>& collapsed)
        {
            KindSettings& k = s.Kind(kind);
            ui.Fade(!s.enabled || !s.outlines);
            if (!ui.BeginCard("Outline Design", collapsed, nullptr,
                    "Each row's outline color: a custom color, or one of the palettes by the entity's state."))
                return;
            const std::vector<ColorSource> sources = SourcesFor(kind, false);
            for (int i = 0; i < rows; ++i)
            {
                RowSettings& row = k.rows[order[i]];
                if (!HasOutlineColor(row.outline)) continue;
                ui.ColorPickRow((std::string(RowLabel(kind, order[i])) + "##outline").c_str(), row.outlineColor, sources,
                    "This row's outline color: your own, or a palette on the Entities page.");
            }
            ui.EndCard();
        }

        void DrawNameplateDesign(Ui& ui, Settings& s, PlateKind kind, std::unordered_set<std::string>& collapsed,
            DesignEditor& editor)
        {
            KindSettings& k = s.Kind(kind);
            ui.Fade(!s.enabled || !s.nameplates);
            if (!ui.BeginCard("Nameplate Design", collapsed, nullptr,
                    "Three lines of text with tags. Click a tag to put it where you last typed, or drag it onto a line."))
                return;
            const float left = ui.cardX + ui.Em(kCardPad);
            for (int i = 0; i < kPlateLines; ++i)
            {
                ui.gui->SetCursorPosX(left);
                ui.Text(kLabel, kLineLabels[i]);
                ui.gui->SetCursorPosX(left);
                char id[48];
                std::snprintf(id, sizeof(id), "##design %s %d", KindKey(kind), i);
                if (ui.LineBox(id, k.lines[i].text, editor.drafts[i]) || editor.drafts[i].typing) editor.line = i;
            }
            bool first = true;
            for (const Tag tag : TagsFor(kind))
            {
                ui.PlaceChip(first, left, ui.ChipRoom(std::strlen(TagName(tag))));
                first = false;
                if (ui.TagChip(tag) && PutTag(k.lines[editor.line].text, editor.drafts[editor.line].caret, tag))
                    ui.changed = true;
            }
            // Under the chips, so nothing above moves under the mouse when it comes and goes.
            if (const std::string note = UnusedTagsNote(k, {Tag::Name}, kShowsNameEnding); !note.empty())
                ui.Note(note.c_str());
            ui.EndCard();
        }

        // Every tab shows the same rows, each control a slider wide and ending at the card's right padding, so nothing
        // moves when the tab changes; a synced line's tab alone shows only its Sync row.
        void DrawLineStyles(Ui& ui, Settings& s, PlateKind kind, std::unordered_set<std::string>& collapsed,
            const std::vector<std::string>& fonts, DesignEditor& editor)
        {
            KindSettings& k = s.Kind(kind);
            const bool off  = !s.enabled || !s.nameplates;
            ui.Fade(off);
            if (!ui.BeginCard("Line Style", collapsed, nullptr, "How each line is drawn. A line's tab holds its settings."))
                return;
            editor.styleTab  = ui.Tabs("line style", editor.styleTab, kLineLabels, kPlateLines);
            const int n      = editor.styleTab;
            LineDesign& line = k.lines[n];
            // Each line's controls have ids of their own: one held, open or typed in is not carried to another tab.
            const std::string scope = std::string("##") + KindKey(kind) + std::to_string(n);
            const auto label        = [&scope](const char* text) { return text + scope; };

            const std::vector<int> targets = SyncTargets(k, n);
            const int synced               = SyncEntry(targets, line.syncWith);
            int sync                       = synced;
            ui.NumberDropdown(label("Sync Line Style").c_str(), sync, 0, static_cast<int>(targets.size()),
                [&targets](int entry) { return SyncEntryLabel(targets, entry); },
                "Borrows every setting below from another line, so the two always match. A line that borrows from this "
                "one is not offered.");
            if (sync != synced) line.syncWith = SyncOfEntry(targets, sync);
            if (line.syncWith != kNoSync)
            {
                ui.Note((std::string("Uses ") + kLineLabels[line.syncWith] + "'s style.").c_str());
                ui.EndCard();
                return;
            }
            const std::vector<ColorSource> sources = SourcesFor(kind, true);
            ui.ChecksDropdown(label("Show").c_str(), line.show, ConditionsFor(kind),
                "When this line shows: always, or when any checked condition holds.");
            ui.Segments(label("Alignment").c_str(), line.align, kAlignLabels, kAlignCount,
                "Where the line sits against the plate's widest line.", ui.Em(kControlWidth) / kAlignCount);
            ui.FontDropdown(label("Font").c_str(), line.font, fonts,
                "The line's font. Default is the font on the Entities page.", kDefaultFontChoice, &line.bold);
            ui.SliderInt(label("Text Size").c_str(), line.textSize, kMinSizePercent, kMaxSizePercent,
                SizeFormat(s, line.textSize).c_str(), "Of the base font size on the Entities page.");
            ui.SliderInt(label("Icon Size").c_str(), line.iconSize, kMinSizePercent, kMaxSizePercent,
                SizeFormat(s, line.iconSize).c_str(), "Every icon on this line, of the base font size.");
            ui.ColorPickRow(label("Color").c_str(), line.color, sources,
                "The text's color: your own, the game's, or a palette by the entity's state.");
            ui.SliderInt(label("Shadow").c_str(), line.shadow, kMinShadow, kMaxShadow, "%d%%",
                "The dark, soft shadow behind the letters that keeps them readable over bright ground. 0 is none.");
            ui.ColorPickRow(label("Shadow Color").c_str(), line.shadowColor, sources,
                "The shadow's color; black as a rule.");
            ui.SliderInt(label("Glow").c_str(), line.glow, kMinGlow, kMaxGlow, "%d%%",
                "A colored halo around the letters, as a mob's name glows in its outline color. 0 is none.");
            ui.Fade(off || line.glow == 0);
            ui.SliderInt(label("Glow Size").c_str(), line.glowSize, kMinGlowSize, kMaxGlowSize, "%d%%",
                "How far the halo spreads.");
            ui.ColorPickRow(label("Glow Color").c_str(), line.glowColor, sources, "The halo's color.");
            ui.EndCard();
        }

        void DrawOrnament(Ui& ui, Settings& s, PlateKind kind, std::unordered_set<std::string>& collapsed)
        {
            OrnamentSettings& o = s.Kind(kind).ornament;
            const bool off      = !s.enabled || !s.nameplates;
            ui.Fade(off);
            if (!ui.BeginCard("Ornament", collapsed, &o.on,
                    "A diamond rule between two of the lines, drawn while both show."))
                return;
            ui.Fade(off || !o.on);
            const std::string scope = std::string("##") + KindKey(kind) + " ornament";
            ui.Segments(("Position" + scope).c_str(), o.place, kOrnamentPlaceLabels, kOrnamentPlaceCount,
                "Which two lines it sits between. It shows only while both do.",
                ui.Em(kControlWidth) / kOrnamentPlaceCount);
            ui.SliderInt(("Width" + scope).c_str(), o.width, kMinOrnamentWidth, kMaxOrnamentWidth, "%d px",
                "The rule's length, before the distance scale.");
            ui.SliderInt(("Height" + scope).c_str(), o.height, kMinOrnamentThickness, kMaxOrnamentThickness, "%d px",
                "The diamond's height.");
            ui.ColorPickRow(("Color" + scope).c_str(), o.color, SourcesFor(kind, false),
                "Its color: your own, or a palette by the entity's state.");
            ui.EndCard();
        }

        void DrawMobCards(Ui& ui, Settings& s, std::unordered_set<std::string>& collapsed,
            const std::vector<std::string>& fonts)
        {
            const bool off = !s.enabled || !s.nameplates;
            // A card whose settings show only through tags says which, while no line of the design holds one of them.
            const auto shown = [&](std::initializer_list<Tag> tags) {
                const std::string note = UnusedTagsNote(s.Kind(PlateKind::Mobs), tags, kShowsTheseEnding);
                if (!note.empty()) ui.Note(note.c_str());
                return note.empty();
            };
            ui.Fade(off);
            if (ui.BeginCard("Fading", collapsed))
            {
                ui.SliderInt("Others' Opacity", s.untargetedOpacity, kMinOpacity, kMaxOpacity, "%d%%",
                    "Other mobs' and pets' names and lines while you have a target, so your target's stand out.");
                ui.Switch("Glow Off While Fighting", s.glowOffWhenFighting,
                    "No glow on a mob's lines once you or your party has claimed it.");
                ui.EndCard();
            }
            if (ui.BeginCard("Detection", collapsed, nullptr,
                    "Which icons $detect shows: aggro, linking and how it detects you, in MobDB's icons."))
            {
                ui.Fade(off || !shown({Tag::Detect}));
                for (int d = 0; d < kDetectionIconCount; ++d)
                {
                    const uint32_t bit = DetectionBit(static_cast<DetectionIcon>(d));
                    bool on            = (s.detectionShown & bit) != 0;
                    ui.Switch(DetectionLabel(static_cast<DetectionIcon>(d)), on, nullptr);
                    s.detectionShown = on ? s.detectionShown | bit : s.detectionShown & ~bit;
                }
                ui.EndCard();
            }
            // The Target widget's popup reads the ranks switch and these colors too, whatever the nameplates do or hold.
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Badges", collapsed, nullptr,
                    "The small text on the icons of $weak, $resist, $status and $immune. The Target widget's popup takes "
                    "its ranks and colors from here too."))
            {
                shown({Tag::Weak, Tag::Resist, Tag::Status, Tag::Immune});
                ui.FontDropdown("Badge Font", s.badgeFontName, fonts);
                ui.Switch("Ranks", s.showRanks, "Phoenix's ranks, -3 to 11, instead of percents. -3 is +50% and 4 is -50%.");
                ui.Swatch("Weak", s.weakBadgeColor, nullptr);
                ui.Swatch("Resist", s.resistBadgeColor, nullptr);
                ui.Swatch("Status Resist", s.rankBadgeColor, nullptr);
                ui.Swatch("Immune", s.immuneBadgeColor, nullptr);
                ui.EndCard();
            }
            const auto cutoff = [&s](int rank) { return CutoffText(rank, s.showRanks); };
            ui.Fade(off);
            if (ui.BeginCard("Weaknesses", collapsed, nullptr,
                    "Which elements $weak and $resist show beside the damage types."))
            {
                ui.Fade(off || !shown({Tag::Weak, Tag::Resist}));
                ui.NumberDropdown("Weak From", s.elementWeakRank, kMaxWeakRank, kNoWeakRank, cutoff,
                    "The least element weakness that gets a badge.");
                ui.NumberDropdown("Resist From", s.elementResistRank, kMinResistRank, kNoResistRank, cutoff,
                    "The least element resistance that gets a badge.");
                ui.NumberDropdown("Max Elements", s.maxElements, kElementCount, 0, ElementCountText,
                    "Past this many, those nearest neutral are left off.");
                ui.EndCard();
            }
            ui.Fade(off);
            if (ui.BeginCard("Statuses", collapsed, nullptr, "Which statuses $status shows, weak or resisted."))
            {
                const bool dim    = off || !shown({Tag::Status});
                const float cut   = ui.Em(kCutoffWidth);
                const float width = ui.Em(kSwitchWidth) + 2.0f * (cut + ui.style.ItemSpacing.x);
                ui.Fade(dim);
                {
                    const float y = ui.Row("##status heads", nullptr, width);
                    const float x = ui.gui->GetCursorPosX() + ui.Em(kSwitchWidth) + ui.style.ItemSpacing.x;
                    ui.gui->SetCursorPos(ImVec2(x, y));
                    ui.gui->AlignTextToFramePadding();
                    ui.Text(kMuted, "Weak");
                    ui.gui->SetCursorPos(ImVec2(x + cut + ui.style.ItemSpacing.x, y));
                    ui.gui->AlignTextToFramePadding();
                    ui.Text(kMuted, "Resist");
                    ui.EndRowAt(ui.cardX, y, ui.gui->GetFrameHeight());
                }
                for (int i = 0; i < kStatusRankCount; ++i)
                {
                    StatusBadgeRule& rule = s.statusBadges[i];
                    const char* status    = StatusRankLabel(static_cast<StatusRank>(i));
                    ui.Fade(dim);
                    const float y = ui.Row(status, nullptr, width);
                    const float x = ui.gui->GetCursorPosX();
                    char id[48];
                    std::snprintf(id, sizeof(id), "##status %s", status);
                    ui.SwitchAt(id, rule.show);
                    ui.Fade(dim || !rule.show);
                    ui.gui->SetCursorPos(ImVec2(x + ui.Em(kSwitchWidth) + ui.style.ItemSpacing.x, y));
                    std::snprintf(id, sizeof(id), "weak %s", status);
                    ui.DropdownAt(id, rule.weakRank, kMaxWeakRank, kNoWeakRank, cutoff, cut);
                    ui.gui->SetCursorPos(ImVec2(x + ui.Em(kSwitchWidth) + 2.0f * ui.style.ItemSpacing.x + cut, y));
                    std::snprintf(id, sizeof(id), "resist %s", status);
                    ui.DropdownAt(id, rule.resistRank, kMinResistRank, kNoResistRank, cutoff, cut);
                    ui.EndRowAt(ui.cardX, y, ui.gui->GetFrameHeight());
                }
                ui.EndCard();
            }
        }

        // typed is the add box's text, kept between frames: ImGui takes typing back on Escape only from a buffer that
        // has kept it.
        void DrawFriends(Ui& ui, Settings& s, std::unordered_set<std::string>& collapsed,
            char (&typed)[kMaxCharacterName + 1])
        {
            ui.Fade(!s.enabled);
            if (!ui.BeginCard("Friends", collapsed, nullptr,
                    "Players named here are Friends, in the rows above and in the Target Type colors. Click a name to "
                    "remove it."))
                return;
            const float left = ui.cardX + ui.Em(kCardPad);
            for (size_t i = 0; i < s.friends.size(); ++i)
            {
                const std::string shown = s.friends[i] + "  x";
                char chip[64];
                std::snprintf(chip, sizeof(chip), "%s##friend %zu", shown.c_str(), i);
                ui.PlaceChip(i == 0, left, ui.ChipRoom(shown.size()));
                if (!ui.Shape(chip, ImVec2(0.0f, 0.0f), kRaised, ui.gui->GetFrameHeight() / 2.0f, kRaisedEdge)) continue;
                s.friends.erase(s.friends.begin() + static_cast<std::ptrdiff_t>(i));
                ui.changed = true;
                break;
            }
            ui.PlaceChip(s.friends.empty(), left, ui.Em(kFilterWidth));
            ui.gui->SetNextItemWidth(ui.Em(kFilterWidth));
            ui.SquareFrames([&] {
                return ui.gui->InputTextWithHint("##add friend", "Add a Name", typed, sizeof(typed),
                    ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
            });
            const std::string rule = "A character's name, " + std::to_string(kMinCharacterName) + " to " +
                                     std::to_string(kMaxCharacterName) + " letters. Enter adds it.";
            ui.Tip(rule.c_str());
            if (ui.gui->IsItemDeactivatedAfterEdit())
                if (std::optional<std::string> added = FriendToAdd(s.friends, typed))
                {
                    s.friends.push_back(std::move(*added));
                    typed[0]   = '\0';
                    ui.changed = true;
                }
            ui.EndCard();
        }

        void DrawTimers(Ui& ui, Settings& s, std::unordered_set<std::string>& collapsed)
        {
            ui.Fade(!s.enabled);
            if (!ui.BeginCard("Placeholder Timers", collapsed)) return;
            ui.Switch("Respawn Timers", s.phTimers,
                "Above your name, a countdown for each placeholder you see die, as Bigmouth Billy [11A] 3:15. It reads "
                "\"up\" when due.");
            ui.Fade(!s.enabled || !s.phTimers);
            ui.SliderInt("Timer Size", s.timerPercent, kMinSizePercent, kMaxSizePercent,
                SizeFormat(s, s.timerPercent).c_str(), "Of the base font size on the Entities page.");
            ui.EndCard();
        }
    }

    void DrawEntitiesPage(Ui& ui, Settings& s, const MenuStatus& status, std::unordered_set<std::string>& collapsed,
        const std::vector<std::string>& fonts)
    {
        ui.Fade(!s.enabled);
        if (ui.BeginCard("Nameplate Style", collapsed, &s.nameplates,
                "HeadsUp's nameplates: names, levels, icons and the rest, as each kind's page designs them."))
        {
            ui.Fade(!s.enabled || !s.nameplates);
            ui.SliderInt("Vertical Offset", s.nameRaise, kMinNameRaise, kMaxNameRaise, "%d px",
                "How far above the game's own spot names sit.");
            ui.Switch("Stack Names", s.stackNames,
                "Names that would cover each other rise one over another. Your target's keeps its place.");
            ui.SliderInt("Base Font Size", s.baseSize, kMinTextSize, kMaxTextSize, "%d px",
                "Every line's text and icon size is a percent of this.");
            ui.FontDropdown("Default Font", s.defaultFont, fonts, "The font of every line whose own font is Default.");
            ui.Switch("Scale with Distance", s.scaleWithDistance, "Sizes follow how far a name's owner is from you.");
            ui.Fade(!s.enabled || !s.nameplates || !s.scaleWithDistance);
            ui.SliderInt("Min Size", s.scaleMin, kMinSizePercent, kMaxSizePercent, "%d%%",
                "The smallest a far name shrinks to.");
            ui.SliderInt("Max Size", s.scaleMax, kMinSizePercent, kMaxSizePercent, "%d%%",
                "The largest a near name grows to.");
            ui.Switch("Scale Your Own Name", s.scaleOwnName, "Off, your own nameplate stays one size.");
            ui.EndCard();
        }
        ui.Fade(!s.enabled);
        if (ui.BeginCard("Outline Style", collapsed, &s.outlines,
                "Colored borders around mobs, and anything else whose rows ask for one."))
        {
            ui.Fade(!s.enabled || !s.outlines);
            ui.SliderFloat("Thickness", s.thickness, kMinThickness, kMaxThickness, "%.1f px", nullptr);
            ui.SliderInt("Smoothness", s.smoothness, kMinSmoothness, kMaxSmoothness, "%d copies",
                "More is rounder and costs a little more.");
            ui.SliderFloat("Range", s.maxDistance, kMinOutlineDistance, kMaxOutlineDistance, "%.0f yalms",
                "Anything farther gets no outline.");
            if (!status.stencilAvailable) ui.Note("Outlines are unavailable: the depth buffer has no stencil bits.");
            ui.EndCard();
        }
        ui.Fade(!s.enabled);
        if (ui.BeginCard("Target Cursor", collapsed, &s.replaceCursor,
                "Draws HeadsUp's cursor above the target's nameplate instead of the game's."))
        {
            ui.Fade(!s.enabled || !s.replaceCursor);
            ui.Switch("Feather", s.cursorFeather, "Phoenix's feather instead of the arrow.");
            ui.SliderInt("Size", s.cursorSize, kMinTextSize, kMaxTextSize, "%d px", nullptr);
            ui.SliderInt("Outline", s.cursorOutline, kMinCursorOutline, kMaxCursorOutline, "%d%%",
                "Its dark edge, as a share of the usual thickness.");
            ui.Switch("Scale", s.scaleCursor, "Larger the nearer your target is.");
            ui.Swatch("Edge Color", s.cursorEdgeColor, "The cursor's dark edge.");
            ui.Note("Its colors are the Target Status palette below.");
            ui.EndCard();
        }
        ui.Fade(!s.enabled);
        DrawPalette(ui, "Colors: Aggression",
            "By whether a mob would attack you, for ordinary mobs, notorious monsters and their placeholders.", collapsed,
            s.palette.aggression, kNatureCount, NatureLabel);
        DrawPalette(ui, "Colors: Toughness",
            "By how a mob cons to you. Easy Prey covers Incredibly Easy Prey, and Very Tough covers Incredibly Tough.",
            collapsed, s.palette.toughness, kLabelShadeCount, ToughnessLabel);
        DrawPalette(ui, "Colors: Claim", "By who has claimed a mob.", collapsed, s.palette.claim, kClaimStateCount,
            ClaimLabel);
        DrawPalette(ui, "Colors: Target Status", "By whether it is your target. The target cursor takes these too.",
            collapsed, s.palette.target, kTargetStateCount, TargetStateLabel, TargetStateTip);
        DrawPalette(ui, "Colors: Target Type", "By what kind of entity it is.", collapsed, s.palette.type,
            kTargetTypeCount, TargetTypeLabel);
        ui.Fade(false);
    }

    void DrawKindPage(Ui& ui, Settings& s, PlateKind kind, std::unordered_set<std::string>& collapsed,
        const std::vector<std::string>& fonts, DesignEditor& editor)
    {
        if (editor.kind != kind) editor = DesignEditor{.kind = kind};
        const KindSettings& k = s.Kind(kind);
        const int rows        = RowCountOf(kind);
        int order[kMaxRows]   = {}; // the rows as the menu lists them
        bool outlineColors = false, plates = false;
        for (int i = 0; i < rows; ++i)
        {
            order[i] = kind == PlateKind::Mobs ? CategoryIndex(kMobRowOrder[i]) : i;
            outlineColors |= HasOutlineColor(k.rows[order[i]].outline);
            plates |= DrawsPlate(k.rows[order[i]].plate);
        }
        DrawVisibility(ui, s, kind, order, rows, collapsed);
        if (outlineColors) DrawOutlineDesign(ui, s, kind, order, rows, collapsed);
        if (plates)
        {
            DrawNameplateDesign(ui, s, kind, collapsed, editor);
            DrawLineStyles(ui, s, kind, collapsed, fonts, editor);
            DrawOrnament(ui, s, kind, collapsed);
        }
        switch (kind)
        {
        case PlateKind::Mobs: DrawMobCards(ui, s, collapsed, fonts); break;
        case PlateKind::Players: DrawFriends(ui, s, collapsed, editor.friendName); break;
        case PlateKind::Self: DrawTimers(ui, s, collapsed); break;
        default: break;
        }
        ui.Fade(false);
    }
}
