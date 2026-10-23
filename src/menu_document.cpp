#include "menu_document.h"

#include "form.h"

#include "bar_document.h"
#include "markup.h"
#include "menu_sizes.h"

#include <algorithm>
#include <cmath>

namespace headsup
{
    namespace
    {
        constexpr float kRunInset     = 0.3f;  // how much shorter a line box's stretches and chips are than the box
        constexpr float kSpacing      = 0.45f; // between rows
        constexpr float kPadX         = 0.6f;  // a control's text from its sides
        constexpr float kHeadHeight   = 2.05f; // the window's title row
        constexpr float kScrollbar    = 0.7f;
        constexpr float kPopupTall    = 18.0f; // the most of a dropdown's list shown before it scrolls
        constexpr float kNavHeight    = 2.0f;
        constexpr float kNavGap       = 0.2f;  // between two pages of the sidebar
        constexpr float kNavSlide     = 0.35f; // how far a page's name moves under the pointer
        constexpr float kToggleKnob   = kToggleHeight - 2.0f * kToggleInset; // its peg's height
        constexpr float kArrow        = 0.35f; // half an arrow's base
        constexpr float kKnob         = 0.35f; // a slider's knob: a bright bar as tall as its track
        constexpr float kCrossLength  = 0.85f; // the close button's cross: each of its two bars
        constexpr float kCrossBar     = 0.12f;
        constexpr float kPillEdge     = 1.0f;  // pixels: a pill's edge, inside its height
    }

    namespace
    {
        // The accent's alpha where it is light and no fill: a slider's filled part and a hovered card's edge, the
        // title's glow, the line under it, a text box that has the keyboard, and the picked page from end to end.
        constexpr uint32_t kWash = 0x59, kGlow = 0x66, kLine = 0xB3, kFocus = 0x99, kPickedStart = 0x47, kPickedEnd = 0x14;
        constexpr uint32_t kAuroraFrame = 0x40, kAuroraPopupFrame = 0x66; // the accent's alpha in Aurora's edges
        constexpr float kHeadLineGap = 0.2f; // from the title row down to the line of light under it
        constexpr const char* kEase  = " 0.16s cubic-out"; // a control between two of its looks
        constexpr const char* kSlide = " 0.18s cubic-out"; // a switch's knob and its track

        uint32_t Faded(uint32_t argb, uint32_t alpha) { return (argb & ~kAlphaMask) | alpha << kAlphaShift; }

        // What is Aurora's alone: glass in place of flat fills, and dark text on the picked choice's bright gradient.
        std::string AuroraMenuRules(const MenuColors& c)
        {
            std::string css;
            css += "body{decorator:vertical-gradient(#111b2cf2 #090e17f2);background-color:transparent;border-color:" +
                   RcssColor(Faded(c.accent, kAuroraFrame)) + ";}";
            css += ".card{background-color:transparent;decorator:vertical-gradient(#1c283cb8 #131b2ab8);}";
            css += ".seg.picked,.tab.picked,.btn.accent{color:" + RcssColor(Opaque(c.inset)) + ";}";
            css += "scrollbarvertical sliderbar{background-color:" + RcssColor(Faded(c.accent, kAuroraFrame)) + ";}";
            css += std::string("#") + kMenuPopupId + ",#" + kMenuTipId + "{border-color:" + RcssColor(Faded(c.accent, kAuroraPopupFrame)) + ";}";
            return css;
        }
    }

    namespace
    {
        // The game's art in place of the window's fill and edge, and of its lists' and tips'.
        std::string SkinMenuRules(const SkinFrame& frame)
        {
            const std::string art = "background-color:transparent;border-width:0px;border-radius:0px;" + SkinDecorator(frame);
            return "body{" + art + "}#" + std::string(kMenuPopupId) + ",#" + kMenuTipId + "{" + art + "}";
        }
    }

    int TextLift(float ascent, float descent, int fontPixels)
    {
        return static_cast<int>(std::lround((ascent - descent - kCapHeightShare * static_cast<float>(fontPixels)) / 2.0f));
    }

    std::string MenuStyle(const MenuLook& look)
    {
        const MenuColors& c = look.colors;
        const auto color    = [](uint32_t argb) { return RcssColor(argb); };
        const std::string frame = Em(kFrame), radius = Em(kRadius), control = Em(kControlWidth);
        const float top    = kGutter + kHeadHeight + 0.5f; // where the sidebar and the page start
        const std::string wash = "horizontal-gradient(" + color(Faded(c.accent, kWash)) + " " + color(Faded(c.accentEnd, kWash)) + ")";
        const std::string fadingOut = " " + color(Faded(c.accentEnd, 0)) + ")";
        // A line of text in a box this many ems tall, lifted so its capitals stand in the middle: the line shorter by
        // twice the lift, or, where the text's own box is the line, the box moved up by it.
        const auto line = [&](float box, float edge = 0.0f) {
            return "line-height:" + Px(box * static_cast<float>(look.fontPixels) - 2.0f * (edge + static_cast<float>(look.textLift))) + ";";
        };
        const std::string lifted = Px(static_cast<float>(-look.textLift));
        const std::string framed = line(kFrame), raised = "line-height:" + frame + ";position:relative;top:" + lifted + ";";
        std::string css;
        css += "body{position:absolute;display:block;box-sizing:border-box;width:" + Em(kWindowWidth) + ";height:" +
               Em(kWindowHeight) + ";min-width:" + Em(kMinWindowWidth) + ";min-height:" + Em(kMinWindowHeight) +
               ";background-color:" + color(c.background) + ";border:1px " + color(c.border) + ";border-radius:" +
               Em(kCardRadius * 1.5f) + ";color:" + color(c.text) + ";font-family:\"" + look.textFont + "\";z-index:5;}";
        // The window comes in a little small and grows to its size.
        // Its fade has no end of its own: it ends at the element's own opacity, under the window's filter.
        css += "body.open{animation:0.22s cubic-out window-in, 0.22s cubic-out window-fade;}";
        css += "@keyframes window-in{from{transform:scale(0.97);}to{transform:scale(1);}}";
        css += "@keyframes window-fade{from{opacity:0;}}";
        css += "div{display:block;}handle{display:block;}";
        css += std::string(".") + kHiddenClass + "{display:none;}";
        // The frame: the title bar moves the window, the corner resizes it.
        css += "#head{position:absolute;left:" + Em(kGutter) + ";right:" + Em(kGutter + kFrame + 0.5f) + ";top:" +
               Em(kGutter) + ";height:" + Em(kHeadHeight) + ";}";
        css += "#title{display:inline-block;font-family:\"" + look.titleFont + "\";font-size:" + Em(kTitleScale) +
               ";color:" + color(c.heading) + ";font-effect:glow(0px 4px 0px 0px " + color(Faded(c.accent, kGlow)) + ");}";
        css += std::string("#") + kMenuVersionId + "{display:inline-block;margin-left:0.5em;color:" + color(c.muted) + ";}";
        css += std::string("#") + kMenuCloseId + "{position:absolute;right:" + Em(kGutter) + ";top:" + Em(kGutter) +
               ";width:" + frame + ";height:" + frame + ";border-radius:" + Em(kFrame / 2.0f) + ";background-color:" +
               color(c.control) + ";}";
        css += std::string("#") + kMenuCloseId + ":hover{background-color:" + color(c.hover) + ";}";
        // Its cross is drawn, two bars turned about the circle's middle: a letter x sits low in its line.
        css += ".cross{position:absolute;left:50%;top:50%;width:" + Em(kCrossLength) + ";height:" + Em(kCrossBar) +
               ";margin-left:" + Em(-kCrossLength / 2.0f) + ";margin-top:" + Em(-kCrossBar / 2.0f) + ";background-color:" +
               color(c.muted) + ";transform:rotate(45deg);}";
        css += ".cross.back{transform:rotate(-45deg);}";
        css += std::string("#") + kMenuCloseId + ":hover .cross{background-color:" + color(c.text) + ";}";
        // A developer's Reload, left of the close button.
        css += "#tools{position:absolute;display:flex;flex-direction:row;right:" + Em(kGutter + kFrame + kControlGap) + ";top:" +
               Em(kGutter) + ";}";
        css += "#tools .btn{margin-left:" + Em(kControlGap) + ";}";
        css += "#head-line{position:absolute;left:" + Em(kGutter) + ";right:" + Em(kGutter) + ";top:" +
               Em(kGutter + kHeadHeight + kHeadLineGap) + ";height:1px;decorator:horizontal-gradient(" +
               color(Faded(c.accent, kLine)) + fadingOut + ";}";
        css += "#resize{position:absolute;right:0px;bottom:0px;width:1.2em;height:1.2em;}";
        css += "#resize-mark{position:absolute;right:0.3em;bottom:0.3em;width:0px;height:0px;border-left:0.5em transparent;"
               "border-bottom:0.5em " + color(c.raisedEdge) + ";}";
        // The pages, as pills: each slides a little under the pointer, and the picked one is filled.
        // The list is as wide as its longest name in the window's font, and the page takes the rest.
        css += "#columns{position:absolute;display:flex;left:" + Em(kGutter) + ";right:" + Em(kGutter) + ";top:" + Em(top) +
               ";bottom:" + Em(kGutter) + ";}";
        css += std::string("#") + kMenuSidebarId + "{flex:0 0 auto;overflow-y:auto;}";
        // What a name moves by under the pointer comes off the room after it, so the list keeps its width.
        css += ".nav{position:relative;height:" + Em(kNavHeight) + ";" + line(kNavHeight) + "padding-left:" +
               Em(kGutter) + ";padding-right:" + Em(kGutter) + ";white-space:nowrap;border-radius:" + Em(kNavHeight / 2.0f) +
               ";margin-bottom:" + Em(kNavGap) + ";color:" + color(c.label) + ";transition:background-color" + kEase +
               ",color" + kEase + ",padding-left" + kEase + ",padding-right" + kEase + ";}";
        css += ".nav.child{margin-left:" + Em(kGutter) + ";}";
        css += ".nav:hover{background-color:" + color(c.accentSoft) + ";padding-left:" + Em(kGutter + kNavSlide) +
               ";padding-right:" + Em(kGutter - kNavSlide) + ";}";
        css += ".nav.picked{background-color:transparent;decorator:horizontal-gradient(" + color(Faded(c.accent, kPickedStart)) +
               " " + color(Faded(c.accentEnd, kPickedEnd)) + ");color:" + color(c.text) + ";}";
        // The page: its title stays put and its cards scroll.
        css += "#main{position:relative;flex:1 1 0px;margin-left:" + Em(kGutter) + ";}";
        css += std::string("#") + kMenuPageTitleId + "{font-family:\"" + look.titleFont + "\";font-size:" +
               Em(kPageTitleScale) + ";color:" + color(c.heading) + ";}";
        css += std::string("#") + kMenuPageId + "{position:absolute;left:0px;right:0px;top:" + Em(kPageTop) + ";bottom:0px;overflow-y:auto;}";
        css += "scrollbarvertical{width:" + Em(kScrollbar) + ";}";
        css += "scrollbarvertical sliderbar{width:" + Em(kScrollbar) + ";min-height:2em;border-radius:" +
               Em(kScrollbar / 2.0f) + ";background-color:" + color(c.control) + ";}";
        css += "scrollbarvertical sliderbar:hover{background-color:" + color(c.hover) + ";}";
        css += "scrollbarvertical sliderbar:active{background-color:" + color(c.accent) + ";}";
        css += "scrollbarvertical sliderarrowdec,scrollbarvertical sliderarrowinc{width:0px;height:0px;}";
        // Cards: the edge lights under the pointer, the arrow turns into place and the rows fade in as one opens.
        const std::string cardSpacing = "margin-bottom:" + Em(kGutter * 0.7f) + ";margin-right:" + Em(kScrollbar + 0.3f) + ";";
        css += std::string(".") + kMenuCardClass + "{background-color:" + color(c.card) + ";border:1px " + color(c.border) +
               ";border-radius:" + Em(kCardRadius) + ";" + cardSpacing + "transition:border-color 0.2s cubic-out;}";
        // A block under the cards, as wide as one.
        css += std::string(".") + kMenuFootClass + "{" + cardSpacing + "}";
        css += std::string(".") + kMenuCardClass + ":hover{border-color:" + color(Faded(c.accent, kWash)) + ";}";
        css += ".card-head{position:relative;height:" + Em(kHeaderRow) + ";}";
        css += ".arrow{position:absolute;left:0.95em;top:" + Em(kHeaderRow / 2.0f - kArrow) + ";width:0px;height:0px;"
               "border-top:" + Em(kArrow) + " transparent;border-bottom:" + Em(kArrow) + " transparent;border-left:0.5em " +
               color(c.muted) + ";}";
        css += ".arrow.open{left:0.85em;top:" + Em(kHeaderRow / 2.0f - 0.2f) + ";border-top:0.5em " + color(c.muted) +
               ";border-bottom-width:0px;border-left:" + Em(kArrow) + " transparent;border-right:" + Em(kArrow) +
               " transparent;animation:0.18s cubic-out turn;}";
        css += "@keyframes turn{from{transform:rotate(-90deg);}to{transform:rotate(0deg);}}";
        css += ".card-head:hover .arrow{border-left-color:" + color(c.text) + ";}";
        css += ".card-head:hover .arrow.open{border-left-color:transparent;border-top-color:" + color(c.text) + ";}";
        css += ".card-title{position:absolute;left:2.1em;top:0px;line-height:" + Em(kHeaderRow / kCardTitleScale) +
               ";font-family:\"" + look.titleFont + "\";font-size:" + Em(kCardTitleScale) + ";color:" + color(c.heading) +
               ";transition:color" + kEase + ";}";
        css += ".card-head:hover .card-title{color:" + color(c.accent) + ";}";
        css += ".check.card-on{position:absolute;right:" + Em(kCardPad) + ";top:" + Em((kHeaderRow - kToggleHeight) / 2.0f) +
               ";width:" + Em(kCardToggleWidth) + ";}";
        css += ".card-body{padding:0px " + Em(kCardPad) + " " + Em(kCardPad * 0.6f) + " " + Em(kCardPad) +
               ";animation:0.2s cubic-out fade-in;}";
        css += "@keyframes fade-in{from{opacity:0;}to{opacity:1;}}";
        // Rows: the label at the left, its controls at the right. A label too long for what they leave it is cut.
        css += ".row{display:flex;flex-direction:row;align-items:center;justify-content:space-between;"
               "min-height:" + frame + ";margin-bottom:" + Em(kSpacing) + ";}";
        css += ".label{flex:1 1 0px;min-width:0px;overflow:hidden;white-space:nowrap;" + raised + "color:" + color(c.label) + ";}";
        css += ".ctl{position:relative;display:flex;flex-direction:row;align-items:center;justify-content:flex-end;flex:0 0 auto;}";
        css += ".ctl > *{margin-left:" + Em(kControlGap) + ";}";
        css += ".line{display:flex;flex-direction:row;flex-wrap:wrap;align-items:center;margin-bottom:" + Em(kSpacing) + ";}";
        css += ".line > *{margin-right:" + Em(kControlGap) + ";}";
        css += ".line.chips > *{margin-bottom:0.3em;}";
        css += ".line.indent{margin-left:" + Em(kGutter) + ";}";
        css += ".gap{flex:1 1 auto;}";
        css += ".note{color:" + color(c.muted) + ";margin-bottom:" + Em(kSpacing) + ";}";
        css += ".row.indent{margin-left:" + Em(kGutter) + ";}";
        css += ".caption{color:" + color(c.muted) + ";" + raised + "}";
        // A column's name over its controls.
        css += ".head{color:" + color(c.label) + ";" + raised + "text-align:center;}";
        css += ".line-name{color:" + color(c.label) + ";" + raised + "}";
        // A design line: its name, then its box across what is left.
        css += ".line.design{flex-wrap:nowrap;}";
        css += ".line.design .line-name{flex:0 0 auto;width:" + Em(kLineNameWidth) + ";}";
        // A card's rows in two columns, a rule between them.
        css += ".columns{display:flex;flex-direction:row;}";
        css += ".column{flex:1 1 0px;min-width:0px;}";
        css += ".column.second{margin-left:" + Em(kCardPad) + ";padding-left:" + Em(kCardPad) + ";border-left:1px " + color(c.border) + ";}";
        css += ".reading{width:" + control + ";" + raised + "}";
        // A yes or no is a toggle as wide as a slider; in a table's column it is a small one in a cell its own width.
        css += ".switch-cell{display:flex;flex-direction:row;justify-content:center;width:" + Em(kToggleWidth) + ";}";
        // Its state stands in its middle in capitals, where the peg never goes.
        css += ".state{position:absolute;left:0px;top:0px;width:100%;text-align:center;pointer-events:none;" + line(kToggleHeight) +
               "text-transform:uppercase;color:" + color(c.text) + ";}";
        css += ".check{position:relative;width:" + Em(kToggleWidth) + ";height:" + Em(kToggleHeight) + ";border-radius:" +
               radius + ";background-color:" + color(c.control) + ";transition:background-color" + kSlide + ";}";
        css += ".check:hover{background-color:" + color(c.hover) + ";}";
        css += ".check.on{background-color:" + color(Faded(c.accent, kWash)) + ";}";
        // The peg takes no click of its own: pressed while it slides under the pointer and let go once it has
        // passed, a click would be on two elements and so on neither.
        css += ".mark{pointer-events:none;}";
        css += ".mark{position:absolute;left:" + Em(kToggleInset) + ";top:" + Em(kToggleInset) + ";width:" + Em(kTogglePeg) +
               ";height:" + Em(kToggleKnob) + ";border-radius:" + Em(kRadius - kToggleInset) + ";background-color:" +
               color(c.muted) + ";transition:left" + kSlide + ",background-color" + kSlide + ";}";
        css += ".check.on .mark{background-color:" + color(c.accent) + ";}";
        for (const auto& [track, width] : {std::pair{".check", kToggleWidth}, std::pair{".check.wide", kControlWidth},
                 std::pair{".check.card-on", kCardToggleWidth}, std::pair{".check.worded", kCardToggleWidth}})
            css += std::string(track) + ".on .mark{left:" + Em(width - kTogglePeg - kToggleInset) + ";}";
        css += ".check.wide{width:" + control + ";}";
        css += ".check.worded{width:" + Em(kCardToggleWidth) + ";}";
        // Joined choices. These, tabs, buttons and lists ease between their looks, and the picked one is the gradient.
        css += std::string(".seg,.tab,.btn,.dd,.opt,.wchip{transition:background-color") + kEase + ",border-color" + kEase +
               ",color" + kEase + ";}";
        css += ".seg.picked,.tab.picked,.btn.accent{decorator:horizontal-gradient(" + color(c.accent) + " " + color(c.accentEnd) + ");}";
        css += ".segs{display:flex;flex-direction:row;}";
        css += ".seg{height:" + frame + ";" + framed + "text-align:center;background-color:" + color(c.control) +
               ";color:" + color(c.muted) + ";}";
        css += ".seg:hover{background-color:" + color(c.hover) + ";}";
        css += ".seg.picked{background-color:" + color(c.accent) + ";color:" + color(c.text) + ";}";
        // A slider, its value written over it.
        css += ".slider{position:relative;width:" + control + ";height:" + frame + ";}";
        css += "input.range{display:block;width:" + control + ";height:" + frame + ";}";
        css += "input.range slidertrack{height:" + frame + ";border-radius:" + radius + ";background-color:" +
               color(c.control) + ";}";
        css += "input.range:hover slidertrack{background-color:" + color(c.hover) + ";}";
        css += "input.range sliderprogress{height:" + frame + ";border-radius:" + radius + ";decorator:" + wash + ";}";
        css += "input.range sliderbar{width:" + Em(kKnob) + ";height:" + frame + ";margin-top:0em;border-radius:" +
               Em(kKnob / 2.0f) + ";background-color:" + color(c.text) + ";}";
        css += "input.range sliderbar:hover,input.range sliderbar:active{background-color:" + color(c.accentHover) + ";}";
        css += "input.range sliderarrowdec,input.range sliderarrowinc{width:0px;height:0px;}";
        css += ".val{position:absolute;left:0px;top:0px;width:100%;" + framed + "text-align:center;pointer-events:none;}";
        // Text boxes: the edge lights while one has the keyboard.
        css += "input.text{box-sizing:border-box;height:" + frame + ";padding:0.2em " + Em(kPadX) + ";border-radius:" +
               radius + ";border:1px transparent;background-color:" + color(c.control) + ";color:" + color(c.text) +
               ";transition:border-color" + kEase + ";}";
        css += "input.text:hover,input.text:focus{background-color:" + color(c.hover) + ";}";
        css += "input.text:focus{border-color:" + color(Faded(c.accent, kFocus)) + ";}";
        css += "input.text:placeholder-shown{color:" + color(c.muted) + ";}";
        css += "input.text selection{background-color:" + color(c.accentSoft) + ";}";
        css += "input.num{width:" + Em(kNumberWidth) + ";}";
        css += std::string("input.num.") + kLiveClass + "{color:" + color(c.muted) + ";}";
        css += ".num-name{width:" + Em(kNumberName) + ";text-align:right;color:" + color(c.muted) + ";" + raised + "}";
        // A design line's box looks a text box, and holds stretches of text to type in with tags as chips between
        // them. It wraps, and its last stretch takes what is left of its row, so a click past the text lands in it.
        css += ".linebox{box-sizing:border-box;flex:1 1 0px;min-width:0px;display:flex;flex-direction:row;flex-wrap:wrap;"
               "align-items:center;min-height:" + frame + ";padding:0px " + Em(kPadX) + ";border-radius:" + radius +
               ";border:1px transparent;background-color:" + color(c.control) + ";transition:border-color" + kEase + ";}";
        css += ".linebox:hover,.linebox.typing{background-color:" + color(c.hover) + ";}";
        css += ".linebox.typing{border-color:" + color(Faded(c.accent, kFocus)) + ";}";
        css += ".linebox input.run{flex:0 0 auto;height:" + Em(kFrame - kRunInset) + ";padding:0.1em 0px;border-width:0px;"
               "border-radius:0px;background-color:transparent;}";
        css += ".linebox input.run:hover,.linebox input.run:focus{background-color:transparent;}";
        css += ".linebox input.run:last-child{flex:1 1 auto;}";
        css += ".linebox .chip{flex:0 0 auto;height:" + Em(kFrame - kRunInset) + ";" + line(kFrame - kRunInset, kPillEdge) +
               "padding:0px 0.5em;margin:0.1em 0.1em;}";
        // A line break takes a row of the box to itself, so what follows it starts the next, as on the nameplate.
        css += ".linebox .chip.break{flex:1 0 100%;margin:0.1em 0px;text-align:center;}";
        css += "input.hex{width:6em;}";
        // A dropdown's closed control; its list is the popup.
        css += ".dd{position:relative;box-sizing:border-box;height:" + frame + ";" + framed + "padding:0px " +
               Em(kFrame) + " 0px " + Em(kPadX) + ";border-radius:" + radius + ";background-color:" + color(c.control) +
               ";overflow:hidden;white-space:nowrap;}";
        css += ".dd:hover,.dd.open{background-color:" + color(c.hover) + ";}";
        css += ".dd-val{overflow:hidden;pointer-events:none;}";
        css += ".dd-arrow{position:absolute;right:0.5em;top:0.6em;width:0px;height:0px;border-left:" + Em(kArrow) +
               " transparent;border-right:" + Em(kArrow) + " transparent;border-top:0.45em " + color(c.text) + ";}";
        css += std::string("#") + kMenuPopupId + "{position:absolute;z-index:50;box-sizing:border-box;padding:0.2em;"
               "background-color:" + color(Opaque(c.card)) + ";border:1px " + color(c.raisedEdge) + ";border-radius:" +
               radius + ";}";
        // The popup drops in under its control.
        css += std::string("#") + kMenuPopupId + ".open{animation:0.16s cubic-out drop-in;}";
        css += "@keyframes drop-in{from{opacity:0;transform:translateY(-6px);}to{opacity:1;transform:translateY(0px);}}";
        css += ".list{max-height:" + Em(kPopupTall) + ";overflow-y:auto;}";
        css += ".opt{height:" + frame + ";" + framed + "padding:0px " + Em(kPadX) + ";border-radius:" + radius +
               ";white-space:nowrap;}";
        css += ".opt:hover,.opt.picked{background-color:" + color(c.accentSoft) + ";}";
        css += ".dot{display:inline-block;width:" + Em(kChipDot) + ";height:" + Em(kChipDot) + ";border-radius:" +
               Em(kChipDot / 2.0f) + ";margin-right:0.5em;background-color:" + color(c.muted) + ";}";
        css += ".on .dot{background-color:" + color(c.accent) + ";}";
        // A color: its swatch, and the sliders its popup holds.
        css += ".swatch{box-sizing:border-box;width:" + frame + ";height:" + frame + ";border-radius:" + radius +
               ";border:1px " + color(c.raisedEdge) + ";}";
        css += ".swatch.none{border-color:transparent;}";
        // The marks a dash can be drawn as, in the Replace Dashes list.
        css += ".dash{display:inline-block;width:0.6em;height:0.6em;background-color:" + color(c.text) + ";}";
        css += ".dash.dot{border-radius:0.3em;}";
        css += ".dash.diamond{width:0.5em;height:0.5em;transform:rotate(45deg);}";
        // The arrow that puts a color back to its default.
        css += ".reset{width:" + frame + ";height:" + frame + ";decorator:image(\"" + ResetSource(c.muted, kResetArrowPixels) +
               "\" contain);}";
        css += ".reset:hover{decorator:image(\"" + ResetSource(c.accent, kResetArrowPixels) + "\" contain);}";
        css += ".picker{padding:0.4em;}";
        css += ".pick-row{position:relative;display:flex;flex-direction:row;align-items:center;margin-bottom:0.3em;}";
        css += ".pick-name{width:1.2em;color:" + color(c.label) + ";}";
        css += ".pick-row input.range{width:9em;}.pick-row .val{position:relative;width:2.6em;text-align:right;}";
        // Buttons and chips.
        css += ".btn{box-sizing:border-box;height:" + frame + ";" + line(kFrame, kPillEdge) + "padding:0px " + Em(kPadX + 0.2f) +
               ";border-radius:" + Em(kFrame / 2.0f) + ";border:" + Px(kPillEdge) + " transparent;text-align:center;white-space:nowrap;}";
        css += ".btn.raised{background-color:" + color(c.control) + ";border-color:" + color(c.raisedEdge) + ";}";
        css += ".btn.raised:hover{background-color:" + color(c.hover) + ";}";
        css += ".btn.flat{border-color:" + color(c.raisedEdge) + ";color:" + color(c.label) + ";}";
        css += ".btn.flat:hover{background-color:" + color(c.accentSoft) + ";}";
        css += ".btn.accent{background-color:" + color(c.accent) + ";}";
        css += ".btn.accent:hover{background-color:" + color(c.accentHover) + ";}";
        // A button in the red of a slider's filled part.
        css += ".btn.wash{background-color:transparent;decorator:" + wash + ";border-color:" + color(c.raisedEdge) + ";color:" +
               color(c.text) + ";}";
        css += ".btn.wash:hover{decorator:horizontal-gradient(" + color(Faded(c.accent, kGlow)) + " " + color(Faded(c.accentEnd, kGlow)) +
               ");}";
        css += ".btn.reset-all{height:" + Em(kTallButton) + ";" + line(kTallButton, kPillEdge) + "border-radius:" +
               Em(kTallButton / 2.0f) + ";text-transform:uppercase;}";
        css += ".btn.waiting{border-color:" + color(c.accent) + ";color:" + color(c.muted) + ";}";
        css += ".btn.round{width:" + frame + ";padding:0px;color:" + color(c.muted) + ";}";
        css += ".btn.step{position:relative;width:" + frame + ";padding:0px;border-radius:" + radius + ";background-color:" +
               color(c.control) + ";}";
        css += ".btn.step:hover{background-color:" + color(c.hover) + ";}";
        css += ".stepval{margin:0px " + Em(kControlGap) + ";height:" + frame + ";" + framed + "text-align:center;border-radius:" + radius +
               ";background-color:" + color(c.control) + ";}";
        css += ".tri{position:absolute;top:" + Em(kFrame / 2.0f - kArrow) + ";width:0px;height:0px;border-top:" + Em(kArrow) +
               " transparent;border-bottom:" + Em(kArrow) + " transparent;}";
        css += ".tri.left{left:0.5em;border-right:0.45em " + color(c.text) + ";}";
        css += ".tri.right{left:0.6em;border-left:0.45em " + color(c.text) + ";}";
        css += ".chip img{width:1em;height:1em;margin-right:0.3em;vertical-align:-0.15em;}";
        // A chip of several tags: each a part of its own to drag, with a quiet word between.
        css += ".chip .part,.chip .or{display:inline-block;}";
        css += ".chip .or{margin:0px 0.4em;opacity:0.65;}";
        css += ".drag{drag:clone;}";
        // Tabs: equal across each row, the open one filled, standing on a rule.
        css += ".tab-row{display:flex;flex-direction:row;margin-bottom:" + Em(kTabGap) + ";}";
        css += ".tab{flex:1 1 0px;height:" + Em(kTabHeight) + ";" + line(kTabHeight) + "margin-right:" +
               Em(kTabGap) + ";text-align:center;white-space:nowrap;border-radius:" + radius + " " + radius +
               " 0px 0px;background-color:" + color(c.control) + ";color:" + color(c.muted) + ";}";
        css += ".tab:hover{background-color:" + color(c.hover) + ";}";
        css += ".tab.picked{background-color:" + color(c.accent) + ";color:" + color(c.text) + ";}";
        css += ".tab-rule{height:" + Em(kTabRule) + ";margin-top:-" + Em(kTabGap) + ";margin-bottom:" + Em(kSpacing) +
               ";decorator:horizontal-gradient(" + color(c.accent) + fadingOut + ";}";
        // The widgets' trays.
        css += ".line.trays{align-items:stretch;flex-wrap:nowrap;}";
        css += ".line.trays > *{margin-right:0.6em;}";
        css += ".tray{flex:1 1 0px;box-sizing:border-box;padding:0.4em 0.45em;border-radius:" + Em(kCardRadius) +
               ";border:1px " + color(c.border) + ";background-color:" + color(c.inset) + ";}";
        css += ".tray-name{height:1.4em;padding-left:0.3em;color:" + color(c.muted) + ";}";
        css += ".wchip{display:block;height:" + Em(kChipHeight) + ";" + line(kChipHeight, kPillEdge) +
               "padding:0px 0.7em;margin-bottom:0.3em;border-radius:" + Em(kChipHeight / 2.0f) + ";text-align:left;color:" +
               color(c.muted) + ";}";
        css += ".wchip.on{color:" + color(c.text) + ";}";
        css += ".wchip.picked{border-color:" + color(c.accent) + ";}";
        css += ".tray-end{height:" + Em(kChipHeight) + ";}";
        // What a control says under the pointer.
        css += std::string("#") + kMenuTipId + "{position:absolute;z-index:60;box-sizing:border-box;max-width:" +
               Em(kTipWidth) + ";padding:0.4em 0.6em;border-radius:" + radius + ";border:1px " + color(c.raisedEdge) +
               ";background-color:" + color(Opaque(c.card)) + ";pointer-events:none;}";

        if (look.theme == MenuTheme::Aurora) css += AuroraMenuRules(c);
        if (look.skin) css += SkinMenuRules(*look.skin);
        // The settings of what is switched off are not shown. Last, and with the body before it, so that no rule
        // that lays a row or a control out puts it back.
        css += std::string("body .") + kOffClass + "{display:none;}";
        return css;
    }

    std::string MenuDocument(const MenuLook& look)
    {
        return "<rml><head><style>" + MenuStyle(look) + "</style></head><body>"
               "<handle id=\"head\" move_target=\"#document\"><div id=\"title\">HeadsUp</div><div id=\"" +
               kMenuVersionId + "\"></div></handle><div id=\"head-line\"></div>" +
               (look.developer ? std::string("<div id=\"tools\"><div class=\"") + kRaisedPill + "\" id=\"" + kMenuReloadId +
                                     "\"></div></div>"
                               : std::string()) +
               "<div id=\"" + kMenuCloseId +
               "\"><div class=\"cross\"></div><div class=\"cross back\"></div></div><div id=\"columns\"><div id=\"" +
               kMenuSidebarId + "\"></div><div id=\"main\"><div id=\"" + kMenuPageTitleId + "\"></div><div id=\"" +
               kMenuPageId + "\"></div></div></div><div id=\"" + kMenuPopupId + "\" class=\"hidden\"></div><div id=\"" + kMenuTipId +
               "\" class=\"hidden\"></div><handle id=\"resize\" size_target=\"#document\"><div id=\"resize-mark\"></div>"
               "</handle></body></rml>";
    }
}
