#include "server_ping.h"

#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include "bar_connection.h" // kRoundTrips
#include "import_slot.h"
#include "native_hook.h" // kGameModule

#include <atomic>

namespace headsup
{
    namespace
    {
        constexpr const char* kSockets = "ws2_32.dll"; // matched without case, as the import table spells it WS2_32
        // FFXiMain.dll imports sendto by Winsock's fixed ordinal. Phoenix's zonetrace.dll redirects it first, so the
        // slot is found by that and each call goes on to what the slot held.
        constexpr WORD kSendToOrdinal = 20;
        constexpr auto kPingEvery     = std::chrono::seconds(1);
        constexpr auto kIdle          = std::chrono::seconds(3); // with nothing taking the round trips, it stops pinging
        constexpr DWORD kTimeoutMs    = 1000;
        constexpr size_t kPayload     = 32;  // as ping.exe sends
        constexpr size_t kReplySlack  = 8;   // IcmpSendEcho's room for an ICMP error
        constexpr DWORD kSettleMs     = 1;   // between looks for a call still inside the hook

        using SendToFn = int(WINAPI*)(SOCKET, const char*, int, int, const sockaddr*, int);
        SendToFn g_Next = nullptr; // what the slot held: Winsock's, or another hook's
        std::atomic<ServerPing*> g_Ping{nullptr};
        std::atomic<int> g_Inside{0}; // calls in the hook now, which Stop waits out so HeadsUp can unload

        int WINAPI SendToHook(SOCKET socket, const char* data, int size, int flags, const sockaddr* to, int toSize)
        {
            ++g_Inside;
            if (ServerPing* ping = g_Ping; ping != nullptr && to != nullptr && toSize >= static_cast<int>(sizeof(sockaddr_in)) &&
                                           to->sa_family == AF_INET)
                ping->SendingTo(reinterpret_cast<const sockaddr_in*>(to)->sin_addr.s_addr);
            const int sent = g_Next(socket, data, size, flags, to, toSize);
            --g_Inside;
            return sent;
        }
    }

    std::string ServerPing::Start()
    {
        if (m_Slot != nullptr) return "";
        HMODULE game = GetModuleHandleA(kGameModule);
        if (game == nullptr) return std::string("the game's ") + kGameModule + " is not loaded";
        void** slot = ImportSlotByOrdinal(game, kSockets, kSendToOrdinal);
        if (slot == nullptr) return std::string("could not reach ") + kGameModule + "'s sendto";
        const HANDLE icmp = IcmpCreateFile();
        if (icmp == INVALID_HANDLE_VALUE) return "Windows would not open a handle to ping with";
        m_Icmp = icmp;
        g_Next = reinterpret_cast<SendToFn>(*slot);
        g_Ping = this;
        if (!WriteSlot(slot, reinterpret_cast<void*>(&SendToHook)))
        {
            Stop();
            return std::string("could not reach ") + kGameModule + "'s sendto";
        }
        m_Slot   = slot;
        m_Thread = std::thread([this] { Run(); });
        return "";
    }

    void ServerPing::Stop()
    {
        if (m_Slot != nullptr)
        {
            // Put back what was there, unless another hook has since taken the slot over from HeadsUp's.
            if (*m_Slot == reinterpret_cast<void*>(&SendToHook)) WriteSlot(m_Slot, reinterpret_cast<void*>(g_Next));
            m_Slot = nullptr;
        }
        g_Ping = nullptr;
        // A call that read the slot just before it changed may not have counted itself in yet.
        Sleep(kSettleMs);
        while (g_Inside > 0)
            Sleep(kSettleMs);
        {
            const std::lock_guard lock(m_Lock);
            m_Stopping = true;
            m_Wake.notify_one();
        }
        // An echo out holds the thread until it is answered or kTimeoutMs pass.
        if (m_Thread.joinable()) m_Thread.join();
        if (m_Icmp != nullptr) IcmpCloseHandle(static_cast<HANDLE>(m_Icmp));
        m_Icmp = nullptr;
    }

    std::vector<double> ServerPing::TakeRoundTrips()
    {
        const std::lock_guard lock(m_Lock);
        m_TakenAt = std::chrono::steady_clock::now();
        std::vector<double> taken;
        taken.swap(m_RoundTrips);
        return taken;
    }

    void ServerPing::SendingTo(uint32_t address)
    {
        const std::lock_guard lock(m_Lock);
        m_Address = address;
    }

    void ServerPing::Run()
    {
        char payload[kPayload] = {};
        alignas(ICMP_ECHO_REPLY) uint8_t reply[sizeof(ICMP_ECHO_REPLY) + kPayload + kReplySlack];
        std::unique_lock lock(m_Lock);
        while (!m_Wake.wait_for(lock, kPingEvery, [this] { return m_Stopping; }))
        {
            const uint32_t address = m_Address;
            if (address == 0 || std::chrono::steady_clock::now() - m_TakenAt > kIdle) continue;
            lock.unlock();
            const DWORD replies = IcmpSendEcho(static_cast<HANDLE>(m_Icmp), address, payload, sizeof(payload), nullptr, reply,
                sizeof(reply), kTimeoutMs);
            lock.lock();
            const auto* echo = reinterpret_cast<const ICMP_ECHO_REPLY*>(reply);
            if (replies == 0 || echo->Status != IP_SUCCESS) continue;
            m_RoundTrips.push_back(static_cast<double>(echo->RoundTripTime));
            if (m_RoundTrips.size() > kRoundTrips) m_RoundTrips.erase(m_RoundTrips.begin());
        }
    }
}
