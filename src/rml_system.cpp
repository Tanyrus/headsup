#include "rml_system.h"

#include "bar_document.h"
#include "utf16.h"

#include <windows.h>

#include <cstring>
#include <utility>

namespace headsup
{
    RmlSystem::RmlSystem(std::function<void(const std::string&)> warn)
        : m_Warn(std::move(warn)), m_Start(std::chrono::steady_clock::now())
    {}

    double RmlSystem::GetElapsedTime()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_Start).count();
    }

    bool RmlSystem::LogMessage(Rml::Log::Type type, const Rml::String& message)
    {
        if (type <= Rml::Log::LT_WARNING && m_Warn) m_Warn(message);
        return true;
    }

    void RmlSystem::JoinPath(Rml::String& translated, const Rml::String& documentPath, const Rml::String& path)
    {
        if (path.rfind(kHostSource, 0) == 0)
            translated = path;
        else
            Rml::SystemInterface::JoinPath(translated, documentPath, path);
    }

    void RmlSystem::SetClipboardText(const Rml::String& text)
    {
        const std::wstring wide = Utf16(text);
        if (!OpenClipboard(nullptr)) return;
        EmptyClipboard();
        const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
        if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes); memory != nullptr)
        {
            if (void* at = GlobalLock(memory); at != nullptr)
            {
                std::memcpy(at, wide.c_str(), bytes);
                GlobalUnlock(memory);
                // The clipboard owns the memory from here; a refused handle is freed.
                if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) GlobalFree(memory);
            }
            else
                GlobalFree(memory);
        }
        CloseClipboard();
    }

    void RmlSystem::GetClipboardText(Rml::String& text)
    {
        text.clear();
        if (!OpenClipboard(nullptr)) return;
        if (HANDLE data = GetClipboardData(CF_UNICODETEXT); data != nullptr)
        {
            if (const auto* wide = static_cast<const wchar_t*>(GlobalLock(data)); wide != nullptr)
            {
                const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
                if (bytes > 1)
                {
                    text.resize(static_cast<size_t>(bytes) - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wide, -1, text.data(), bytes, nullptr, nullptr);
                }
                GlobalUnlock(data);
            }
        }
        CloseClipboard();
    }
}
