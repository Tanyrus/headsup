#pragma once

#include "search_server.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace headsup
{
    // The search server's address as the client keeps it: 4 bytes as they go on the wire, and the port.
    struct SearchEndpoint
    {
        uint32_t address = 0;
        uint16_t port    = 0;
    };

    // Sends one ID list at a time on a thread of its own, so the game never waits on the network. The game's thread
    // asks and takes the answer; nothing here calls Ashita.
    class SearchClient
    {
    public:
        struct Result
        {
            std::vector<SearchedPlayer> players; // every packet's
            std::string failure;                 // empty when the whole answer came
        };

        ~SearchClient();
        // Starts a request; false while one is out.
        bool Ask(SearchEndpoint to, std::vector<uint32_t> ids, uint32_t nonce);
        // The last request's result, once.
        std::optional<Result> Take();
        // Ends the request out, if any, and the thread.
        void Stop();

    private:
        void Run();
        Result Exchange(SearchEndpoint to, const SearchRequest& request);

        std::thread m_Thread;
        std::mutex m_Lock;
        std::condition_variable m_Wake;
        std::optional<std::pair<SearchEndpoint, SearchRequest>> m_Job;
        std::optional<Result> m_Result;
        bool m_Busy = false, m_Stopping = false;
        uintptr_t m_Socket = ~uintptr_t{0}; // the one out, which Stop closes to end a wait on it
    };
}
