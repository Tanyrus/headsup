#include "nameplate_render.h"

#include "argb.h"
#include "cursor_picture.h"
#include "entity_state.h"
#include "plate_compose.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <span>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr float kOutlinePerHeight = 0.1f; // a badge's outline width, per pixel of its height
        constexpr float kStackGap         = 2.0f; // screen pixels between plates stacked over one another
        constexpr float kStackLineWidth   = 1.0f; // screen pixels: the line from a risen plate to where it stood
        constexpr const char* kLineSample = "H"; // GDI gives a line's height with it, and a capital's top and baseline
        // From the mockup's CSS: a 22 px name's shadows blur 2 and 3 px, 1 px lower, and its glow 14 px.
        constexpr float kShadowDropPerHeight = 0.05f;
        constexpr float kGlowPerHeight       = 0.64f; // the glow's blur radius, per pixel of name height
        constexpr float kOrnamentShadowPerHeight = 0.3f; // the diamond's shadow blur: 3 px on a 9 px ornament

        // What a cursor's colors are read from: the entity it is over, as the target it is to you.
        EntityState CursorState(EntityState of, TargetState target)
        {
            of.target = target;
            return of;
        }

        LineStyle StyleOf(const LineLook& look)
        {
            return LineStyle{look.font, look.style, look.shadowColor, look.shadow};
        }

        // Adds the time from its making to its end to spent.
        class Stopwatch
        {
        public:
            explicit Stopwatch(double& spentMs) : m_Spent(spentMs), m_Start(std::chrono::steady_clock::now()) {}
            ~Stopwatch() { m_Spent += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_Start).count(); }
            Stopwatch(const Stopwatch&)            = delete;
            Stopwatch& operator=(const Stopwatch&) = delete;

        private:
            double& m_Spent;
            std::chrono::steady_clock::time_point m_Start;
        };

        // A texture is scaled from the height it was drawn at, which lags a size step while it waits to be redrawn.
        float FitOf(float shown, int drawnHeight)
        {
            return drawnHeight > 0 ? shown / static_cast<float>(drawnHeight) : 0.0f;
        }

        int OutlineRadius(int pixelHeight)
        {
            return WholePixels(static_cast<float>(pixelHeight) * kOutlinePerHeight);
        }
    }

    void NameplateRenderer::FailNames(std::string what)
    {
        m_NamesFailed = true;
        m_NameFailure = std::move(what);
    }

    void NameplateRenderer::SetDevice(IDirect3DDevice8* device)
    {
        m_Device = device;
        D3DCAPS8 caps{};
        if (device == nullptr || FAILED(device->GetDeviceCaps(&caps))) caps = D3DCAPS8{};
        m_LargestSide = TextTextureSide(caps.MaxTextureWidth, caps.MaxTextureHeight);
    }

    void NameplateRenderer::LeaveOut(PlateTexture& t, TextureKey key)
    {
        ReleaseTexture(t.texture);
        t       = PlateTexture{};
        t.key   = std::move(key);
        t.unfit = true;
    }

    bool NameplateRenderer::Prepare(PlateTexture& t, const char* text, const TextLook& look, int pixelHeight)
    {
        TextureKey key{text, pixelHeight, 0, look};
        if (t.key == key && (t.texture != nullptr || t.unfit)) return true;
        if (!RedrawNow(t.texture != nullptr, m_RedrawMs)) return true;
        // A name new in a busy frame is drawn at once without its glow, which costs the most: a later frame adds it.
        if (t.texture == nullptr && look.glow != 0 && !RedrawNow(true, m_RedrawMs))
        {
            TextLook plain = look;
            plain.glow     = 0;
            return Prepare(t, text, plain, pixelHeight);
        }
        const Stopwatch stopwatch(m_RedrawMs);
        // What a height's shadow, glow and edge measure.
        struct Style
        {
            float shadow = 0.0f, glowBlur = 0.0f;
            int drop = 0, edge = 0, margin = 0;
        };
        auto styleAt = [&](int height) {
            const auto size = static_cast<float>(height);
            Style style;
            style.shadow   = size * kShadowPerHeight;
            style.drop     = static_cast<int>(std::lround(size * kShadowDropPerHeight));
            style.glowBlur = look.glow != 0 ? size * kGlowPerHeight * look.glowSize : 0.0f;
            style.edge     = OutlineRadius(height);
            style.margin =
                look.line.outline ? style.edge : BlurMargin(std::max(style.glowBlur, style.shadow + static_cast<float>(style.drop)));
            return style;
        };
        // A text with dashes drawn as marks is its stretches of letters and the marks, side by side.
        const std::vector<TextPiece> pieces = SplitMarks(text);
        const bool marked = std::ranges::any_of(pieces, [](const TextPiece& piece) { return piece.mark != DashMark::None; });
        const char* font  = look.line.font.c_str();
        // The height of a line of this font, which a mark is sized by: 0 when GDI cannot say.
        auto lineHeight = [&](int at) {
            int width = 0, line = 0;
            return m_Gdi.Extent(kLineSample, font, at, look.line.style, width, line) ? line : 0;
        };
        // What of each piece's width the next may stand in: a stretch of italic letters is measured and drawn with
        // room for its last letter's lean, which a mark after it would otherwise stand clear of.
        std::vector<int> widths(pieces.size()), room(pieces.size());
        auto extent = [&](int at, int& width, int& height) {
            if (!marked) return m_Gdi.Extent(text, font, at, look.line.style, width, height);
            height = lineHeight(at);
            for (size_t i = 0; i < pieces.size(); ++i)
            {
                const bool letters = pieces[i].mark == DashMark::None;
                int pieceHeight    = 0;
                if (!letters)
                    widths[i] = MarkWidth(pieces[i].mark, height);
                else if (!m_Gdi.Extent(pieces[i].text.c_str(), font, at, look.line.style, widths[i], pieceHeight))
                    return false;
                room[i] = letters ? ItalicLean(at, look.line.style) : 0;
            }
            width = JoinedWidth(widths, room);
            return height > 0;
        };
        // Measured before it is drawn: a long line at a tall height is more than GDI has the memory to draw at all.
        bool sized = true;
        const std::optional<int> height = RasterThatFits(pixelHeight, m_LargestSide, [&](int at) {
            ImageSize image{0, 0, styleAt(at).margin};
            sized = sized && extent(at, image.width, image.height);
            return sized ? image : ImageSize{};
        });
        // Too long at any height is the design's to shorten: failing here would turn every nameplate off.
        if (!height)
        {
            LeaveOut(t, std::move(key));
            return true;
        }
        // Wanted taller than fits, a text is the same pixels at every such height: the texture it has serves.
        if (sized && t.texture != nullptr && t.raster == *height && t.key.text == key.text && t.key.look == key.look)
        {
            t.key = std::move(key);
            return true;
        }
        const Style style = styleAt(*height);
        Coverage coverage;
        auto rasterize = [&] {
            // GDI's underline is the font's one row, which comes and goes as the text is drawn a little smaller or
            // larger than its image, and it underlines each stretch of letters apart, knowing nothing of the marks
            // between them: where the font says where its underline goes, the letters are drawn without and one line
            // thick enough to hold is drawn under them all.
            int under = 0, thick = 0;
            const bool underlined = HasStyle(look.line.style, FontStyle::Underline) &&
                                    m_Gdi.UnderlineBand(font, *height, look.line.style, under, thick);
            const uint8_t letterStyle = underlined ? look.line.style & ~StyleBit(FontStyle::Underline) : look.line.style;
            const auto underline = [&] {
                if (underlined) Underline(coverage, style.margin + under, UnderlineThickness(*height, thick), style.margin);
            };
            if (!marked)
            {
                if (!m_Gdi.Rasterize(text, font, *height, letterStyle, 0, style.margin, coverage)) return false;
                underline();
                return true;
            }
            const int line = lineHeight(*height);
            std::vector<Coverage> drawn(pieces.size());
            for (size_t i = 0; i < pieces.size(); ++i)
            {
                if (pieces[i].mark != DashMark::None)
                    drawn[i] = MarkCoverage(pieces[i].mark, line, style.margin);
                else if (!m_Gdi.Rasterize(pieces[i].text.c_str(), font, *height, letterStyle, 0, style.margin, drawn[i]))
                    return false;
                room[i] = pieces[i].mark == DashMark::None ? ItalicLean(*height, look.line.style) : 0;
            }
            coverage = Joined(drawn, room, style.margin);
            underline();
            return line > 0;
        };
        if (!sized || !rasterize())
        {
            FailNames("GDI could not draw text in " + look.line.font + " at " + std::to_string(*height) + " px");
            return false;
        }
        const Layers layers{style.shadow, style.drop, style.glowBlur, look.color, look.line.shadowColor, look.glow,
            look.glowStrength, look.line.shadowStrength};
        Image image =
            look.line.outline ? Outlined(coverage, style.edge, look.color, look.line.shadowColor) : Styled(coverage, layers);
        if (!Upload(t, std::move(image), std::move(key))) return false;
        const LineInk ink = m_Gdi.FontInk(font, *height, kLineSample);
        t.halo            = static_cast<float>(style.margin);
        t.raster          = *height;
        t.inkTop          = ink.top;
        t.inkBottom       = ink.baseline;
        return true;
    }

    const NameplateRenderer::PlateTexture* NameplateRenderer::PrepareBadge(const IconDraw& icon, const LineStyle& line, int pixelHeight)
    {
        if (!HasBadge(icon) || m_NamesFailed) return nullptr;
        PlateTexture& badge = m_Badges.Use(BadgeKey{icon.badge, icon.badgeColor, pixelHeight}, m_Frame);
        return Prepare(badge, icon.badge.c_str(), TextLook{line, icon.badgeColor}, pixelHeight) ? &badge : nullptr;
    }

    bool NameplateRenderer::PrepareOrnament(PlateTexture& t, uint32_t color, int pixelHeight, int width, const LineStyle& shadowLine)
    {
        TextureKey key{"", pixelHeight, width, TextLook{shadowLine, color}};
        if (t.key == key && (t.texture != nullptr || t.unfit)) return true;
        if (!RedrawNow(t.texture != nullptr, m_RedrawMs)) return true;
        const Stopwatch stopwatch(m_RedrawMs);
        auto shadowAt = [](int height) { return static_cast<float>(height) * kOrnamentShadowPerHeight; };
        auto widthAt  = [&](int height) {
            return WholePixels(static_cast<float>(width) * static_cast<float>(height) / static_cast<float>(pixelHeight));
        };
        const std::optional<int> height = RasterThatFits(pixelHeight, m_LargestSide,
            [&](int at) { return ImageSize{widthAt(at), at, BlurMargin(shadowAt(at))}; });
        if (!height)
        {
            LeaveOut(t, std::move(key));
            return true;
        }
        const float shadow = shadowAt(*height);
        if (!Upload(t, Ornament(widthAt(*height), *height, shadow, shadowLine.shadowStrength, color, shadowLine.shadowColor),
                std::move(key)))
            return false;
        t.halo   = static_cast<float>(BlurMargin(shadow));
        t.raster = *height;
        return true;
    }

    bool NameplateRenderer::PrepareCursor(PlateTexture& t, const EntityState& over, int pixelHeight, const Settings& settings)
    {
        const uint32_t color = CursorColorOf(settings.cursorColor, over, settings.palette);
        const uint32_t edge  = CursorColorOf(settings.cursorEdgeColor, over, settings.palette);
        const int pixelWidth = CursorPixelWidth(pixelHeight, settings.cursorWidth, settings.cursorHeight);
        const int radius     = CursorEdgePixels(pixelHeight, settings.cursorOutlineThickness);
        // Its line's shadow is its edge: the color, and the pixels where a text has its strength.
        const CursorStyle style = settings.cursorStyle;
        const LineStyle shape{CursorStyleName(style), 0, edge, static_cast<float>(radius)};
        if (style == CursorStyle::Chocobo && m_Chocobo.argb.empty()) m_Chocobo = ChocoboPicture();
        const Image* picture = style == CursorStyle::Chocobo ? &m_Chocobo : style == CursorStyle::File && m_CursorFile ? &*m_CursorFile : nullptr;
        // A file's picture read again is another picture under the same name.
        TextureKey key{style == CursorStyle::File ? std::to_string(m_CursorFileSerial) : std::string(), pixelHeight, pixelWidth,
            TextLook{shape, color}};
        if (t.texture != nullptr && t.key == key) return true;
        if (!RedrawNow(t.texture != nullptr, m_RedrawMs)) return true;
        const Stopwatch stopwatch(m_RedrawMs);
        const CursorDrawing drawing = DrawCursor(style, picture, pixelWidth, pixelHeight, radius, color, edge);
        if (!Upload(t, drawing.image, std::move(key))) return false;
        t.tip = drawing.tip;
        return true;
    }

    void NameplateRenderer::SetCursorFile(std::optional<Image> picture)
    {
        m_CursorFile = std::move(picture);
        ++m_CursorFileSerial;
    }

    void NameplateRenderer::SetGameFont(std::optional<GameFontFace> face)
    {
        ReleasePlates();
        m_Gdi.SetGameFont(std::move(face));
    }

    bool NameplateRenderer::Upload(PlateTexture& t, const Image& image, TextureKey key)
    {
        float u = 0.0f, v = 0.0f;
        IDirect3DTexture8* texture = CreateTexture(m_Device, image.argb.data(), image.width, image.height, u, v);
        if (texture == nullptr)
        {
            FailNames("Direct3D could not make a " + std::to_string(image.width) + "x" + std::to_string(image.height) + " texture");
            return false;
        }
        ReleaseTexture(t.texture);
        t = PlateTexture{texture, std::move(key), static_cast<float>(image.width), static_cast<float>(image.height), u, v};
        t.raster = t.key.height;
        return true;
    }

    void NameplateRenderer::Update(const Tracker& tracker, const GameNames& names, const NameHook& hook, const Settings& settings, float toX,
        float toY, const CursorTargets& targets, double now, uint16_t selfIndex, const std::vector<TimerLine>& selfTimers)
    {
        ++m_Frame;
        m_RedrawMs = 0.0;
        m_Quads.clear();
        m_CursorNames.clear();
        // m_Shown's entries are filled in place, so their lines' text keeps its room from frame to frame.
        size_t shownCount        = 0;
        const float screenWidth  = names.BackBufferWidth();
        const float screenHeight = names.BackBufferHeight();
        // Cursors are drawn after every nameplate, so another name never covers one.
        std::vector<Quad> cursorQuads;
        int rank        = kPlainRank; // of the plate whose quads are being added
        uint32_t opaque = kOpaqueAlpha; // and its alpha
        auto addQuad = [&](std::vector<Quad>& into, IDirect3DTexture8* texture, float x, float y, float width, float height, float u,
                           float v, float depth, uint32_t tint) {
            // Whole pixels in the target keep the text as sharp as it was drawn.
            const GpuQuad quad{texture, std::round(x * toX), std::round(y * toY), width, height, 0.0f, u, v, depth,
                WithAlpha(tint, opaque)};
            into.push_back(Quad{quad, rank});
        };
        // Text, the ornament and the cursor, whose textures have square pixels.
        auto addSquare = [&](std::vector<Quad>& into, const PlateTexture& t, float x, float y, float fit, float depth) {
            if (t.unfit) return;
            const SquareQuad q = PlaceSquarePixels(x, y, t.width, t.height, t.halo, fit, toX, toY);
            addQuad(into, t.texture, q.x, q.y, q.width, q.height, t.u, t.v, depth, kWhite);
        };
        // A texture shown at this height as the layout places it: on screen, without its halo.
        auto boxOf = [&](const PlateTexture& t, float shown) {
            const float scale = FitOf(shown, t.raster) / toY;
            return ItemSize{(t.width - 2.0f * t.halo) * scale, (t.height - 2.0f * t.halo) * scale, t.inkTop * scale,
                t.inkBottom * scale};
        };
        // Every plate but yours is sized by how far its owner is from you, with the camera's own distance behind you
        // added: unlike the game's name size, that changes smoothly as you walk and not at all as the camera turns.
        const ActorInfo* you     = tracker.FindIndex(selfIndex);
        const NameFrame* ownName = hook.Drawn(selfIndex);
        const float ownDepth     = ownName != nullptr ? NameFromFrame(*ownName, names.TargetScaleX(), names.TargetScaleY()).depth : 0.0f;
        const std::optional<float> cameraBack =
            you != nullptr ? CameraBehind(ownDepth, names.SteadyCamera(), you->feet) : std::nullopt;
        const float minScale        = static_cast<float>(settings.scaleMin) / kPercent;
        const float maxScale        = static_cast<float>(settings.scaleMax) / kPercent;
        const LineLook timerLook    = TimerLook(settings);
        const LineStyle timerLine   = StyleOf(timerLook);
        const uint32_t timerUpColor = ToArgb(settings.palette.aggression[static_cast<int>(Nature::Placeholder)]);
        const LineStyle badgeLine   = BadgeLineStyle(settings);
        // The plates whose names HeadsUp draws, to stack: each one's box and which quads are its.
        struct Stacked
        {
            Plate* plate;
            PlateBox box;
            int rank;
            float depth;
            uint16_t index;
            size_t quads, quadsEnd, cursors, cursorsEnd, shown;
            float centerX;     // where its line down to where it stood goes
            uint32_t color;    // its bottom line's text color, the line's
            uint32_t alpha;    // and its opacity
        };
        std::vector<Stacked> stacked;
        if (NameplatesOn(settings) && !m_NamesFailed && m_Device != nullptr)
        {
            for (const ActorPtr actor : tracker.Actors())
            {
                const ActorInfo* info = tracker.Find(actor);
                if (info == nullptr) continue;
                const NameFrame* frame = hook.Drawn(info->index);
                if (frame == nullptr) continue;
                const DrawnName gameName =
                    NameFromFrame(*frame, names.TargetScaleX(), names.TargetScaleY(), info->index == targets.subTarget);
                // Checked where our name goes: a seated player's head can be in view with the game's name above the screen.
                ScreenBox anchor = PlaceName(gameName.box, names.SteadyCamera(), info->feet, info->pose);
                if (!NameOnScreen(anchor, screenWidth, screenHeight)) continue;
                // The game drawing the name this frame is all a plate's lines wait for.
                ComposedPlate composed = ComposePlate(*info, tracker.Player(), settings, gameName.color);
                if (m_Icons.Failed()) DropIcons(composed);
                const TargetState target = TargetStateOf(info->index, targets);
                const bool showCursor    = CursorShown(info->index, settings, tracker.Player(), targets);
                const TargetState under  = showCursor ? TargetUnderPick(info->index, targets) : TargetState::None;
                const bool timersHere =
                    info->index == selfIndex && !selfTimers.empty() && !HiddenInEvent(settings, tracker.Player());
                if (composed.lines.empty() && !showCursor && !timersHere) continue;
                Plate& p = m_Plates[info->index];
                rank     = PlateRank(info->index, targets);
                opaque   = PlateAlpha(settings.opacity, rank, targets.target != 0, info->distance);
                p.frame  = m_Frame;

                const bool self = info->index == selfIndex;
                // Yours, and any plate while the camera or you are not known, is sized by its name's distance.
                const float sized = cameraBack && !self
                                        ? ScaleAtDistance(*cameraBack + Distance(info->feet, you->feet), minScale, maxScale)
                                        : NameScale(gameName.depth, gameName.box.Height(), screenHeight, minScale, maxScale);
                p.scale                 = EaseScale(p.scale, sized, now - p.scaleTime);
                p.scaleTime             = now;
                const bool scales       = settings.scaleWithDistance;
                const float scale       = scales ? p.scale : 1.0f;
                const float timerShown  = timerLook.textSize * scale * toY;
                const float cursorScale = settings.scaleCursor ? p.scale : 1.0f;
                const float cursorShown = static_cast<float>(settings.cursorHeight) * cursorScale * toY;
                auto raster             = [&](float shown, int current) {
                    return scales ? RasterHeight(shown, current) : FixedRasterHeight(shown);
                };
                // A line's text height and a run's badge text height, in the image.
                auto textShown  = [&](const ComposedLine& line) { return line.look.textSize * scale * toY; };
                auto badgeShown = [&](const RunShape& run) { return run.iconSize * kBadgeShare * toY; };
                p.cursorRaster = CursorRaster(cursorShown, p.cursorRaster, settings.scaleCursor, settings.cursorHeight);
                const OrnamentSettings& ornament = settings.Kind(PlateKindOf(info->state)).ornament;
                const float ornamentShown        = static_cast<float>(ornament.height) * scale * toY;

                bool lineUsed[kPlateLines] = {};
                m_LeftOut.clear();
                // Each design line's text textures, its rows' one after another. False once GDI or Direct3D refused one.
                size_t textsUsed[kPlateLines] = {};
                auto prepareTexts = [&]() {
                    for (const ComposedLine& line : composed.lines)
                    {
                        LineTextures& lt      = p.lines[line.source];
                        lineUsed[line.source] = true;
                        lt.raster             = raster(textShown(line), lt.raster);
                        TextLook look{StyleOf(line.look), line.look.color};
                        if (line.look.glow > 0.0f)
                        {
                            look.glow         = line.look.glowColor;
                            look.glowStrength = line.look.glow;
                            look.glowSize     = line.look.glowSize;
                        }
                        size_t& texts = textsUsed[line.source];
                        for (const PlateItem& item : line.items)
                        {
                            if (item.kind != ItemKind::Text) continue;
                            if (texts == lt.texts.size()) lt.texts.emplace_back();
                            PlateTexture& text = lt.texts[texts++];
                            if (!Prepare(text, item.text.c_str(), look, lt.raster)) return false;
                            m_LeftOut.push_back(text.unfit);
                        }
                    }
                    for (int source = 0; source < kPlateLines; ++source)
                    {
                        LineTextures& lt = p.lines[source];
                        for (size_t extra = textsUsed[source]; extra < lt.texts.size(); ++extra)
                            ReleaseTexture(lt.texts[extra].texture);
                        lt.texts.resize(textsUsed[source]);
                    }
                    return true;
                };
                if (!prepareTexts()) break;
                // A text that fits no texture leaves the plate before it is measured, so nothing keeps room for it.
                if (std::ranges::any_of(m_LeftOut, [](uint8_t leftOut) { return leftOut != 0; })) DropTexts(composed, m_LeftOut);
                const size_t lineCount      = composed.lines.size();
                const OrnamentAt ornamentAt = OrnamentLineOf(composed);
                // The plate's texts are its lines' textures in order, less the ones left out.
                auto nextText = [](const LineTextures& lt, size_t& at) -> const PlateTexture& {
                    while (lt.texts[at].unfit)
                        ++at;
                    return lt.texts[at++];
                };
                bool anyBadge = false;
                m_IconBadges.clear();
                if (m_Inputs.size() < lineCount) m_Inputs.resize(lineCount);
                m_TextStart.resize(lineCount);
                size_t textAt[kPlateLines] = {};
                // Each line's sizes for the layout, with its badges and the ornament over it. False as above.
                auto measureLines = [&]() {
                    for (size_t i = 0; i < lineCount; ++i)
                    {
                        const ComposedLine& line = composed.lines[i];
                        LineTextures& lt         = p.lines[line.source];
                        m_TextStart[i]           = textAt[line.source];
                        size_t& text             = textAt[line.source];
                        m_TextSizes.clear();
                        for (size_t j = 0; j < line.items.size(); ++j)
                        {
                            const PlateItem& item = line.items[j];
                            if (item.kind == ItemKind::Text) m_TextSizes.push_back(boxOf(nextText(lt, text), textShown(line)));
                            if (std::ranges::any_of(item.icons, HasBadge))
                            {
                                p.badgeRaster = raster(badgeShown(ShapeOfRun(line, j, scale)), p.badgeRaster);
                                anyBadge      = true;
                            }
                            for (const IconDraw& draw : item.icons)
                                m_IconBadges.push_back(PrepareBadge(draw, badgeLine, p.badgeRaster));
                        }
                        MeasureLine(line, m_TextSizes, scale, m_Inputs[i]);
                        if (static_cast<int>(i) != ornamentAt.line) continue;
                        lt.ornamentRaster = raster(ornamentShown, lt.ornamentRaster);
                        const int width   = WholePixels(static_cast<float>(lt.ornamentRaster) * static_cast<float>(ornament.width) /
                                                        static_cast<float>(ornament.height));
                        if (!PrepareOrnament(lt.ornament, composed.ornamentColor, lt.ornamentRaster, width, StyleOf(line.look)))
                            return false;
                        const ItemSize box         = boxOf(lt.ornament, ornamentShown);
                        m_Inputs[i].ornamentWidth  = box.width;
                        m_Inputs[i].ornamentHeight = box.height;
                        m_Inputs[i].ornamentUnder  = ornamentAt.under;
                    }
                    return true;
                };
                if (!measureLines() ||
                    (showCursor && !PrepareCursor(p.cursor, CursorState(info->state, target), p.cursorRaster, settings)) ||
                    (under != TargetState::None &&
                        !PrepareCursor(p.cursorUnder, CursorState(info->state, under), p.cursorRaster, settings)))
                    break;
                const int ornamentSource =
                    ornamentAt.line != kNoOrnament ? composed.lines[static_cast<size_t>(ornamentAt.line)].source : kNoOrnament;
                for (int source = 0; source < kPlateLines; ++source)
                {
                    LineTextures& lt = p.lines[source];
                    if (!lineUsed[source])
                        ReleaseLine(lt);
                    else if (source != ornamentSource)
                    {
                        ReleaseTexture(lt.ornament.texture);
                        lt.ornamentRaster = 0;
                    }
                }
                if (!anyBadge) p.badgeRaster = 0;
                for (size_t i = timersHere ? selfTimers.size() : 0; i < p.timers.size(); ++i)
                    ReleaseTexture(p.timers[i].texture);
                p.timers.resize(timersHere ? selfTimers.size() : 0);
                p.timerRaster   = timersHere ? raster(timerShown, p.timerRaster) : 0;
                int timersReady = 0;
                for (; timersReady < static_cast<int>(p.timers.size()); ++timersReady)
                {
                    const TimerLine& line = selfTimers[timersReady];
                    const TextLook look{timerLine, line.up ? timerUpColor : timerLook.color};
                    if (!Prepare(p.timers[timersReady], line.text.c_str(), look, p.timerRaster)) break;
                }

                const ItemSize cursorBox = showCursor ? boxOf(p.cursor, cursorShown) : ItemSize{};
                float timerHeight        = 0.0f;
                for (int i = 0; i < timersReady; ++i)
                    timerHeight = std::max(timerHeight, boxOf(p.timers[i], timerShown).height);
                if (lineCount > 0)
                {
                    const float raise = static_cast<float>(settings.nameRaise) * scale;
                    anchor.minY -= raise;
                    anchor.maxY -= raise;
                }
                const PlateLayout layout = LayoutPlate(anchor, std::span<const LineInput>(m_Inputs.data(), lineCount),
                    cursorBox.width, cursorBox.height, p.cursor.tip, timersReady, timerHeight,
                    static_cast<float>(settings.cursorRaise) * cursorScale);
                const float depth      = gameName.depth;
                const size_t firstQuad = m_Quads.size(), firstCursor = cursorQuads.size();
                auto addText = [&](const PlateTexture& t, float x, float y, float shown) {
                    addSquare(m_Quads, t, x, y, FitOf(shown, t.raster), depth);
                };
                // The lowest line first: each line and ornament above lies over the glow of the name under it.
                for (size_t i = lineCount; i-- > 0;)
                {
                    const ComposedLine& line = composed.lines[i];
                    const LineTextures& lt   = p.lines[line.source];
                    const LinePlace& place   = layout.lines[i];
                    size_t text              = m_TextStart[i];
                    for (size_t j = 0; j < line.items.size(); ++j)
                        if (line.items[j].kind == ItemKind::Text)
                            addText(nextText(lt, text), place.items[j].x, place.items[j].y, textShown(line));
                    if (place.ornamentHeight > 0.0f) addText(lt.ornament, place.ornament.x, place.ornament.y, ornamentShown);
                }
                if (shownCount == m_Shown.size()) m_Shown.emplace_back();
                Shown& shown     = m_Shown[shownCount++];
                shown.index      = info->index;
                shown.nameX      = 0.0f;
                shown.nameY      = 0.0f;
                shown.nameHeight = 0;
                shown.nameColor  = kWhite;
                shown.lineCount  = static_cast<int>(lineCount);
                shown.lines.resize(lineCount);
                bool named       = false;
                // Icons after every text, over the shadows beside them, in the order their badges were prepared in.
                size_t firstBadge = 0;
                for (size_t i = 0; i < lineCount; ++i)
                {
                    const ComposedLine& line = composed.lines[i];
                    const LinePlace& place   = layout.lines[i];
                    float iconSize           = 0.0f;
                    int drawn                = 0;
                    for (size_t j = 0; j < line.items.size(); ++j)
                    {
                        const PlateItem& item = line.items[j];
                        const ItemPlace at    = place.items[j];
                        if (item.name && !named)
                        {
                            named            = true;
                            shown.nameX      = at.x;
                            shown.nameY      = at.y;
                            shown.nameHeight = static_cast<int>(std::lround(textShown(line) / toY));
                            shown.nameColor  = line.look.color;
                        }
                        if (item.icons.empty()) continue;
                        const RunShape run = ShapeOfRun(line, j, scale);
                        iconSize           = run.iconSize;
                        for (size_t k = 0; k < item.icons.size(); ++k)
                        {
                            const IconDraw& draw    = item.icons[k];
                            const IconTexture* icon = m_Icons.For(m_Device, draw.icon, draw.paint);
                            if (icon == nullptr) break;
                            const IconSpot in = IconInRun(run, item, k);
                            const IconSpot spot{at.x + in.x, at.y + in.y};
                            addQuad(m_Quads, icon->texture, spot.x, spot.y, run.iconSize * toX, run.iconSize * toY, icon->u, icon->v, depth,
                                kWhite);
                            ++drawn;
                            const PlateTexture* badge = m_IconBadges[firstBadge + k];
                            if (badge == nullptr) continue;
                            const ItemSize box    = boxOf(*badge, badgeShown(run));
                            const IconSpot corner = BadgeAt(spot, run.iconSize, box.width, box.height);
                            addText(*badge, corner.x, corner.y, badgeShown(run));
                        }
                        firstBadge += item.icons.size();
                    }
                    ShownLine& shownLine = shown.lines[i];
                    JoinTexts(line, shownLine.text);
                    shownLine.x      = place.left;
                    shownLine.y      = place.top;
                    shownLine.height = static_cast<int>(std::lround(m_Inputs[i].textHeight > 0.0f ? textShown(line) / toY : iconSize));
                    shownLine.icons  = drawn;
                }
                if (showCursor)
                {
                    // Over everything, as the game's own arrow is: at the name's depth, the target's own model, a
                    // door its name sits inside or anything the game has faded out near the camera would hide it.
                    const float fit = FitOf(cursorShown, p.cursor.raster);
                    float top       = layout.cursor.y + CursorBob(now, cursorBox.height, settings.cursorFloat);
                    auto addCursor  = [&](const PlateTexture& cursor) {
                        const CursorSpot kept = KeepOnScreen(CursorSpot{layout.cursor.x, top}, cursorBox.width, cursorBox.height,
                            screenWidth, screenHeight, m_CoveredTop);
                        addSquare(cursorQuads, cursor, kept.x, kept.y, fit, 0.0f);
                    };
                    // The cursor is no part of the nameplate: it stays whole however faint the plate is.
                    const uint32_t plate = std::exchange(opaque, kOpaqueAlpha);
                    if (under != TargetState::None)
                    {
                        addCursor(p.cursorUnder);
                        top -= cursorBox.height * kPickedCursorRise;
                    }
                    addCursor(p.cursor);
                    opaque = plate;
                    m_CursorNames.push_back(CursorName{info->index, gameName.box});
                }
                for (int i = 0; i < timersReady; ++i)
                {
                    const PlateTexture& t = p.timers[i];
                    addText(t, layout.centerX - boxOf(t, timerShown).width / 2.0f,
                        layout.timersY + static_cast<float>(i) * layout.timerStep, timerShown);
                }
                // Only a name HeadsUp draws can move: the game's own stays where the game puts it.
                if (settings.stackNames && lineCount > 0)
                    stacked.push_back(Stacked{&p, BoxOf(layout), rank, depth, info->index, firstQuad,
                        m_Quads.size(), firstCursor, cursorQuads.size(), shownCount - 1, layout.centerX,
                        composed.lines.back().look.color, opaque});
                else
                    p.rise = 0.0f;
            }
        }
        m_Shown.resize(shownCount);
        // Your target's plate first, then nearer ones, keep their places, and each plate another would cover rises
        // over it, by whole pixels of the image so its text stays sharp.
        std::ranges::sort(stacked, [](const Stacked& a, const Stacked& b) {
            if (a.rank != b.rank) return a.rank > b.rank;
            return a.depth != b.depth ? a.depth < b.depth : a.index < b.index;
        });
        std::vector<PlateBox> boxes;
        for (const Stacked& s : stacked)
            boxes.push_back(s.box);
        const std::vector<float> rises = StackRises(boxes, kStackGap);
        if (settings.stackLine && !stacked.empty() && m_Solid == nullptr && !m_SolidRefused)
        {
            m_Solid        = CreateTexture(m_Device, &kWhite, 1, 1, m_SolidU, m_SolidV);
            m_SolidRefused = m_Solid == nullptr;
        }
        for (size_t i = 0; i < stacked.size(); ++i)
        {
            const Stacked& s = stacked[i];
            Plate& p         = *s.plate;
            p.rise           = EaseRise(p.rise, rises[i], now - p.riseTime);
            p.riseTime       = now;
            const float up   = std::round(p.rise * toY);
            for (size_t q = s.quads; q < s.quadsEnd; ++q)
                m_Quads[q].gpu.y -= up;
            // A cursor kept on screen must not rise back over the top edge with its plate.
            for (size_t q = s.cursors; q < s.cursorsEnd; ++q)
            {
                GpuQuad& cursor = cursorQuads[q].gpu;
                cursor.y = KeepOnScreen(CursorSpot{cursor.x, cursor.y - up}, cursor.width, cursor.height, screenWidth * toX,
                    screenHeight * toY, m_CoveredTop * toY).y;
            }
            // Drawn after the plate it belongs to, in the plate's rank and at its depth.
            if (const std::optional<PlateBox> line = StackLeader(s.centerX, s.box.bottom, p.rise, kStackLineWidth);
                line && settings.stackLine && m_Solid != nullptr)
            {
                rank   = s.rank;
                opaque = s.alpha;
                addQuad(m_Quads, m_Solid, line->left, line->top, std::max(1.0f, std::round((line->right - line->left) * toX)),
                    (line->bottom - line->top) * toY, m_SolidU, m_SolidV, s.depth, s.color);
            }
            Shown& shown = m_Shown[s.shown];
            if (shown.nameHeight > 0) shown.nameY -= p.rise;
            for (int line = 0; line < shown.lineCount; ++line)
                shown.lines[line].y -= p.rise;
        }

        m_PlateCursors = m_CursorNames.size();
        std::stable_sort(m_Quads.begin(), m_Quads.end(),
            [](const Quad& a, const Quad& b) { return DrawnBefore(a.rank, a.gpu.depth, b.rank, b.gpu.depth); });
        m_Quads.insert(m_Quads.end(), cursorQuads.begin(), cursorQuads.end());

        for (auto it = m_Plates.begin(); it != m_Plates.end();)
        {
            if (it->second.frame == m_Frame)
            {
                ++it;
                continue;
            }
            ReleasePlate(it->second);
            it = m_Plates.erase(it);
        }
        for (PlateTexture& badge : m_Badges.TakeIdle(m_Frame, kIdleFrames))
            ReleaseTexture(badge.texture);
        m_Gdi.Sweep();
    }

    std::vector<LoneCursor> NameplateRenderer::LoneCursorsDue(const Settings& settings, const PlayerState& player,
        const CursorTargets& targets, float screenWidth, float screenHeight) const
    {
        if (!NameplatesOn(settings) || m_NamesFailed || m_Device == nullptr) return {};
        std::vector<uint16_t> withNameplateCursor;
        for (size_t i = 0; i < m_PlateCursors; ++i)
            withNameplateCursor.push_back(m_CursorNames[i].index);
        std::vector<LoneCursor> lone = LoneCursors(targets, settings, player, withNameplateCursor);
        std::erase_if(lone, [&](const LoneCursor& c) { return !AnchorOnScreen(c.x, c.y, screenWidth, screenHeight); });
        return lone;
    }

    void NameplateRenderer::DrawLoneCursors(const std::vector<LoneCursor>& lone, const Settings& settings, double now,
        float screenWidth, float screenHeight)
    {
        m_CursorNames.resize(m_PlateCursors);
        const float shown = static_cast<float>(settings.cursorHeight);
        std::vector<GpuQuad> quads;
        for (const LoneCursor& cursor : lone)
        {
            PlateTexture& t = cursor.under ? m_LoneCursorUnder : m_LoneCursor;
            // A target with no name is no entity HeadsUp knows: a door, read as the NPC or object it is.
            if (!PrepareCursor(t, CursorState(EntityState{}, cursor.target), CursorRaster(shown, 0, false, settings.cursorHeight), settings))
                break;
            if (t.unfit) continue;
            const float fit       = FitOf(shown, t.raster);
            const float width     = t.width * fit, height = t.height * fit;
            const CursorSpot at = CursorAtAnchor(cursor.x, cursor.y, width, height, t.tip, static_cast<float>(settings.cursorRaise));
            const CursorSpot kept = KeepOnScreen(CursorSpot{at.x, at.y + CursorBob(now, height, settings.cursorFloat)}, width,
                height, screenWidth, screenHeight, m_CoveredTop);
            const SquareQuad q = PlaceSquarePixels(kept.x, kept.y, t.width, t.height, t.halo, fit, 1.0f, 1.0f);
            quads.push_back(GpuQuad{t.texture, std::round(q.x), std::round(q.y), q.width, q.height, 0.0f, t.u, t.v, 0.0f, kWhite});
            m_CursorNames.push_back(CursorName{cursor.index, ScreenBox{}});
        }
        if (quads.empty()) return;
        SetQuadStates(m_Device, false);
        for (const GpuQuad& quad : quads)
            DrawQuad(m_Device, quad, false);
        m_Device->SetTexture(0, nullptr);
    }

    void NameplateRenderer::ReleaseLine(LineTextures& line)
    {
        for (PlateTexture& text : line.texts)
            ReleaseTexture(text.texture);
        line.texts.clear();
        ReleaseTexture(line.ornament.texture);
        line.raster         = 0;
        line.ornamentRaster = 0;
    }

    void NameplateRenderer::ReleasePlate(Plate& plate)
    {
        for (LineTextures& line : plate.lines)
            ReleaseLine(line);
        ReleaseTexture(plate.cursor.texture);
        ReleaseTexture(plate.cursorUnder.texture);
        for (PlateTexture& timer : plate.timers)
            ReleaseTexture(timer.texture);
        plate.timers.clear();
    }

    void NameplateRenderer::Clear()
    {
        m_Quads.clear();
        m_Shown.clear();
        m_CursorNames.clear();
        m_PlateCursors = 0;
    }

    void NameplateRenderer::ReleasePlates()
    {
        for (auto& [index, plate] : m_Plates)
            ReleasePlate(plate);
        m_Plates.clear();
        ReleaseTexture(m_LoneCursor.texture);
        ReleaseTexture(m_LoneCursorUnder.texture);
        for (PlateTexture& badge : m_Badges.TakeAll())
            ReleaseTexture(badge.texture);
        ReleaseTexture(m_Solid);
        m_SolidRefused = false;
        m_Gdi.Release();
    }

    void NameplateRenderer::Draw(bool depthTest)
    {
        if (m_Quads.empty() || m_Device == nullptr) return;
        SetQuadStates(m_Device, depthTest);
        for (const Quad& q : m_Quads)
            DrawQuad(m_Device, q.gpu, depthTest);
        m_Device->SetTexture(0, nullptr); // the plates' textures may be released before the game binds its own
    }

    void NameplateRenderer::Release()
    {
        ReleasePlates();
        m_Icons.Release();
        Clear();
    }

    std::string NameplateRenderer::TakeNameFailure() { return std::exchange(m_NameFailure, std::string()); }

    std::string NameplateRenderer::TakeIconFailure() { return m_Icons.TakeFailure(); }
}
