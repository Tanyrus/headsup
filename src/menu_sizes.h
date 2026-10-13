#pragma once

namespace headsup
{
    // Every size in ems, the font size the menu draws its text at, so it holds at any UI scale.
    constexpr float kPercent        = 100.0f;
    constexpr float kTitleScale     = 1.55f; // the window's name
    constexpr float kPageTitleScale = 1.4f;
    constexpr float kCardTitleScale = 1.12f;
    constexpr float kWindowWidth    = 46.0f;
    constexpr float kWindowHeight   = 36.0f;
    constexpr float kMinWindowWidth  = 34.0f;
    constexpr float kMinWindowHeight = 16.0f;
    constexpr float kSidebarWidth   = 8.5f;
    constexpr float kGutter         = 1.0f;  // around the window's parts and between cards
    constexpr float kCardPad        = 0.9f;  // a card's content from its edges
    constexpr float kCardRadius     = 0.6f;
    constexpr float kRadius         = 0.4f;  // controls
    constexpr float kSwitchWidth    = 2.3f;
    constexpr float kSegmentWidth   = 4.6f;
    constexpr float kControlWidth   = 12.0f; // sliders and dropdowns
    constexpr float kCutoffWidth    = 5.2f;  // a status badge's cutoff dropdowns
    // A mode's dropdown, so "When Targeted" shows whole: 13 letters, the frame's padding and its arrow.
    constexpr float kModeWidth      = 9.4f;
    constexpr float kNumberWidth    = 4.2f;  // a number to type
    constexpr float kNumberName     = 1.1f;  // the X or Y before it
    constexpr float kFilterWidth    = 12.0f;
    constexpr float kGroupNameWidth = 9.0f;
    constexpr float kWideSegmentWidth = 5.4f; // a choice as long as "Diamonds"
    constexpr float kChipHeight     = 1.9f;
    constexpr float kChipDot        = 0.5f;
    constexpr float kTabHeight      = 2.0f;
    constexpr float kTabRule        = 0.12f; // the line a strip of tabs stands on
    constexpr float kTabGap         = 0.12f; // between two tabs, and two rows of them: a couple of pixels
    constexpr float kTipWidth       = 20.0f;
    constexpr float kOffAlpha       = 0.42f; // the settings of a feature that is off are drawn this faint
    constexpr float kHeaderRow      = 2.4f;  // a card's title row
}
