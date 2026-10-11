#include "currencies.h"

#include "bytes.h"

#include <algorithm>

namespace headsup
{
    void ReadCurrencies(CurrencyValues& values, uint16_t id, const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || (id != kCurrencies1Packet && id != kCurrencies2Packet)) return;
        uint32_t needed = 0;
        for (const CurrencyInfo& c : kCurrencyTable)
            if (c.packet == id) needed = std::max<uint32_t>(needed, c.offset + c.bytes);
        if (size < needed) return;
        for (int i = 0; i < kCurrencyCount; ++i)
        {
            const CurrencyInfo& c = kCurrencyTable[i];
            if (c.packet != id) continue;
            values[i] = c.bytes == 2 ? ReadAt<uint16_t>(data, c.offset)
                                     : static_cast<uint32_t>(std::max(0, ReadAt<int32_t>(data, c.offset)));
        }
    }

    std::optional<int> CurrencyOfKey(std::string_view key)
    {
        for (int i = 0; i < kCurrencyCount; ++i)
            if (key == kCurrencyTable[i].key) return i;
        return std::nullopt;
    }
}
