#include "text_raster.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

namespace headsup
{
    namespace
    {
        int CALLBACK AddFamily(const LOGFONTA* font, const TEXTMETRICA* metrics, DWORD type, LPARAM families)
        {
            (void)metrics;
            if ((type & TRUETYPE_FONTTYPE) != 0) reinterpret_cast<std::vector<std::string>*>(families)->push_back(font->lfFaceName);
            return 1;
        }
    }

    std::vector<std::string> InstalledFontFamilies()
    {
        std::vector<std::string> families;
        HDC dc = CreateCompatibleDC(nullptr);
        if (dc == nullptr) return families;
        LOGFONTA every{};
        every.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExA(dc, &every, AddFamily, reinterpret_cast<LPARAM>(&families), 0);
        DeleteDC(dc);
        return families;
    }

    bool RasterizeText(const char* text, const char* family, int pixelHeight, bool bold, int margin, Coverage& out)
    {
        out      = Coverage{};
        HDC dc   = CreateCompatibleDC(nullptr);
        HFONT font = CreateFontA(-pixelHeight, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family);
        bool drawn = false;
        if (dc != nullptr && font != nullptr)
        {
            const HGDIOBJ oldFont = SelectObject(dc, font);
            const int length      = static_cast<int>(std::strlen(text));
            SIZE size{};
            GetTextExtentPoint32A(dc, text, length, &size);
            const int width  = size.cx + 2 * margin;
            const int height = size.cy + 2 * margin;
            BITMAPINFO info{};
            info.bmiHeader.biSize        = sizeof(info.bmiHeader);
            info.bmiHeader.biWidth       = width;
            info.bmiHeader.biHeight      = -height; // rows top to bottom
            info.bmiHeader.biPlanes      = 1;
            info.bmiHeader.biBitCount    = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void* bits                   = nullptr;
            const HBITMAP bitmap         = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (bitmap != nullptr && bits != nullptr)
            {
                const HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
                const size_t pixels     = static_cast<size_t>(width) * static_cast<size_t>(height);
                std::memset(bits, 0, pixels * 4);
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, RGB(255, 255, 255));
                TextOutA(dc, margin, margin, text, length);
                GdiFlush();
                const auto* bgrx = static_cast<const uint8_t*>(bits);
                out.width        = width;
                out.height       = height;
                out.alpha.resize(pixels);
                for (size_t i = 0; i < pixels; ++i)
                    out.alpha[i] = std::max({bgrx[i * 4], bgrx[i * 4 + 1], bgrx[i * 4 + 2]});
                SelectObject(dc, oldBitmap);
                drawn = true;
            }
            if (bitmap != nullptr) DeleteObject(bitmap);
            SelectObject(dc, oldFont);
        }
        if (font != nullptr) DeleteObject(font);
        if (dc != nullptr) DeleteDC(dc);
        return drawn;
    }
}
