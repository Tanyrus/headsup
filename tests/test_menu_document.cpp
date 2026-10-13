#include "menu_document.h"
#include "test.h"

#include <string>

using namespace headsup;

namespace
{
    bool Has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }
}

TEST(the_window_has_its_frame_and_the_parts_its_owner_fills)
{
    const std::string rml = MenuDocument(MenuLook{MenuTheme::Phoenix, "Segoe UI", "Marcellus SC"});
    CHECK(Has(rml, "<handle id=\"head\" move_target=\"#document\">") && Has(rml, "size_target=\"#document\""));
    for (const char* id : {kMenuSidebarId, kMenuPageId, kMenuPageTitleId, kMenuVersionId, kMenuCloseId, kMenuPopupId, kMenuTipId})
        CHECK(Has(rml, (std::string("id=\"") + id + "\"").c_str()));
    // 46 by 36 ems, the page's dark red at 97% and the fonts it was given.
    CHECK(Has(rml, "width:46em;height:36em;min-width:34em;min-height:16em;background-color:#150c0cf7;"));
    CHECK(Has(rml, "font-family:\"Segoe UI\"") && Has(rml, "font-family:\"Marcellus SC\""));
    CHECK(Has(rml, ".seg.picked{background-color:#c55151ff;"));
    CHECK(!Has(rml, "transition") && !Has(rml, "@keyframes"));
}

TEST(aurora_is_slate_and_cyan_with_switches_that_slide)
{
    CHECK(std::string(MenuThemeName(MenuTheme::Aurora)) == "Aurora" && std::string(MenuThemeName(MenuTheme::Phoenix)) == "Phoenix");
    CHECK_EQ(MenuColorsOf(MenuTheme::Aurora).accent, 0xFF5EE6FFu);
    const std::string rml = MenuDocument(MenuLook{MenuTheme::Aurora, "Segoe UI", "Marcellus SC"});
    // A switch: its knob slides and its track takes the accent, eased.
    CHECK(Has(rml, ".mark{display:block;left:0.15em;top:0.15em;width:1em;height:1em;border-radius:0.5em;"));
    CHECK(Has(rml, "transition:left 0.18s cubic-out,background-color 0.18s cubic-out;"));
    CHECK(Has(rml, ".check.on .mark{left:1.45em;background-color:#5ee6ffff;}"));
    // The window and its popup come in with a short animation, and a slider's filled part is a gradient.
    CHECK(Has(rml, "body.open{animation:0.22s cubic-out aurora-window;}"));
    CHECK(Has(rml, "@keyframes aurora-drop{from{opacity:0;transform:translateY(-6px);}to{opacity:1;transform:translateY(0px);}}"));
    CHECK(Has(rml, "input.range sliderprogress{"));
    CHECK(Has(rml, "decorator:horizontal-gradient(#5ee6ff59 #b48cff59);"));
}
