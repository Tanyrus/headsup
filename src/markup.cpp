#include "markup.h"

#include "argb.h"

#include <cstdio>

namespace headsup
{
    std::string EscapedRml(std::string_view text)
    {
        std::string out;
        out.reserve(text.size());
        for (const char c : text)
            switch (c)
            {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
            }
        return out;
    }

    std::string RcssColor(uint32_t argb)
    {
        char text[16];
        std::snprintf(text, sizeof(text), "#%02x%02x%02x%02x", Channel(argb, kRedShift), Channel(argb, kGreenShift),
            Channel(argb, kBlueShift), Channel(argb, kAlphaShift));
        return text;
    }

    std::string RcssNumber(float value)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%.2f", static_cast<double>(value));
        std::string out = text;
        while (out.back() == '0')
            out.pop_back();
        if (out.back() == '.') out.pop_back();
        return out == "-0" ? "0" : out;
    }

    std::string Px(float value) { return RcssNumber(value) + "px"; }
    std::string Em(float value) { return RcssNumber(value) + "em"; }
}
