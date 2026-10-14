#pragma once

#include "test.h"
#include "window_layout.h"

#include <algorithm>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace test
{
    // The window's layout as a player's own file could make it: pages, cards and settings moved, renamed, kept to
    // other kinds. Layout() gives it as the window takes it, good until the next edit.
    class EditedLayout
    {
    public:
        explicit EditedLayout(const headsup::WindowLayout& from = headsup::kWindowLayout) : m_From(from)
        {
            const headsup::LayoutPage& mobs = from.PageOf(headsup::Page::Mobs);
            for (const headsup::LayoutCard& card : from.CardsOf(mobs))
                m_Entity.push_back(CardOf(card));
            for (const headsup::LayoutPage& page : from.pages)
            {
                Page& kept = m_Pages.emplace_back(Page{page, {}});
                if (page.firstCard == mobs.firstCard) continue;
                for (const headsup::LayoutCard& card : from.CardsOf(page))
                    kept.cards.push_back(CardOf(card));
            }
        }

        void MovePage(std::string_view key, size_t at)
        {
            const auto page = Find(m_Pages, key, [](const Page& p) { return p.page.key; });
            Page moved      = *page;
            m_Pages.erase(page);
            m_Pages.insert(m_Pages.begin() + static_cast<std::ptrdiff_t>(std::min(at, m_Pages.size())), moved);
        }
        // To one of the entity pages: into the cards the five share.
        void MoveCard(std::string_view key, std::string_view toPage, size_t at)
        {
            Card moved = Take(key);
            std::vector<Card>& to = CardsOf(toPage);
            to.insert(to.begin() + static_cast<std::ptrdiff_t>(std::min(at, to.size())), moved);
        }
        void MoveSetting(std::string_view key, std::string_view toCard, size_t at)
        {
            headsup::LayoutSetting moved{};
            ForEachCard([&](Card& card) {
                const auto found = std::ranges::find_if(card.settings, [&](const auto& s) { return key == s.key; });
                if (found == card.settings.end()) return;
                moved = *found;
                card.settings.erase(found);
            });
            Card& to = CardNamed(toCard);
            to.settings.insert(to.settings.begin() + static_cast<std::ptrdiff_t>(std::min(at, to.settings.size())), moved);
        }
        void RenamePage(std::string_view key, std::string name) { Find(m_Pages, key, [](const Page& p) { return p.page.key; })->page.name = Kept(std::move(name)); }
        void RenameCard(std::string_view key, std::string name) { CardNamed(key).card.name = Kept(std::move(name)); }
        void RenameSetting(std::string_view key, std::string name)
        {
            ForEachCard([&](Card& card) {
                for (headsup::LayoutSetting& setting : card.settings)
                    if (key == setting.key) setting.name = Kept(name);
            });
        }
        void SetKinds(std::string_view card, uint8_t kinds) { CardNamed(card).card.kinds = kinds; }

        headsup::WindowLayout Layout()
        {
            m_FlatPages.clear();
            m_FlatCards.clear();
            m_FlatSettings.clear();
            const auto flatten = [&](std::vector<Card>& cards) {
                const int first = static_cast<int>(m_FlatCards.size());
                for (Card& card : cards)
                {
                    card.card.firstSetting = static_cast<int>(m_FlatSettings.size());
                    card.card.settingCount = static_cast<int>(card.settings.size());
                    m_FlatSettings.insert(m_FlatSettings.end(), card.settings.begin(), card.settings.end());
                    m_FlatCards.push_back(card.card);
                }
                return first;
            };
            const int entity = flatten(m_Entity);
            for (Page& page : m_Pages)
            {
                const bool shares = page.page.firstCard == m_From.PageOf(headsup::Page::Mobs).firstCard;
                page.page.cardCount = static_cast<int>(shares ? m_Entity.size() : page.cards.size());
                const int first     = shares ? entity : flatten(page.cards);
                m_FlatPages.push_back(page.page);
                m_FlatPages.back().firstCard = first;
            }
            return headsup::WindowLayout{m_FlatPages, m_FlatCards, m_FlatSettings, m_From.rows, m_From.options,
                m_From.lists, m_From.words};
        }

    private:
        struct Card
        {
            headsup::LayoutCard card;
            std::vector<headsup::LayoutSetting> settings;
        };
        struct Page
        {
            headsup::LayoutPage page;
            std::vector<Card> cards;
        };

        Card CardOf(const headsup::LayoutCard& card) const
        {
            const auto settings = m_From.SettingsOf(card);
            return Card{card, {settings.begin(), settings.end()}};
        }
        template <typename List, typename KeyOf>
        static typename List::iterator Find(List& list, std::string_view key, KeyOf keyOf)
        {
            const auto found = std::ranges::find_if(list, [&](const auto& item) { return key == keyOf(item); });
            if (found == list.end()) throw Failure{"the layout has no " + std::string(key)};
            return found;
        }
        template <typename Visit>
        void ForEachCard(Visit visit)
        {
            for (Card& card : m_Entity)
                visit(card);
            for (Page& page : m_Pages)
                for (Card& card : page.cards)
                    visit(card);
        }
        Card& CardNamed(std::string_view key)
        {
            Card* found = nullptr;
            ForEachCard([&](Card& card) {
                if (key == card.card.key) found = &card;
            });
            if (found == nullptr) throw Failure{"the layout has no card " + std::string(key)};
            return *found;
        }
        Card Take(std::string_view key)
        {
            Card taken = CardNamed(key);
            const auto drop = [&](std::vector<Card>& cards) {
                std::erase_if(cards, [&](const Card& card) { return key == card.card.key; });
            };
            drop(m_Entity);
            for (Page& page : m_Pages)
                drop(page.cards);
            return taken;
        }
        std::vector<Card>& CardsOf(std::string_view page)
        {
            Page& to = *Find(m_Pages, page, [](const Page& p) { return p.page.key; });
            return to.page.firstCard == m_From.PageOf(headsup::Page::Mobs).firstCard ? m_Entity : to.cards;
        }
        const char* Kept(std::string text) { return m_Names.emplace_back(std::move(text)).c_str(); }

        headsup::WindowLayout m_From;
        std::vector<Card> m_Entity;
        std::vector<Page> m_Pages;
        std::vector<headsup::LayoutPage> m_FlatPages;
        std::vector<headsup::LayoutCard> m_FlatCards;
        std::vector<headsup::LayoutSetting> m_FlatSettings;
        std::deque<std::string> m_Names;
    };
}
