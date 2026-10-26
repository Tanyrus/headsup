#include "search_client.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <utility>

namespace headsup
{
    namespace
    {
        constexpr DWORD kWaitMs        = 5000; // to connect, and for each read
        constexpr long kMsPerSecond    = 1000;
        constexpr WORD kWinsockVersion = MAKEWORD(2, 2);

        std::string Failed(const char* what) { return std::string(what) + " (" + std::to_string(WSAGetLastError()) + ")"; }

        bool ReadAll(SOCKET s, uint8_t* into, size_t want)
        {
            for (size_t got = 0; got < want;)
            {
                const int n = recv(s, reinterpret_cast<char*>(into + got), static_cast<int>(want - got), 0);
                if (n <= 0) return false;
                got += static_cast<size_t>(n);
            }
            return true;
        }
    }

    SearchClient::~SearchClient() { Stop(); }

    bool SearchClient::Ask(SearchEndpoint to, std::vector<uint32_t> ids, uint32_t nonce)
    {
        std::lock_guard<std::mutex> lock(m_Lock);
        if (m_Busy || m_Stopping) return false;
        if (!m_Thread.joinable()) m_Thread = std::thread([this] { Run(); });
        m_Job.emplace(to, IdListRequest(ids, nonce));
        m_Busy = true;
        m_Wake.notify_one();
        return true;
    }

    std::optional<SearchClient::Result> SearchClient::Take()
    {
        std::lock_guard<std::mutex> lock(m_Lock);
        return std::exchange(m_Result, std::nullopt);
    }

    void SearchClient::Stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_Lock);
            m_Stopping = true;
            if (m_Socket != ~uintptr_t{0}) closesocket(static_cast<SOCKET>(std::exchange(m_Socket, ~uintptr_t{0})));
            m_Wake.notify_one();
        }
        if (m_Thread.joinable()) m_Thread.join();
    }

    void SearchClient::Run()
    {
        WSADATA wsa{};
        const bool started = WSAStartup(kWinsockVersion, &wsa) == 0;
        for (;;)
        {
            std::pair<SearchEndpoint, SearchRequest> job;
            {
                std::unique_lock<std::mutex> lock(m_Lock);
                m_Wake.wait(lock, [this] { return m_Stopping || m_Job.has_value(); });
                if (m_Stopping) break;
                job = std::move(*m_Job);
                m_Job.reset();
            }
            Result result = started ? Exchange(job.first, job.second) : Result{{}, "Winsock did not start"};
            std::lock_guard<std::mutex> lock(m_Lock);
            m_Result = std::move(result);
            m_Busy   = false;
        }
        if (started) WSACleanup();
    }

    SearchClient::Result SearchClient::Exchange(SearchEndpoint to, const SearchRequest& request)
    {
        const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) return {{}, Failed("no socket")};
        {
            std::lock_guard<std::mutex> lock(m_Lock);
            if (m_Stopping)
            {
                closesocket(s);
                return {{}, "stopping"};
            }
            m_Socket = static_cast<uintptr_t>(s);
        }
        const auto done = [&](Result r) {
            std::lock_guard<std::mutex> lock(m_Lock);
            if (m_Socket != ~uintptr_t{0}) closesocket(s);
            m_Socket = ~uintptr_t{0};
            return r;
        };
        // A connect with its own wait: blocking, it could hold the thread, and so an unload, for a minute.
        u_long nonBlocking = 1;
        ioctlsocket(s, FIONBIO, &nonBlocking);
        sockaddr_in address{};
        address.sin_family      = AF_INET;
        address.sin_port        = htons(to.port);
        address.sin_addr.s_addr = to.address;
        if (connect(s, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR &&
            WSAGetLastError() != WSAEWOULDBLOCK)
            return done({{}, Failed("could not connect")});
        fd_set writable, failed;
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        FD_SET(s, &writable);
        FD_SET(s, &failed);
        timeval wait{static_cast<long>(kWaitMs / kMsPerSecond), 0};
        if (select(0, nullptr, &writable, &failed, &wait) != 1 || !FD_ISSET(s, &writable))
            return done({{}, "the search server did not answer the connection"});
        u_long blocking = 0;
        ioctlsocket(s, FIONBIO, &blocking);
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&kWaitMs), sizeof(kWaitMs));
        if (send(s, reinterpret_cast<const char*>(request.bytes.data()), static_cast<int>(request.bytes.size()), 0) !=
            static_cast<int>(request.bytes.size()))
            return done({{}, Failed("could not send")});
        Result result;
        for (;;)
        {
            std::vector<uint8_t> packet(sizeof(uint16_t));
            if (!ReadAll(s, packet.data(), packet.size())) return done({{}, "the answer stopped"});
            const std::optional<size_t> size = SearchPacketSize(packet);
            if (!size) return done({{}, "the answer's size is wrong"});
            packet.resize(*size);
            if (!ReadAll(s, packet.data() + sizeof(uint16_t), *size - sizeof(uint16_t))) return done({{}, "the answer stopped"});
            const std::optional<SearchPacket> read = ReadSearchPacket(packet, request.answerKey);
            if (!read) return done({{}, "the answer could not be read"});
            result.players.insert(result.players.end(), read->players.begin(), read->players.end());
            if (read->last) return done(std::move(result));
        }
    }
}
