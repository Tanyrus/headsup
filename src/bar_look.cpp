#include "bar_look.h"

#include "argb.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kNeutralColor = 0xFF8D8D8D;
        // A window in a panel's colors: the page's alpha, each step of a card, a control and a hovered one toward the
        // text, the trays toward black, a label's and a muted word's share of the way to the subtitle and the page,
        // and the alphas of the edges and the soft accent, as the Phoenix window has them.
        constexpr uint32_t kMenuPageAlpha = 0xF7, kMenuEdgeAlpha = 0x24, kMenuRaisedAlpha = 0x3D, kMenuSoftAlpha = 0x47;
        constexpr float kCardStep = 0.07f, kInsetShade = 0.4f, kLabelShare = 0.3f, kMutedShare = 0.4f;
        constexpr float kTitleScale      = 1.35f; // a popup title, in popupText
        constexpr float kCoinScale       = 0.75f; // gil's coin, in icons
        constexpr float kFrameRadius     = 3.0f;  // the ornate target frame and hover tint
        constexpr int kLitTarget         = 2;     // a hovered ornate target's tint, in hover tints' alpha
        constexpr float kProgressShare   = 0.05f; // of the bar's height, for a widget's progress bar
        constexpr float kLeastProgress   = 2.0f;
        constexpr float kBesideWidth     = 2.0f;  // a bar beside the text: this many bar heights long
        constexpr float kBesideShare     = 0.14f; // and this tall, of the bar's height
        constexpr float kHelpBarShare    = 0.5f;   // of the screen's width, the widest the help bar gets
        constexpr float kHelpBarMinWidth = 320.0f; // or this on a narrow screen
        constexpr float kHelpPadShare    = 0.5f;   // of the gap: clear above and below the help bar's row
        constexpr int kOuterGap          = 2;          // an ornate popup's outer border
        constexpr uint32_t kCardFill     = 0x12FFFFFF; // a popup's section card: white over the panel, faintly
        constexpr uint32_t kCardEdge     = 0x1EFFFFFF;
        constexpr float kCardRadius      = 6.0f;
        // The pills' section headings' letter-spacing, in their size: the approved mockup's 1.2 px at 10.5 px.
        constexpr float kHeadingTracking = 1.2f / 10.5f;

        const char* Interned(const std::string& name)
        {
            static std::set<std::string> names; // a set's names never move
            return names.insert(name).first->c_str();
        }

        uint32_t AlphaTimes(uint32_t argb, int times)
        {
            const int alpha = std::min(0xFF, Channel(argb, kAlphaShift) * times);
            return (argb & ~kAlphaMask) | static_cast<uint32_t>(alpha) << kAlphaShift;
        }
    }

    uint32_t BarLook::BarColor(const Span& span) const
    {
        return span.tone == Tone::Normal ? palette.title : PopupColor(span);
    }

    uint32_t BarLook::PopupColor(const Span& span) const
    {
        switch (span.tone)
        {
        case Tone::Normal: return palette.text;
        case Tone::Dim: return palette.subtitle;
        case Tone::Heading: return palette.heading;
        case Tone::Tag: return palette.tagText;
        case Tone::Neutral: return kNeutralColor;
        case Tone::Weak: return ToArgb(settings.weakBadgeColor);
        case Tone::Resist: return ToArgb(settings.resistBadgeColor);
        case Tone::Immune: return ToArgb(settings.immuneBadgeColor);
        case Tone::Own: return span.argb;
        }
        return palette.text;
    }

    const BarPalette& PaletteFor(const Settings& settings, const GameSkinLook* skin)
    {
        if (settings.barTheme == BarTheme::GameWindow) return skin != nullptr ? skin->palette : PaletteOf(kGameArtFallback);
        return settings.barTheme == BarTheme::Custom ? settings.customPalette : PaletteOf(settings.barTheme);
    }

    bool UsesGameArt(const Settings& settings)
    {
        return settings.barTheme == BarTheme::GameWindow ||
               (settings.barTheme == BarTheme::Custom && settings.customPalette.shape == BarShape::Window);
    }

    MenuTheme MenuThemeFor(const Settings& settings, const GameSkinLook* skin)
    {
        if (settings.barTheme == BarTheme::Aurora) return MenuTheme::Aurora;
        if (skin != nullptr && UsesGameArt(settings)) return MenuTheme::GameWindow;
        return MenuTheme::Phoenix;
    }

    MenuColors MenuColorsFor(const Settings& settings, const GameSkinLook* skin)
    {
        switch (MenuThemeFor(settings, skin))
        {
        case MenuTheme::Aurora: return kAuroraMenuColors;
        case MenuTheme::GameWindow: return skin->menu;
        case MenuTheme::Phoenix: break;
        }
        if (settings.barTheme == BarTheme::PhoenixRedOrnate || settings.barTheme == BarTheme::PhoenixRedPills)
            return kPhoenixMenuColors;
        return MenuColorsFrom(PaletteFor(settings, skin));
    }

    MenuColors MenuColorsFrom(const BarPalette& p)
    {
        const uint32_t page = p.panelBottom | kAlphaMask;
        const auto toward = [](uint32_t from, uint32_t to, float share) {
            const auto channel = [&](int shift) {
                const float a = Channel(from, shift), b = Channel(to, shift);
                return static_cast<uint8_t>(std::lround(a + (b - a) * share));
            };
            return Opaque(channel(kRedShift), channel(kGreenShift), channel(kBlueShift));
        };
        return MenuColors{WithAlpha(page, kMenuPageAlpha), toward(page, p.text, kCardStep), toward(page, p.text, 2.0f * kCardStep),
            toward(page, p.text, 3.0f * kCardStep), toward(page, kBlack, kInsetShade), WithAlpha(p.subtitle, kMenuEdgeAlpha),
            WithAlpha(p.subtitle, kMenuRaisedAlpha), p.text, p.title, toward(p.text, p.subtitle, kLabelShare),
            toward(p.subtitle, page, kMutedShare), p.accent, p.accentEnd, WithAlpha(p.accent, kMenuSoftAlpha), p.accentEnd};
    }

    SkinFrame FrameAsSet(const Settings& settings, SkinFrame frame)
    {
        if (!settings.gameWindowBorder) frame.edge = 0;
        return frame;
    }

    const char* WidgetTitleFont(const Settings& settings, BarShape shape, const WidgetOptions& options)
    {
        return options.titleFont.empty() ? BarTitleFont(settings, shape) : Interned(options.titleFont);
    }

    const char* WidgetTextFont(const Settings& settings, const WidgetOptions& options)
    {
        return options.textFont.empty() ? BarTextFont(settings) : Interned(options.textFont);
    }

    int WidgetTextSize(const Settings& settings, const WidgetOptions& options)
    {
        return options.textSize == kPanelTextSize ? settings.barTextSize : options.textSize;
    }

    float WidgetPadding(const BarMetrics& metrics, const WidgetOptions& options)
    {
        return options.padding == kThemePadding ? metrics.padding : static_cast<float>(options.padding);
    }

    const char* BarTitleFont(const Settings& settings, BarShape shape)
    {
        return settings.barTitleFont.empty() ? TitleFont(shape) : Interned(settings.barTitleFont);
    }

    const char* BarTextFont(const Settings& settings)
    {
        return settings.barTextFont.empty() ? kDetailFont : Interned(settings.barTextFont);
    }

    TextFace BarLook::TitleFace() const { return TextFace{titleFont, static_cast<float>(lines.title)}; }

    TextFace BarLook::TitleFace(const WidgetContent& content) const
    {
        return content.figures ? TextFace{detailFont, static_cast<float>(lines.title)} : TitleFace();
    }

    TextFace BarLook::DetailFace() const { return TextFace{detailFont, static_cast<float>(lines.detail)}; }

    TextFace BarLook::MeterFace() const { return TextFace{detailFont, static_cast<float>(lines.side)}; }

    TextFace BarLook::FaceOf(PopupFont font) const
    {
        switch (font)
        {
        case PopupFont::Title: return TextFace{titleFont, metrics.popupText * kTitleScale};
        case PopupFont::Heading:
        {
            const auto pixels  = static_cast<float>(WholePixels(metrics.heading));
            const int tracking = ornate ? 0 : static_cast<int>(std::lround(pixels * kHeadingTracking));
            return TextFace{titleFont, metrics.heading, tracking};
        }
        case PopupFont::Pin: return TextFace{titleFont, metrics.heading};
        case PopupFont::Text: break;
        }
        return TextFace{detailFont, metrics.popupText};
    }

    float BarLook::Leading(const WidgetContent& content) const
    {
        return static_cast<float>(content.icons.size()) * (metrics.icon + metrics.gap) +
               (content.coin ? static_cast<float>(CoinPixels(metrics)) + metrics.gap : 0.0f);
    }

    std::optional<BoxStyle> BarLook::BoxOf(BarWidget widget, bool lit) const
    {
        const bool isTarget = widget == BarWidget::Target;
        if (ornate)
        {
            if (!isTarget && !lit) return std::nullopt;
            // The target's frame always has the tint, so a hovered one has more of it.
            const uint32_t fill = isTarget && lit ? AlphaTimes(palette.highlight, kLitTarget) : palette.highlight;
            return BoxStyle{.radius = kFrameRadius, .top = fill, .bottom = fill, .border = isTarget ? palette.accent : 0u};
        }
        const bool flat = widget == BarWidget::Zone || widget == BarWidget::EarthTime;
        if (flat && !lit) return std::nullopt;
        const uint32_t fill = lit ? palette.highlight : palette.pill;
        return BoxStyle{.radius = metrics.pillRadius, .top = fill, .bottom = fill,
            .border = isTarget || lit ? palette.accent : palette.pillBorder};
    }

    int CoinPixels(const BarMetrics& metrics) { return WholePixels(metrics.icon * kCoinScale); }

    float ProgressHeight(const BarMetrics& metrics, bool beside)
    {
        return std::max(kLeastProgress, std::round(metrics.height * (beside ? kBesideShare : kProgressShare)));
    }

    float BesideWidth(const BarMetrics& metrics) { return std::round(metrics.height * kBesideWidth); }

    float UnderLine(const BarPalette& palette) { return palette.underLine != 0 ? 1.0f : 0.0f; }
    float BarBottom(const BarPalette& palette, const BarMetrics& metrics) { return metrics.height + UnderLine(palette); }
    float HelpBarTop(const BarPalette& palette, const BarMetrics& metrics)
    {
        return BarBottom(palette, metrics) + std::round(metrics.gap);
    }
    float HelpBarLeft(const BarMetrics& metrics) { return std::round(metrics.gap); }
    float HelpBarPad(const BarMetrics& metrics) { return std::round(metrics.gap * kHelpPadShare); }
    TextFace HelpFace(const BarLook& look) { return TextFace{look.detailFont, static_cast<float>(look.lines.title)}; }
    float HelpBarWidth(float screenWidth)
    {
        return std::min(screenWidth, std::max(kHelpBarMinWidth, std::floor(screenWidth * kHelpBarShare)));
    }

    BoxStyle PopupPanel(const BarLook& look)
    {
        const BarPalette& p = look.palette;
        return BoxStyle{.radius = look.metrics.panelRadius, .top = p.panelTop, .bottom = p.panelBottom,
            .border = p.panelBorder, .outer = p.panelOuter, .outerGap = kOuterGap};
    }

    BoxStyle PopupCard(const BarLook& look)
    {
        return BoxStyle{.radius = std::max(look.metrics.panelRadius, kCardRadius), .top = kCardFill, .bottom = kCardFill,
            .border = kCardEdge};
    }

    BoxStyle PopupTag(const BarLook& look, float height)
    {
        const BarPalette& p = look.palette;
        return BoxStyle{.radius = height / 2.0f, .top = p.tagFill, .bottom = p.tagFill,
            .border = look.ornate ? p.tagBorder : 0u};
    }

    BoxStyle HelpBarPanel(const BarLook& look)
    {
        const BarPalette& p = look.palette;
        return BoxStyle{.radius = look.metrics.panelRadius, .top = p.panelTop, .bottom = p.panelBottom,
            .border = p.panelBorder};
    }

}
