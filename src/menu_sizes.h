#pragma once

namespace headsup
{
    // Every size in ems, the font size the menu draws its text at, so it holds at any UI scale.
    constexpr float kTitleScale     = 1.55f; // the window's name
    constexpr float kPageTitleScale = 1.4f;
    constexpr float kCardTitleScale = 1.12f;
    constexpr float kWindowWidth    = 50.0f;
    constexpr float kWindowHeight   = 36.0f;
    constexpr float kMinWindowWidth  = 42.0f; // a row of two mode dropdowns and a few letters of its label
    constexpr float kMinWindowHeight = 16.0f;
    constexpr float kGutter         = 1.0f;  // around the window's parts and between cards
    constexpr float kCardPad        = 0.9f;  // a card's content from its edges
    constexpr float kCardRadius     = 0.6f;
    constexpr float kRadius         = 0.4f;  // controls
    constexpr float kToggleHeight   = 1.4f;
    constexpr float kToggleInset    = 0.2f;  // a toggle's peg from its track's edge
    constexpr float kTogglePeg      = 2.0f;  // the peg's width: a rectangle that slides from end to end
    constexpr float kToggleWidth    = 2.0f * kTogglePeg + 2.0f * kToggleInset; // a yes or no in a table: room to slide
    constexpr float kCardToggleWidth = 7.0f; // a card title's: OFF clears its peg in the widest font
    constexpr float kControlWidth   = 15.0f; // sliders and dropdowns
    constexpr float kFrame          = 1.6f;  // a control's height: a line of text and its padding above and below
    constexpr float kTallButton     = 2.0f;  // a page's own wide button, a little taller than a control
    constexpr float kControlGap     = 0.5f;  // between a row's controls
    constexpr float kPageTop        = 2.4f;  // where a page's cards start under its title
    constexpr float kCutoffWidth    = 5.2f;  // a status badge's cutoff dropdowns
    // A mode's dropdown, so "Vanilla Nameplate" shows whole: 17 letters, the frame's padding and its arrow.
    constexpr float kModeWidth      = 11.2f;
    constexpr float kNumberWidth    = 4.2f;  // a number to type
    constexpr float kNumberName     = 1.1f;  // the X or Y before it
    constexpr float kFilterWidth    = 12.0f;
    constexpr float kGroupNameWidth = 9.0f;
    constexpr float kChipHeight     = 1.9f;
    constexpr float kChipDot        = 0.5f;
    constexpr float kTabHeight      = 2.0f;
    constexpr float kTabRule        = 0.12f; // the line a strip of tabs stands on
    constexpr float kTabGap         = 0.12f; // between two tabs, and two rows of them: a couple of pixels
    constexpr float kTabPadding     = 0.6f;  // either side of a name in tabs as wide as their names
    constexpr int kTabFillGrow      = 1000;  // the last row's filler grows this much faster than its tabs
    constexpr float kTipWidth       = 20.0f;
    constexpr float kHeaderRow      = 2.4f;  // a card's title row
    constexpr float kLineNameWidth  = 6.6f;  // "Middle Line", before its design box
    constexpr int kResetArrowPixels = 48;    // the reset arrow's picture, drawn smaller than it is
}
