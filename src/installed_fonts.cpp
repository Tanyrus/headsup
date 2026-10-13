#include "installed_fonts.h"

#include "settings.h"

#include <utility>
#include <windows.h>

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

        DWORD WINAPI ListInstalledFonts(LPVOID into)
        {
            *static_cast<std::vector<std::string>*>(into) = InstalledFontFamilies();
            return 0;
        }
    }

    std::string WindowsFontPath(const char* file)
    {
        char windows[MAX_PATH] = {};
        const UINT length      = GetWindowsDirectoryA(windows, MAX_PATH);
        return std::string(windows, length < MAX_PATH ? length : 0) + "\\Fonts\\" + file;
    }

    bool MenuFontInstalled(const MenuFont& font)
    {
        return font.file == nullptr || GetFileAttributesA(WindowsFontPath(font.file).c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    InstalledFonts::~InstalledFonts() { Wait(); }

    void InstalledFonts::Wait()
    {
        if (m_Thread == nullptr) return;
        WaitForSingleObject(m_Thread, INFINITE);
        CloseHandle(m_Thread);
        m_Thread = nullptr;
    }

    const std::vector<std::string>& InstalledFonts::Choices(const std::string& current)
    {
        if (!m_Asked)
        {
            m_Asked  = true;
            m_Thread = CreateThread(nullptr, 0, ListInstalledFonts, &m_Installed, 0, nullptr);
            if (m_Thread == nullptr) m_Choices = FontChoices(InstalledFontFamilies(), current);
        }
        if (m_Thread != nullptr && WaitForSingleObject(m_Thread, 0) == WAIT_OBJECT_0)
        {
            Wait();
            m_Choices = FontChoices(std::move(m_Installed), current);
        }
        if (m_Choices.empty()) m_Choices = FontChoices({}, current);
        return m_Choices;
    }
}
