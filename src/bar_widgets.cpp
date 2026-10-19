#include "bar_widgets.h"

#include "argb.h"
#include "bar_game_help.h"
#include "bar_job.h"
#include "bar_server_time.h"
#include "npcdata.h"
#include "ph_timers.h"
#include "titles.h"
#include "vanadiel.h"
#include "zones.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <span>
#include <string_view>
#include <utility>

namespace headsup
{
    namespace
    {
        const char* const kContainerNames[kContainerCount] = {"Inventory", "Mog Safe", "Storage", "Temporary",
            "Mog Locker", "Mog Satchel", "Mog Sack", "Mog Case", "Mog Wardrobe", "Mog Safe 2", "Mog Wardrobe 2",
            "Mog Wardrobe 3", "Mog Wardrobe 4", "Mog Wardrobe 5", "Mog Wardrobe 6", "Mog Wardrobe 7", "Mog Wardrobe 8"};
        constexpr const char* kItemsMenu = "Items";
        // The Mog House's own short captions for some of them, as the game's strip titles their lists.
        struct ShortCaption
        {
            const char* caption;
            int container;
        };
        constexpr ShortCaption kShortCaptions[] = {{"Locker", 4}, {"M.Ward.", 8}, {"M.Ward. 2", 10}, {"M.Ward. 3", 11},
            {"M.Ward. 4", 12}, {"M.Ward. 5", 13}, {"M.Ward. 6", 14}, {"M.Ward. 7", 15}, {"M.Ward. 8", 16}};
        constexpr uint32_t kElementColors[kElementCount] = {0xFFFF8A3C, 0xFF8FD8FF, kBarGreen, 0xFFD8B25A, 0xFFC58CFF,
            0xFF5B9DFF, 0xFFFFFFFF, 0xFF9A8AA8};
        constexpr int kDamageColumns = 4, kElementColumns = 4, kStatusColumns = 5, kStatColumns = 4, kBagColumns = 2;
        constexpr int kListColumns = 3; // TP moves and spells
        constexpr const char* kStatNames[kStatCount] = {"STR", "DEX", "VIT", "AGI", "INT", "MND", "CHR", "MEva"};
        constexpr const char* kCombatNames[kCombatCount] = {"HP", "MP", "ATT", "DEF", "EVA", "ACC"};
        constexpr const char* kDamageKindNames[kDamageKindCount] = {"Physical", "Magic", "Ranged", "Breath"};
        constexpr std::pair<MobTrait, const char*> kTraitNames[] = {{kTraitRegen, "Regen"}, {kTraitRefresh, "Refresh"},
            {kTraitRegain, "Regain"}, {kTraitShadows, "Shadows"}, {kTraitCounter, "Counter"}};
        constexpr int kWholeAbsorb = 100; // percent
        constexpr int kForecastDays = 5, kForecastColumns = 4;
        constexpr uint16_t kGilItem  = 0xFFFF;

        Span Percent(int percent)
        {
            return Text(PercentText(percent), percent > 0 ? Tone::Weak : percent < 0 ? Tone::Resist : Tone::Neutral);
        }

        Span Ranked(std::string text, int rank)
        {
            return Text(std::move(text), rank < 0 ? Tone::Weak : rank > 0 ? Tone::Resist : Tone::Neutral);
        }

        // The data gives zone 0, the unknown one, a calendar of its own, so only a known zone has odds.
        std::optional<WeatherOdds> OddsOn(const BarState& state, uint32_t vanaDay)
        {
            const ZoneRecord* zone = KnownZone(state.zone);
            return zone != nullptr ? WeatherOn(*zone, vanaDay) : std::nullopt;
        }

        bool IsMob(const ActorInfo* target)
        {
            return target != nullptr && target->kind == EntityKind::Mob && target->mob != nullptr;
        }

        // An NPC target's record; nullptr for anything else, or an NPC the data has nothing to show of.
        const NpcRecord* NpcOf(const ActorInfo* target)
        {
            return target != nullptr && target->kind == EntityKind::Npc ? NpcById(target->serverId) : nullptr;
        }

        // "WAR 75 / NIN 37", or the game's "? ? ?" for an anonymous player, whose jobs come as 0.
        std::string CheckedJobs(const InspectGeneral& g)
        {
            return g.mainJob == 0 ? "? ? ?" : JobsText(g.mainJob, g.mainLevel, g.subJob, g.subLevel);
        }

        // The tracker fills a mob's level only while it lives; a dead target has none, but its record still names
        // its aggro or passive icon.
        bool HasLevel(const ActorInfo& target) { return target.level.text[0] != '\0'; }

        // Every icon of its nature: aggro first, then linking and how it detects.
        std::vector<Icon> MobIcons(const ActorInfo& target)
        {
            const IconSet icons = IconsFor(target.mob, kAllDetection);
            return std::vector<Icon>(icons.icons, icons.icons + icons.count);
        }

        std::optional<Icon> MobIcon(const ActorInfo& target)
        {
            const std::vector<Icon> icons = MobIcons(target);
            return icons.empty() ? std::nullopt : std::optional<Icon>(icons.front());
        }

        uint32_t NameColor(const ActorInfo& target, const Settings& settings) { return RowColor(target.state, settings); }

        uint32_t ShadeColor(const ActorInfo& target, const Settings& settings)
        {
            return ToArgb(settings.palette.toughness[static_cast<int>(target.level.shade)]);
        }

        // "15y": a sense's range in yalms.
        std::string Yalms(int yalms) { return std::to_string(yalms) + "y"; }

        // The nameplate's icons as words, so the popup and the plates say the same, sight and sound with how far
        // they reach, then NM and whether a beastmaster can charm it.
        PopupRow Tags(const MobRecord& mob)
        {
            const IconSet icons = IconsFor(&mob, kAllDetection);
            const MobInfo& info = InfoOf(mob);
            PopupRow row{RowKind::Tags, 0, {}};
            for (int i = 0; i < icons.count; ++i)
            {
                std::string tag = IconTag(icons.icons[i]);
                if (icons.icons[i] == Icon::Sight || icons.icons[i] == Icon::TrueSight) tag += " " + Yalms(info.sight);
                if (icons.icons[i] == Icon::Sound) tag += " " + Yalms(info.sound);
                row.cells.push_back(Cell(Text(std::move(tag), Tone::Tag)));
            }
            if (mob.flags & kMobNotorious) row.cells.push_back(Cell(Text("NM", Tone::Tag)));
            if (mob.flags & kMobCharmable) row.cells.push_back(Cell(Text("Charmable", Tone::Tag)));
            return row;
        }

        // What it does by itself, and what its melee hits add.
        PopupRow Traits(const MobRecord& mob)
        {
            const MobInfo& info = InfoOf(mob);
            PopupRow row{RowKind::Tags, 0, {}};
            for (const auto& [trait, name] : kTraitNames)
                if (info.traits & trait) row.cells.push_back(Cell(Text(name, Tone::Tag)));
            if (*info.addEffect != '\0')
                row.cells.push_back(Cell(Text(std::string_view(info.addEffect) == kUnknownAddEffect
                                                  ? std::string("Additional effect")
                                                  : std::string("Additional effect: ") + info.addEffect,
                    Tone::Tag)));
            return row;
        }

        // "THF/BLM"; nothing for a mob without a job.
        std::string MobJobs(const MobInfo& info)
        {
            const std::string main = JobName(info.mainJob), sub = JobName(info.subJob);
            return main.empty() ? std::string() : sub.empty() ? main : main + "/" + sub;
        }

        // A cell of a name and its number, as the stats are.
        PopupCell Numbered(const char* name, int64_t value)
        {
            return Cell(ListOf(Text(name, Tone::Dim), Text(" " + std::to_string(value))));
        }

        // A grid of names, kListColumns a line; none when there are no names.
        std::optional<PopupSection> NameList(std::string heading, const std::vector<std::string>& names)
        {
            if (names.empty()) return std::nullopt;
            PopupRow grid = Grid(kListColumns, names.size());
            for (const std::string& name : names)
                grid.cells.push_back(Cell(Text(name)));
            return PopupSection{std::move(heading), ListOf(std::move(grid))};
        }

        // The spells it casts at a level: each whose levels hold it, by the game's name.
        std::vector<std::string> SpellsAt(const MobRecord& mob, int level, const BarState& state)
        {
            std::vector<std::string> names;
            if (state.spellName == nullptr) return names;
            for (const MobSpell& spell : SpellsOf(mob))
            {
                const char* name = level >= spell.minLevel && level <= spell.maxLevel ? state.spellName(spell.spell) : nullptr;
                if (name != nullptr && *name != '\0') names.emplace_back(name);
            }
            return names;
        }

        // "20:00"
        std::string Hour(int hour) { return Printed("%02d:00", hour); }

        // Phoenix's Treasure Hunter tiers in per mille: a rate rolls as the highest at or below it (mobdata.h).
        constexpr uint16_t kDropTiers[] = {1000, 240, 150, 100, 50, 10, 5, 1};
        constexpr double kPerMillePerPercent = 10.0;

        uint16_t Tier(uint16_t rate)
        {
            for (const uint16_t tier : kDropTiers)
                if (rate >= tier) return tier;
            return 0;
        }

        // "24%", "0.5%", "1.7%".
        std::string ChanceText(double perMille)
        {
            const double percent = perMille / kPerMillePerPercent;
            return percent == std::floor(percent) ? Printed("%.0f%%", percent) : Printed("%.1f%%", percent);
        }

        // What a mob drops at Treasure Hunter 0, and what can be stolen or despoiled from it: each by the game's
        // name, without the entries a group picks for nothing.
        std::vector<PopupSection> Loot(const MobRecord& mob, const BarState& state)
        {
            PopupSection drops{"Drops", {}}, steal{"Steal", {}}, despoil{"Despoil", {}};
            for (const MobDrop& d : DropsOf(mob))
            {
                const char* name = state.itemName != nullptr ? state.itemName(d.item) : nullptr;
                if (d.item == 0 || name == nullptr || *name == '\0') continue;
                switch (d.kind)
                {
                case DropKind::Drop:
                {
                    double chance = Tier(d.rate);
                    if (d.group != 0)
                    {
                        const MobDropGroup& g = DropGroupOf(mob, d.group);
                        chance = g.weight > 0 ? static_cast<double>(g.rate) * d.rate / g.weight : 0.0;
                    }
                    if (chance > 0.0) drops.rows.push_back(Labeled(name, Text(ChanceText(chance))));
                    break;
                }
                case DropKind::Steal: steal.rows.push_back(LineOf(name)); break;
                case DropKind::Despoil: despoil.rows.push_back(LineOf(name)); break;
                }
            }
            std::vector<PopupSection> loot;
            for (PopupSection* s : {&drops, &steal, &despoil})
                if (!s->rows.empty()) loot.push_back(std::move(*s));
            return loot;
        }

        PopupSection Spawn(const MobRecord& mob, const BarState& state)
        {
            PopupSection spawn{"Spawn", {}};
            const MobInfo& info = InfoOf(mob);
            spawn.rows.push_back(LineOf(mob.respawn > 0 ? "Respawn " + Countdown(mob.respawn) : "No respawn timer"));
            if (info.windowStart != kNoWindow)
                spawn.rows.push_back(LineOf("Spawns " + Hour(info.windowStart) + " to " + Hour(info.windowEnd)));
            if (info.lotteryChance > 0)
            {
                spawn.rows.push_back(LineOf(std::to_string(info.lotteryChance) + "% pop chance"));
                if (info.lotteryCooldown > 0)
                    spawn.rows.push_back(LineOf(Countdown(info.lotteryCooldown) + " cooldown after its kill"));
            }
            if (mob.placeholderOf != 0)
            {
                const MobRecord* nm = MobById(mob.placeholderOf);
                const int chance    = nm != nullptr ? InfoOf(*nm).lotteryChance : 0;
                spawn.rows.push_back(LineOf(std::string("Placeholder for ") + (nm != nullptr ? nm->name : "an NM") +
                    (chance > 0 ? " (" + std::to_string(chance) + "%)" : "")));
                if (state.targetPhSeconds)
                {
                    const double left = *state.targetPhSeconds;
                    spawn.rows.push_back(LineOf(left > 0.0 ? "Back in " + Countdown(left) : "Due now", Tone::Weak));
                }
            }
            if (mob.placeholders > 0)
                spawn.rows.push_back(LineOf(std::to_string(mob.placeholders) +
                    (mob.placeholders == 1 ? " placeholder" : " placeholders") + " in this zone"));
            if (mob.placeholderOf == 0 && mob.placeholders == 0)
                spawn.rows.push_back(LineOf("Not a placeholder", Tone::Dim));
            return spawn;
        }

        // Items by the game's names, gil as "Gil", without those it has no name for.
        std::vector<std::string> ItemNames(std::span<const uint16_t> items, const BarState& state)
        {
            std::vector<std::string> names;
            for (const uint16_t item : items)
            {
                const char* name = item == kGilItem              ? "Gil"
                                   : state.itemName != nullptr ? state.itemName(item)
                                                               : nullptr;
                if (name != nullptr && *name != '\0') names.emplace_back(name);
            }
            return names;
        }

        // The mobs it pops by Phoenix's names, each once.
        std::vector<std::string> MobNames(std::span<const uint32_t> mobs)
        {
            std::vector<std::string> names;
            for (const uint32_t id : mobs)
            {
                const MobRecord* mob = MobById(id);
                if (mob != nullptr && std::ranges::find(names, mob->name) == names.end()) names.emplace_back(mob->name);
            }
            return names;
        }

        std::vector<std::string> KeyItemNames(std::span<const uint16_t> keyItems)
        {
            std::vector<std::string> names;
            for (const uint16_t keyItem : keyItems)
                if (const char* name = KeyItemNameOf(keyItem)) names.emplace_back(name);
            return names;
        }

        constexpr size_t kNpcTextChars = 44; // a message's line in the popup
        // Before what an act leads to; a font without the arrow draws Montserrat's (IsFallbackFont).
        constexpr const char* kResult = "\xE2\x86\x92 ";

        std::string JoinedNames(const std::vector<std::string>& names)
        {
            std::string text;
            for (const std::string& name : names)
                text += (text.empty() ? "" : ", ") + name;
            return text;
        }

        // A message's words with its parameters in their <item>, <keyitem> and <number>, in order; other marks stay.
        std::string SayWords(const NpcSay& say, const BarState& state)
        {
            const char* words = WordsOf(say);
            if (words == nullptr) return {};
            const std::span<const uint32_t> params = ParamsOf(say);
            size_t next = 0;
            std::string out;
            for (std::string_view rest = words; !rest.empty();)
            {
                const size_t open  = rest.find('<');
                const size_t close = open == std::string_view::npos ? open : rest.find('>', open);
                if (close == std::string_view::npos)
                {
                    out += rest;
                    break;
                }
                out += rest.substr(0, open);
                const std::string_view tag = rest.substr(open + 1, close - open - 1);
                std::string filled(rest.substr(open, close - open + 1));
                if ((tag == "item" || tag == "keyitem" || tag == "number") && next < params.size())
                {
                    const uint32_t value = params[next++];
                    const char* name     = tag == "number"   ? nullptr
                                           : tag == "keyitem" ? KeyItemNameOf(static_cast<uint16_t>(value))
                                           : state.itemName != nullptr ? state.itemName(static_cast<uint16_t>(value))
                                                                       : nullptr;
                    if (tag == "number") filled = std::to_string(value);
                    else if (name != nullptr && *name != '\0') filled = name;
                }
                out += filled;
                rest = rest.substr(close + 1);
            }
            return out;
        }

        // "22:00", or "22:00-03:59" for a run of hours.
        std::string HourRange(int from, int to) { return from == to ? Hour(from) : Hour(from) + "-" + Printed("%02d:59", to); }

        // An act's conditions in words, its quest and step first: "In A Chocobos Tale, step 4, after its NM falls".
        std::string WhenWords(const NpcAct& act, bool capital = true)
        {
            std::vector<std::string> first, rest;
            for (const NpcWhenEntry& when : WhenOf(act))
            {
                const auto keyItem = [&] {
                    const char* name = KeyItemNameOf(when.a);
                    return std::string(name != nullptr ? name : "a key item");
                };
                switch (when.kind)
                {
                case kWhenHour: rest.push_back("at " + HourRange(when.a, when.b)); break;
                case kWhenWeather:
                    if (const std::optional<Weather> weather = WeatherFromId(when.a))
                        rest.push_back(std::string("in ") + WeatherName(*weather));
                    break;
                case kWhenJob: rest.push_back(std::string("as ") + JobName(static_cast<uint8_t>(when.a))); break;
                case kWhenDay: rest.push_back(std::string("on ") + WeekdayName(when.a)); break;
                case kWhenNation:
                    rest.push_back(std::string("for ") + NationName(static_cast<Nation>(when.a + 1)) + "'s citizens");
                    break;
                case kWhenKeyItem: rest.push_back("holding " + keyItem()); break;
                case kWhenNoKeyItem: rest.push_back("without " + keyItem()); break;
                case kWhenKeyItems: rest.push_back("holding its key items"); break;
                case kWhenQuest: rest.push_back("during its quest"); break;
                case kWhenLink:
                    if (const NpcLink* link = LinkAt(when.a)) first.insert(first.begin(), std::string("in ") + link->name);
                    break;
                case kWhenStep: first.push_back("step " + std::to_string(when.a)); break;
                case kWhenPrimed: rest.push_back("once readied"); break;
                case kWhenNmKilled: rest.push_back("after its NM falls"); break;
                case kWhenRoll: rest.push_back("on a lucky roll"); break;
                case kWhenMob: rest.push_back(std::string("when ") + when.name + " dies"); break;
                case kWhenZoneStart: rest.push_back("as the zone starts"); break;
                default: break;
                }
            }
            first.insert(first.end(), rest.begin(), rest.end());
            std::string text = JoinedNames(first);
            if (capital && !text.empty()) text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
            return text;
        }

        // An act's rows: its conditions, dim, what it says, then what follows it.
        void AddActRows(PopupSection& section, const NpcAct& act, const BarState& state)
        {
            if (const std::string when = WhenWords(act); !when.empty()) section.rows.push_back(LineOf(when, Tone::Dim));
            for (const NpcSay& say : SaysOf(act))
                for (const std::string& line : WrapWords(SayWords(say, state), kNpcTextChars))
                    section.rows.push_back(LineOf(line));
            if (const std::vector<std::string> pops = MobNames(PopsOf(act)); !pops.empty())
                section.rows.push_back(LineOf(kResult + std::string("Pops ") + JoinedNames(pops)));
            if (const std::vector<std::string> gives = ItemNames(GivesOf(act), state); !gives.empty())
                section.rows.push_back(LineOf(kResult + std::string("Gives ") + JoinedNames(gives)));
            if (const std::vector<std::string> keyItems = KeyItemNames(KeyItemsOf(act)); !keyItems.empty())
                section.rows.push_back(LineOf(kResult + std::string("Gives key item ") + JoinedNames(keyItems)));
            if (act.event) section.rows.push_back(LineOf(kResult + std::string("A cutscene"), Tone::Dim));
        }

        // What clicking it does, and what each trade the probe found does, its items first; none when the probe saw
        // nothing. A trade the probe found takes the place of the plain list of what its scripts check for.
        bool AddActs(std::vector<PopupSection>& sections, const NpcRecord& npc, const BarState& state)
        {
            PopupSection click{"Click", {}}, trade{"Trade", {}};
            for (const NpcAct& act : ActsOf(npc))
            {
                if (act.kind == kActClick) AddActRows(click, act, state);
                if (act.kind != kActTrade) continue;
                std::vector<std::string> items;
                for (const NpcActItem& entry : ItemsOf(act))
                {
                    const std::vector<std::string> named = ItemNames(std::span<const uint16_t>(&entry.item, 1), state);
                    const std::string name = named.empty() ? "an item" : named.front();
                    items.push_back(entry.qty > 1 ? std::to_string(entry.qty) + " x " + name : name);
                }
                std::string line;
                for (const std::string& item : items)
                    line += (line.empty() ? "" : " + ") + item;
                trade.rows.push_back(LineOf(line));
                AddActRows(trade, act, state);
            }
            if (!click.rows.empty()) sections.push_back(std::move(click));
            const bool traded = !trade.rows.empty();
            if (traded) sections.push_back(std::move(trade));
            return traded;
        }

        // When its zone shows and hides it: "Appears at 22:00, gone at 07:00"; empty for a ??? always there.
        std::string AppearsWords(const NpcRecord& npc)
        {
            std::vector<std::string> shows, hides;
            for (const NpcAct& act : ActsOf(npc))
            {
                const auto when = WhenOf(act);
                if (when.size() == 1 && when[0].kind == kWhenZoneStart) continue;
                if (act.kind == kActShows) shows.push_back(WhenWords(act, false));
                if (act.kind == kActHides) hides.push_back("gone " + WhenWords(act, false));
            }
            if (shows.empty()) return {};
            shows.insert(shows.end(), hides.begin(), hides.end());
            return "Appears " + JoinedNames(shows);
        }

        // A name a line, and a dim note when its scripts do more of it than the data could read; none when there is
        // neither.
        void AddListing(std::vector<PopupSection>& sections, std::string heading, const std::vector<std::string>& names,
            bool unread = false)
        {
            if (names.empty() && !unread) return;
            PopupSection section{std::move(heading), {}};
            for (const std::string& name : names)
                section.rows.push_back(LineOf(name));
            if (unread)
                section.rows.push_back(
                    LineOf(names.empty() ? "Worked out by its script" : "And more its script works out", Tone::Dim));
            sections.push_back(std::move(section));
        }

        // What a shop always stocks, then what a nation's shop holds back until the nation places well in conquest,
        // under the place it needs (NpcStockItem's ranks).
        constexpr std::pair<uint8_t, const char*> kStockPlaces[] = {
            {2, "When its nation is 1st or 2nd in conquest"}, {1, "When its nation is 1st in conquest, to its citizens"}};

        void AddShop(std::vector<PopupSection>& sections, const NpcRecord& npc, const BarState& state)
        {
            const NpcShop* shop  = ShopOf(npc);
            const bool unread    = (npc.unread & kNpcUnreadShop) != 0;
            PopupSection section = {"Shop", {}};
            const auto add = [&](uint8_t place, const char* note) {
                bool first = true;
                for (const NpcStockItem& entry : shop != nullptr ? StockOf(*shop) : std::span<const NpcStockItem>())
                {
                    const uint8_t at  = entry.rank == 1 || entry.rank == 2 ? entry.rank : 0;
                    const char* name = state.itemName != nullptr ? state.itemName(entry.item) : nullptr;
                    if (at != place || name == nullptr || *name == '\0') continue;
                    if (first && note != nullptr) section.rows.push_back(LineOf(note, Tone::Dim));
                    first = false;
                    // The name in the normal tone: the dim lines naming a place are the headings among them.
                    section.rows.push_back(Labeled(Text(name), Text(Grouped(entry.price) + " gil")));
                }
            };
            add(0, nullptr);
            for (const auto& [place, note] : kStockPlaces)
                add(place, note);
            if (section.rows.empty() && !unread) return;
            if (!section.rows.empty() && shop->fame)
                section.rows.push_back(LineOf("Prices before fame, which moves them up to 10% either way", Tone::Dim));
            if (unread)
                section.rows.push_back(LineOf(
                    section.rows.empty() ? "Worked out by its script" : "And more its script works out", Tone::Dim));
            sections.push_back(std::move(section));
        }

        // Its quests (hidden ones too), missions and battlefields, each by name, a quest's and mission's log after it.
        void AddLinks(std::vector<PopupSection>& sections, const NpcRecord& npc)
        {
            PopupSection quests{"Quests", {}}, missions{"Missions", {}}, battlefields{"Battlefields", {}};
            for (const NpcLink* link : LinksOf(npc))
            {
                const char* log = LogNameOf(*link);
                switch (link->kind)
                {
                case kLinkQuest:
                case kLinkHidden:
                    quests.rows.push_back(Labeled(Text(link->name), Text(log != nullptr ? log : "Hidden", Tone::Dim)));
                    break;
                case kLinkMission:
                    missions.rows.push_back(Labeled(Text(link->name), Text(log != nullptr ? log : "", Tone::Dim)));
                    break;
                default: battlefields.rows.push_back(LineOf(link->name)); break;
                }
            }
            for (PopupSection* s : {&quests, &missions, &battlefields})
                if (!s->rows.empty()) sections.push_back(std::move(*s));
        }

        // What it is for, its shop, what it takes in trade and what that pops, what it gives, the key items it gives
        // and checks for, and its quests, missions and battlefields.
        std::vector<PopupSection> NpcSections(const ActorInfo& target, const NpcRecord& npc, const BarState& state)
        {
            std::vector<PopupSection> sections = ListOf(PopupSection{"",
                ListOf(Line(Cell(Text("ID " + Printed("%03X", target.serverId & kTargetIndexBits), Tone::Dim))))});
            // What it is for, as a mob's tags say what it does.
            PopupRow purposes{RowKind::Tags, 0, {}};
            for (const char* purpose : PurposesOf(npc))
                purposes.cells.push_back(Cell(Text(purpose, Tone::Tag)));
            if (!purposes.cells.empty()) sections[0].rows.push_back(std::move(purposes));
            if (const std::string appears = AppearsWords(npc); !appears.empty()) sections[0].rows.push_back(LineOf(appears));
            AddShop(sections, npc, state);
            if (!AddActs(sections, npc, state))
                AddListing(sections, "Trade", ItemNames(TradesOf(npc), state), (npc.unread & kNpcUnreadTrade) != 0);
            AddListing(sections, "Pops", MobNames(PopsOf(npc)), (npc.unread & kNpcUnreadPops) != 0);
            AddListing(sections, "Gives", ItemNames(GivesOf(npc), state), (npc.unread & kNpcUnreadGives) != 0);
            AddListing(sections, "Key items", KeyItemNames(KeyItemsOf(npc)));
            AddListing(sections, "Checks for", KeyItemNames(NeedsOf(npc)));
            AddLinks(sections, npc);
            return sections;
        }

        PopupContent TargetPopup(const BarState& state, const Settings& settings)
        {
            PopupContent popup;
            const ActorInfo* target = state.target;
            if (target == nullptr)
            {
                popup.title = ListOf(Text("No target", Tone::Dim));
                return popup;
            }
            if (!IsMob(target))
            {
                popup.title = ListOf(Text(target->name));
                if (target->kind == EntityKind::Player && state.targetCheck)
                {
                    const PlayerCheck& c = *state.targetCheck;
                    PopupSection rows;
                    rows.rows.push_back(Labeled("Jobs", Text(CheckedJobs(c.general))));
                    if (c.general.masterLevel > 0)
                        rows.rows.push_back(Labeled("Master level", Text(std::to_string(c.general.masterLevel))));
                    if (!c.general.linkshell.empty())
                        rows.rows.push_back(Labeled("Linkshell", Text(c.general.linkshell)));
                    if (const std::string title = TitleName(c.message.title); !title.empty())
                        rows.rows.push_back(Labeled("Title", Text(title)));
                    else if (c.message.title != 0)
                        rows.rows.push_back(Labeled("Title", Text("#" + std::to_string(c.message.title))));
                    rows.rows.push_back(LineOf("As of your check", Tone::Dim));
                    popup.sections = ListOf(std::move(rows));
                }
                else if (const NpcRecord* npc = NpcOf(target))
                    popup.sections = NpcSections(*target, *npc, state);
                return popup;
            }
            const MobRecord& mob      = *target->mob;
            const MobResistances& res = ResistancesOf(mob);
            popup.icon  = MobIcon(*target);
            popup.title = ListOf(Colored(target->name, NameColor(*target, settings)));
            const MobDefenses& defenses = DefensesOf(mob);
            PopupRow identity{RowKind::Line, 0, {}};
            if (HasLevel(*target))
                identity.cells.push_back(Cell(Colored(target->level.text, ShadeColor(*target, settings))));
            if (const std::string jobs = MobJobs(InfoOf(mob)); !jobs.empty()) identity.cells.push_back(Cell(Text(jobs)));
            identity.cells.push_back(Cell(Text("ID " + Printed("%03X", target->serverId & kTargetIndexBits), Tone::Dim)));

            PopupRow damage = Grid(kDamageColumns, kFirstElementSdt);
            for (int i = 0; i < kFirstElementSdt; ++i)
                damage.cells.push_back(Cell(Percent(SdtPercent(res.sdt[i])), SdtIcon(i)));
            PopupSection damageTaken{"Damage taken", ListOf(std::move(damage))};
            PopupRow kinds = Grid(kDamageColumns, kDamageKindCount);
            for (int k = 0; k < kDamageKindCount; ++k)
                if (const int percent = SdtPercent(defenses.damageTaken[k]); percent != 0)
                    kinds.cells.push_back(Cell(ListOf(Text(kDamageKindNames[k], Tone::Dim), Text(" "), Percent(percent))));
            if (!kinds.cells.empty()) damageTaken.rows.push_back(std::move(kinds));

            const RankNumbers numbers{settings.rankDisplay, settings.countAccuracy, state.caster ? &*state.caster : nullptr,
                MagicTargetOf(mob, target->checked ? target->checked->level : 0)};
            PopupRow elements = Grid(kElementColumns, kElementCount);
            for (int e = 0; e < kElementCount; ++e)
            {
                const int percent = SdtPercent(res.sdt[kFirstElementSdt + e]);
                const int rank    = res.elementRanks[e];
                const int absorb  = defenses.absorb[e];
                elements.cells.push_back(Cell(absorb > 0 ? Text(absorb >= kWholeAbsorb ? std::string("Absorbs")
                                                                                       : "Absorbs " + std::to_string(absorb) + "%",
                                                               Tone::Immune)
                                              : percent != 0
                                                  ? Percent(percent)
                                                  : Ranked(ElementRankText(static_cast<Element>(e), rank, numbers), rank),
                    SdtIcon(kFirstElementSdt + e)));
            }

            PopupRow statuses = Grid(kStatusColumns, kStatusRankCount);
            for (int s = 0; s < kStatusRankCount; ++s)
            {
                const bool immuneTo = (res.immunity & StatusImmunity(s)) != 0;
                const int rank      = res.statusRanks[s];
                statuses.cells.push_back(Cell(immuneTo ? Text(kTimesSign, Tone::Immune)
                                                       : Ranked(StatusRankText(static_cast<StatusRank>(s), rank, numbers), rank),
                    StatusIcon(s)));
            }

            PopupRow immune{RowKind::Line, 0, {}};
            const InfoIconSet immunities = InfoIconsOf(&res, BadgeRules{}, BadgeGroup::Immune);
            for (int i = 0; i < immunities.count; ++i)
                immune.cells.push_back(PopupCell{immunities.icons[i].icon, true, {}});
            if (immune.cells.empty()) immune.cells.push_back(Cell(Text("None", Tone::Neutral)));

            PopupSection identityRows{"", ListOf(std::move(identity), Tags(mob))};
            if (PopupRow traits = Traits(mob); !traits.cells.empty()) identityRows.rows.push_back(std::move(traits));
            popup.sections = ListOf(std::move(identityRows), std::move(damageTaken),
                PopupSection{"Elements", ListOf(std::move(elements))},
                PopupSection{"Statuses", ListOf(std::move(statuses))},
                PopupSection{"Immune", ListOf(std::move(immune))});
            // At the level the chances are figured for: the check's, else the top of its range.
            const int level = numbers.target ? numbers.target->level : mob.maxLevel;
            if (numbers.target)
            {
                PopupSection stats{"Stats at Lv " + std::to_string(level), {}};
                if (const MobCombat* combat = CombatAt(mob, level))
                {
                    PopupRow row = Grid(kStatColumns, kCombatCount);
                    for (int i = 0; i < kCombatCount; ++i)
                        row.cells.push_back(Numbered(kCombatNames[i], combat->value[i]));
                    stats.rows.push_back(std::move(row));
                }
                PopupRow row = Grid(kStatColumns, kStatCount);
                for (int i = 0; i < kStatCount; ++i)
                    row.cells.push_back(Numbered(kStatNames[i], numbers.target->stats.value[i]));
                stats.rows.push_back(std::move(row));
                popup.sections.push_back(std::move(stats));
            }
            std::vector<std::string> moves;
            for (const MobSkill& skill : SkillsOf(mob))
                moves.emplace_back(skill.name);
            if (std::optional<PopupSection> list = NameList("TP moves", moves)) popup.sections.push_back(std::move(*list));
            if (std::optional<PopupSection> list = NameList("Spells", SpellsAt(mob, level, state)))
                popup.sections.push_back(std::move(*list));
            popup.sections.push_back(Spawn(mob, state));
            for (PopupSection& loot : Loot(mob, state))
                popup.sections.push_back(std::move(loot));
            return popup;
        }

        Span WeekdayText(const VanaTime& t)
        {
            return Colored(WeekdayName(t.weekday), ElementColor(WeekdayElement(t.weekday)));
        }

        std::string MoonText(const VanaTime& t) { return MoonPhaseName(t.moonPercent, t.moonDirection); }

        bool InConquest(const ZoneRecord* zone) { return zone != nullptr && zone->region < kConquestRegionCount; }

        // Who holds the zone's region; no one before a conquest update or outside the conquest regions.
        Nation OwnerOf(const ZoneRecord* zone, const BarState& state)
        {
            return InConquest(zone) && state.owners ? (*state.owners)[zone->region] : Nation::None;
        }

        std::optional<Icon> WeatherIcon(Weather weather)
        {
            const std::optional<Element> element = WeatherElement(weather);
            if (!element) return std::nullopt;
            return SdtIcon(kFirstElementSdt + static_cast<int>(*element));
        }

        Span BagFill(const BagCount& bag)
        {
            return Text(std::to_string(bag.used) + " / " + std::to_string(bag.max),
                bag.used >= bag.max ? Tone::Resist : Tone::Normal);
        }
    }

    uint32_t ElementColor(Element element) { return kElementColors[static_cast<int>(element)]; }

    std::optional<Span> StripBagFill(const BarState& state)
    {
        return state.stripBag ? std::optional<Span>(BagFill(*state.stripBag)) : std::nullopt;
    }

    std::optional<uint32_t> GilOf(uint16_t item, uint32_t count)
    {
        return item == kGilItem ? std::optional<uint32_t>(count) : std::nullopt;
    }

    std::optional<BagCount> BagOf(uint32_t used, uint32_t max)
    {
        return max > 0 ? std::optional<BagCount>(BagCount{used, max}) : std::nullopt;
    }

    std::optional<int> ContainerNamed(const std::string& caption)
    {
        for (int c = 0; c < kContainerCount; ++c)
            if (caption == kContainerNames[c]) return c;
        // The Items menu opens on the inventory under its own name.
        if (caption == kItemsMenu) return static_cast<int>(kInventory);
        for (const ShortCaption& shortened : kShortCaptions)
            if (caption == shortened.caption) return shortened.container;
        return std::nullopt;
    }

    BarNeeds NeedsOf(const Settings& settings, std::optional<BarWidget> open)
    {
        auto shown = [&](BarWidget widget) { return settings.barWidgets[static_cast<int>(widget)]; };
        BarNeeds needs;
        // The Clock in a zone of its own goes by the time alone, as its server line and its popup's resets do; in the
        // computer's zone, by what Windows makes of the time.
        const bool clock = shown(BarWidget::EarthTime), zonedClock = clock && settings.clockZone.has_value();
        needs.unixTime   = shown(BarWidget::Weather) || shown(BarWidget::VanaTime) || zonedClock ||
                           (clock && (settings.clockServerTime || open == BarWidget::EarthTime)) ||
                           (shown(BarWidget::Zone) && open == BarWidget::Zone); // the popup counts down to the tally
        needs.localTime  = clock && !zonedClock;
        needs.zone       = shown(BarWidget::Zone) || shown(BarWidget::Weather) ||
                     (shown(BarWidget::Signet) && open == BarWidget::Signet); // the popup says where it works
        needs.target     = shown(BarWidget::Target) || shown(BarWidget::Distance);
        needs.gil        = shown(BarWidget::Gil);
        needs.strip      = shown(BarWidget::GameHelp);
        const bool memoryPopup = shown(BarWidget::Memory) && open == BarWidget::Memory;
        needs.frames     = shown(BarWidget::Fps) || memoryPopup;
        needs.job        = shown(BarWidget::Job);
        needs.connection = shown(BarWidget::Connection);
        needs.party      = shown(BarWidget::Party);
        needs.signet     = shown(BarWidget::Signet);
        needs.memory     = shown(BarWidget::Memory);
        needs.cpu        = memoryPopup;
        needs.linuxxi    = memoryPopup;
        // Ashita counts a container's items by walking its slots, so the others wait for the popup that lists them.
        if (shown(BarWidget::Bags)) needs.containers = open == BarWidget::Bags ? kContainerCount : 1;
        return needs;
    }

    namespace
    {
        WidgetContent ContentOf(BarWidget widget, const BarState& state, const Settings& settings);
    }

    WidgetContent BarWidgetContent(BarWidget widget, const BarState& state, const Settings& settings)
    {
        // Each widget's own content, less its second line or icons where they are switched off.
        WidgetContent w = ContentOf(widget, state, settings);
        const int i     = static_cast<int>(widget);
        if (!settings.barSecondLine[i])
        {
            w.subtitle.clear();
            w.subtitleIcons.clear();
        }
        if (!settings.barIcons[i])
        {
            w.icons.clear();
            w.titleIcons.clear();
            w.subtitleIcons.clear();
            w.coin = false;
        }
        return w;
    }

    namespace
    {
    WidgetContent ContentOf(BarWidget widget, const BarState& state, const Settings& settings)
    {
        WidgetContent w;
        w.shown = settings.barWidgets[static_cast<int>(widget)];
        if (!w.shown) return w;
        switch (widget)
        {
        case BarWidget::Zone:
        {
            const ZoneRecord* zone = KnownZone(state.zone);
            const std::string_view region = ZoneRegion(zone);
            w.title = ListOf(Text(ZoneTitle(state.zone)));
            // A conquest region always has a name.
            if (const Nation owner = OwnerOf(zone, state); owner != Nation::None)
                w.subtitle = ListOf(Text(std::string(region), Tone::Dim), Text(kDot, Tone::Dim),
                    Colored(NationName(owner), NationColor(owner)));
            else if (!region.empty())
                w.subtitle = ListOf(Text(std::string(region), Tone::Dim));
            w.hasPopup = state.zone != 0;
            break;
        }
        case BarWidget::Weather:
        {
            const std::optional<WeatherOdds> today = OddsOn(state, VanaAt(state.unixMs).day);
            if (!state.weather && !today)
            {
                w.shown = false;
                break;
            }
            const Weather now = state.weather ? *state.weather : today->normal;
            if (const std::optional<Icon> icon = WeatherIcon(now)) w.icons.assign(IsDoubleWeather(now) ? 2 : 1, *icon);
            w.title = ListOf(Text(WeatherName(now)));
            // Today's chance of it; the forecast is in the popup.
            for (const WeatherChance& c : today ? Chances(*today) : WeatherChances{})
                if (c.weather == now) w.subtitle = ListOf(Text(std::to_string(c.percent) + "%", Tone::Dim));
            w.hasPopup = today.has_value();
            break;
        }
        case BarWidget::VanaTime:
        {
            const VanaTime t = VanaAt(state.unixMs);
            w.title    = ListOf(WeekdayText(t), Text(" " + HourMinute(t.hour, t.minute)));
            w.subtitle = ListOf(Text(MoonText(t) + " " + std::to_string(t.moonPercent) + "%", Tone::Dim));
            w.hasPopup = true;
            break;
        }
        case BarWidget::Target:
            if (state.target == nullptr)
                w.title = ListOf(Text("No target", Tone::Dim));
            else if (state.target->kind != EntityKind::Mob)
            {
                w.title = ListOf(Text(state.target->name));
                if (state.target->kind == EntityKind::Player && state.targetCheck)
                {
                    w.subtitle = ListOf(Text(CheckedJobs(state.targetCheck->general), Tone::Dim));
                    w.hasPopup = true;
                }
                w.hasPopup = w.hasPopup || NpcOf(state.target) != nullptr;
            }
            else
            {
                if (settings.barTargetDetails)
                    w.titleIcons = MobIcons(*state.target);
                else if (const std::optional<Icon> icon = MobIcon(*state.target))
                    w.icons.push_back(*icon);
                w.title = ListOf(Colored(state.target->name, NameColor(*state.target, settings)));
                if (HasLevel(*state.target))
                    w.subtitle = ListOf(Colored(state.target->level.text, ShadeColor(*state.target, settings)));
                if (settings.barTargetDetails && IsMob(state.target))
                {
                    // Weaknesses by SDT or rank, as the badges find them by default.
                    const InfoIconSet found = InfoIconsFor(&ResistancesOf(*state.target->mob), BadgeRules{});
                    for (int i = 0; i < found.count; ++i)
                        if (found.icons[i].badge == BadgeKind::Weak || found.icons[i].badge == BadgeKind::WeakRank)
                            w.subtitleIcons.push_back(found.icons[i].icon);
                }
                w.hasPopup = IsMob(state.target); // a mob the data lacks gets its name, level and con, no popup
            }
            break;
        case BarWidget::Gil:
        {
            // The picked currency; gil with its coin.
            const int picked = settings.barCurrency;
            if (picked == kGilCurrency)
            {
                w.coin  = true;
                w.title = ListOf(Text(state.gil ? Grouped(*state.gil) : "-"));
            }
            else
            {
                const std::optional<uint32_t>& amount = state.currencies[picked];
                w.title    = ListOf(Text(amount ? Grouped(*amount) : "-"));
                w.subtitle = ListOf(Text(kCurrencyTable[picked].name, Tone::Dim));
            }
            w.hasPopup = true;
            break;
        }
        case BarWidget::Bags:
            w.title    = ListOf(state.bags[0] ? BagFill(*state.bags[0]) : Text("-"));
            w.subtitle = ListOf(Text("Bags", Tone::Dim));
            w.hasPopup = true;
            break;
        case BarWidget::EarthTime:
            w.title    = ListOf(Text(settings.clockZone
                    ? ZoneClockText(ZoneAt(*settings.clockZone, state.unixMs), settings.clock24, settings.clockSeconds)
                    : ClockText(state.earthHour, state.earthMinute, settings.clock24,
                          settings.clockSeconds ? state.earthSecond : -1)));
            if (settings.clockServerTime)
                w.subtitle = ListOf(Text(
                    ZoneClockText(ZoneAt(ClockZone::Japan, state.unixMs), settings.clock24, false), Tone::Dim));
            w.hasPopup = true;
            break;
        case BarWidget::GameHelp:
            return WidgetContent{};
        case BarWidget::Fps:
            return FpsWidget(state.frames);
        case BarWidget::Job:
            return JobWidget(state.job, settings.expBar, settings.jobNames, settings.jobLayout);
        case BarWidget::Connection:
            return ConnectionWidget(state.connection, settings.barLatency);
        case BarWidget::Party:
            return PartyWidget(state.party);
        case BarWidget::Signet:
            return SignetWidget(state.signet);
        case BarWidget::Distance:
            // Hidden without a target, as the game's own distance shows nothing then.
            w.shown = state.targetDistance && std::isfinite(*state.targetDistance);
            if (!w.shown) break;
            w.title   = ListOf(Text(Printed("%.1f", *state.targetDistance)), Text(" yalms", Tone::Dim));
            w.figures = true;
            break;
        case BarWidget::Memory:
            return MemoryWidget(state.memory);
        }
        return w;
    }

    }

    PopupContent BarPopupContent(BarWidget widget, const BarState& state, const Settings& settings)
    {
        PopupContent popup;
        switch (widget)
        {
        case BarWidget::Zone:
        {
            const ZoneRecord* zone = KnownZone(state.zone);
            const std::string_view region = ZoneRegion(zone);
            popup.title = ListOf(Text(ZoneTitle(state.zone)));
            // A zone past the data has its number for a title already.
            if (zone != nullptr) popup.title.push_back(Text(" " + std::to_string(state.zone), Tone::Dim));
            if (!region.empty())
                popup.sections.push_back(PopupSection{
                    "", ListOf(Line(Cell(ListOf(Text("Region ", Tone::Dim), Text(std::string(region))))))});
            if (InConquest(zone) && state.shares)
            {
                PopupSection influence{"Influence", {}};
                const Nation owner = OwnerOf(zone, state);
                for (const RegionShare& share : *state.shares)
                {
                    PopupRow row = Labeled(Colored(NationName(share.nation), NationColor(share.nation)),
                        Text(std::to_string(share.percent) + "%"));
                    row.cells[0].star = share.nation == owner;
                    influence.rows.push_back(std::move(row));
                }
                influence.rows.push_back(Labeled("Tally in", Text(SpanText(SecondsToTally(state.unixMs)))));
                popup.sections.push_back(std::move(influence));
            }
            break;
        }
        case BarWidget::Weather:
        {
            const VanaTime t = VanaAt(state.unixMs);
            popup.title      = ListOf(Text("Weather"));
            const std::optional<WeatherOdds> today = OddsOn(state, t.day);
            const Weather now = state.weather ? *state.weather : today ? today->normal : Weather::None;
            PopupCell current = Cell(Text(WeatherName(now)), WeatherIcon(now));
            if (!state.weather) current.text.push_back(Text(" (likely)", Tone::Dim));
            PopupRow grid   = Grid(kForecastColumns, kForecastDays * kForecastColumns);
            bool oneWeather = true; // on every day
            for (int d = 0; d < kForecastDays; ++d)
            {
                const int weekday = (t.weekday + d) % kVanaDaysPerWeek;
                grid.cells.push_back(
                    Cell(Colored(d == 0 ? "Today" : WeekdayName(weekday), ElementColor(WeekdayElement(weekday)))));
                const std::optional<WeatherOdds> odds = OddsOn(state, t.day + static_cast<uint32_t>(d));
                const WeatherChances chances = odds ? Chances(*odds) : WeatherChances{};
                oneWeather = oneWeather && chances.count == 1;
                for (int c = 0; c < kForecastColumns - 1; ++c)
                {
                    if (c >= chances.count)
                    {
                        grid.cells.emplace_back();
                        continue;
                    }
                    const WeatherChance& chance = chances.chances[c];
                    grid.cells.push_back(Cell(Text(std::string(WeatherName(chance.weather)) + " " +
                                                   std::to_string(chance.percent) + "%"),
                        WeatherIcon(chance.weather)));
                }
            }
            PopupSection forecast{"Forecast", ListOf(std::move(grid))};
            // One weather on every day (Phoenix's static zones: Promyvion, Temenos): rolling again changes nothing.
            if (!oneWeather)
                forecast.rows.push_back(
                    LineOf("Phoenix rolls the weather again every 3 to 30 Earth minutes.", Tone::Dim));
            popup.sections = ListOf(PopupSection{"", ListOf(Line(std::move(current)))}, std::move(forecast));
            break;
        }
        case BarWidget::VanaTime:
        {
            const VanaTime t = VanaAt(state.unixMs);
            popup.title      = ListOf(Text(std::to_string(t.year) + "/" + Printed("%02d", t.month) + "/" +
                                           Printed("%02d", t.dayOfMonth)));
            popup.sections   = ListOf(PopupSection{"",
                ListOf(Line(Cell(ListOf(WeekdayText(t), Text(" " + HourMinute(t.hour, t.minute))))),
                    LineOf(MoonText(t) + " (" + std::to_string(t.moonPercent) + "%)"),
                    LineOf("Next day in " + Countdown(SecondsToNextDay(state.unixMs)), Tone::Dim),
                    LineOf("Next phase in " + Countdown(SecondsToNextPhase(state.unixMs)), Tone::Dim))});
            break;
        }
        case BarWidget::Target: return TargetPopup(state, settings);
        case BarWidget::Gil:
        {
            // Gil, then every currency the server gave an amount of; those it has none of stay out.
            popup.title = ListOf(Text("Currencies"));
            PopupSection rows;
            rows.rows.push_back(Labeled("Gil", Text(state.gil ? Grouped(*state.gil) : "-")));
            bool sent = false;
            for (int c = kGilCurrency + 1; c < kCurrencyCount; ++c)
            {
                const std::optional<uint32_t>& amount = state.currencies[c];
                sent = sent || amount.has_value();
                if (amount && *amount > 0) rows.rows.push_back(Labeled(kCurrencyTable[c].name, Text(Grouped(*amount))));
            }
            if (!sent) rows.rows.push_back(LineOf("The rest show once the server sends them.", Tone::Dim));
            popup.sections = ListOf(std::move(rows));
            break;
        }
        case BarWidget::Bags:
        {
            popup.title   = ListOf(Text("Bags"));
            PopupRow grid = Grid(kBagColumns, kBagColumns * kContainerCount);
            for (int c = 0; c < kContainerCount; ++c)
            {
                if (!state.bags[c]) continue;
                grid.cells.push_back(Cell(Text(kContainerNames[c])));
                grid.cells.push_back(Cell(BagFill(*state.bags[c])));
            }
            popup.sections = ListOf(PopupSection{"", ListOf(std::move(grid))});
            break;
        }
        case BarWidget::EarthTime:
        {
            int year = state.earthYear, month = state.earthMonth, day = state.earthDay;
            if (settings.clockZone)
            {
                const ZoneTime there = ZoneAt(*settings.clockZone, state.unixMs);
                year = there.year, month = there.month, day = there.day;
            }
            popup.title =
                ListOf(Text(std::to_string(year) + "-" + Printed("%02d", month) + "-" + Printed("%02d", day)));
            popup.sections = ListOf(ServerTimeRows(state.unixMs));
            break;
        }
        case BarWidget::GameHelp:
            return GameHelpPopup(state.strip, StripBagFill(state));
        case BarWidget::Fps:
            return FpsPopup(state.frames);
        case BarWidget::Job:
            return JobPopup(state.job);
        case BarWidget::Connection:
            return ConnectionPopup(state.connection);
        case BarWidget::Party:
            return PartyPopup(state.party);
        case BarWidget::Signet:
            return SignetPopup(state.signet, state.zone);
        case BarWidget::Distance: break;
        case BarWidget::Memory:
            return MemoryPopup(state.memory, state.cpuPercent, state.frames, state.linuxxi);
        }
        return popup;
    }
}
