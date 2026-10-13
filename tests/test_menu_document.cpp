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
    // 50 by 36 ems, the page's dark red at 97% and the fonts it was given.
    CHECK(Has(rml, "width:50em;height:36em;min-width:38em;min-height:16em;background-color:#150c0cf7;"));
    CHECK(Has(rml, "font-family:\"Segoe UI\"") && Has(rml, "font-family:\"Marcellus SC\""));
    CHECK(Has(rml, ".seg.picked{background-color:#c55151ff;"));
}

TEST(phoenix_moves_and_lights_up_in_its_own_red_and_keeps_its_checkboxes)
{
    const std::string rml = MenuDocument(MenuLook{MenuTheme::Phoenix, "Segoe UI", "Marcellus SC"});
    // The window and a popup come in with a short animation, and a card's rows fade in.
    CHECK(Has(rml, "body.open{animation:0.22s cubic-out window-in;}"));
    CHECK(Has(rml, "#popup.open{animation:0.16s cubic-out drop-in;}"));
    CHECK(Has(rml, "@keyframes drop-in{from{opacity:0;transform:translateY(-6px);}to{opacity:1;transform:translateY(0px);}}"));
    CHECK(Has(rml, "animation:0.2s cubic-out fade-in;}"));
    // Its light runs from the royal red (C55151) into the ember (E06F5E): the line under the title fading out,
    // a slider's filled part at about a third, the picked choice whole.
    CHECK(Has(rml, "decorator:horizontal-gradient(#c55151b3 #e06f5e00);"));
    CHECK(Has(rml, "input.range sliderprogress{"));
    CHECK(Has(rml, "decorator:horizontal-gradient(#c5515159 #e06f5e59);"));
    CHECK(Has(rml, ".seg.picked,.tab.picked,.btn.accent{decorator:horizontal-gradient(#c55151ff #e06f5eff);}"));
    // A yes or no is still a square box, 1.6 em, and its mark grows from the middle.
    CHECK(Has(rml, ".check{position:relative;width:1.6em;height:1.6em;"));
    CHECK(!Has(rml, ".check{width:2.6em;"));
    CHECK(Has(rml, ".check.on .mark{left:0.4em;top:0.4em;width:0.8em;height:0.8em;}"));
    // A slider's knob is a bright bar as tall as its track, in both themes.
    CHECK(Has(rml, "input.range sliderbar{width:0.35em;height:1.6em;margin-top:0em;border-radius:0.17em;background-color:#fff4f0ff;}"));
    // The glass is Aurora's alone.
    CHECK(!Has(rml, "vertical-gradient"));
}

TEST(aurora_is_slate_and_cyan_with_switches_that_slide)
{
    CHECK(std::string(MenuThemeName(MenuTheme::Aurora)) == "Aurora" && std::string(MenuThemeName(MenuTheme::Phoenix)) == "Phoenix");
    CHECK_EQ(MenuColorsOf(MenuTheme::Aurora).accent, 0xFF5EE6FFu);
    const std::string rml = MenuDocument(MenuLook{MenuTheme::Aurora, "Segoe UI", "Marcellus SC"});
    // A switch: its knob slides and its track takes the accent, eased.
    CHECK(Has(rml, ".mark{left:0.15em;top:0.15em;width:1em;height:1em;border-radius:0.5em;"));
    CHECK(Has(rml, "transition:left 0.18s cubic-out,background-color 0.18s cubic-out;"));
    CHECK(Has(rml, ".check.on .mark{left:1.45em;top:0.15em;width:1em;height:1em;background-color:#5ee6ffff;}"));
    // The window and its popup come in with a short animation, and a slider's filled part is a gradient.
    CHECK(Has(rml, "body.open{animation:0.22s cubic-out window-in;}"));
    CHECK(Has(rml, "@keyframes drop-in{from{opacity:0;transform:translateY(-6px);}to{opacity:1;transform:translateY(0px);}}"));
    CHECK(Has(rml, "input.range sliderprogress{"));
    CHECK(Has(rml, "decorator:horizontal-gradient(#5ee6ff59 #b48cff59);"));
    CHECK(Has(rml, "input.range sliderbar{width:0.35em;height:1.6em;margin-top:0em;border-radius:0.17em;background-color:#eaf2ffff;}"));
    // Its own: glass down the window, and dark text on the bright picked choice.
    CHECK(Has(rml, "body{decorator:vertical-gradient(#111b2cf2 #090e17f2);"));
    CHECK(Has(rml, ".seg.picked,.tab.picked,.btn.accent{color:#080c13ff;}"));
}
