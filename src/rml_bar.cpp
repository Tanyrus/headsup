#include "rml_bar.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr float kFitSteps = 10.0f; // a fitted text's room is kept in tenths of a pixel
        constexpr int kFewLetters = 3;     // the least of the target's name worth its room on the bar
        constexpr const char* kDocumentName      = "top panel";
        constexpr const char* kPopupDocumentName = "top panel popup";

        void Set(Rml::Element* element, Rml::PropertyId property, std::optional<float>& was, float value)
        {
            if (element == nullptr || was == value) return;
            was = value;
            element->SetProperty(property, Rml::Property(value, Rml::Unit::PX));
        }
    }

    void RmlBar::Close()
    {
        if (m_Document != nullptr) m_Document->Close();
        m_Document = nullptr;
        m_Visible  = false;
        m_Shown    = {};
        m_Dividers = {};
        m_RightCap = nullptr;
        m_Help     = nullptr;
        m_HelpRml.clear();
        ClosePopup();
    }

    void RmlBar::ClosePopup()
    {
        if (m_PopupDocument != nullptr) m_PopupDocument->Close();
        m_PopupDocument = nullptr;
        m_Popup         = nullptr;
        m_PopupMarked   = false;
        m_PopupLeft = m_PopupTop = m_PopupWidth = m_PopupHeight = Px{};
    }

    const RmlBar::FaceLine& RmlBar::FaceLineOf(RmlHost& host, const char* family, int pixels)
    {
        for (const FaceLine& known : m_FaceLines)
            if (known.pixels == pixels && known.family == family) return known;
        const Rml::FontMetrics metrics = host.Metrics(family, pixels);
        const LineInk ink              = m_Gdi.FontInk(family, pixels, kDetailGlyphs);
        int width = 0, height = 0;
        m_Gdi.Extent("H", family, pixels, false, width, height);
        FaceLine line{family, pixels, static_cast<float>(height), ink.top, ink.bottom,
            WholePixels(metrics.ascent + metrics.descent)};
        // RmlUi puts a line's baseline its font's ascent under half of what the line's height has over the font's.
        const float baseline =
            (static_cast<float>(line.rmlHeight) - (metrics.ascent + metrics.descent)) / 2.0f + metrics.ascent;
        line.drop = std::round(ink.baseline - baseline);
        return m_FaceLines.emplace_back(line);
    }

    void RmlBar::Release()
    {
        Close();
        m_Gdi.Release();
    }

    float RmlBar::Width(RmlHost& host, const std::string& text, const TextFace& face) const
    {
        return host.TextWidth(text, face.family, WholePixels(face.size), static_cast<float>(face.tracking));
    }

    float RmlBar::SpansWidth(RmlHost& host, const std::vector<Span>& spans, const TextFace& face) const
    {
        float width = 0.0f;
        for (const Span& s : spans)
            width += Width(host, s.text, face);
        return width;
    }

    float RmlBar::LineWidth(RmlHost& host, const std::vector<Span>& spans, const std::vector<Icon>& icons,
        const TextFace& face) const
    {
        if (icons.empty()) return SpansWidth(host, spans, face);
        const auto n = static_cast<float>(icons.size());
        return SpansWidth(host, spans, face) + kLineIconGap * 2.0f + n * std::round(face.size) + (n - 1.0f) * kLineIconGap;
    }

    char RmlBar::WidestDigitOf(RmlHost& host, const TextFace& face)
    {
        const int pixels = WholePixels(face.size);
        for (const FaceDigit& known : m_WidestDigits)
            if (known.pixels == pixels && known.family == face.family) return known.digit;
        const char digit = WidestDigit([&](char d) { return Width(host, std::string(1, d), face); });
        m_WidestDigits.push_back(FaceDigit{face.family, pixels, digit});
        return digit;
    }

    float RmlBar::SteadyWidth(RmlHost& host, std::vector<Span> spans, const std::vector<Icon>& icons, const TextFace& face)
    {
        // Never less than the line as it reads now, should a pair of its real figures kern wider.
        const float shown = LineWidth(host, spans, icons, face);
        const char digit  = WidestDigitOf(host, face);
        for (Span& s : spans)
            s.text = SteadyFigures(s.text, digit);
        return std::max(shown, LineWidth(host, spans, icons, face));
    }

    const std::string& RmlBar::Fitted(RmlHost& host, Memo<FitKey, std::string>& memo, std::string text,
        const TextFace& face, float room)
    {
        // Rounded down, so the cut text never needs more than the room it was given.
        const long tenths = std::lround(std::floor(room * kFitSteps));
        return memo.Get(FitKey{std::move(text), face.family, WholePixels(face.size), tenths}, [&](const FitKey& key) {
            return FitText(key.text, static_cast<float>(key.room) / kFitSteps,
                [&](const std::string& s) { return Width(host, s, face); });
        });
    }

    BarLinePlace RmlBar::PlaceLine(RmlHost& host, const char* family, int pixels, const char* glyphs, float top,
        float aloneTop)
    {
        BarLinePlace line;
        if (pixels <= 0) return line;
        const LineInk ink = m_Gdi.FontInk(family, pixels, glyphs);
        int width = 0, height = 0;
        m_Gdi.Extent("H", family, pixels, false, width, height);
        const Rml::FontMetrics metrics = host.Metrics(family, pixels);
        line.height                    = WholePixels(metrics.ascent + metrics.descent);
        // RmlUi puts a line's baseline its font's ascent under half of what the line's height has over the font's.
        const float baseline =
            (static_cast<float>(line.height) - (metrics.ascent + metrics.descent)) / 2.0f + metrics.ascent;
        const float shift = ink.baseline - baseline;
        line.top          = std::round(top) + shift;
        line.aloneTop     = std::round(aloneTop) + shift;
        // The icons after a line's text are centered on the line GDI draws, as tall as its letters' size.
        const float icon = std::round(static_cast<float>(pixels));
        line.iconDrop    = std::round(std::round(top) + (static_cast<float>(height) - icon) / 2.0f) - std::round(line.top);
        return line;
    }

    const RmlBar::Rows& RmlBar::RowsFor(RmlHost& host, const BarMetrics& m, BarShape shape, int barText,
        const char* titleFont, const char* detailFont)
    {
        return m_Rows.Get(LinesKey{m.height, shape, barText, titleFont, detailFont}, [&](const LinesKey&) {
            Rows rows;
            rows.lines = FitTextRow(m, [&](bool title, int pixels) {
                return title ? m_Gdi.FontInk(titleFont, pixels, kTitleGlyphs)
                             : m_Gdi.FontInk(detailFont, pixels, kDetailGlyphs);
            });
            const TextRow& t = rows.lines;
            rows.place.title   = PlaceLine(host, titleFont, t.title, kTitleGlyphs, t.titleY, t.aloneY);
            rows.place.figures = PlaceLine(host, detailFont, t.title, kTitleGlyphs, t.titleY, t.aloneY);
            rows.place.detail  = PlaceLine(host, detailFont, t.detail, kDetailGlyphs, t.detailY, t.detailY);
            // A line box is as tall for any text in its font: the caption's and the help's, as GDI draws them.
            int width = 0, caption = 0, help = 0;
            m_Gdi.Extent("H", titleFont, t.title, false, width, caption);
            m_Gdi.Extent("H", detailFont, t.title, false, width, help);
            rows.helpBarHeight = std::round(static_cast<float>(std::max(caption, help)) + 2.0f * HelpBarPad(m));
            return rows;
        });
    }

    bool RmlBar::Document(RmlHost& host, const BarLook& look, BarTheme theme, const BarTextPlace& place)
    {
        DocumentKey key{theme, look.metrics, look.row, look.lines, place, look.titleFont, look.detailFont};
        if (m_Document != nullptr && key == m_DocumentKey) return true;
        Close();
        m_Document = host.Bar()->LoadDocumentFromMemory(BarDocument(look, theme, place), kDocumentName);
        if (m_Document == nullptr) return false;
        m_DocumentKey = std::move(key);
        for (int w = 0; w < kBarWidgetCount; ++w)
            if (OnTheBar(static_cast<BarWidget>(w)))
                m_Shown[w].element = m_Document->GetElementById(WidgetId(static_cast<BarWidget>(w)));
        for (int d = 0; d < kDividerCount; ++d)
            m_Dividers[d].element = m_Document->GetElementById(DividerId(d));
        m_RightCap     = m_Document->GetElementById(kRightCapId);
        m_RightCapLeft = Px{};
        m_Help         = m_Document->GetElementById(kHelpId);
        m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        m_Visible = true;
        return true;
    }

    BarLayout RmlBar::PlaceWidgets(RmlHost& host, const BarLook& look, const BarState& state, float screenWidth,
        Widgets& widgets)
    {
        const BarMetrics& m = look.metrics;
        std::array<float, kBarWidgetCount> natural{};
        for (int i = 0; i < kBarWidgetCount; ++i)
        {
            WidgetContent& c = widgets.contents[i];
            c                = BarWidgetContent(static_cast<BarWidget>(i), state, look.settings);
            if (!c.shown) continue;
            if (!look.lines.twoLines) OneLine(c, look.settings.barSubtitle);
            widgets.hasPopup[i] = c.hasPopup;
            Shown& shown        = m_Shown[i];
            if (!shown.measured || !(shown.content == c))
            {
                shown.content  = c;
                shown.measured = true;
                shown.natural  = 2.0f * m.padding + look.Leading(c) +
                                std::max(SteadyWidth(host, c.title, c.titleIcons, look.TitleFace(c)),
                                    look.lines.twoLines ? SteadyWidth(host, c.subtitle, c.subtitleIcons, look.DetailFace())
                                                        : 0.0f) +
                                (c.progress && c.progressBeside ? m.gap + BesideWidth(m) : 0.0f);
            }
            natural[i] = shown.natural;
        }
        // The target goes when its room cannot hold its padding, its icons and the start of its name.
        WidgetContent& target = widgets.contents[static_cast<int>(BarWidget::Target)];
        const float targetMinimum =
            2.0f * m.padding + look.Leading(target) +
            (target.title.empty() ? 0.0f
                                  : Width(host, Shortest(target.title.front().text, kFewLetters), look.TitleFace()));
        const BarLayout layout = LayoutBar(screenWidth, m, look.palette.shape, natural, targetMinimum,
            look.settings.barSections, look.settings.barSeparators);
        if (layout.targetWidth > 0.0f && layout.targetWidth < natural[static_cast<int>(BarWidget::Target)] &&
            !target.title.empty())
        {
            // Each line's icons keep their room; its text is cut.
            const float room   = layout.targetWidth - 2.0f * m.padding - look.Leading(target);
            std::string& title = target.title.front().text;
            title = Fitted(host, m_TitleFit, std::move(title), look.TitleFace(),
                room - LineWidth(host, {}, target.titleIcons, look.TitleFace()));
            if (look.lines.twoLines && !target.subtitle.empty())
            {
                std::string& detail = target.subtitle.front().text;
                detail = Fitted(host, m_SubtitleFit, std::move(detail), look.DetailFace(),
                    room - LineWidth(host, {}, target.subtitleIcons, look.DetailFace()));
            }
        }
        return layout;
    }

    void RmlBar::ShowWidgets(const BarLook& look, const BarLayout& layout, const Widgets& widgets,
        const BarInput& input, float screenWidth)
    {
        const BarMetrics& m = look.metrics;
        for (int w = 0; w < kBarWidgetCount; ++w)
        {
            Shown& shown = m_Shown[w];
            if (shown.element == nullptr) continue;
            const auto widget   = static_cast<BarWidget>(w);
            const BarRect* rect = RectOf(layout, widget);
            shown.element->SetClass(kHiddenClass, rect == nullptr);
            if (rect == nullptr) continue;
            // A plain ornate widget keeps the divider's clear space either side as its margin.
            const float inset = look.ornate && widget != BarWidget::Target ? DiamondClearance(m) : 0.0f;
            Set(shown.element, Rml::PropertyId::Left, shown.left.value, rect->x);
            Set(shown.element, Rml::PropertyId::Width, shown.width.value, rect->width - 2.0f * inset);
            shown.element->SetClass(kLitClass,
                widgets.hasPopup[w] && (input.Hovered() == widget || input.Open() == widget));
            const WidgetContent& content = widgets.contents[w];
            if (shown.marked && shown.drawn == content) continue;
            shown.drawn  = content;
            shown.marked = true;
            shown.element->SetInnerRML(WidgetRml(content, look));
        }
        const auto diamond = static_cast<float>(WholePixels(m.diamond));
        for (size_t d = 0; d < m_Dividers.size(); ++d)
        {
            Divider& divider = m_Dividers[d];
            if (divider.element == nullptr) continue;
            const bool shown = d < layout.dividers.size();
            divider.element->SetClass(kHiddenClass, !shown);
            if (shown)
                Set(divider.element, Rml::PropertyId::Left, divider.left.value,
                    std::round(layout.dividers[d] - diamond / 2.0f));
        }
        const auto cap = static_cast<float>(WholePixels(m.diamond * kCapScale));
        Set(m_RightCap, Rml::PropertyId::Left, m_RightCapLeft.value, std::round(screenWidth - m.cap / 2.0f - cap / 2.0f));
    }

    BarRect RmlBar::ShowHelpBar(RmlHost& host, const BarLook& look, const Rows& rows, const BarState& state, float top,
        float offsetX, float screenWidth, bool& cut)
    {
        cut = false;
        HelpBarParts parts;
        if (look.settings.barWidgets[static_cast<int>(BarWidget::GameHelp)] && state.strip &&
            !(state.strip->caption.empty() && state.strip->help.empty()))
        {
            const BarMetrics& m        = look.metrics;
            const TextFace captionFace = look.TitleFace(), helpFace = HelpFace(look);
            parts.bag                  = StripBagFill(state);
            const float bagWidth       = parts.bag ? Width(host, parts.bag->text, captionFace) : 0.0f;
            const float left           = HelpBarLeft(m);
            const float room =
                HelpBarWidth(screenWidth) - left - 2.0f * m.padding - (bagWidth > 0.0f ? bagWidth + m.gap : 0.0f);
            const HelpBarText& text = m_HelpBar.Get(
                HelpBarKey{*state.strip, captionFace.family, helpFace.family, WholePixels(captionFace.size),
                    WholePixels(helpFace.size), std::lround(std::floor(room * kFitSteps)),
                    std::lround(m.padding * kFitSteps)},
                [&](const HelpBarKey& k) {
                    return HelpBarLine(k.strip, static_cast<float>(k.room) / kFitSteps,
                        static_cast<float>(k.gap) / kFitSteps,
                        [&](const std::string& s) { return Width(host, s, captionFace); },
                        [&](const std::string& s) { return Width(host, s, helpFace); });
                });
            cut           = text.cut;
            parts.caption = text.caption;
            parts.help    = text.help;
            parts.row     = PlaceHelpBar(left, top, rows.helpBarHeight, m.padding, m.gap,
                Width(host, parts.caption, captionFace), bagWidth, Width(host, parts.help, helpFace));
            // Where the player moved it from its place under the top bar.
            for (float* x : {&parts.row.box.x, &parts.row.captionX, &parts.row.bagX, &parts.row.helpX})
                *x += offsetX;
        }
        std::string rml = HelpBarRml(parts, look, rows.place);
        if (m_Help != nullptr && rml != m_HelpRml)
        {
            m_Help->SetInnerRML(rml);
            m_HelpRml = std::move(rml);
        }
        return parts.row.box;
    }

    bool RmlBar::PopupDocumentFor(RmlHost& host, const BarLook& look, BarTheme theme)
    {
        PopupDocumentKey key{theme, look.metrics, look.titleFont, look.detailFont};
        if (m_PopupDocument != nullptr && key == m_PopupDocumentKey) return true;
        ClosePopup();
        for (int f = 0; f < kPopupFontCount; ++f)
        {
            const TextFace face  = look.FaceOf(static_cast<PopupFont>(f));
            const int pixels     = WholePixels(face.size);
            const FaceLine& line = FaceLineOf(host, face.family, pixels);
            m_PopupFaces[f]      = PopupFace{face.family, pixels, line.rmlHeight, face.tracking, line.drop};
        }
        m_PopupDocument =
            host.Overlay()->LoadDocumentFromMemory(PopupDocument(look, m_PopupFaces, theme), kPopupDocumentName);
        if (m_PopupDocument == nullptr) return false;
        m_PopupDocumentKey = std::move(key);
        m_Popup            = m_PopupDocument->GetElementById(kPopupId);
        m_PopupDocument->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        return m_Popup != nullptr;
    }

    BarRect RmlBar::ShowPopup(RmlHost& host, const BarLook& look, BarTheme theme, const BarState& state,
        const BarLayout& layout, float screenWidth, float screenHeight, const BarInput& input, bool hidden)
    {
        const std::optional<BarWidget> open = input.Open();
        const BarRect* anchor               = open ? RectOf(layout, *open) : nullptr;
        if (anchor == nullptr)
        {
            if (m_Popup != nullptr)
            {
                m_Popup->SetClass(kHiddenClass, true);
                m_Popup->SetClass(kOpenClass, false);
            }
            return BarRect{};
        }
        if (!PopupDocumentFor(host, look, theme)) return BarRect{};
        const BarPalette& p = look.palette;
        const BarMetrics& m = look.metrics;
        auto measure = [&](const Span& s, PopupFont f) {
            const TextFace face  = look.FaceOf(f);
            const FaceLine& line = FaceLineOf(host, face.family, WholePixels(face.size));
            return TextSize{Width(host, s.text, face), line.height, line.inkTop, line.inkBottom, 0.0f};
        };
        // The ornate panel's outer border and the line under the strip count: the gap is from them. The help bar's
        // popup goes under the help bar's box.
        const auto out        = static_cast<float>(OuterPad(PopupPanel(look)));
        const float barBottom = *open == BarWidget::GameHelp ? anchor->y + anchor->height : BarBottom(p, m);
        PopupKey key{BarPopupContent(*open, state, look.settings), input.Pinned() == open, m.height, m.popupText,
            p.shape, PopupRoom(barBottom, screenHeight, m.gap, out)};
        bool fresh            = false;
        const PopupLayout& pl = m_PopupLayout.Get(std::move(key), [&](const PopupKey& k) {
            fresh           = true;
            PopupLayout cut = LayoutPopup(k.content, m, p.shape, k.pinned, measure);
            CutPopup(cut, k.room, m.popupPadding);
            return cut;
        });
        const BarRect rect =
            PlacePopup(*anchor, barBottom, pl.width, pl.height, screenWidth, screenHeight, m.gap, out);
        if (fresh || !m_PopupMarked)
        {
            m_Popup->SetInnerRML(PopupRml(pl, look, m_PopupFaces));
            m_PopupMarked = true;
        }
        Set(m_Popup, Rml::PropertyId::Left, m_PopupLeft.value, std::round(rect.x));
        Set(m_Popup, Rml::PropertyId::Top, m_PopupTop.value, std::round(rect.y));
        Set(m_Popup, Rml::PropertyId::Width, m_PopupWidth.value, rect.width);
        Set(m_Popup, Rml::PropertyId::Height, m_PopupHeight.value, rect.height);
        m_Popup->SetClass(kHiddenClass, hidden);
        m_Popup->SetClass(kOpenClass, !hidden);
        return rect;
    }

    void RmlBar::Update(RmlHost& host, const BarState& state, const Settings& settings, float screenWidth,
        float screenHeight, BarInput& input, bool hidden)
    {
        m_HelpReach.reset();
        m_BarBottom = 0.0f;
        if (!BarOn(settings) || !host.Running() || screenWidth <= 0.0f)
        {
            Close();
            m_TitlePixels = 0;
            input.Frame(BarLayout{}, {});
            input.SetPopup(BarRect{});
            return;
        }
        const BarPalette& p = PaletteOf(settings.barTheme);
        const BarMetrics m  = MetricsFor(settings.barHeight, p.shape, settings.barTextSize, settings.popupTextSize);
        // The player's fonts where RmlUi can read them, the theme's where it cannot.
        const char* titleFont = BarTitleFont(settings, p.shape);
        if (!host.EnsureFont(titleFont)) titleFont = TitleFont(p.shape);
        const char* detailFont = BarTextFont(settings);
        if (!host.EnsureFont(detailFont)) detailFont = kDetailFont;
        const Rows& rows = RowsFor(host, m, p.shape, settings.barTextSize, titleFont, detailFont);
        const BarLook look{p, settings, m, PillRow(m), rows.lines, titleFont, p.shape == BarShape::Ornate, detailFont};
        m_TitlePixels = look.lines.title;
        m_BarBottom   = BarBottom(p, m);
        if (!Document(host, look, settings.barTheme, rows.place))
        {
            input.Frame(BarLayout{}, {});
            input.SetPopup(BarRect{});
            return;
        }
        Widgets widgets;
        BarLayout layout = PlaceWidgets(host, look, state, screenWidth, widgets);
        ShowWidgets(look, layout, widgets, input, screenWidth);
        // The help bar where the player moved it from its place under the top bar.
        bool helpCut        = false;
        const float helpX   = static_cast<float>(settings.helpBarX);
        const float helpTop = HelpBarTop(p, m) + static_cast<float>(settings.helpBarY);
        const BarRect help  = ShowHelpBar(host, look, rows, state, helpTop, helpX, screenWidth, helpCut);
        if (helpCut)
        {
            // Hovered, it opens the popup with the rest, as a widget would.
            layout.widgets.push_back(PlacedWidget{BarWidget::GameHelp, help});
            widgets.hasPopup[static_cast<int>(BarWidget::GameHelp)] = true;
        }
        if (settings.barWidgets[static_cast<int>(BarWidget::GameHelp)])
            m_HelpReach = BarRect{HelpBarLeft(m) + helpX, helpTop, HelpBarWidth(screenWidth), rows.helpBarHeight};
        input.Frame(layout, widgets.hasPopup);
        input.SetPopup(
            ShowPopup(host, look, settings.barTheme, state, layout, screenWidth, screenHeight, input, hidden));
        if (m_Visible == hidden)
        {
            m_Visible = !hidden;
            if (hidden)
                m_Document->Hide();
            else
                m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        }
        m_Gdi.Sweep();
    }
}
