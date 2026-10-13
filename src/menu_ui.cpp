#include "menu_ui.h"

#include "menu.h"

namespace headsup
{
    std::string TitleCase(const char* begin, const char* end)
    {
        return TitleCaseText(std::string_view(begin, static_cast<size_t>(end - begin)));
    }
    std::string TitleCase(const char* text) { return TitleCase(text, text + std::strlen(text)); }

    void ApplyPhoenixStyle(ImGuiStyle& s, float em)
    {
        ImVec4* c                        = s.Colors;
        c[ImGuiCol_Text]                 = kText;
        c[ImGuiCol_TextDisabled]         = kMuted;
        c[ImGuiCol_TextSelectedBg]       = kAccentSoft;
        c[ImGuiCol_InputTextCursor]      = kText;
        c[ImGuiCol_WindowBg]             = kBackground;
        c[ImGuiCol_ChildBg]              = kClear;
        c[ImGuiCol_PopupBg]              = kCard;
        c[ImGuiCol_Border]               = kBorder;
        c[ImGuiCol_FrameBg]              = kControl;
        c[ImGuiCol_FrameBgHovered]       = kHover;
        c[ImGuiCol_FrameBgActive]        = kHover;
        c[ImGuiCol_CheckMark]            = kAccent;
        c[ImGuiCol_SliderGrab]           = kAccent;
        c[ImGuiCol_SliderGrabActive]     = kAccentHover;
        c[ImGuiCol_Button]               = kControl;
        c[ImGuiCol_ButtonHovered]        = kHover;
        c[ImGuiCol_ButtonActive]         = kAccentSoft;
        c[ImGuiCol_Header]               = kAccentSoft;
        c[ImGuiCol_HeaderHovered]        = kAccentSoft;
        c[ImGuiCol_HeaderActive]         = kAccent;
        c[ImGuiCol_Separator]            = kBorder;
        c[ImGuiCol_ScrollbarBg]          = kClear;
        c[ImGuiCol_ScrollbarGrab]        = kControl;
        c[ImGuiCol_ScrollbarGrabHovered] = kHover;
        c[ImGuiCol_ScrollbarGrabActive]  = kAccent;
        c[ImGuiCol_TableBorderLight]     = kClear;
        c[ImGuiCol_TableBorderStrong]    = kClear;
        c[ImGuiCol_DragDropTarget]       = kAccent;
        s.WindowRounding    = kCardRadius * em * 1.5f;
        s.ChildRounding     = kCardRadius * em;
        s.FrameRounding     = kRadius * em;
        s.PopupRounding     = kRadius * em;
        s.GrabRounding      = em;
        s.ScrollbarRounding = em;
        s.TabRounding       = kRadius * em;
        s.WindowBorderSize  = 1.0f;
        s.FrameBorderSize   = 0.0f;
        s.PopupBorderSize   = 1.0f;
        s.WindowPadding     = ImVec2(kGutter * em, kGutter * em);
        s.FramePadding      = ImVec2(0.6f * em, 0.3f * em);
        s.ItemSpacing       = ImVec2(0.5f * em, 0.45f * em);
        s.ItemInnerSpacing  = ImVec2(0.4f * em, 0.3f * em);
        s.CellPadding       = ImVec2(0.0f, 0.0f);
        s.GrabMinSize       = 0.9f * em;
        s.ScrollbarSize     = 0.7f * em;
        s.ButtonTextAlign   = ImVec2(0.5f, 0.5f);
        s.DisabledAlpha     = 1.0f; // decoration drawn disabled keeps its color
    }
}
