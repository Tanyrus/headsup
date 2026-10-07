#include "pointer_swap.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace headsup
{
    namespace
    {
        constexpr const char* kGameModule    = "FFXiMain.dll";
        constexpr const char* kUser32        = "user32.dll"; // matched without case, as the import table spells it USER32
        constexpr const char* kSetCursor     = "SetCursor";
        constexpr const char* kReplacedPointers[] = {"mousenor.ani", "mousehit.ani"}; // the normal and the hover pointers
        constexpr DWORD kCursorFormat        = 0x00030000; // what CreateIconFromResourceEx expects of an icon or cursor

        using SetCursorFn = HCURSOR(WINAPI*)(HCURSOR);
        SetCursorFn g_RealSetCursor = nullptr;
        PointerSwap* g_Swap         = nullptr;

        HCURSOR WINAPI SetCursorHook(HCURSOR cursor)
        {
            return g_Swap != nullptr ? g_Swap->OnSetCursor(cursor) : g_RealSetCursor(cursor);
        }

        // The module's import address slot that holds `function` from `library`; nullptr when it has none.
        void** ImportSlot(HMODULE module, const char* library, void* function)
        {
            auto* base     = reinterpret_cast<uint8_t*>(module);
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
            const IMAGE_DATA_DIRECTORY& imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (imports.VirtualAddress == 0) return nullptr;
            for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress); d->Name != 0; ++d)
            {
                if (_stricmp(reinterpret_cast<const char*>(base + d->Name), library) != 0) continue;
                for (auto* slot = reinterpret_cast<void**>(base + d->FirstThunk); *slot != nullptr; ++slot)
                    if (*slot == function) return slot;
            }
            return nullptr;
        }

        bool Write(void** slot, void* value)
        {
            DWORD old = 0;
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
            *slot = value;
            VirtualProtect(slot, sizeof(void*), old, &old);
            return true;
        }

        // A bitmap's pixels at 32 bits, top to bottom.
        std::vector<uint8_t> Pixels(HDC dc, HBITMAP bitmap, LONG width, LONG height)
        {
            BITMAPINFO info{};
            info.bmiHeader.biSize        = sizeof(info.bmiHeader);
            info.bmiHeader.biWidth       = width;
            info.bmiHeader.biHeight      = -height;
            info.bmiHeader.biPlanes      = 1;
            info.bmiHeader.biBitCount    = 32;
            info.bmiHeader.biCompression = BI_RGB;
            std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
            if (GetDIBits(dc, bitmap, 0, static_cast<UINT>(height), pixels.data(), &info, DIB_RGB_COLORS) != height) return {};
            return pixels;
        }
    }

    std::string PointerSwap::Start()
    {
        if (Running()) return "";
        HMODULE game = GetModuleHandleA(kGameModule);
        if (game == nullptr) return std::string("the game's ") + kGameModule + " is not loaded";
        char path[MAX_PATH] = {};
        if (GetModuleFileNameA(game, path, MAX_PATH) == 0) return "the game's folder was not found";
        const std::string folder(path, std::string(path).find_last_of("\\/") + 1);
        for (const char* name : kReplacedPointers)
        {
            std::ifstream in(folder + name, std::ios::binary);
            const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
            const std::vector<CursorFrame> frames = ReadCursorFile(bytes.data(), bytes.size());
            if (frames.empty())
            {
                Release();
                return "could not read the game's pointer " + folder + name;
            }
            m_Replaced.insert(m_Replaced.end(), frames.begin(), frames.end());
        }

        m_Chocobo = ChocoboPointer();
        for (CursorFrame& f : m_Chocobo)
        {
            HICON frame = CreateIconFromResourceEx(f.resource.data(), static_cast<DWORD>(f.resource.size()), FALSE, kCursorFormat,
                static_cast<int>(f.image.width), static_cast<int>(f.image.height), LR_DEFAULTCOLOR);
            if (frame == nullptr)
            {
                Release();
                return "Windows could not make the chocobo pointer";
            }
            m_Frames.push_back(frame);
        }

        g_RealSetCursor = reinterpret_cast<SetCursorFn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA(kUser32), kSetCursor)));
        void** slot     = ImportSlot(game, kUser32, reinterpret_cast<void*>(g_RealSetCursor));
        if (slot == nullptr || !Write(slot, reinterpret_cast<void*>(&SetCursorHook)))
        {
            Release();
            return std::string("could not reach ") + kGameModule + "'s " + kSetCursor;
        }
        m_Slot = slot;
        g_Swap = this;
        QueryPerformanceFrequency(&m_Frequency);
        QueryPerformanceCounter(&m_Start);
        return "";
    }

    void PointerSwap::Stop()
    {
        if (m_Slot != nullptr)
        {
            Write(m_Slot, reinterpret_cast<void*>(g_RealSetCursor));
            m_Slot = nullptr;
        }
        g_Swap = nullptr;
        if (g_RealSetCursor != nullptr && Ours(GetCursor()) && m_GamePointer != nullptr) g_RealSetCursor(m_GamePointer);
        Release();
    }

    void PointerSwap::Release()
    {
        for (HCURSOR frame : m_Frames)
            DestroyCursor(frame);
        m_Frames.clear();
        m_Known.clear();
        m_Replaced.clear();
        m_Chocobo.clear();
        m_GamePointer = nullptr;
    }

    HCURSOR PointerSwap::OnSetCursor(HCURSOR cursor)
    {
        if (cursor == nullptr || !Replaced(cursor)) return g_RealSetCursor(cursor);
        m_GamePointer = cursor;
        return g_RealSetCursor(Frame());
    }

    void PointerSwap::Animate()
    {
        if (!Running()) return;
        const HCURSOR current = GetCursor();
        if (Ours(current) && current != Frame()) g_RealSetCursor(Frame());
    }

    bool PointerSwap::Ours(HCURSOR cursor) const { return std::find(m_Frames.begin(), m_Frames.end(), cursor) != m_Frames.end(); }

    HCURSOR PointerSwap::Frame() const
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        const double seconds = static_cast<double>(now.QuadPart - m_Start.QuadPart) / static_cast<double>(m_Frequency.QuadPart);
        return m_Frames[FrameAt(m_Chocobo, seconds)];
    }

    bool PointerSwap::Replaced(HCURSOR cursor)
    {
        if (const auto known = m_Known.find(cursor); known != m_Known.end()) return known->second;
        bool replaced = false;
        ICONINFO info{};
        if (GetIconInfo(cursor, &info))
        {
            BITMAP bm{};
            if (info.hbmColor != nullptr && GetObjectA(info.hbmColor, sizeof(bm), &bm) == sizeof(bm))
            {
                HDC dc                           = GetDC(nullptr);
                const std::vector<uint8_t> color = Pixels(dc, info.hbmColor, bm.bmWidth, bm.bmHeight);
                const std::vector<uint8_t> mask  = Pixels(dc, info.hbmMask, bm.bmWidth, bm.bmHeight);
                ReleaseDC(nullptr, dc);
                if (!color.empty() && !mask.empty())
                {
                    const CursorImage image = ImageFromBitmaps(static_cast<uint32_t>(bm.bmWidth), static_cast<uint32_t>(bm.bmHeight),
                        static_cast<uint16_t>(info.xHotspot), static_cast<uint16_t>(info.yHotspot), color, mask);
                    replaced = std::any_of(m_Replaced.begin(), m_Replaced.end(), [&](const CursorFrame& f) { return SameCursor(f.image, image); });
                }
            }
            if (info.hbmColor != nullptr) DeleteObject(info.hbmColor);
            if (info.hbmMask != nullptr) DeleteObject(info.hbmMask);
        }
        m_Known[cursor] = replaced;
        return replaced;
    }
}
