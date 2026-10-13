#include "menu_document.h"

#include "markup.h"
#include "menu_sizes.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        constexpr float kFrame        = 1.6f;  // a control's height: a line of text and its padding above and below
        constexpr float kSpacing      = 0.45f; // between rows
        constexpr float kControlGap   = 0.5f;  // between a row's controls
        constexpr float kPadX         = 0.6f;  // a control's text from its sides
        constexpr float kHeadHeight   = 2.05f; // the window's title row
        constexpr float kScrollbar    = 0.7f;
        constexpr float kPopupTall    = 18.0f; // the most of a dropdown's list shown before it scrolls
        constexpr float kNavHeight    = 2.0f;
        constexpr float kMark         = 0.8f;  // a checked box's mark
        constexpr float kArrow        = 0.35f; // half an arrow's base
    }

    const char* MenuThemeName(MenuTheme theme)
    {
        static const char* const kNames[kMenuThemeCount] = {"Phoenix", "Aurora"};
        return kNames[std::clamp(static_cast<int>(theme), 0, kMenuThemeCount - 1)];
    }

    namespace
    {
        constexpr uint32_t kAuroraViolet = 0xFFB48CFF; // what Aurora's cyan runs into

        uint32_t Faded(uint32_t argb, uint32_t alpha) { return (argb & 0x00FFFFFF) | alpha << 24; }

        // What Aurora adds: glass in place of flat fills, gradients from cyan to violet, and every change of state
        // eased: switches whose knobs slide, pages that slide under the pointer, a window and popups that drop in.
        std::string AuroraMenuRules(const MenuColors& c)
        {
            const uint32_t cyan = c.accent, violet = kAuroraViolet;
            const std::string a = RcssColor(cyan), v = RcssColor(violet), dark = RcssColor(c.inset | 0xFF000000);
            const std::string wash = "horizontal-gradient(" + RcssColor(Faded(cyan, 0x59)) + " " + RcssColor(Faded(violet, 0x59)) + ")";
            const std::string bright = "horizontal-gradient(" + a + " " + v + ")";
            const char* ease = " 0.16s cubic-out";
            std::string css;
            css += "body{decorator:vertical-gradient(#111b2cf2 #090e17f2);background-color:transparent;border-color:" +
                   RcssColor(Faded(cyan, 0x40)) + ";}";
            css += "body.open{animation:0.22s cubic-out aurora-window;}";
            css += "@keyframes aurora-window{from{opacity:0;transform:scale(0.97);}to{opacity:1;transform:scale(1);}}";
            css += "#title{font-effect:glow(0px 4px 0px 0px " + RcssColor(Faded(cyan, 0x66)) + ");}";
            css += "#head-line{display:block;position:absolute;left:1em;right:1em;top:3.25em;height:1px;decorator:"
                   "horizontal-gradient(" + RcssColor(Faded(cyan, 0xB3)) + " " + RcssColor(Faded(violet, 0)) + ");}";
            // The pages: a bar grows beside the picked one, and each slides a little under the pointer.
            css += std::string(".nav{transition:background-color") + ease + ",color" + ease + ",padding-left" + ease + ";}";
            css += ".nav:hover{padding-left:1.35em;}";
            css += ".nav.picked{background-color:transparent;decorator:horizontal-gradient(" + RcssColor(Faded(cyan, 0x47)) +
                   " " + RcssColor(Faded(violet, 0x14)) + ");}";
            css += ".nav-mark{display:block;left:0.35em;top:1em;width:0.2em;height:0em;border-radius:0.1em;"
                   "transition:height 0.2s back-out,top 0.2s back-out;}";
            css += ".nav.picked .nav-mark{top:0.45em;height:1.1em;}";
            // Cards of glass; a card's arrow turns into place and its rows fade in when it opens.
            css += ".card{background-color:transparent;decorator:vertical-gradient(#1c283cb8 #131b2ab8);"
                   "transition:border-color 0.2s cubic-out;}";
            css += ".card:hover{border-color:" + RcssColor(Faded(cyan, 0x59)) + ";}";
            css += std::string(".card-title{transition:color") + ease + ";}";
            css += ".card-head:hover .card-title{color:" + a + ";}";
            css += ".arrow.open{animation:0.18s cubic-out aurora-turn;}";
            css += "@keyframes aurora-turn{from{transform:rotate(-90deg);}to{transform:rotate(0deg);}}";
            css += ".card-body{animation:0.2s cubic-out aurora-fade;}";
            css += "@keyframes aurora-fade{from{opacity:0;}to{opacity:1;}}";
            // A switch: its knob slides and its track takes the accent.
            css += ".check{width:2.6em;height:1.3em;border-radius:0.65em;transition:background-color 0.18s cubic-out;}";
            css += ".check.on{background-color:" + RcssColor(Faded(cyan, 0x59)) + ";}";
            css += ".mark{display:block;left:0.15em;top:0.15em;width:1em;height:1em;border-radius:0.5em;background-color:" +
                   RcssColor(c.muted) + ";transition:left 0.18s cubic-out,background-color 0.18s cubic-out;}";
            css += ".check.on .mark{left:1.45em;background-color:" + a + ";}";
            css += ".check.card-on{top:0.55em;}.switch-room{width:2.6em;}";
            // Choices, tabs and buttons ease between their looks; the picked one is the gradient.
            css += std::string(".seg,.tab,.btn,.dd,.opt,.wchip{transition:background-color") + ease + ",border-color" + ease +
                   ",color" + ease + ";}";
            css += ".seg.picked,.tab.picked,.btn.accent{decorator:" + bright + ";color:" + dark + ";}";
            css += ".tab-rule{background-color:transparent;decorator:horizontal-gradient(" + a + " " +
                   RcssColor(Faded(violet, 0)) + ");}";
            css += ".wchip.picked{border-color:" + a + ";}";
            // A slider's filled part is a wash of the gradient, and its knob a bright bar.
            css += "input.range sliderprogress{height:1.6em;border-radius:0.4em;decorator:" + wash + ";}";
            css += "input.range sliderbar{width:0.35em;height:1.6em;margin-top:0em;border-radius:0.175em;background-color:" +
                   RcssColor(c.text) + ";}";
            css += std::string("input.text{border:1px transparent;transition:border-color") + ease + ";}";
            css += "input.text:focus{border-color:" + RcssColor(Faded(cyan, 0x99)) + ";}";
            css += "scrollbarvertical sliderbar{background-color:" + RcssColor(Faded(cyan, 0x40)) + ";}";
            // The popup drops in under its control.
            css += std::string("#") + kMenuPopupId + ".open{animation:0.16s cubic-out aurora-drop;}";
            css += "@keyframes aurora-drop{from{opacity:0;transform:translateY(-6px);}to{opacity:1;transform:translateY(0px);}}";
            css += std::string("#") + kMenuPopupId + ",#" + kMenuTipId + "{border-color:" + RcssColor(Faded(cyan, 0x66)) + ";}";
            return css;
        }
    }

    const MenuColors& MenuColorsOf(MenuTheme theme)
    {
        static const MenuColors kColors[kMenuThemeCount] = {kPhoenixMenuColors, kAuroraMenuColors};
        return kColors[std::clamp(static_cast<int>(theme), 0, kMenuThemeCount - 1)];
    }

    std::string MenuDocument(const MenuLook& look)
    {
        const MenuColors& c = MenuColorsOf(look.theme);
        const auto color    = [](uint32_t argb) { return RcssColor(argb); };
        const std::string frame = Em(kFrame), radius = Em(kRadius), control = Em(kControlWidth);
        const float top    = kGutter + kHeadHeight + 0.5f; // where the sidebar and the page start
        const float mainX  = kGutter + kSidebarWidth + kGutter;
        std::string css;
        css += "body{position:absolute;display:block;box-sizing:border-box;width:" + Em(kWindowWidth) + ";height:" +
               Em(kWindowHeight) + ";min-width:" + Em(kMinWindowWidth) + ";min-height:" + Em(kMinWindowHeight) +
               ";background-color:" + color(c.background) + ";border:1px " + color(c.border) + ";border-radius:" +
               Em(kCardRadius * 1.5f) + ";color:" + color(c.text) + ";font-family:\"" + look.textFont + "\";z-index:5;}";
        css += "div{display:block;}handle{display:block;}";
        css += ".hidden{display:none;}";
        css += ".off{opacity:" + RcssNumber(kOffAlpha) + ";pointer-events:none;}.label.off,.card-head.off{pointer-events:auto;}";
        // The frame: the title bar moves the window, the corner resizes it.
        css += "#head{position:absolute;left:" + Em(kGutter) + ";right:" + Em(kGutter + kFrame + 0.5f) + ";top:" +
               Em(kGutter) + ";height:" + Em(kHeadHeight) + ";}";
        css += "#title{display:inline-block;font-family:\"" + look.titleFont + "\";font-size:" + Em(kTitleScale) +
               ";color:" + color(c.heading) + ";}";
        css += std::string("#") + kMenuVersionId + "{display:inline-block;margin-left:0.5em;color:" + color(c.muted) + ";}";
        css += std::string("#") + kMenuCloseId + "{position:absolute;right:" + Em(kGutter) + ";top:" + Em(kGutter) +
               ";width:" + frame + ";height:" + frame + ";line-height:" + frame + ";text-align:center;border-radius:" +
               Em(kFrame / 2.0f) + ";background-color:" + color(c.control) + ";color:" + color(c.muted) + ";}";
        css += std::string("#") + kMenuCloseId + ":hover{background-color:" + color(c.hover) + ";}";
        css += "#head-line{display:none;}";
        css += "#resize{position:absolute;right:0px;bottom:0px;width:1.2em;height:1.2em;}";
        css += "#resize-mark{position:absolute;right:0.3em;bottom:0.3em;width:0px;height:0px;border-left:0.5em transparent;"
               "border-bottom:0.5em " + color(c.raisedEdge) + ";}";
        // The pages, as pills.
        css += std::string("#") + kMenuSidebarId + "{position:absolute;left:" + Em(kGutter) + ";top:" + Em(top) +
               ";width:" + Em(kSidebarWidth) + ";bottom:" + Em(kGutter) + ";}";
        css += ".nav{position:relative;height:" + Em(kNavHeight) + ";line-height:" + Em(kNavHeight) + ";padding-left:" +
               Em(kGutter) + ";border-radius:" + Em(kNavHeight / 2.0f) + ";margin-bottom:" + Em(kSpacing) + ";color:" +
               color(c.label) + ";}";
        css += ".nav.child{margin-left:" + Em(kGutter) + ";}";
        css += ".nav:hover{background-color:" + color(c.accentSoft) + ";}";
        css += ".nav.picked{background-color:" + color(c.accentSoft) + ";color:" + color(c.text) + ";}";
        css += ".nav-mark{display:none;position:absolute;left:0.4em;top:0.825em;width:0.35em;height:0.35em;"
               "border-radius:0.175em;background-color:" + color(c.accent) + ";}";
        css += ".nav.picked .nav-mark{display:block;}";
        // The page: its title stays put and its cards scroll.
        css += "#main{position:absolute;left:" + Em(mainX) + ";right:" + Em(kGutter) + ";top:" + Em(top) + ";bottom:" +
               Em(kGutter) + ";}";
        css += std::string("#") + kMenuPageTitleId + "{font-family:\"" + look.titleFont + "\";font-size:" +
               Em(kPageTitleScale) + ";color:" + color(c.heading) + ";}";
        css += std::string("#") + kMenuPageId + "{position:absolute;left:0px;right:0px;top:2.4em;bottom:0px;overflow-y:auto;}";
        css += "scrollbarvertical{width:" + Em(kScrollbar) + ";}";
        css += "scrollbarvertical sliderbar{width:" + Em(kScrollbar) + ";min-height:2em;border-radius:" +
               Em(kScrollbar / 2.0f) + ";background-color:" + color(c.control) + ";}";
        css += "scrollbarvertical sliderbar:hover{background-color:" + color(c.hover) + ";}";
        css += "scrollbarvertical sliderbar:active{background-color:" + color(c.accent) + ";}";
        css += "scrollbarvertical sliderarrowdec,scrollbarvertical sliderarrowinc{width:0px;height:0px;}";
        // Cards.
        css += std::string(".") + kMenuCardClass + "{background-color:" + color(c.card) + ";border:1px " + color(c.border) +
               ";border-radius:" + Em(kCardRadius) + ";margin-bottom:" + Em(kGutter * 0.7f) + ";margin-right:" +
               Em(kScrollbar + 0.3f) + ";}";
        css += ".card-head{position:relative;height:" + Em(kHeaderRow) + ";}";
        css += ".arrow{position:absolute;left:0.95em;top:" + Em(kHeaderRow / 2.0f - kArrow) + ";width:0px;height:0px;"
               "border-top:" + Em(kArrow) + " transparent;border-bottom:" + Em(kArrow) + " transparent;border-left:0.5em " +
               color(c.muted) + ";}";
        css += ".arrow.open{left:0.85em;top:" + Em(kHeaderRow / 2.0f - 0.2f) + ";border-top:0.5em " + color(c.muted) +
               ";border-bottom-width:0px;border-left:" + Em(kArrow) + " transparent;border-right:" + Em(kArrow) +
               " transparent;}";
        css += ".card-head:hover .arrow{border-left-color:" + color(c.text) + ";}";
        css += ".card-head:hover .arrow.open{border-left-color:transparent;border-top-color:" + color(c.text) + ";}";
        css += ".card-title{position:absolute;left:2.1em;top:0px;line-height:" + Em(kHeaderRow / kCardTitleScale) +
               ";font-family:\"" + look.titleFont + "\";font-size:" + Em(kCardTitleScale) + ";color:" + color(c.heading) + ";}";
        css += ".check.card-on{position:absolute;right:" + Em(kCardPad) + ";top:" + Em((kHeaderRow - kFrame) / 2.0f) + ";}";
        css += ".card-body{padding:0px " + Em(kCardPad) + " " + Em(kCardPad * 0.6f) + " " + Em(kCardPad) + ";}";
        // Rows: the label at the left, its controls at the right.
        css += ".row{display:flex;flex-direction:row;flex-wrap:wrap;align-items:center;justify-content:space-between;"
               "min-height:" + frame + ";margin-bottom:" + Em(kSpacing) + ";}";
        css += ".label{flex:1 1 auto;line-height:" + frame + ";color:" + color(c.label) + ";}";
        css += ".ctl{position:relative;display:flex;flex-direction:row;align-items:center;justify-content:flex-end;flex:0 0 auto;}";
        css += ".ctl > *{margin-left:" + Em(kControlGap) + ";}";
        css += ".line{display:flex;flex-direction:row;flex-wrap:wrap;align-items:center;margin-bottom:" + Em(kSpacing) + ";}";
        css += ".line > *{margin-right:" + Em(kControlGap) + ";}";
        css += ".line.chips > *{margin-bottom:0.3em;}";
        css += ".line.indent{margin-left:" + Em(kGutter) + ";}";
        css += ".gap{flex:1 1 auto;}";
        css += ".note{color:" + color(c.muted) + ";margin-bottom:" + Em(kSpacing) + ";}";
        css += ".caption{color:" + color(c.muted) + ";line-height:" + frame + ";}";
        css += ".line-name{color:" + color(c.label) + ";margin-bottom:0.15em;}";
        css += ".reading{width:" + control + ";line-height:" + frame + ";}";
        css += ".switch-room{display:flex;flex-direction:row;justify-content:flex-end;width:" + Em(kSwitchWidth) + ";}";
        // A checkbox, its mark shown while it is on.
        css += ".check{position:relative;width:" + frame + ";height:" + frame + ";border-radius:" + radius +
               ";background-color:" + color(c.control) + ";}";
        css += ".check:hover{background-color:" + color(c.hover) + ";}";
        css += ".mark{display:none;position:absolute;left:" + Em((kFrame - kMark) / 2.0f) + ";top:" +
               Em((kFrame - kMark) / 2.0f) + ";width:" + Em(kMark) + ";height:" + Em(kMark) + ";border-radius:0.2em;"
               "background-color:" + color(c.accent) + ";}";
        css += ".check.on .mark{display:block;}";
        // Joined choices.
        css += ".segs{display:flex;flex-direction:row;}";
        css += ".seg{height:" + frame + ";line-height:" + frame + ";text-align:center;background-color:" + color(c.control) +
               ";color:" + color(c.muted) + ";}";
        css += ".seg:hover{background-color:" + color(c.hover) + ";}";
        css += ".seg.picked{background-color:" + color(c.accent) + ";color:" + color(c.text) + ";}";
        // A slider, its value written over it.
        css += ".slider{position:relative;width:" + control + ";height:" + frame + ";}";
        css += "input.range{display:block;width:" + control + ";height:" + frame + ";}";
        css += "input.range slidertrack{height:" + frame + ";border-radius:" + radius + ";background-color:" +
               color(c.control) + ";}";
        css += "input.range:hover slidertrack{background-color:" + color(c.hover) + ";}";
        css += "input.range sliderbar{width:0.9em;height:1.2em;margin-top:0.2em;border-radius:0.45em;background-color:" +
               color(c.accent) + ";}";
        css += "input.range sliderbar:hover,input.range sliderbar:active{background-color:" + color(c.accentHover) + ";}";
        css += "input.range sliderarrowdec,input.range sliderarrowinc{width:0px;height:0px;}";
        css += ".val{position:absolute;left:0px;top:0px;width:100%;line-height:" + frame + ";text-align:center;"
               "pointer-events:none;}";
        // Text boxes.
        css += "input.text{box-sizing:border-box;height:" + frame + ";padding:0.2em " + Em(kPadX) + ";border-radius:" +
               radius + ";background-color:" + color(c.control) + ";color:" + color(c.text) + ";}";
        css += "input.text:hover,input.text:focus{background-color:" + color(c.hover) + ";}";
        css += "input.text:placeholder-shown{color:" + color(c.muted) + ";}";
        css += "input.text selection{background-color:" + color(c.accentSoft) + ";}";
        css += "input.num{width:" + Em(kNumberWidth) + ";}";
        css += ".num-name{width:" + Em(kNumberName) + ";text-align:right;color:" + color(c.muted) + ";line-height:" + frame + ";}";
        css += "input.linebox{display:block;width:100%;height:" + Em(kFrame + 1.2f) + ";padding-top:0.8em;margin-bottom:" +
               Em(kSpacing) + ";}";
        css += "input.hex{width:6em;}";
        // A dropdown's closed control; its list is the popup.
        css += ".dd{position:relative;box-sizing:border-box;height:" + frame + ";line-height:" + frame + ";padding:0px " +
               Em(kFrame) + " 0px " + Em(kPadX) + ";border-radius:" + radius + ";background-color:" + color(c.control) +
               ";overflow:hidden;white-space:nowrap;}";
        css += ".dd:hover,.dd.open{background-color:" + color(c.hover) + ";}";
        css += ".dd-val{overflow:hidden;pointer-events:none;}";
        css += ".dd-arrow{position:absolute;right:0.5em;top:0.6em;width:0px;height:0px;border-left:" + Em(kArrow) +
               " transparent;border-right:" + Em(kArrow) + " transparent;border-top:0.45em " + color(c.text) + ";}";
        css += std::string("#") + kMenuPopupId + "{position:absolute;z-index:50;box-sizing:border-box;padding:0.2em;"
               "background-color:" + color(c.card | 0xFF000000) + ";border:1px " + color(c.raisedEdge) + ";border-radius:" +
               radius + ";}";
        css += ".list{max-height:" + Em(kPopupTall) + ";overflow-y:auto;}";
        css += ".opt{height:" + frame + ";line-height:" + frame + ";padding:0px " + Em(kPadX) + ";border-radius:" + radius +
               ";white-space:nowrap;}";
        css += ".opt:hover,.opt.picked{background-color:" + color(c.accentSoft) + ";}";
        css += ".opt.locked{opacity:" + RcssNumber(kOffAlpha) + ";pointer-events:none;}";
        css += ".dot{display:inline-block;width:" + Em(kChipDot) + ";height:" + Em(kChipDot) + ";border-radius:" +
               Em(kChipDot / 2.0f) + ";margin-right:0.5em;background-color:" + color(c.muted) + ";}";
        css += ".on .dot{background-color:" + color(c.accent) + ";}";
        // A color: its swatch, and the sliders its popup holds.
        css += ".swatch{box-sizing:border-box;width:" + frame + ";height:" + frame + ";border-radius:" + radius +
               ";border:1px " + color(c.raisedEdge) + ";}";
        css += ".swatch.none{border-color:transparent;}";
        css += ".picker{padding:0.4em;}";
        css += ".pick-row{position:relative;display:flex;flex-direction:row;align-items:center;margin-bottom:0.3em;}";
        css += ".pick-name{width:1.2em;color:" + color(c.label) + ";}";
        css += ".pick-row input.range{width:9em;}.pick-row .val{position:relative;width:2.6em;text-align:right;}";
        // Buttons and chips.
        css += ".btn{box-sizing:border-box;height:" + frame + ";line-height:" + Em(kFrame - 0.15f) + ";padding:0px " + Em(kPadX + 0.2f) +
               ";border-radius:" + Em(kFrame / 2.0f) + ";border:1px transparent;text-align:center;white-space:nowrap;}";
        css += ".btn.raised{background-color:" + color(c.control) + ";border-color:" + color(c.raisedEdge) + ";}";
        css += ".btn.raised:hover{background-color:" + color(c.hover) + ";}";
        css += ".btn.flat{border-color:" + color(c.raisedEdge) + ";color:" + color(c.label) + ";}";
        css += ".btn.flat:hover{background-color:" + color(c.accentSoft) + ";}";
        css += ".btn.accent{background-color:" + color(c.accent) + ";}";
        css += ".btn.accent:hover{background-color:" + color(c.accentHover) + ";}";
        css += ".btn.waiting{border-color:" + color(c.accent) + ";color:" + color(c.muted) + ";}";
        css += ".btn.round{width:" + frame + ";padding:0px;color:" + color(c.muted) + ";}";
        css += ".btn.step{position:relative;width:" + frame + ";padding:0px;border-radius:" + radius + ";background-color:" +
               color(c.control) + ";}";
        css += ".btn.step:hover{background-color:" + color(c.hover) + ";}";
        css += ".tri{position:absolute;top:" + Em(kFrame / 2.0f - kArrow) + ";width:0px;height:0px;border-top:" + Em(kArrow) +
               " transparent;border-bottom:" + Em(kArrow) + " transparent;}";
        css += ".tri.left{left:0.5em;border-right:0.45em " + color(c.text) + ";}";
        css += ".tri.right{left:0.6em;border-left:0.45em " + color(c.text) + ";}";
        css += ".drag{drag:clone;}";
        // Tabs: equal across each row, the open one filled, standing on a rule.
        css += ".tab-row{display:flex;flex-direction:row;margin-bottom:" + Em(kTabGap) + ";}";
        css += ".tab{flex:1 1 0px;height:" + Em(kTabHeight) + ";line-height:" + Em(kTabHeight) + ";margin-right:" +
               Em(kTabGap) + ";text-align:center;white-space:nowrap;border-radius:" + radius + " " + radius +
               " 0px 0px;background-color:" + color(c.control) + ";color:" + color(c.muted) + ";}";
        css += ".tab:hover{background-color:" + color(c.hover) + ";}";
        css += ".tab.picked{background-color:" + color(c.accent) + ";color:" + color(c.text) + ";}";
        css += ".tab-rule{height:" + Em(kTabRule) + ";margin-top:-" + Em(kTabGap) + ";margin-bottom:" + Em(kSpacing) +
               ";background-color:" + color(c.accent) + ";}";
        // The widgets' trays.
        css += ".line.trays{align-items:stretch;flex-wrap:nowrap;}";
        css += ".line.trays > *{margin-right:0.6em;}";
        css += ".tray{flex:1 1 0px;box-sizing:border-box;padding:0.4em 0.45em;border-radius:" + Em(kCardRadius) +
               ";border:1px " + color(c.border) + ";background-color:" + color(c.inset) + ";}";
        css += ".tray-name{height:1.4em;padding-left:0.3em;color:" + color(c.muted) + ";}";
        css += ".wchip{display:block;height:" + Em(kChipHeight) + ";line-height:" + Em(kChipHeight - 0.15f) +
               ";padding:0px 0.7em;margin-bottom:0.3em;border-radius:" + Em(kChipHeight / 2.0f) + ";text-align:left;color:" +
               color(c.muted) + ";}";
        css += ".wchip.on{color:" + color(c.text) + ";}";
        css += ".wchip.picked{border-color:" + color(c.accent) + ";}";
        css += ".tray-end{height:" + Em(kChipHeight) + ";}";
        // What a control says under the pointer.
        css += std::string("#") + kMenuTipId + "{position:absolute;z-index:60;box-sizing:border-box;max-width:" +
               Em(kTipWidth) + ";padding:0.4em 0.6em;border-radius:" + radius + ";border:1px " + color(c.raisedEdge) +
               ";background-color:" + color(c.card | 0xFF000000) + ";pointer-events:none;}";

        if (look.theme == MenuTheme::Aurora) css += AuroraMenuRules(c);

        return "<rml><head><style>" + css + "</style></head><body>"
               "<handle id=\"head\" move_target=\"#document\"><div id=\"title\">HeadsUp</div><div id=\"" +
               kMenuVersionId + "\"></div></handle><div id=\"head-line\"></div><div id=\"" + kMenuCloseId + "\">x</div><div id=\"" + kMenuSidebarId +
               "\"></div><div id=\"main\"><div id=\"" + kMenuPageTitleId + "\"></div><div id=\"" + kMenuPageId +
               "\"></div></div><div id=\"" + kMenuPopupId + "\" class=\"hidden\"></div><div id=\"" + kMenuTipId +
               "\" class=\"hidden\"></div><handle id=\"resize\" size_target=\"#document\"><div id=\"resize-mark\"></div>"
               "</handle></body></rml>";
    }
}
