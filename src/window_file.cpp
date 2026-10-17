#include "window_file.h"

#include "json_text.h"

#include <algorithm>
#include <bit>
#include <optional>
#include <ranges>
#include <set>
#include <utility>

namespace headsup
{
    namespace
    {
        // A kinds list's names: PlateKind's order, and the entity pages' keys, as gen_window.py's KINDS.
        constexpr std::string_view kKindKeys[kPlateKindCount] = {"mobs", "players", "self", "npcs", "pets"};
        constexpr std::string_view kIdMark = "##"; // what a label holds before the name of its control's id
        // The second tip of a list's option and of a setting's row, where the code has one.
        constexpr std::string_view kClaimRowsTip = "tipOnClaimRows", kBottomLineTip = "tipBottomLine";

        struct Refused
        {
            std::string problem;
        };
        // A text as the tables hold it: one for every kind, or one a kind.
        struct Words
        {
            const char* shared              = nullptr;
            const char* const* byKind       = nullptr;
            bool given                      = false;
        };

        bool IsKey(std::string_view key)
        {
            bool hyphenAllowed = false;
            for (const char c : key)
            {
                if (c == '-' ? !hyphenAllowed : !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return false;
                hyphenAllowed = c != '-';
            }
            return hyphenAllowed;
        }
        std::string Quoted(std::string_view text) { return "\"" + std::string(text) + "\""; }
        std::string_view FieldName(std::string_view field)
        {
            return field == kClaimRowsTip ? "claim rows' tip" : field == kBottomLineTip ? "bottom line's tip" : field;
        }
        std::string KindNames(uint8_t kinds)
        {
            std::string names;
            for (int k = 0; k < kPlateKindCount; ++k)
                if ((kinds & KindBit(static_cast<PlateKind>(k))) != 0) names += (names.empty() ? "" : ", ") + std::string(kKindKeys[k]);
            return names;
        }
        const JsonValue kNoList = [] {
            JsonValue empty;
            empty.kind = JsonValue::Kind::List;
            return empty;
        }();

        class Reader
        {
        public:
            Reader(const WindowLayout& builtIn, OwnedLayout& out) : m_Built(builtIn), m_Out(out) {}

            void Layout(const JsonValue& top, std::string_view version)
            {
                const std::set<std::string_view> parts(top.names.begin(), top.names.end());
                if (top.kind != JsonValue::Kind::Object || parts != std::set<std::string_view>{"version", "pages", "entity", "lists", "words"})
                    Bad("the top level must be an object with version, pages, entity, lists and words");
                const JsonValue& given = *top.Field("version");
                if (given.kind != JsonValue::Kind::Text || given.text.empty()) Bad("the version must be text");
                if (given.text != version) Bad("version " + given.text + " is not this HeadsUp's " + std::string(version));
                const JsonValue& entity = *top.Field("entity");
                Fields(entity, "the entity part", {"cards"});
                Lists(*top.Field("lists"));
                Pages(*top.Field("pages"));
                const int entityFirst = static_cast<int>(m_Out.cards.size());
                if (const JsonValue* cards = entity.Field("cards")) Cards(*cards, "the entity cards", true, {});
                for (LayoutPage& page : m_Out.pages)
                    if (IsKindPage(page.key))
                    {
                        page.firstCard = entityFirst;
                        page.cardCount = static_cast<int>(m_Out.cards.size()) - entityFirst;
                    }
                LeftOut();
                WordsOf(*top.Field("words"), nullptr, "");
            }

        private:
            const WindowLayout& m_Built;
            OwnedLayout& m_Out;
            std::vector<std::pair<std::string, std::string>> m_Placed; // each setting's key and its card's
            std::set<std::string> m_SeenCards;

            [[noreturn]] static void Bad(std::string problem) { throw Refused{std::move(problem)}; }
            const char* Keep(const std::string& text)
            {
                m_Out.texts.push_back(text);
                return m_Out.texts.back().c_str();
            }
            static bool IsKindPage(std::string_view key) { return std::ranges::find(kKindKeys, key) != std::end(kKindKeys); }

            static void Listed(const JsonValue& value, const std::string& what)
            {
                if (value.kind != JsonValue::Kind::List) Bad(what + " must be a list");
            }
            static void Fields(const JsonValue& value, const std::string& what, std::initializer_list<std::string_view> allowed)
            {
                if (value.kind != JsonValue::Kind::Object) Bad(what + " must be an object");
                for (const std::string& field : value.names)
                    if (std::ranges::find(allowed, field) == allowed.end()) Bad(what + " has an unknown field " + Quoted(field));
            }
            static std::string KeyText(const JsonValue& value)
            {
                const JsonValue* key = value.kind == JsonValue::Kind::Object ? value.Field("key") : nullptr;
                return key != nullptr && key->kind == JsonValue::Kind::Text ? key->text : std::string();
            }
            static std::string KeyOf(const JsonValue& value, const std::string& what)
            {
                const JsonValue* key = value.Field("key");
                if (key == nullptr) Bad(what + " has no key");
                if (key->kind != JsonValue::Kind::Text || !IsKey(key->text))
                    Bad("the key " + Quoted(key->kind == JsonValue::Kind::Text ? key->text : "") +
                        " must be lower-case letters, digits and single hyphens");
                return key->text;
            }
            static void Plain(const JsonValue& text, std::string_view field, const std::string& owner)
            {
                if (text.kind != JsonValue::Kind::Text || text.text.empty())
                    Bad("the " + std::string(FieldName(field)) + " of " + owner + " must be text");
                if (field == "name" && text.text.find(kIdMark) != std::string::npos)
                    Bad("the name of " + owner + " holds " + Quoted(kIdMark));
            }
            // A name or a tip: text, or where kinds are shown, text by kind that covers them.
            Words TextOf(const JsonValue& value, std::string_view field, const std::string& owner, std::optional<uint8_t> shown = {})
            {
                const JsonValue* text = value.Field(field);
                if (text == nullptr) return {};
                if (text->kind != JsonValue::Kind::Object)
                {
                    Plain(*text, field, owner);
                    return {Keep(text->text), nullptr, true};
                }
                const std::string named = "the " + std::string(FieldName(field)) + " of " + owner;
                if (!shown) Bad(named + " is by kind, which only the entity cards have");
                std::array<const char*, kPlateKindCount> byKind{};
                for (size_t i = 0; i < text->names.size(); ++i)
                {
                    const auto kind = std::ranges::find(kKindKeys, text->names[i]);
                    if (kind == std::end(kKindKeys)) Bad(named + " names " + Quoted(text->names[i]) + ", which is no kind");
                    Plain(text->items[i], field, owner);
                    byKind[static_cast<size_t>(kind - std::begin(kKindKeys))] = Keep(text->items[i].text);
                }
                for (int k = 0; k < kPlateKindCount; ++k)
                    if ((*shown & KindBit(static_cast<PlateKind>(k))) != 0 && byKind[static_cast<size_t>(k)] == nullptr)
                        Bad(named + " names no text for " + Quoted(kKindKeys[k]));
                m_Out.byKind.push_back(byKind);
                return {nullptr, m_Out.byKind.back().data(), true};
            }
            Words Named(const JsonValue& value, const std::string& owner, bool hasName, std::optional<uint8_t> shown = {})
            {
                const Words name = TextOf(value, "name", owner, shown);
                if (hasName && !name.given) Bad(owner + " has no name");
                return name;
            }
            static std::optional<uint8_t> KindsOf(const JsonValue& value, const std::string& owner)
            {
                const JsonValue* kinds = value.Field("kinds");
                if (kinds == nullptr) return std::nullopt;
                const bool texts = kinds->kind == JsonValue::Kind::List &&
                                   std::ranges::all_of(kinds->items, [](const JsonValue& k) { return k.kind == JsonValue::Kind::Text; });
                if (!texts) Bad("the kinds of " + owner + " must be a list of kinds");
                uint8_t bits = 0;
                for (const JsonValue& kind : kinds->items)
                {
                    const auto found = std::ranges::find(kKindKeys, kind.text);
                    if (found == std::end(kKindKeys)) Bad("the kinds of " + owner + " name " + Quoted(kind.text) + ", which is none");
                    bits |= KindBit(static_cast<PlateKind>(found - std::begin(kKindKeys)));
                }
                return bits;
            }

            void Lists(const JsonValue& value)
            {
                std::set<std::string> seen;
                Listed(value, "lists");
                const JsonValue& listed = value;
                for (size_t number = 1; number <= listed.items.size(); ++number)
                {
                    const JsonValue& list = listed.items[number - 1];
                    Fields(list, "the list " + Quoted(KeyText(list)), {"key", "options"});
                    const std::string key = KeyOf(list, "list " + std::to_string(number));
                    const auto built = std::ranges::find(m_Built.lists, key, [](const LayoutList& l) { return std::string_view(l.key); });
                    if (built == m_Built.lists.end()) Bad(Quoted(key) + " is no list");
                    if (!seen.insert(key).second) Bad("the list " + Quoted(key) + " is used twice");
                    const int first = static_cast<int>(m_Out.options.size());
                    const JsonValue* options = list.Field("options");
                    Options(options != nullptr ? *options : kNoList, "the list " + Quoted(key), m_Built.OptionsOf(built->list));
                    m_Out.lists.push_back(LayoutList{built->list, built->key, first, static_cast<int>(m_Out.options.size()) - first});
                }
                for (const LayoutList& list : m_Built.lists)
                    if (!seen.contains(list.key)) Bad("the list " + Quoted(list.key) + " is left out");
            }

            void Options(const JsonValue& value, const std::string& owner, std::span<const LayoutOption> built)
            {
                Listed(value, "the options of " + owner);
                const JsonValue& listed = value;
                if (listed.items.size() != built.size())
                    Bad(owner + " lists " + std::to_string(listed.items.size()) + " options, and the control has " + std::to_string(built.size()));
                for (size_t i = 0; i < built.size(); ++i)
                {
                    const JsonValue& option = listed.items[i];
                    const std::string what  = "option " + std::to_string(i + 1) + " of " + owner;
                    const bool second       = built[i].otherTip != nullptr;
                    if (second)
                        Fields(option, what, {"name", "tip", kClaimRowsTip});
                    else
                        Fields(option, what, {"name", "tip"});
                    const bool nameless = built[i].name == nullptr;
                    const Words name    = Named(option, what, !nameless);
                    if (nameless && name.given) Bad(what + " has a name, and the control shows its own");
                    m_Out.options.push_back(LayoutOption{name.shared, TextOf(option, "tip", what).shared,
                        second ? TextOf(option, kClaimRowsTip, what).shared : nullptr});
                }
            }

            void Pages(const JsonValue& value)
            {
                std::set<std::string> seen;
                Listed(value, "pages");
                const JsonValue& listed = value;
                for (size_t number = 1; number <= listed.items.size(); ++number)
                {
                    const JsonValue& page = listed.items[number - 1];
                    const std::string what = "page " + std::to_string(number);
                    if (page.kind == JsonValue::Kind::Object && page.Field("key") == nullptr) Bad(what + " has no key");
                    Fields(page, what, {"key", "name", "child", "cards"});
                    const std::string key = KeyOf(page, what);
                    const std::string owner = "the page " + Quoted(key);
                    const auto built = std::ranges::find(m_Built.pages, key, [](const LayoutPage& p) { return std::string_view(p.key); });
                    if (built == m_Built.pages.end()) Bad(Quoted(key) + " is no page");
                    if (!seen.insert(key).second) Bad(owner + " is used twice");
                    const JsonValue* child = page.Field("child");
                    if (child != nullptr && child->kind != JsonValue::Kind::Flag) Bad("the child of " + owner + " must be true or false");
                    const Words name = Named(page, owner, true);
                    const int first  = static_cast<int>(m_Out.cards.size());
                    if (IsKindPage(key))
                    {
                        if (page.Field("cards") != nullptr) Bad(owner + " shares the entity cards and cannot have cards of its own");
                    }
                    else
                    {
                        if (const JsonValue* cards = page.Field("cards")) Cards(*cards, owner, false, key);
                        if (static_cast<int>(m_Out.cards.size()) == first) Bad(owner + " has no cards");
                    }
                    m_Out.pages.push_back(LayoutPage{built->page, built->key, name.shared, child != nullptr && child->flag, first,
                        static_cast<int>(m_Out.cards.size()) - first});
                }
                for (const LayoutPage& page : m_Built.pages)
                    if (!seen.contains(page.key)) Bad("the page " + Quoted(page.key) + " is left out");
            }

            void Cards(const JsonValue& value, const std::string& where, bool inEntity, const std::string& page)
            {
                Listed(value, "the cards of " + where);
                const JsonValue& listed = value;
                for (size_t number = 1; number <= listed.items.size(); ++number)
                {
                    const JsonValue& card = listed.items[number - 1];
                    const std::string what = "card " + std::to_string(number) + " of " + where;
                    if (card.kind == JsonValue::Kind::Object && card.Field("key") == nullptr) Bad(what + " has no key");
                    Fields(card, "the card " + Quoted(KeyText(card)), {"key", "name", "description", "kinds", "settings"});
                    const std::string key   = KeyOf(card, what);
                    const std::string owner = "the card " + Quoted(key);
                    if (!m_SeenCards.insert(key).second) Bad(owner + " is used twice");
                    const std::optional<uint8_t> kinds = KindsOf(card, Quoted(key));
                    if (kinds && !inEntity) Bad(Quoted(key) + " has kinds, which only the entity cards have");
                    const JsonValue* about = card.Field("description");
                    if (about != nullptr) Plain(*about, "description", owner);
                    const Words name = Named(card, owner, true);
                    const auto built = std::ranges::find(m_Built.cards, key, [](const LayoutCard& c) { return std::string_view(c.key); });
                    const bool known = built != m_Built.cards.end();
                    const size_t at  = m_Out.cards.size();
                    m_Out.cards.push_back(LayoutCard{known ? built->piece : CardPiece::Plain, known ? built->key : Keep(key), name.shared,
                        about != nullptr ? Keep(about->text) : nullptr, kinds.value_or(kEveryKind),
                        static_cast<int>(m_Out.settings.size()), 0, known && built->switched});
                    const std::string place = "card " + key + ", " + (page.empty() ? std::string("the entity cards") : "page " + page);
                    const JsonValue* settings = card.Field("settings");
                    const JsonValue& listedSettings = settings != nullptr ? *settings : kNoList;
                    Listed(listedSettings, "the settings of " + owner);
                    for (const JsonValue& setting : listedSettings.items)
                        Setting(setting, key, place, inEntity, kinds);
                    m_Out.cards[at].settingCount = static_cast<int>(m_Out.settings.size()) - m_Out.cards[at].firstSetting;
                    if (m_Out.cards[at].settingCount == 0) Bad(owner + " has no settings");
                }
            }

            void Setting(const JsonValue& value, const std::string& card, const std::string& place, bool inEntity,
                std::optional<uint8_t> cardKinds)
            {
                const std::string given = KeyText(value);
                if (value.kind == JsonValue::Kind::Object && value.Field("key") == nullptr) Bad("a setting of " + place + " has no key");
                const auto built = std::ranges::find(m_Built.settings, given, [](const LayoutSetting& s) { return std::string_view(s.key); });
                const bool known = built != m_Built.settings.end();
                const bool hasWords = known && std::ranges::any_of(m_Built.words, [&](const LayoutWord& w) { return w.setting != nullptr && given == w.setting; });
                std::vector<std::string_view> allowed{"key", "name", "tip", "kinds", "indent"};
                if (known && built->optionCount > 0) allowed.push_back("options");
                if (known && built->rowCount > 0) allowed.push_back("rows");
                if (hasWords) allowed.push_back("words");
                const std::string what = "the setting " + Quoted(given);
                if (value.kind != JsonValue::Kind::Object) Bad(what + " must be an object");
                for (const std::string& field : value.names)
                    if (std::ranges::find(allowed, field) == allowed.end()) Bad(what + " has an unknown field " + Quoted(field));
                const std::string key = KeyOf(value, "a setting of " + place);
                if (!known) Bad(Quoted(key) + " is no key of any setting (" + place + ")");
                const auto placed = std::ranges::find(m_Placed, key, &std::pair<std::string, std::string>::first);
                if (placed != m_Placed.end()) Bad(what + " is used twice (cards " + placed->second + " and " + card + ")");
                m_Placed.emplace_back(key, card);
                const std::string owner = Quoted(key);
                if (built->entity && !inEntity) Bad(owner + " is an entity's design and can only go in the entity cards (" + place + ")");
                const std::optional<uint8_t> kinds = KindsOf(value, owner);
                if (kinds && !inEntity) Bad(owner + " has kinds, which only the entity cards have");
                if (built->codeKinds != kEveryKind && kinds != built->codeKinds)
                    Bad(owner + " must name the kinds the code has it on: " + KindNames(built->codeKinds));
                std::optional<uint8_t> shown;
                if (inEntity)
                {
                    shown = static_cast<uint8_t>(cardKinds.value_or(kEveryKind) & kinds.value_or(kEveryKind));
                    if (*shown == 0) Bad(owner + " shows on no page: the kinds of its card leave none of its own");
                    if (!built->entity && std::popcount(*shown) != 1)
                        Bad(owner + " would show on " + std::to_string(std::popcount(*shown)) +
                            " pages: a setting that is not of an entity's design needs one kind in the entity cards, on it or on its card");
                }
                const JsonValue* indent = value.Field("indent");
                if (indent != nullptr && indent->kind != JsonValue::Kind::Flag) Bad("the indent of " + owner + " must be true or false");
                const Words tip     = TextOf(value, "tip", owner, shown);
                const bool unnamed  = built->name == nullptr && built->nameByKind == nullptr;
                const Words name    = Named(value, what, !unnamed, shown);
                if (unnamed && name.given) Bad(what + " has a name, and the code shows none");
                const int firstOption = static_cast<int>(m_Out.options.size());
                const JsonValue* options = value.Field("options");
                Options(options != nullptr ? *options : kNoList, owner, m_Built.OptionsOf(*built));
                const int firstRow = static_cast<int>(m_Out.rows.size());
                if (built->rowCount > 0)
                {
                    const JsonValue* rows = value.Field("rows");
                    Rows(rows != nullptr ? *rows : kNoList, *built, shown);
                }
                m_Out.settings.push_back(LayoutSetting{built->piece, built->key, name.shared, tip.shared, name.byKind, tip.byKind,
                    kinds.value_or(kEveryKind), built->entity, firstOption, static_cast<int>(m_Out.options.size()) - firstOption,
                    firstRow, static_cast<int>(m_Out.rows.size()) - firstRow, built->rowsFixed, built->codeKinds,
                    indent != nullptr && indent->flag});
                if (hasWords)
                {
                    const JsonValue* words = value.Field("words");
                    WordsOf(words != nullptr ? *words : kNoList, built->key, " of " + owner);
                }
            }

            void Rows(const JsonValue& value, const LayoutSetting& setting, std::optional<uint8_t> shown)
            {
                const std::span<const LayoutRow> built = m_Built.RowsOf(setting);
                const std::string of = " of " + Quoted(setting.key);
                std::vector<std::string> order;
                Listed(value, "the rows" + of);
                const JsonValue& listed = value;
                for (size_t number = 1; number <= listed.items.size(); ++number)
                {
                    const JsonValue& row = listed.items[number - 1];
                    const std::string given = KeyText(row);
                    const auto known = std::ranges::find(built, given, [](const LayoutRow& r) { return std::string_view(r.key); });
                    const bool second = known != built.end() && known->otherTip != nullptr;
                    if (second)
                        Fields(row, "the row " + Quoted(given) + of, {"key", "name", "tip", "kinds", kBottomLineTip});
                    else
                        Fields(row, "the row " + Quoted(given) + of, {"key", "name", "tip", "kinds"});
                    const std::string key   = KeyOf(row, "row " + std::to_string(number) + of);
                    const std::string owner = "the row " + Quoted(key) + of;
                    if (known == built.end()) Bad(Quoted(key) + " is no row" + of);
                    if (std::ranges::find(order, key) != order.end()) Bad(owner + " is used twice");
                    order.push_back(key);
                    const std::optional<uint8_t> kinds = KindsOf(row, owner);
                    if (known->kinds == kEveryKind && kinds) Bad(owner + " has kinds, and the code has it on every kind its setting is on");
                    if (known->kinds != kEveryKind && kinds != known->kinds)
                        Bad(owner + " must name the kinds the code has it on: " + KindNames(known->kinds));
                    const std::optional<uint8_t> rowShown =
                        shown ? std::optional(static_cast<uint8_t>(*shown & kinds.value_or(kEveryKind))) : std::nullopt;
                    const Words name = Named(row, owner, true, rowShown);
                    const Words tip  = TextOf(row, "tip", owner, rowShown);
                    m_Out.rows.push_back(LayoutRow{known->row, known->key, name.shared, tip.shared, name.byKind, tip.byKind,
                        second ? TextOf(row, kBottomLineTip, owner, rowShown).shared : nullptr, kinds.value_or(kEveryKind)});
                }
                for (const LayoutRow& row : built)
                    if (std::ranges::find(order, row.key) == order.end()) Bad("the row " + Quoted(row.key) + of + " is left out");
                if (setting.rowsFixed && !std::ranges::equal(order, built, {}, {}, [](const LayoutRow& r) { return std::string_view(r.key); }))
                {
                    std::string keys;
                    for (const LayoutRow& row : built)
                        keys += (keys.empty() ? "" : ", ") + std::string(row.key);
                    Bad("the rows" + of + " come in the code's order: " + keys);
                }
            }

            // The words of a setting, or with none the window's own.
            void WordsOf(const JsonValue& value, const char* setting, const std::string& of)
            {
                const auto belongs = [&](const LayoutWord& w) {
                    return setting == nullptr ? w.setting == nullptr : w.setting != nullptr && std::string_view(w.setting) == setting;
                };
                std::set<std::string> seen;
                Listed(value, "the words" + of);
                const JsonValue& listed = value;
                for (size_t number = 1; number <= listed.items.size(); ++number)
                {
                    const JsonValue& word = listed.items[number - 1];
                    Fields(word, "the word " + Quoted(KeyText(word)) + of, {"key", "name", "tip"});
                    const std::string key   = KeyOf(word, "word " + std::to_string(number) + of);
                    const std::string owner = "the word " + Quoted(key) + of;
                    const auto known = std::ranges::find_if(m_Built.words, [&](const LayoutWord& w) { return belongs(w) && key == w.key; });
                    if (known == m_Built.words.end()) Bad(Quoted(key) + " is no word" + of);
                    if (!seen.insert(key).second) Bad(owner + " is used twice");
                    const bool named = known->name != nullptr;
                    const Words name = Named(word, owner, named);
                    if (!named && name.given) Bad(owner + " has a name, and the code shows only its tip");
                    m_Out.words.push_back(LayoutWord{known->word, name.shared, TextOf(word, "tip", owner).shared, known->key, known->setting});
                }
                for (const LayoutWord& word : m_Built.words)
                    if (belongs(word) && !seen.contains(word.key)) Bad("the word " + Quoted(word.key) + of + " is left out");
            }

            void LeftOut()
            {
                for (const LayoutSetting& setting : m_Built.settings)
                    if (std::ranges::find(m_Placed, std::string_view(setting.key), [](const auto& p) { return std::string_view(p.first); }) ==
                        m_Placed.end())
                        Bad("the setting " + Quoted(setting.key) + " is left out");
                for (const LayoutCard& card : m_Built.cards)
                    if (card.switched && !m_SeenCards.contains(card.key))
                        Bad("the card " + Quoted(card.key) + " is left out: its switch is the only one of its setting");
            }
        };
    }

    std::string ReadWindowFile(std::string_view text, const WindowLayout& builtIn, std::string_view version, OwnedLayout& out)
    {
        std::string problem;
        const std::optional<JsonValue> top = ParseJson(text, problem);
        if (!top) return "not JSON (" + problem + ")";
        OwnedLayout read;
        try
        {
            Reader(builtIn, read).Layout(*top, version);
        }
        catch (const Refused& refused)
        {
            return refused.problem;
        }
        out = std::move(read);
        return {};
    }
}
