#include "bar_render.h"

#include "argb.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr float kRadiusScale     = 100.0f; // an image key keeps a radius in hundredths
        constexpr float kFitSteps        = 10.0f;  // a fitted text's room is kept in tenths of a pixel
        constexpr int kFewLetters        = 3;      // the least of the target's name worth its room on the bar
        enum ImageKind : int
        {
            kStrip,
            kBox,
            kDiamond,
            kCoin,
            kRule,
            kStar,
        };

        // Text is rasterized white and takes its color from its quad's tint; the tint keeps its own alpha, as the
        // text never took a color's.
        uint32_t Colored(uint32_t tint, uint32_t color) { return (tint & kAlphaMask) | (color & ~kAlphaMask); }

    }

    std::string BarRenderer::TakeFailure() { return std::exchange(m_Failure, std::string()); }

    void BarRenderer::Fail(std::string what)
    {
        m_Failed  = true;
        m_Failure = std::move(what);
        m_Quads.clear(); // the frame that failed draws none of itself
    }

    bool BarRenderer::Upload(Texture& t, const Image& image, float halo)
    {
        float u = 0.0f, v = 0.0f;
        IDirect3DTexture8* texture = CreateTexture(m_Device, image.argb.data(), image.width, image.height, u, v);
        if (texture == nullptr)
        {
            Fail("Direct3D could not make a " + std::to_string(image.width) + "x" + std::to_string(image.height) +
                 " texture");
            return false;
        }
        ReleaseTexture(t.texture);
        const auto width = static_cast<float>(image.width), height = static_cast<float>(image.height);
        t = Texture{texture, width, height, u, v, halo, {}, {}, {}};
        return true;
    }

    bool BarRenderer::Upload(Texture& t, const SlicedBox& box)
    {
        if (!Upload(t, box.image)) return false;
        t.width = static_cast<float>(box.width);
        t.spans = SliceSpans(box, t.u);
        return true;
    }

    const BarRenderer::Texture* BarRenderer::Text(const std::string& text, const TextFace& face)
    {
        if (text.empty() || m_Failed) return nullptr;
        const char* font = face.family;
        const int pixels = WholePixels(face.size);
        Texture& t       = m_Texts.Use(TextView{text, font, pixels, face.tracking}, m_Frame);
        if (t.texture != nullptr) return &t;
        const float shadow = static_cast<float>(pixels) * kShadowPerHeight;
        const int margin   = BlurMargin(shadow + 1.0f);
        Coverage coverage;
        if (!m_Gdi.Rasterize(text.c_str(), font, pixels, false, face.tracking, margin, coverage))
        {
            Fail("GDI could not draw text in " + std::string(font));
            return nullptr;
        }
        Layers layers;
        layers.shadowBlur     = shadow;
        layers.shadowDrop     = 1;
        layers.color          = kWhite;
        layers.shadowColor    = kBlack;
        layers.shadowStrength = kShadowStrength;
        if (!Upload(t, Styled(coverage, layers), static_cast<float>(margin))) return nullptr;
        t.ink  = FindInk(coverage, kInkCoverage);
        t.line = m_Gdi.FontInk(font, pixels, kDetailGlyphs);
        return &t;
    }

    const TextRow& BarRenderer::Lines(const BarMetrics& m, BarShape shape, int barText, const char* titleFont,
        const char* detailFont)
    {
        return m_Lines.Get(LinesKey{m.height, shape, barText, titleFont, detailFont}, [&](const LinesKey&) {
            return FitTextRow(m, [&](bool title, int pixels) {
                return title ? m_Gdi.FontInk(titleFont, pixels, kTitleGlyphs)
                             : m_Gdi.FontInk(detailFont, pixels, kDetailGlyphs);
            });
        });
    }

    float BarRenderer::ShortestWidth(Memo<ShortestKey, float>& memo, const std::string& text, const TextFace& face)
    {
        return memo.Get(ShortestKey{Shortest(text, kFewLetters), face.family, WholePixels(face.size)},
            [&](const ShortestKey& key) { return m_Gdi.Measure(key.text.c_str(), face.family, key.pixels); });
    }

    const std::string& BarRenderer::Fitted(Memo<FitKey, std::string>& memo, std::string text, const TextFace& face,
        float room)
    {
        // Rounded down, so the cut text never needs more than the room it was given.
        const long tenths = std::lround(std::floor(room * kFitSteps));
        return memo.Get(FitKey{std::move(text), face.family, WholePixels(face.size), tenths}, [&](const FitKey& key) {
            return FitText(key.text, static_cast<float>(key.room) / kFitSteps,
                [&](const std::string& s) { return m_Gdi.Measure(s.c_str(), face.family, key.pixels); });
        });
    }

    template <typename Make>
    const BarRenderer::Texture* BarRenderer::Picture(const ImageKey& key, const Make& make)
    {
        if (m_Failed) return nullptr;
        Texture& t = m_Images.Use(key, m_Frame);
        if (t.texture != nullptr) return &t;
        return Upload(t, make()) ? &t : nullptr;
    }

    const BarRenderer::Texture* BarRenderer::BoxPicture(float width, float height, const BoxStyle& style)
    {
        const int w = WholePixels(width), h = WholePixels(height);
        const int radius = static_cast<int>(std::lround(style.radius * kRadiusScale));
        const ImageKey key{.kind = kBox, .width = w, .height = h, .radius = radius, .outerGap = style.outerGap,
            .colors = {style.top, style.bottom, style.border, style.outer}};
        return Picture(key, [&] { return SliceBox(w, h, style); });
    }

    void BarRenderer::AddText(const Texture* t, float x, float y, uint32_t tint)
    {
        if (t == nullptr) return;
        m_Quads.push_back(GpuQuad{t->texture, std::round(x - t->halo), std::round(y - t->halo), t->width, t->height,
            0.0f, t->u, t->v, 0.0f, tint});
    }

    void BarRenderer::AddImage(const Texture* t, float x, float y, float width, float height, uint32_t tint)
    {
        if (t == nullptr) return;
        m_Quads.push_back(
            GpuQuad{t->texture, std::round(x), std::round(y), width, height, 0.0f, t->u, t->v, 0.0f, tint});
    }

    void BarRenderer::AddIcon(Icon icon, float x, float y, float width, float height, uint32_t tint)
    {
        if (m_Failed) return;
        const IconTexture* t = m_Icons.For(m_Device, icon);
        if (t == nullptr)
        {
            Fail(m_Icons.TakeFailure());
            return;
        }
        m_Quads.push_back(
            GpuQuad{t->texture, std::round(x), std::round(y), width, height, 0.0f, t->u, t->v, 0.0f, tint});
    }

    void BarRenderer::AddBox(const Texture* t, float x, float y, uint32_t tint)
    {
        if (t == nullptr) return;
        x = std::round(x), y = std::round(y);
        for (const SliceSpan& s : t->spans)
            m_Quads.push_back(GpuQuad{t->texture, x + static_cast<float>(s.x), y, static_cast<float>(s.width),
                t->height, s.u0, s.u1, t->v, 0.0f, tint});
    }

    TextSize BarRenderer::MeasuredSize(const Texture* text)
    {
        if (text == nullptr) return TextSize{};
        const float width = text->width - 2.0f * text->halo;
        return TextSize{width, text->height - 2.0f * text->halo, text->line.top, text->line.bottom,
            std::max(0.0f, static_cast<float>(text->ink.right) - text->halo - width)};
    }

    float BarRenderer::SpansWidth(const std::vector<Span>& spans, const TextFace& face)
    {
        float width = 0.0f;
        for (const Span& s : spans)
            width += MeasuredSize(Text(s.text, face)).width;
        return width;
    }

    float BarRenderer::LineWidth(const std::vector<Span>& spans, const std::vector<Icon>& icons, const TextFace& face)
    {
        if (icons.empty()) return SpansWidth(spans, face);
        const auto n = static_cast<float>(icons.size());
        return SpansWidth(spans, face) + kLineIconGap * 2.0f + n * std::round(face.size) + (n - 1.0f) * kLineIconGap;
    }

    char BarRenderer::WidestDigitOf(const TextFace& face)
    {
        const int pixels = WholePixels(face.size);
        for (const FaceDigit& known : m_WidestDigits)
            if (known.pixels == pixels && known.family == face.family) return known.digit;
        const char digit = WidestDigit([&](char d) {
            const char text[] = {d, '\0'};
            return m_Gdi.Measure(text, face.family, pixels);
        });
        m_WidestDigits.push_back(FaceDigit{face.family, pixels, digit});
        return digit;
    }

    float BarRenderer::SteadyWidth(std::vector<Span> spans, const std::vector<Icon>& icons, const TextFace& face)
    {
        // Never less than the line as it reads now, should a pair of its real figures kern wider.
        const float shown = LineWidth(spans, icons, face);
        const char digit  = WidestDigitOf(face);
        for (Span& s : spans)
            s.text = SteadyFigures(s.text, digit);
        return std::max(shown, LineWidth(spans, icons, face));
    }

    void BarRenderer::Update(const BarState& state, const Settings& settings, float screenWidth, float screenHeight,
        BarInput& input, bool hidden)
    {
        ++m_Frame;
        m_Quads.clear();
        m_PopupStart = 0;
        m_HelpReach.reset();
        m_BarBottom = 0.0f;
        if (!BarOn(settings) || m_Device == nullptr || m_Failed || screenWidth <= 0.0f)
        {
            // A bar drawn nowhere keeps nothing; its textures are made again when it is drawn.
            Release();
            m_TitlePixels = 0;
            input.Frame(BarLayout{}, {});
            input.SetPopup(BarRect{});
            return;
        }
        const BarPalette& p   = PaletteOf(settings.barTheme);
        const BarMetrics m = MetricsFor(settings.barHeight, p.shape, settings.barTextSize, settings.popupTextSize);
        const char* titleFont  = BarTitleFont(settings, p.shape);
        const char* detailFont = BarTextFont(settings);
        const bool ornate      = p.shape == BarShape::Ornate;
        const BarLook look{p, settings, m, PillRow(m), Lines(m, p.shape, settings.barTextSize, titleFont, detailFont),
            titleFont, ornate, detailFont};
        m_TitlePixels = look.lines.title;
        Widgets widgets;
        m_BarBottom      = BarBottom(p, m);
        BarLayout layout = PlaceWidgets(look, state, screenWidth, widgets);
        AddStrip(look, layout, screenWidth);
        AddWidgets(look, layout, widgets, input);
        // The help bar where the player moved it from its place under the top bar.
        bool helpCut          = false;
        const float helpX     = static_cast<float>(settings.helpBarX);
        const float helpTop   = HelpBarTop(p, m) + static_cast<float>(settings.helpBarY);
        const size_t helpFrom = m_Quads.size();
        BarRect help          = AddHelpBar(look, state, helpTop, screenWidth, helpCut);
        for (size_t i = helpFrom; i < m_Quads.size(); ++i)
            m_Quads[i].x += helpX;
        help.x += helpX;
        if (helpCut)
        {
            // Hovered, it opens the popup with the rest, as a widget would.
            layout.widgets.push_back(PlacedWidget{BarWidget::GameHelp, help});
            widgets.hasPopup[static_cast<int>(BarWidget::GameHelp)] = true;
        }
        if (settings.barWidgets[static_cast<int>(BarWidget::GameHelp)])
        {
            m_HelpReach = BarRect{HelpBarLeft(m) + helpX, helpTop, HelpBarWidth(screenWidth), HelpBarHeight(look)};
        }
        input.Frame(layout, widgets.hasPopup);
        m_PopupStart = m_Quads.size();
        input.SetPopup(AddPopup(look, state, layout, screenWidth, screenHeight, input));
        if (hidden)
        {
            m_Quads.clear();
            m_PopupStart = 0;
        }

        for (Texture& t : m_Texts.TakeIdle(m_Frame, kIdleFrames))
            ReleaseTexture(t.texture);
        for (Texture& t : m_Images.TakeIdle(m_Frame, kIdleFrames))
            ReleaseTexture(t.texture);
        m_Gdi.Sweep();
    }

    void BarRenderer::Off()
    {
        Release();
        m_PopupStart  = 0;
        m_TitlePixels = 0;
        m_BarBottom   = 0.0f;
        m_HelpReach.reset();
    }

    BarLayout BarRenderer::PlaceWidgets(const BarLook& look, const BarState& state, float screenWidth,
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
            natural[i]          = 2.0f * m.padding + look.Leading(c) +
                         std::max(SteadyWidth(c.title, c.titleIcons, look.TitleFace(c)),
                             look.lines.twoLines ? SteadyWidth(c.subtitle, c.subtitleIcons, look.DetailFace()) : 0.0f) +
                         (c.progress && c.progressBeside ? m.gap + BesideWidth(m) : 0.0f);
        }
        // The target goes when its room cannot hold its padding, its icons and the start of its name.
        WidgetContent& target = widgets.contents[static_cast<int>(BarWidget::Target)];
        const float targetMinimum =
            2.0f * m.padding + look.Leading(target) +
            (target.title.empty() ? 0.0f : ShortestWidth(m_Shortest, target.title.front().text, look.TitleFace()));
        const BarLayout layout = LayoutBar(screenWidth, m, look.palette.shape, natural, targetMinimum,
            look.settings.barSections, look.settings.barSeparators);
        if (layout.targetWidth > 0.0f && layout.targetWidth < natural[static_cast<int>(BarWidget::Target)] &&
            !target.title.empty())
        {
            // Each line's icons keep their room; its text is cut.
            const float room = layout.targetWidth - 2.0f * m.padding - look.Leading(target);
            std::string& title = target.title.front().text;
            title = Fitted(m_TitleFit, std::move(title), look.TitleFace(),
                room - LineWidth({}, target.titleIcons, look.TitleFace()));
            if (look.lines.twoLines && !target.subtitle.empty())
            {
                std::string& detail = target.subtitle.front().text;
                detail = Fitted(m_SubtitleFit, std::move(detail), look.DetailFace(),
                    room - LineWidth({}, target.subtitleIcons, look.DetailFace()));
            }
        }
        return layout;
    }

    void BarRenderer::AddStrip(const BarLook& look, const BarLayout& layout, float screenWidth)
    {
        const BarPalette& p = look.palette;
        const BarMetrics& m = look.metrics;
        const int stripRows = WholePixels(m.height), belowRows = WholePixels(m.below);
        const ImageKey stripKey{.kind = kStrip, .width = 1, .height = stripRows, .below = belowRows,
            .colors = {p.stripTop, p.stripMiddle, p.stripBottom, p.bottomLine, p.underLine, p.below}};
        const Texture* strip = Picture(stripKey, [&] {
            return StripColumn(stripRows, belowRows, p.stripTop, p.stripMiddle, p.stripBottom, p.bottomLine,
                p.underLine, p.below);
        });
        if (strip != nullptr) AddImage(strip, 0.0f, 0.0f, screenWidth, strip->height);
        auto diamond = [&](float size, uint32_t c) {
            const int s = WholePixels(size);
            return Picture(ImageKey{.kind = kDiamond, .width = s, .height = s, .colors = {c}},
                [&] { return Diamond(s, c); });
        };
        const float midY       = look.row.y + look.row.height / 2.0f;
        if (look.ornate)
        {
            const Texture* cap = diamond(m.diamond * kCapScale, p.accent);
            for (const float cx : {m.cap / 2.0f, screenWidth - m.cap / 2.0f})
                if (cap != nullptr)
                    AddImage(cap, cx - cap->width / 2.0f, midY - cap->height / 2.0f, cap->width, cap->height);
        }
        // Pill palettes have no divider color: their diamonds take the accent.
        const Texture* divider = diamond(m.diamond, look.ornate ? p.divider : p.accent);
        for (const float dx : layout.dividers)
            if (divider != nullptr)
                AddImage(divider, dx - divider->width / 2.0f, midY - divider->height / 2.0f, divider->width,
                    divider->height);
    }

    void BarRenderer::AddWidgets(const BarLook& look, const BarLayout& layout, const Widgets& widgets,
        const BarInput& input)
    {
        const BarMetrics& m  = look.metrics;
        const BarRow& row    = look.row;
        const TextRow& lines = look.lines;
        for (const PlacedWidget& placed : layout.widgets)
        {
            const int index        = static_cast<int>(placed.widget);
            const WidgetContent& c = widgets.contents[index];
            const BarRect& r       = placed.rect;
            const bool lit         = widgets.hasPopup[index] &&
                             (input.Hovered() == placed.widget || input.Open() == placed.widget);
            if (const std::optional<WidgetBox> box = look.BoxOf(placed, lit))
                AddBox(BoxPicture(box->rect.width, box->rect.height, box->style), box->rect.x, box->rect.y);
            float x         = r.x + m.padding;
            const float mid = row.y + row.height / 2.0f;
            for (const Icon icon : c.icons)
            {
                AddIcon(icon, x, mid - m.icon / 2.0f, m.icon, m.icon);
                x += m.icon + m.gap;
            }
            if (c.coin)
            {
                const int s = CoinPixels(m);
                const Texture* coin =
                    Picture(ImageKey{.kind = kCoin, .width = s, .height = s, .colors = {kCoinColor}},
                        [&] { return Coin(s, kCoinColor); });
                if (coin != nullptr) AddImage(coin, x, mid - coin->height / 2.0f, coin->width, coin->height);
                x += static_cast<float>(s) + m.gap;
            }
            auto line = [&](const std::vector<Span>& spans, const std::vector<Icon>& icons, const TextFace& face,
                            float y) {
                float sx = x;
                for (const Span& s : spans)
                {
                    const Texture* t = Text(s.text, face);
                    AddText(t, sx, y, Colored(kWhite, look.BarColor(s)));
                    sx += MeasuredSize(t).width;
                }
                if (icons.empty()) return;
                // Centered on the line's box, as tall as its letters' size.
                const float size = std::round(face.size);
                const float iy   = std::round(y + (MeasuredSize(Text("H", face)).height - size) / 2.0f);
                sx += kLineIconGap * 2.0f;
                for (const Icon icon : icons)
                {
                    AddIcon(icon, sx, iy, size, size);
                    sx += size + kLineIconGap;
                }
            };
            if (!lines.twoLines || c.subtitle.empty())
                line(c.title, c.titleIcons, look.TitleFace(c), lines.aloneY);
            else
            {
                line(c.title, c.titleIcons, look.TitleFace(c), lines.titleY);
                line(c.subtitle, c.subtitleIcons, look.DetailFace(), lines.detailY);
            }
            if (c.progress) AddProgress(look, r, *c.progress, c.progressBeside);
        }
    }

    void BarRenderer::AddProgress(const BarLook& look, const BarRect& widget, float done, bool beside)
    {
        // A thin track along the pill row's foot, inside its border, or a taller one at the widget's right end,
        // centered on the row; filled in the accent from the left.
        const BarPalette& p = look.palette;
        const BarMetrics& m = look.metrics;
        const float height  = ProgressHeight(m, beside);
        const float width   = beside ? BesideWidth(m) : widget.width - 2.0f * m.padding;
        const float x       = beside ? widget.x + widget.width - m.padding - width : widget.x + m.padding;
        const float y       = beside ? std::round(look.row.y + (look.row.height - height) / 2.0f)
                                     : look.row.y + look.row.height - kProgressInset - height;
        if (width <= 0.0f) return;
        const auto solid = [&](uint32_t color) {
            return Picture(ImageKey{.kind = kStrip, .width = 1, .height = 1, .colors = {color, color, color, color}},
                [&] { return StripColumn(1, 0, color, color, color, color, 0, 0); });
        };
        if (const Texture* track = solid(p.divider)) AddImage(track, x, y, width, height);
        const float filled = std::round(width * std::clamp(done, 0.0f, 1.0f));
        if (const Texture* fill = solid(p.accent); fill != nullptr && filled > 0.0f) AddImage(fill, x, y, filled, height);
    }

    float BarRenderer::HelpBarHeight(const BarLook& look)
    {
        // A line box is as tall for any text in its font.
        const float caption = MeasuredSize(Text("H", look.TitleFace())).height;
        const float help    = MeasuredSize(Text("H", HelpFace(look))).height;
        return std::round(std::max(caption, help) + 2.0f * HelpBarPad(look.metrics));
    }

    BarRect BarRenderer::AddHelpBar(const BarLook& look, const BarState& state, float top, float screenWidth,
        bool& cut)
    {
        cut = false;
        if (!look.settings.barWidgets[static_cast<int>(BarWidget::GameHelp)] || !state.strip ||
            (state.strip->caption.empty() && state.strip->help.empty()))
            return BarRect{};
        const BarPalette& p = look.palette;
        const BarMetrics& m = look.metrics;
        const TextFace captionFace = look.TitleFace(), helpFace = HelpFace(look);
        const std::optional<Span> bag = StripBagFill(state);
        const Texture* bagText        = bag ? Text(bag->text, captionFace) : nullptr;
        const float bagWidth          = MeasuredSize(bagText).width;
        const float left              = HelpBarLeft(m);
        const float room = HelpBarWidth(screenWidth) - left - 2.0f * m.padding - (bagWidth > 0.0f ? bagWidth + m.gap : 0.0f);
        const int captionPixels = WholePixels(captionFace.size), helpPixels = WholePixels(helpFace.size);
        const HelpBarText& text = m_HelpBar.Get(
            HelpBarKey{*state.strip, captionFace.family, helpFace.family, captionPixels, helpPixels,
                std::lround(std::floor(room * kFitSteps)), std::lround(m.padding * kFitSteps)},
            [&](const HelpBarKey& k) {
                return HelpBarLine(k.strip, static_cast<float>(k.room) / kFitSteps,
                    static_cast<float>(k.gap) / kFitSteps,
                    [&](const std::string& s) { return m_Gdi.Measure(s.c_str(), captionFace.family, captionPixels); },
                    [&](const std::string& s) { return m_Gdi.Measure(s.c_str(), helpFace.family, helpPixels); });
            });
        cut = text.cut;

        // One row in a panel like a popup's, bordered all round: the caption in a widget's title color, the bag's
        // count, then the help in a popup's text color, each centered on the row.
        const Texture* caption = text.caption.empty() ? nullptr : Text(text.caption, captionFace);
        const Texture* help    = text.help.empty() ? nullptr : Text(text.help, helpFace);
        const HelpBarRow row   = PlaceHelpBar(left, top, HelpBarHeight(look), m.padding, m.gap,
            MeasuredSize(caption).width, bagWidth, MeasuredSize(help).width);
        // Without its background, the text alone on the game.
        if (look.settings.helpBarBackground)
            AddBox(BoxPicture(row.box.width, row.box.height, HelpBarPanel(look)), row.box.x, row.box.y);
        const auto add = [&](const Texture* t, float x, uint32_t color) {
            AddText(t, x, row.box.y + (row.box.height - MeasuredSize(t).height) / 2.0f, Colored(kWhite, color));
        };
        add(caption, row.captionX, p.title);
        if (bag) add(bagText, row.bagX, look.BarColor(*bag));
        add(help, row.helpX, p.text);
        return row.box;
    }

    BarRect BarRenderer::AddPopup(const BarLook& look, const BarState& state, const BarLayout& layout,
        float screenWidth, float screenHeight, const BarInput& input)
    {
        const std::optional<BarWidget> open = input.Open();
        const BarRect* anchor               = open ? RectOf(layout, *open) : nullptr;
        if (anchor == nullptr) return BarRect{};
        const BarPalette& p       = look.palette;
        const BarMetrics& m       = look.metrics;
        auto measure = [&](const Span& s, PopupFont f) { return MeasuredSize(Text(s.text, look.FaceOf(f))); };
        const BoxStyle panel = PopupPanel(look);
        // The ornate panel's outer border and the line under the strip count: the gap is from them. The help bar's
        // popup goes under the help bar's box.
        const auto out        = static_cast<float>(OuterPad(panel));
        const float barBottom =
            *open == BarWidget::GameHelp ? anchor->y + anchor->height : BarBottom(p, m);
        PopupKey key{BarPopupContent(*open, state, look.settings), input.Pinned() == open, m.height, m.popupText,
            p.shape, PopupRoom(barBottom, screenHeight, m.gap, out)};
        const PopupLayout& pl = m_Popup.Get(std::move(key), [&](const PopupKey& k) {
            PopupLayout cut = LayoutPopup(k.content, m, p.shape, k.pinned, measure);
            CutPopup(cut, k.room, m.popupPadding);
            return cut;
        });
        const BarRect popupRect =
            PlacePopup(*anchor, barBottom, pl.width, pl.height, screenWidth, screenHeight, m.gap, out);
        AddBox(BoxPicture(popupRect.width, popupRect.height, panel), popupRect.x - out, popupRect.y - out);
        for (const PopupPiece& piece : pl.pieces)
        {
            const float px      = popupRect.x + piece.x, py = popupRect.y + piece.y;
            const TextFace face = look.FaceOf(piece.font);
            switch (piece.kind)
            {
            case PopupPiece::Kind::Text:
                AddText(Text(piece.span.text, face), px, py, Colored(kWhite, look.PopupColor(piece.span)));
                break;
            case PopupPiece::Kind::ImmuneMark:
            {
                const Texture* t = Text(piece.span.text, face);
                if (t == nullptr) break;
                if (const std::optional<BarPoint> at =
                        CenteredInk(BarRect{px, py, piece.width, piece.height}, t->ink, t->halo))
                    AddText(t, at->x, at->y, Colored(kWhite, look.PopupColor(piece.span)));
                break;
            }
            case PopupPiece::Kind::Icon:
                AddIcon(piece.icon, px, py, piece.width, piece.height);
                break;
            case PopupPiece::Kind::Star:
            {
                const int s = WholePixels(piece.width);
                const ImageKey key{.kind = kStar, .width = s, .height = s, .colors = {kStarColor}};
                AddImage(Picture(key, [&] { return Star(s, kStarColor); }), px, py, static_cast<float>(s),
                    static_cast<float>(s));
                break;
            }
            case PopupPiece::Kind::Rule:
            {
                const int w = WholePixels(piece.width);
                const ImageKey key{.kind = kRule, .width = w, .height = 1, .colors = {p.divider}};
                AddImage(Picture(key, [&] { return FadingRule(w, p.divider); }), px, py, piece.width, 1.0f);
                break;
            }
            case PopupPiece::Kind::Card:
            {
                AddBox(BoxPicture(piece.width, piece.height, PopupCard(look)), px, py);
                break;
            }
            case PopupPiece::Kind::Tag:
            {
                AddBox(BoxPicture(piece.width, piece.height, PopupTag(look, piece.height)), px, py);
                break;
            }
            }
        }
        return popupRect;
    }

    void BarRenderer::Draw()
    {
        if (m_PopupStart == 0 || m_Device == nullptr) return;
        SetQuadStates(m_Device, false);
        for (size_t i = 0; i < m_PopupStart; ++i)
            DrawQuad(m_Device, m_Quads[i], false);
        m_Device->SetTexture(0, nullptr);
    }

    void BarRenderer::DrawPopupQuads()
    {
        if (m_PopupStart >= m_Quads.size() || m_Device == nullptr) return;
        SetQuadStates(m_Device, false);
        for (size_t i = m_PopupStart; i < m_Quads.size(); ++i)
            DrawQuad(m_Device, m_Quads[i], false);
        m_Device->SetTexture(0, nullptr);
    }

    void BarRenderer::DrawPopup(IGuiManager* gui, float scale) const
    {
        if (gui == nullptr || m_PopupStart >= m_Quads.size()) return;
        // The window covers every quad, the panel's outer border too; nothing in it takes the pointer.
        float left = m_Quads[m_PopupStart].x, top = m_Quads[m_PopupStart].y, right = left, bottom = top;
        for (size_t i = m_PopupStart; i < m_Quads.size(); ++i)
        {
            const GpuQuad& q = m_Quads[i];
            left             = std::min(left, q.x);
            top              = std::min(top, q.y);
            right            = std::max(right, q.x + q.width);
            bottom           = std::max(bottom, q.y + q.height);
        }
        // ImGui's PushStyleVar is overloaded, which abi_check refuses: the style is set and put back by hand.
        ImGuiStyle& style    = gui->GetStyle();
        const ImGuiStyle was = style;
        style.WindowPadding    = ImVec2(0.0f, 0.0f);
        style.WindowBorderSize = 0.0f;
        style.PopupBorderSize  = 0.0f;
        style.WindowMinSize    = ImVec2(1.0f, 1.0f);
        style.Alpha            = 1.0f;
        gui->SetNextWindowPos(ImVec2(left * scale, top * scale), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        gui->SetNextWindowSize(ImVec2((right - left) * scale, (bottom - top) * scale), ImGuiCond_Always);
        constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoDecoration |
                                            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
                                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (gui->Begin("##HeadsUp popup", nullptr, kFlags))
            for (size_t i = m_PopupStart; i < m_Quads.size(); ++i)
            {
                const GpuQuad& q = m_Quads[i];
                gui->SetCursorScreenPos(ImVec2(q.x * scale, q.y * scale));
                const float channel = 1.0f / 255.0f;
                const ImVec4 tint((q.tint >> 16 & 0xFF) * channel, (q.tint >> 8 & 0xFF) * channel,
                    (q.tint & 0xFF) * channel, (q.tint >> 24) * channel);
                // Ashita's renderer takes a texture's ID as its IDirect3DTexture8.
                gui->ImageWithBg(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(q.texture))),
                    ImVec2(q.width * scale, q.height * scale), ImVec2(q.u0, 0.0f), ImVec2(q.u1, q.v),
                    ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint);
            }
        gui->End();
        style = was;
    }

    void BarRenderer::Release()
    {
        for (Texture& t : m_Texts.TakeAll())
            ReleaseTexture(t.texture);
        for (Texture& t : m_Images.TakeAll())
            ReleaseTexture(t.texture);
        m_Icons.Release();
        m_Gdi.Release();
        m_Quads.clear();
    }
}
