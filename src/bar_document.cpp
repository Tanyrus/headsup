#include "bar_document.h"

#include "argb.h"
#include "bar_layout.h"
#include "bar_placement.h"
#include "markup.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace headsup
{
    namespace
    {
        constexpr std::string_view kIconKind = "icon", kStripKind = "strip", kDiamondKind = "diamond", kCoinKind = "coin",
                                   kStarKind = "star", kResetKind = "reset";
        // RmlUi blurs a glow by a Gaussian of this share of its width; the bar's shadow is a CSS blur radius, twice
        // its Gaussian's.
        constexpr float kGlowSigmaPerWidth = 0.4f;
        constexpr float kSigmaPerRadius    = 0.5f;
        constexpr int kShadowDrop          = 1; // pixels down
        // The accent's alpha in the target's frame as it breathes, from and to, and in a lit title's glow.
        constexpr uint32_t kBreatheLow = 0x59, kBreatheHigh = 0xCC, kLitGlow = 0x80;
        // How a widget eases into its lit look.
        const std::string kEaseBackground = "background-color 0.2s cubic-out", kEaseBorder = "border-color 0.2s cubic-out";
        constexpr float kGlint             = 2.0f;       // the line of light at the foot of a glass strip, in pixels
        constexpr uint32_t kGlassTop = 0xF21B2740, kGlassFoot = 0xF20C1220; // a popup's panel, down its height
        constexpr uint32_t kGlassFrame    = 0x40;       // the accent's alpha in a popup's frame
        constexpr char kSeparator      = '/';
        // A strip's name: its kind, top, middle, bottom, last row, the row under and what fades below, then its border
        // and its rows.
        constexpr size_t kStripColors = 6, kStripParts = 1 + kStripColors + 3;
        constexpr int kHexBase         = 16;
        constexpr float kPercentWhole  = 100.0f;
        constexpr float kBoxBorder     = 1.0f; // a pill's or frame's, inside its edge
        // The widgets' content starts a pixel under the pill row's top in every shape, bordered or not.
        constexpr float kContentInset  = 1.0f;

        std::string Hex(uint32_t value)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%08x", value);
            return text;
        }

        // The parts of a name after its prefix, between its separators.
        std::vector<std::string_view> Parts(std::string_view name)
        {
            std::vector<std::string_view> parts;
            while (true)
            {
                const size_t at = name.find(kSeparator);
                parts.push_back(name.substr(0, at));
                if (at == std::string_view::npos) break;
                name.remove_prefix(at + 1);
            }
            return parts;
        }

        std::optional<uint32_t> Number(std::string_view text, int base = 10)
        {
            uint32_t value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
            if (error != std::errc() || end != text.data() + text.size()) return std::nullopt;
            return value;
        }

        std::string Spans(const std::vector<Span>& spans, const BarLook& look)
        {
            std::string rml;
            for (const Span& span : spans)
                rml += "<span style=\"color:" + RcssColor(look.BarColor(span)) + ";\">" + EscapedRml(span.text) + "</span>";
            return rml;
        }

        std::string Line(const char* classes, const std::vector<Span>& spans, const std::vector<Icon>& icons,
            const BarLook& look)
        {
            std::string rml = std::string("<div class=\"") + classes + "\"><span class=\"t\">" + Spans(spans, look) + "</span>";
            if (!icons.empty())
            {
                rml += "<span class=\"line-icons\">";
                for (const Icon icon : icons)
                    rml += "<img class=\"line-icon\" src=\"" + IconSource(icon) + "\"/>";
                rml += "</span>";
            }
            return rml + "</div>";
        }

        // A box's fill, edge and corners as BoxOf gives them; no box is a clear one, its room kept.
        std::string BoxRules(const std::optional<BoxStyle>& box)
        {
            if (!box) return "background-color:transparent;border-color:transparent;";
            const BoxStyle& style = *box;
            return "background-color:" + RcssColor(style.top) + ";border-color:" +
                   (style.border != 0 ? RcssColor(style.border) : std::string("transparent")) + ";border-radius:" +
                   Px(style.radius) + ";";
        }

        // The bar's text shadow as a font effect: the letters blurred, once a pixel down and once in place.
        std::string ShadowEffect(int pixels)
        {
            const float sigma = static_cast<float>(pixels) * kShadowPerHeight * kSigmaPerRadius;
            const std::string blur = Px(static_cast<float>(WholePixels(sigma / kGlowSigmaPerWidth)));
            const std::string color = RcssColor(WithAlpha(kBlack, static_cast<uint32_t>(std::lround(kShadowStrength * 255.0f))));
            return "font-effect:glow(0px " + blur + " 0px " + Px(static_cast<float>(kShadowDrop)) + " " + color +
                   "),glow(0px " + blur + " 0px 0px " + color + ");";
        }

        std::string FaceRules(const std::string& font, int pixels, int lineHeight)
        {
            return "font-family:\"" + font + "\";font-size:" + Px(static_cast<float>(pixels)) + ";line-height:" +
                   Px(static_cast<float>(lineHeight)) + ";";
        }

        std::string LineRules(const std::string& font, int pixels, const BarLinePlace& line, float rowTop)
        {
            return FaceRules(font, pixels, line.height) + "margin-top:" + Px(std::round(line.top) - rowTop) + ";" +
                   ShadowEffect(pixels);
        }

        // A box's place and size as an inline style: whole pixels, as the bar's boxes are drawn.
        std::string BoxAt(float x, float y, float width, float height)
        {
            return "left:" + Px(std::round(x)) + ";top:" + Px(std::round(y)) + ";width:" +
                   Px(static_cast<float>(WholePixels(width))) + ";height:" + Px(static_cast<float>(WholePixels(height))) + ";";
        }

        std::string TextAt(const char* classes, float x, float y, uint32_t color, std::string_view text)
        {
            return std::string("<div class=\"p ") + classes + "\" style=\"left:" + Px(std::round(x)) + ";top:" +
                   Px(std::round(y)) + ";color:" + RcssColor(color) + ";\">" + EscapedRml(text) + "</div>";
        }

        // A panel's fill top to bottom, its edge and its corners.
        std::string PanelRules(const BoxStyle& style)
        {
            return "box-sizing:border-box;border:1px " + (style.border != 0 ? RcssColor(style.border) : std::string("transparent")) +
                   ";border-radius:" + Px(style.radius) + ";decorator:vertical-gradient(" + RcssColor(style.top) + " " +
                   RcssColor(style.bottom) + ");";
        }

        // A box drawn in the game's art, which is its own edge.
        std::string ArtRules(const SkinFrame& frame) { return "box-sizing:border-box;" + SkinDecorator(frame); }

        // A box of one fill, its edge and, when it has them, its corners.
        std::string FlatRules(const BoxStyle& style, bool corners)
        {
            return "box-sizing:border-box;border:1px " + (style.border != 0 ? RcssColor(style.border) : std::string("transparent")) +
                   ";" + (corners ? "border-radius:" + Px(style.radius) + ";" : std::string()) + "background-color:" +
                   RcssColor(style.top) + ";";
        }
    }

    std::string IconSource(Icon icon)
    {
        return std::string(kHostSource) + std::string(kIconKind) + kSeparator + std::to_string(static_cast<int>(icon));
    }

    std::string StripSource(const BarPalette& palette, int rows, int belowRows)
    {
        std::string name = std::string(kHostSource) + std::string(kStripKind);
        const uint32_t top = palette.stripTop, middle = palette.gradient ? palette.stripMiddle : top,
                       bottom = palette.gradient ? palette.stripBottom : top;
        for (const uint32_t color : {top, middle, bottom, palette.bottomLine, palette.underLine, palette.below})
            name += kSeparator + Hex(color);
        return name + kSeparator + std::to_string(palette.border) + kSeparator + std::to_string(rows) + kSeparator +
               std::to_string(belowRows);
    }

    std::string DiamondSource(uint32_t argb, int pixels)
    {
        return std::string(kHostSource) + std::string(kDiamondKind) + kSeparator + Hex(argb) + kSeparator +
               std::to_string(pixels);
    }

    std::string CoinSource(int pixels)
    {
        return std::string(kHostSource) + std::string(kCoinKind) + kSeparator + std::to_string(pixels);
    }

    std::string StarSource(int pixels)
    {
        return std::string(kHostSource) + std::string(kStarKind) + kSeparator + std::to_string(pixels);
    }

    std::string ResetSource(uint32_t argb, int pixels)
    {
        return std::string(kHostSource) + std::string(kResetKind) + kSeparator + Hex(argb) + kSeparator + std::to_string(pixels);
    }

    std::optional<Image> HostImage(std::string_view name)
    {
        if (name.substr(0, kHostSource.size()) != kHostSource) return std::nullopt;
        const std::vector<std::string_view> parts = Parts(name.substr(kHostSource.size()));
        const std::string_view kind               = parts[0];
        auto number = [&](size_t part, int base = 10) {
            return part < parts.size() ? Number(parts[part], base) : std::nullopt;
        };
        if (kind == kIconKind && parts.size() == 2)
        {
            const std::optional<uint32_t> icon = number(1);
            if (!icon || *icon >= static_cast<uint32_t>(kIconCount)) return std::nullopt;
            const IconBitmap& bitmap = IconImage(static_cast<Icon>(*icon));
            Image image{static_cast<int>(bitmap.width), static_cast<int>(bitmap.height), {}};
            image.argb.resize(static_cast<size_t>(bitmap.width) * bitmap.height);
            // The bitmap's bytes are B, G, R, A: an ARGB word's bytes in memory.
            std::memcpy(image.argb.data(), bitmap.bgra, image.argb.size() * sizeof(uint32_t));
            return image;
        }
        if (kind == kStripKind && parts.size() == kStripParts)
        {
            std::array<uint32_t, kStripColors> colors{};
            for (size_t i = 0; i < kStripColors; ++i)
                if (const std::optional<uint32_t> color = number(1 + i, kHexBase))
                    colors[i] = *color;
                else
                    return std::nullopt;
            const std::optional<uint32_t> border = number(1 + kStripColors), rows = number(2 + kStripColors),
                                          below = number(3 + kStripColors);
            if (!border || !rows || !below || *border > static_cast<uint32_t>(kMaxStripBorder)) return std::nullopt;
            return StripColumn(static_cast<int>(*rows), static_cast<int>(*below), colors[0], colors[1], colors[2], colors[3],
                colors[4], colors[5], static_cast<int>(*border));
        }
        if ((kind == kDiamondKind || kind == kResetKind) && parts.size() == 3)
        {
            const std::optional<uint32_t> color = number(1, kHexBase), pixels = number(2);
            if (!color || !pixels) return std::nullopt;
            return kind == kDiamondKind ? Diamond(static_cast<int>(*pixels), *color) : ResetArrow(static_cast<int>(*pixels), *color);
        }
        if ((kind == kCoinKind || kind == kStarKind) && parts.size() == 2)
        {
            const std::optional<uint32_t> pixels = number(1);
            if (!pixels) return std::nullopt;
            return kind == kCoinKind ? Coin(static_cast<int>(*pixels), kCoinColor) : Star(static_cast<int>(*pixels), kStarColor);
        }
        return std::nullopt;
    }

    std::string WidgetId(BarWidget widget) { return std::string("w-") + BarWidgetName(widget); }
    std::string DividerId(int divider) { return "d-" + std::to_string(divider); }

    namespace
    {
        // A color with another alpha.
        uint32_t Faded(uint32_t argb, uint32_t alpha) { return (argb & ~kAlphaMask) | alpha << kAlphaShift; }

        // The glass shape's own: a line of light under the strip, and widgets that show no pill until they are lit.
        std::string GlassBarRules(const BarLook& look)
        {
            const uint32_t cyan = look.palette.accent, violet = look.palette.accentEnd;
            const std::string c = RcssColor(cyan), v = RcssColor(violet);
            std::string css;
            css += ".glint{position:absolute;top:" + Px(look.metrics.height - kGlint) + ";height:" + Px(kGlint) + ";}";
            css += ".glint.a{left:0%;width:30%;decorator:horizontal-gradient(" + RcssColor(Faded(cyan, 0)) + " " + c + ");}";
            css += ".glint.b{left:30%;width:40%;decorator:horizontal-gradient(" + c + " " + v + ");}";
            css += ".glint.c{left:70%;width:30%;decorator:horizontal-gradient(" + v + " " + RcssColor(Faded(violet, 0)) + ");}";
            css += ".pills .widget,.pills .widget.flat{border-radius:" + Px(look.row.height / 2.0f) +
                   ";background-color:transparent;border-color:transparent;}";
            css += ".pills .widget.target{background-color:" + RcssColor(Faded(cyan, 0x14)) + ";}";
            css += ".pills .widget.lit{background-color:" + RcssColor(Faded(cyan, 0x2E)) + ";border-color:" +
                   RcssColor(Faded(cyan, 0xB3)) + ";}";
            css += ".progress,.beside{background-color:#ffffff1f;}";
            return css;
        }
    }

    std::string BarDocument(const BarLook& look, const BarTextPlace& place)
    {
        const BarPalette& p  = look.palette;
        const BarMetrics& m  = look.metrics;
        const BarRow& row    = look.row;
        const TextRow& lines = look.lines;
        const int stripRows = WholePixels(m.height), belowRows = WholePixels(m.below);
        // The game's art frames the bar alone: the glow other themes fade out under the strip would hang its frame low.
        const float stripHeight =
            static_cast<float>(stripRows + (look.skin ? 0 : belowRows)) + (p.underLine != 0 ? 1.0f : 0.0f);
        const float mid         = row.y + row.height / 2.0f;
        const float rowTop      = row.y + kContentInset; // where a widget's content starts
        const float clear       = DiamondClearance(m);
        const int divider       = WholePixels(m.diamond), cap = WholePixels(m.diamond * kCapScale);
        const int coin          = CoinPixels(m);
        const float underHeight = ProgressHeight(m, false), besideHeight = ProgressHeight(m, true);
        const std::string capSource     = DiamondSource(p.accent, cap);
        const std::string dividerSource = DiamondSource(look.ornate ? p.divider : p.accent, divider);
        auto boxOf = [&](BarWidget widget, bool lit) { return look.BoxOf(widget, lit); };
        auto square = [](float size) { return "width:" + Px(size) + ";height:" + Px(size) + ";"; };
        auto centered = [&](float size) { return Px(std::round(mid - size / 2.0f)); };
        const std::string shape = look.ornate ? ".ornate" : ".pills";

        std::string css;
        css += "body{display:block;width:100%;height:100%;font-family:\"" + std::string(look.detailFont) + "\";color:" +
               RcssColor(p.title) + ";}";
        css += "div{display:block;}";
        css += std::string(".") + kHiddenClass + "{display:none;}";
        css += std::string("#") + kStripId + "{position:absolute;left:0px;top:0px;width:100%;height:" + Px(stripHeight) + ";" +
               (look.skin ? SkinDecorator(*look.skin) : "decorator:image(\"" + StripSource(p, stripRows, belowRows) + "\");") + "}";
        css += ".cap{position:absolute;top:" + centered(static_cast<float>(cap)) + ";" + square(static_cast<float>(cap)) + "}";
        css += "#cap-left{left:" + Px(std::round(m.cap / 2.0f - static_cast<float>(cap) / 2.0f)) + ";}";
        css += ".divider{position:absolute;top:" + centered(static_cast<float>(divider)) + ";" +
               square(static_cast<float>(divider)) + "}";

        // Every widget takes the pill row. A bordered one keeps its border inside its room; a plain ornate one
        // keeps the divider's clear space either side as margin, so its tint ends short of the diamonds.
        css += ".widget{position:absolute;top:" + Px(row.y) + ";height:" + Px(row.height) +
               ";box-sizing:border-box;white-space:nowrap;border-width:" + Px(kBoxBorder) + ";padding:0px " +
               Px(m.padding - kBoxBorder) + ";" + BoxRules(std::nullopt) +
               "transition:" + kEaseBackground + "," + kEaseBorder + ";}";
        if (look.ornate)
        {
            css += ".ornate .widget{border-width:0px;margin:0px " + Px(clear) + ";padding:" + Px(kContentInset) + " " +
                   Px(m.padding - clear) + " 0px " + Px(m.padding - clear) + ";}";
            css += ".ornate .widget.lit{" + BoxRules(boxOf(BarWidget::Zone, true)) + "}";
            css += ".ornate .widget.target{border-width:" + Px(kBoxBorder) + ";margin:0px;padding:0px " +
                   Px(m.padding - kBoxBorder) + ";" + BoxRules(boxOf(BarWidget::Target, false)) + "}";
            css += ".ornate .widget.target.lit{" + BoxRules(boxOf(BarWidget::Target, true)) + "}";
        }
        else
        {
            css += ".pills .widget{" + BoxRules(boxOf(BarWidget::Weather, false)) + "}";
            css += ".pills .widget.flat{" + BoxRules(boxOf(BarWidget::Zone, false)) + "}";
            css += ".pills .widget.target{" + BoxRules(boxOf(BarWidget::Target, false)) + "}";
            css += ".pills .widget.lit{" + BoxRules(boxOf(BarWidget::Weather, true)) + "}";
        }
        // A widget eases into its lit look (above) and its title glows there; the target's frame breathes from the
        // accent into what it runs into until it is lit.
        // RmlUi refuses a transition on a property an animation drives, once a border side, as the breathing comes back.
        css += ".widget.target{animation:2.4s sine-in-out infinite alternate breathe;transition:" + kEaseBackground + ";}";
        css += "@keyframes breathe{from{border-color:" + RcssColor(Faded(p.accent, kBreatheLow)) + ";}to{border-color:" +
               RcssColor(Faded(p.accentEnd, kBreatheHigh)) + ";}}";
        css += std::string(".widget.") + kLitClass + "{animation:none;}";
        css += std::string(".widget.") + kLitClass + " .title{font-effect:glow(0px 3px 0px 0px " +
               RcssColor(Faded(p.accent, kLitGlow)) + ");}";

        css += ".content{position:relative;height:100%;}";
        css += ".icon,.coin,.text,.beside,.line-icons,.line-icon,.t{display:inline-block;vertical-align:top;}";
        css += ".icon{" + square(m.icon) + "margin-top:" + Px(std::round(mid - m.icon / 2.0f) - rowTop) + ";margin-right:" +
               Px(m.gap) + ";}";
        css += ".coin{" + square(static_cast<float>(coin)) + "margin-top:" +
               Px(std::round(mid - static_cast<float>(coin) / 2.0f) - rowTop) + ";margin-right:" + Px(m.gap) + ";}";
        css += ".t{white-space:pre;}";
        css += ".title{" + LineRules(look.titleFont, lines.title, place.title, rowTop) + "}";
        css += ".title.alone{margin-top:" + Px(std::round(place.title.aloneTop) - rowTop) + ";}";
        css += ".title.figures{" + LineRules(look.detailFont, lines.title, place.figures, rowTop) + "}";
        css += ".title.figures.alone{margin-top:" + Px(std::round(place.figures.aloneTop) - rowTop) + ";}";
        css += ".line-icons{margin-left:" + Px(kLineIconGap) + ";}";
        css += ".line-icon{margin-left:" + Px(kLineIconGap) + ";}";
        css += ".title .line-icon{" + square(static_cast<float>(lines.title)) + "margin-top:" + Px(place.title.iconDrop) + ";}";
        css += ".title.figures .line-icon{margin-top:" + Px(place.figures.iconDrop) + ";}";
        if (lines.twoLines)
        {
            const float underTitle   = std::round(place.title.top) + static_cast<float>(place.title.height);
            const float underFigures = std::round(place.figures.top) + static_cast<float>(place.figures.height);
            css += ".detail{" + LineRules(look.detailFont, lines.detail, place.detail, underTitle) + "}";
            css += ".detail.fig{margin-top:" + Px(std::round(place.detail.top) - underFigures) + ";}";
            css += ".detail .line-icon{" + square(static_cast<float>(lines.detail)) + "margin-top:" +
                   Px(place.detail.iconDrop) + ";}";
        }
        css += ".progress{position:absolute;left:0px;right:0px;top:" +
               Px(row.y + row.height - kProgressInset - underHeight - rowTop) + ";height:" + Px(underHeight) +
               ";background-color:" + RcssColor(p.divider) + ";}";
        css += ".beside{margin-left:" + Px(m.gap) + ";width:" + Px(BesideWidth(m)) + ";height:" + Px(besideHeight) +
               ";margin-top:" + Px(std::round(row.y + (row.height - besideHeight) / 2.0f) - rowTop) +
               ";background-color:" + RcssColor(p.divider) + ";}";
        // Progress is a gradient that pulses.
        css += ".fill{height:100%;decorator:horizontal-gradient(" + RcssColor(p.accent) + " " + RcssColor(p.accentEnd) +
               ");animation:1.8s sine-in-out infinite alternate pulse;}";
        css += "@keyframes pulse{from{opacity:0.65;}to{opacity:1;}}";
        // Game help's bar: its pieces are placed in the screen's own pixels.
        css += std::string("#") + kHelpId + "{position:absolute;left:0px;top:0px;width:0px;height:0px;}";
        css += ".p{position:absolute;white-space:pre;}";
        css += ".help-box{position:absolute;" + (look.skin ? ArtRules(*look.skin) : PanelRules(HelpBarPanel(look))) + "}";
        css += ".hc{" + FaceRules(look.titleFont, lines.title, place.title.height) + ShadowEffect(lines.title) + "}";
        css += ".hh{" + FaceRules(look.detailFont, lines.title, place.figures.height) + ShadowEffect(lines.title) + "}";
        const bool glass = look.palette.shape == BarShape::Glass;
        if (glass) css += GlassBarRules(look);

        std::string rml = "<rml><head><style>" + css + "</style></head><body class=\"" + shape.substr(1) + "\">";
        rml += std::string("<div id=\"") + kStripId + "\"></div>";
        if (glass) rml += "<div class=\"glint a\"></div><div class=\"glint b\"></div><div class=\"glint c\"></div>";
        if (look.ornate)
            rml += "<img class=\"cap\" id=\"cap-left\" src=\"" + capSource + "\"/><img class=\"cap\" id=\"" + kRightCapId +
                   "\" src=\"" + capSource + "\"/>";
        for (int d = 0; d < kDividerCount; ++d)
            rml += "<img class=\"divider hidden\" id=\"" + DividerId(d) + "\" src=\"" + dividerSource + "\"/>";
        for (int w = 0; w < kBarWidgetCount; ++w)
        {
            const auto widget = static_cast<BarWidget>(w);
            if (!OnTheBar(widget)) continue;
            // Zone and Earth time show no pill until lit.
            const bool flat = !look.ornate && !boxOf(widget, false);
            rml += std::string("<div class=\"widget hidden") + (widget == BarWidget::Target ? " target" : "") +
                   (flat ? " flat" : "") + "\" id=\"" + WidgetId(widget) + "\"></div>";
        }
        rml += std::string("<div id=\"") + kHelpId + "\"></div>";
        return rml + "</body></rml>";
    }

    std::string HelpBarRml(const HelpBarParts& parts, const BarLook& look, const BarTextPlace& place)
    {
        const BarRect& box = parts.row.box;
        if (box.width <= 0.0f) return {};
        std::string rml;
        if (look.settings.helpBarBackground)
            rml += "<div class=\"help-box\" style=\"" + BoxAt(box.x, box.y, box.width, box.height) + "\"></div>";
        auto centered = [&](int lineHeight) { return box.y + (box.height - static_cast<float>(lineHeight)) / 2.0f; };
        const float captionY = centered(place.title.height);
        if (!parts.caption.empty()) rml += TextAt("hc", parts.row.captionX, captionY, look.palette.title, parts.caption);
        if (parts.bag) rml += TextAt("hc", parts.row.bagX, captionY, look.BarColor(*parts.bag), parts.bag->text);
        if (!parts.help.empty())
            rml += TextAt("hh", parts.row.helpX, centered(place.figures.height), look.palette.text, parts.help);
        return rml;
    }

    std::string PopupDocument(const BarLook& look, const PopupFaces& faces)
    {
        const BoxStyle panel = PopupPanel(look);
        std::string css = "body{display:block;width:100%;height:100%;pointer-events:none;z-index:10;}";
        css += "div{display:block;}";
        css += std::string(".") + kHiddenClass + "{display:none;}";
        // A popup drops in from its widget.
        css += std::string("#") + kPopupId + "{position:absolute;transform-origin:50% 0%;}";
        css += std::string("#") + kPopupId + "." + kOpenClass + "{animation:0.2s cubic-out drop-in;}";
        css += "@keyframes drop-in{from{opacity:0;transform:translateY(-10px) scale(0.96);}to{opacity:1;"
               "transform:translateY(0px) scale(1);}}";
        css += ".p{position:absolute;white-space:pre;}";
        for (int f = 0; f < kPopupFontCount; ++f)
            css += ".f" + std::to_string(f) + "{" + FaceRules(faces[f].family, faces[f].pixels, faces[f].line) +
                   "letter-spacing:" + Px(static_cast<float>(faces[f].tracking)) + ";" + ShadowEffect(faces[f].pixels) + "}";
        if (panel.outer != 0)
            css += "#outer{position:absolute;box-sizing:border-box;border:1px " + RcssColor(panel.outer) +
                   ";border-radius:" + Px(panel.radius + static_cast<float>(OuterPad(panel))) + ";}";
        css += "#panel{position:absolute;left:0px;top:0px;" + (look.skin ? ArtRules(*look.skin) : PanelRules(panel)) + "}";
        css += ".card{" + FlatRules(PopupCard(look), true) + "}";
        css += ".tag{" + FlatRules(PopupTag(look, 0.0f), false) + "}";
        css += ".rule{height:1px;decorator:horizontal-gradient(" + RcssColor(look.palette.divider) + " " +
               RcssColor(look.palette.divider & ~kAlphaMask) + ");}";
        css += ".mark{text-align:center;}";
        if (look.palette.shape == BarShape::Glass)
        {
            // Its panel and cards are glass.
            css += ".card{decorator:vertical-gradient(#ffffff17 #ffffff08);background-color:transparent;border-color:" +
                   RcssColor(Faded(look.palette.accent, 0x33)) + ";}";
            css += "#panel{background-color:transparent;decorator:vertical-gradient(" + RcssColor(kGlassTop) + " " +
                   RcssColor(kGlassFoot) + ");border-color:" + RcssColor(Faded(look.palette.accent, kGlassFrame)) + ";}";
        }
        return "<rml><head><style>" + css + "</style></head><body><div id=\"" + kPopupId + "\" class=\"" + kHiddenClass +
               "\"></div></body></rml>";
    }

    std::string PopupRml(const PopupLayout& layout, const BarLook& look, const PopupFaces& faces)
    {
        const BoxStyle panel = PopupPanel(look);
        const auto out       = static_cast<float>(OuterPad(panel));
        auto size = [](float width, float height) {
            return "width:" + Px(static_cast<float>(WholePixels(width))) + ";height:" +
                   Px(static_cast<float>(WholePixels(height))) + ";";
        };
        std::string rml;
        if (panel.outer != 0)
            rml += "<div id=\"outer\" style=\"left:" + Px(-out) + ";top:" + Px(-out) + ";" +
                   size(layout.width + 2.0f * out, layout.height + 2.0f * out) + "\"></div>";
        rml += "<div id=\"panel\" style=\"" + size(layout.width, layout.height) + "\"></div>";
        for (const PopupPiece& piece : layout.pieces)
        {
            const std::string face  = "f" + std::to_string(static_cast<int>(piece.font));
            const uint32_t color    = look.PopupColor(piece.span);
            const std::string place = "left:" + Px(std::round(piece.x)) + ";top:" + Px(std::round(piece.y)) + ";";
            switch (piece.kind)
            {
            case PopupPiece::Kind::Text:
                rml += TextAt(face.c_str(), piece.x, std::round(piece.y) + faces[static_cast<size_t>(piece.font)].drop, color,
                    piece.span.text);
                break;
            case PopupPiece::Kind::ImmuneMark:
                rml += "<div class=\"p " + face + " mark\" style=\"" + BoxAt(piece.x, piece.y, piece.width, piece.height) +
                       "line-height:" + Px(static_cast<float>(WholePixels(piece.height))) + ";color:" + RcssColor(color) +
                       ";\">" + EscapedRml(piece.span.text) + "</div>";
                break;
            case PopupPiece::Kind::Icon:
                rml += "<img class=\"p\" src=\"" + IconSource(piece.icon) + "\" style=\"" + place + "width:" +
                       Px(piece.width) + ";height:" + Px(piece.height) + ";\"/>";
                break;
            case PopupPiece::Kind::Star:
            {
                const int pixels = WholePixels(piece.width);
                rml += "<img class=\"p\" src=\"" + StarSource(pixels) + "\" style=\"" + place +
                       size(static_cast<float>(pixels), static_cast<float>(pixels)) + "\"/>";
                break;
            }
            case PopupPiece::Kind::Rule:
                rml += "<div class=\"p rule\" style=\"" + place + "width:" + Px(static_cast<float>(WholePixels(piece.width))) +
                       ";\"></div>";
                break;
            case PopupPiece::Kind::Card:
                rml += "<div class=\"p card\" style=\"" + BoxAt(piece.x, piece.y, piece.width, piece.height) + "\"></div>";
                break;
            case PopupPiece::Kind::Tag:
                rml += "<div class=\"p tag\" style=\"" + BoxAt(piece.x, piece.y, piece.width, piece.height) +
                       "border-radius:" + Px(PopupTag(look, piece.height).radius) + ";\"></div>";
                break;
            }
        }
        return rml;
    }

    std::string WidgetRml(const WidgetContent& content, const BarLook& look)
    {
        const bool alone = !look.lines.twoLines || content.subtitle.empty();
        std::string rml  = "<div class=\"content\">";
        for (const Icon icon : content.icons)
            rml += "<img class=\"icon\" src=\"" + IconSource(icon) + "\"/>";
        if (content.coin) rml += "<img class=\"coin\" src=\"" + CoinSource(CoinPixels(look.metrics)) + "\"/>";
        rml += "<div class=\"text\">";
        const std::string title = std::string("title") + (content.figures ? " figures" : "") + (alone ? " alone" : "");
        rml += Line(title.c_str(), content.title, content.titleIcons, look);
        if (!alone) rml += Line(content.figures ? "detail fig" : "detail", content.subtitle, content.subtitleIcons, look);
        rml += "</div>";
        if (content.progress)
        {
            const float done = std::clamp(*content.progress, 0.0f, 1.0f) * kPercentWhole;
            rml += std::string("<div class=\"") + (content.progressBeside ? "beside" : "progress") +
                   "\"><div class=\"fill\" style=\"width: " + RcssNumber(done) + "%\"></div></div>";
        }
        return rml + "</div>";
    }
}
