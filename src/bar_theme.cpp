#include "bar_theme.h"

#include "ini_text.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <vector>

namespace headsup
{
    namespace
    {
        constexpr const char* kThemeSection = "theme";
        constexpr const char* kShapeKey = "Shape", *kBorderKey = "Border", *kGradientKey = "Gradient";
        constexpr const char* kTrueText = "true", *kFalseText = "false";
        constexpr int kHex = 16;

        std::optional<int> NumberIn(const std::optional<std::string>& text, int base)
        {
            int value = 0;
            if (!text || text->empty()) return std::nullopt;
            const char* end = text->data() + text->size();
            const auto [at, problem] = std::from_chars(text->data(), end, value, base);
            return problem == std::errc{} && at == end ? std::optional<int>(value) : std::nullopt;
        }

        constexpr float kTitleShare = 0.40f, kDetailShare = 0.33f, kPopupTextShare = 0.39f; // of H

        // Alpha is the first byte: 0x1A is 10%, 0x33 20%, 0x59 35%, 0x80 50% and 0xF7 97%.
        constexpr BarPalette kPalettes[kFixedThemeCount] = {
            {.shape = BarShape::Ornate, .stripTop = 0xFF2A2219, .stripMiddle = 0xFF15110C, .stripBottom = 0xFF0B0907,
                .bottomLine = 0xFFC9A24A, .underLine = 0xFF3D2F14, .below = 0x59C9A24A, .accent = 0xFFC9A24A, .accentEnd = 0xFFEFE3C6,
                .pill = 0, .pillBorder = 0, .title = 0xFFEFE3C6, .subtitle = 0xFFA8936D, .divider = 0xFF8A6D2C,
                .panelTop = 0xF71C160F, .panelBottom = 0xF70C0A08, .panelBorder = 0xFFC9A24A, .panelOuter = 0xFF3D2F14,
                .text = 0xFFE4D8BD, .heading = 0xFFC9A24A, .tagFill = 0, .tagBorder = 0xFF6B5422, .tagText = 0xFFD8C49A,
                .highlight = 0x1AC9A24A},
            {.shape = BarShape::Pills, .stripTop = 0xFF2B2B30, .stripMiddle = 0xFF212125, .stripBottom = 0xFF17171A,
                .bottomLine = 0xFF000000, .underLine = 0, .below = 0x80000000, .accent = 0xFF5B8DEF, .accentEnd = 0xFF9CC2FF,
                .pill = 0xFF222226, .pillBorder = 0xFF3A3A42, .title = 0xFFE9E9EE, .subtitle = 0xFF8E8E99, .divider = 0,
                .panelTop = 0xFF1D1D21, .panelBottom = 0xFF1D1D21, .panelBorder = 0xFF3A3A42, .panelOuter = 0,
                .text = 0xFFDEDEE5, .heading = 0xFF8E8E99, .tagFill = 0xFF2C2C33, .tagBorder = 0, .tagText = 0xFFC5C5D0,
                .highlight = 0xFF2A2A30},
            {.shape = BarShape::Ornate, .stripTop = 0xFF2B1A1A, .stripMiddle = 0xFF180E0E, .stripBottom = 0xFF0F0909,
                .bottomLine = 0xFFC55151, .underLine = 0xFF3A1B1B, .below = 0x59C55151, .accent = 0xFFC55151, .accentEnd = 0xFFE06F5E,
                .pill = 0, .pillBorder = 0, .title = 0xFFFFF8F8, .subtitle = 0xFFD2ABAB, .divider = 0xFF8A4545,
                .panelTop = 0xF7291C1C, .panelBottom = 0xF7180E0E, .panelBorder = 0xFFC55151, .panelOuter = 0xFF3A1B1B,
                .text = 0xFFF3E6E6, .heading = 0xFFD2ABAB, .tagFill = 0, .tagBorder = 0xFF6E3434, .tagText = 0xFFE8C9C9,
                .highlight = 0x1AC55151},
            {.shape = BarShape::Pills, .stripTop = 0xFF2E1C1C, .stripMiddle = 0xFF231515, .stripBottom = 0xFF180E0E,
                .bottomLine = 0xFF000000, .underLine = 0, .below = 0x80000000, .accent = 0xFFC55151, .accentEnd = 0xFFE06F5E,
                .pill = 0xFF291C1C, .pillBorder = 0x33D2ABAB, .title = 0xFFFFF8F8, .subtitle = 0xFF8A6B6B, .divider = 0,
                .panelTop = 0xFF180E0E, .panelBottom = 0xFF180E0E, .panelBorder = 0x33D2ABAB, .panelOuter = 0,
                .text = 0xFFFFF8F8, .heading = 0xFFD2ABAB, .tagFill = 0xFF321F1F, .tagBorder = 0, .tagText = 0xFFE8C9C9,
                .highlight = 0xFF3A2525},
            {.shape = BarShape::Glass, .stripTop = 0xE6131C2B, .stripMiddle = 0xE60E1522, .stripBottom = 0xE60A0F18,
                .bottomLine = 0xFF1B2636, .underLine = 0, .below = 0x595EE6FF, .accent = 0xFF5EE6FF, .accentEnd = 0xFFB48CFF,
                .pill = 0x1AFFFFFF, .pillBorder = 0x24B8D8FF, .title = 0xFFEAF2FF, .subtitle = 0xFF8FA3BF, .divider = 0,
                .panelTop = 0xF2131C2B, .panelBottom = 0xF20A0F18, .panelBorder = 0x595EE6FF, .panelOuter = 0,
                .text = 0xFFE3ECFA, .heading = 0xFF5EE6FF, .tagFill = 0x265EE6FF, .tagBorder = 0, .tagText = 0xFFCFE9FF,
                .highlight = 0x335EE6FF},
        };
    }

    const BarPalette& PaletteOf(BarTheme theme)
    {
        return kPalettes[std::clamp(static_cast<int>(theme), 0, kFixedThemeCount - 1)];
    }

    BarMetrics MetricsFor(int barHeight, BarShape shape, int barText, int popupText)
    {
        const float h = static_cast<float>(std::clamp(barHeight, kMinBarHeight, kMaxBarHeight));
        // A set popup size stands for the height whose Auto popup text it is, so the whole popup scales together.
        const float u = popupText > kAutoPopupTextSize ? static_cast<float>(popupText) / kPopupTextShare : h;
        const bool setBar    = barText > kAutoBarTextSize;
        const float title    = setBar ? static_cast<float>(barText) : kTitleShare * h;
        // At Auto exactly as before: a share of 0.33 H taken through the title could round a pixel the other way.
        const float subtitle = setBar ? title * kDetailShare / kTitleShare : kDetailShare * h;
        return BarMetrics{.height = h, .title = title, .subtitle = subtitle, .titleSet = setBar,
            .icon = 0.50f * h, .padding = 0.33f * h, .gap = 0.17f * h, .cap = 0.64f * h, .diamond = 0.22f * h,
            .pillHeight = 0.80f * h, .pillRadius = 0.17f * h, .below = 0.28f * h, .popupText = kPopupTextShare * u,
            .heading = 0.33f * u, .popupPadding = 0.36f * u, .rowGap = 0.11f * u,
            .panelRadius = (shape == BarShape::Ornate ? 0.11f : 0.28f) * u};
    }

    const char* TitleFont(BarShape shape) { return shape == BarShape::Ornate ? kMarcellusSC : kDetailFont; }

    std::string ThemeFileText(const BarPalette& palette)
    {
        std::vector<KeyText> values;
        for (const PaletteColor& color : kPaletteColors)
        {
            char hex[16];
            std::snprintf(hex, sizeof(hex), "%08x", palette.*color.field);
            values.push_back({color.name, hex});
        }
        values.push_back({kShapeKey, kBarShapeNames[static_cast<int>(palette.shape)]});
        values.push_back({kBorderKey, std::to_string(palette.border)});
        values.push_back({kGradientKey, palette.gradient ? kTrueText : kFalseText});
        return WithIniValues("", kThemeSection, values);
    }

    std::optional<BarPalette> PaletteFromThemeFile(std::string_view file)
    {
        BarPalette palette = PaletteOf(BarTheme::PhoenixGold);
        bool any           = false;
        for (const PaletteColor& color : kPaletteColors)
            if (const std::optional<std::string> text = IniValue(file, kThemeSection, color.name))
            {
                uint32_t value = 0;
                const char* end = text->data() + text->size();
                const auto [at, problem] = std::from_chars(text->data(), end, value, kHex);
                if (problem != std::errc{} || at != end || text->empty()) continue;
                palette.*color.field = value;
                any                  = true;
            }
        const std::optional<std::string> shape = IniValue(file, kThemeSection, kShapeKey);
        for (int i = 0; shape && i < kBarShapeCount; ++i)
            if (*shape == kBarShapeNames[i]) palette.shape = static_cast<BarShape>(i);
        if (const std::optional<int> border = NumberIn(IniValue(file, kThemeSection, kBorderKey), 10))
            palette.border = std::clamp(*border, 0, kMaxStripBorder);
        if (const std::optional<std::string> gradient = IniValue(file, kThemeSection, kGradientKey))
            palette.gradient = *gradient != kFalseText;
        return any ? std::optional<BarPalette>(palette) : std::nullopt;
    }
}
