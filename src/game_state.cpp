#include "game_state.h"

#include <algorithm>
#include <utility>

namespace headsup
{
    namespace
    {
        // What a zone-in owes goes at once, though never within kConquestAskGap of the last ask; after that, every
        // kPopupRefresh while refresh.
        bool RequestDue(double now, bool owed, const std::optional<double>& asked, bool refresh)
        {
            if (owed) return !asked || now - *asked >= kConquestAskGap;
            return refresh && (!asked || now - *asked >= kPopupRefresh);
        }
    }

    PacketFollowUp GameState::Received(uint16_t id, const uint8_t* data, uint32_t size)
    {
        PacketFollowUp next;
        switch (id)
        {
        case kZoneInPacket:
            m_PlayerStatus.clear(); // target indexes are reused in the next zone
            m_PlayerSyncs.clear();
            m_Looks.clear();
            m_LookChanges.clear();
            m_Untargetable.clear();
            m_CalledForHelp.clear();
            m_ConquestOwed   = true;
            m_CurrenciesOwed = true;
            m_Shares.reset();
            m_Weather    = ParseZoneInWeather(data, size);
            next.zonedIn = true;
            if (const std::optional<uint32_t> player = ParseZoneInPlayer(data, size))
            {
                // Another character's points are not yours: unknown until the server says.
                if (m_Player && *m_Player != *player)
                {
                    m_ConquestBytes.clear();
                    m_Currencies = CurrencyValues{};
                }
                m_Player = *player;
            }
            break;
        case kWeatherPacket:
            if (const auto weather = ParseWeather(data, size)) m_Weather = *weather;
            break;
        case kEventNumPacket:
            next.guardEvent = ParseGuardEvent(data, size);
            break;
        case kConquestPacket:
            m_ConquestOwed = false; // whether HeadsUp asked or not
            if (const auto owners = ParseConquest(data, size)) m_Owners = *owners;
            if (const auto shares = ParseRegionShares(data, size)) m_Shares = *shares;
            if (data != nullptr) m_ConquestBytes.assign(data, data + size);
            break;
        case kCurrencies1Packet:
        case kCurrencies2Packet:
            ReadCurrencies(m_Currencies, id, data, size);
            break;
        case kOtherPlayerPacket:
            if (const auto update = ParseOtherPlayer(data, size))
            {
                if (update->despawn)
                {
                    m_PlayerStatus.erase(update->index);
                    m_PlayerSyncs.erase(update->index);
                    m_Looks.erase(update->index);
                }
                else if (update->status)
                    m_PlayerStatus[update->index] = *update->status;
                if (const std::optional<PlayerLook> look = update->despawn ? std::nullopt : ParseLook(data, size))
                {
                    const auto [seen, first] = m_Looks.try_emplace(update->index, *look);
                    if (!first && seen->second != *look) m_LookChanges.push_back(update->index);
                    seen->second = *look;
                }
            }
            break;
        case kLevelMessagePacket:
            if (const auto change = ParseLevelMessage(data, size))
            {
                PlayerSync& sync = m_PlayerSyncs[change->index];
                sync.level       = change->level;
                sync.held        = false;
                sync.sameJob     = true;
            }
            break;
        case kEntityUpdatePacket:
            if (const auto update = ParseEntityUpdate(data, size))
            {
                if (update->despawn || update->untargetable == false)
                    m_Untargetable.erase(update->index);
                else if (update->untargetable == true)
                    m_Untargetable.insert(update->index);
                if (update->despawn || update->calledForHelp == false)
                    m_CalledForHelp.erase(update->index);
                else if (update->calledForHelp == true)
                    m_CalledForHelp.insert(update->index);
            }
            break;
        case kCharSyncPacket:
            if (const auto sync = ParseCharSync(data, size))
            {
                m_PlayerSyncs[sync->index] = PlayerSync{sync->synced, sync->held, sync->level};
            }
            break;
        case kOwnStatusPacket:
            if (const auto own = ParseOwnStatus(data, size)) m_OwnStatus = *own;
            break;
        case kCheckReplyPacket:
            next.check  = ParseCheckReply(data, size);
            next.answer = ParseCheckAnswer(data, size);
            next.mobDied = ParseMobDeath(data, size);
            break;
        case kInspectMessagePacket:
            m_CheckMessage = ParseInspectMessage(data, size);
            break;
        case kInspectGeneralPacket:
            if (const auto general = ParseInspectGeneral(data, size))
            {
                m_PlayerChecks[general->serverId] = PlayerCheck{*general, m_CheckMessage.value_or(InspectMessage{}), ++m_Order};
                if (const auto it = m_PlayerSyncs.find(general->index); it != m_PlayerSyncs.end()) it->second.level = 0;
                m_CheckMessage.reset();
            }
            break;
        }
        return next;
    }

    const PlayerStatus* GameState::PlayerStatusOf(uint32_t index, uint32_t self) const
    {
        if (index == self) return m_OwnStatus ? &*m_OwnStatus : nullptr;
        const auto it = m_PlayerStatus.find(static_cast<uint16_t>(index));
        return it != m_PlayerStatus.end() ? &it->second : nullptr;
    }

    PlayerSync GameState::SyncOf(uint16_t index) const
    {
        const auto it = m_PlayerSyncs.find(index);
        return it != m_PlayerSyncs.end() ? it->second : PlayerSync{};
    }

    bool GameState::Untargetable(uint16_t index) const { return m_Untargetable.count(index) != 0; }
    bool GameState::CalledForHelp(uint16_t index) const { return m_CalledForHelp.count(index) != 0; }

    bool GameState::ConquestRequestDue(double now, bool open, bool refresh) const
    {
        return open && RequestDue(now, m_ConquestOwed, m_ConquestAsked, refresh);
    }

    void GameState::ConquestRequested(double now)
    {
        m_ConquestOwed  = false;
        m_ConquestAsked = now;
    }

    bool GameState::CurrencyRequestDue(double now, bool open, bool refresh) const
    {
        return open && RequestDue(now, m_CurrenciesOwed, m_CurrenciesAsked, refresh);
    }

    void GameState::CurrencyRequested(double now)
    {
        m_CurrenciesOwed  = false;
        m_CurrenciesAsked = now;
    }

    const PlayerCheck* GameState::PlayerCheckOf(uint32_t serverId) const
    {
        const auto it = m_PlayerChecks.find(serverId);
        return it != m_PlayerChecks.end() ? &it->second : nullptr;
    }

    void GameState::LookedUp(uint32_t serverId, uint16_t index, const SearchedPlayer& answer)
    {
        m_Lookups[serverId] = Lookup{PlayerJobs{answer.mainJob, answer.mainLevel, answer.subJob, answer.subLevel}, ++m_Order};
        if (const auto it = m_PlayerSyncs.find(index); it != m_PlayerSyncs.end()) it->second.level = 0;
    }

    std::optional<PlayerJobs> GameState::ReportedJobs(uint32_t serverId) const
    {
        const auto check = m_PlayerChecks.find(serverId);
        const auto found = m_Lookups.find(serverId);
        const bool checked = check != m_PlayerChecks.end(), looked = found != m_Lookups.end();
        if (!checked && !looked) return std::nullopt;
        const PlayerJobs jobs = looked && (!checked || found->second.order > check->second.order)
                                    ? found->second.jobs
                                    : PlayerJobs{check->second.general.mainJob, check->second.general.mainLevel,
                                          check->second.general.subJob, check->second.general.subLevel};
        return jobs.mainJob != 0 ? std::optional(jobs) : std::nullopt;
    }

    std::vector<uint16_t> GameState::TakeLookChanges() { return std::exchange(m_LookChanges, {}); }

    bool SharesLinkshell(const PlayerCheck* check, std::span<const std::string> yours)
    {
        if (check == nullptr || check->general.linkshell.empty()) return false;
        return std::ranges::find(yours, check->general.linkshell) != yours.end();
    }
}
