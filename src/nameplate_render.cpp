#include "nameplate_render.h"

#include "argb.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr float kOutlinePerHeight = 0.1f; // the cursor's and badges' outline width, per pixel of their height
        constexpr float kStackGap         = 2.0f; // screen pixels between plates stacked over one another
        constexpr float kPercent          = 100.0f;
        // From the mockup's CSS: a 22 px name's shadows blur 2 and 3 px, 1 px lower, and its glow 14 px.
        constexpr float kShadowDropPerHeight = 0.05f;
        constexpr float kGlowPerHeight       = 0.64f; // the glow's blur radius, per pixel of name height
        constexpr float kOrnamentShadowPerHeight = 0.3f; // the diamond's shadow blur: 3 px on a 9 px ornament

        uint32_t CursorColor(CursorKind kind, const Settings& settings)
        {
            return ToArgb(kind == CursorKind::SubTarget    ? settings.subCursorColor
                          : kind == CursorKind::OutOfRange ? settings.outOfRangeCursorColor
                          : kind == CursorKind::Locked     ? settings.lockedCursorColor
                                                           : settings.cursorColor);
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

    bool NameplateRenderer::Prepare(PlateTexture& t, const char* text, const TextLook& look, int pixelHeight)
    {
        TextureKey key{text, pixelHeight, 0, look};
        if (t.texture != nullptr && t.key == key) return true;
        if (!RedrawNow(t.texture != nullptr, m_RedrawMs)) return true;
        // A name new in a busy frame is drawn at once without its glow, which costs the most: a later frame adds it.
        if (t.texture == nullptr && look.glow != 0 && !RedrawNow(true, m_RedrawMs))
        {
            TextLook plain = look;
            plain.glow     = 0;
            return Prepare(t, text, plain, pixelHeight);
        }
        const Stopwatch stopwatch(m_RedrawMs);
        const auto height    = static_cast<float>(pixelHeight);
        const float shadow   = height * kShadowPerHeight;
        const int drop       = static_cast<int>(std::lround(height * kShadowDropPerHeight));
        const float glowBlur = look.glow != 0 ? height * kGlowPerHeight * look.glowSize : 0.0f;
        const int edge       = OutlineRadius(pixelHeight);
        const int margin = look.line.outline ? edge : BlurMargin(std::max(glowBlur, shadow + static_cast<float>(drop)));
        Coverage coverage;
        int markX = kNoMarkColumn;
        if (!m_Gdi.Rasterize(text, look.line.font.c_str(), pixelHeight, look.line.bold, 0, margin, look.markAt,
                coverage, markX))
        {
            FailNames("GDI could not draw text in " + look.line.font + " at " + std::to_string(pixelHeight) + " px");
            return false;
        }
        const Layers layers{shadow, drop, glowBlur, look.color, look.line.shadowColor, look.glow, look.glowStrength,
            look.line.shadowStrength, markX, look.markColor};
        Image image = look.line.outline ? Outlined(coverage, edge, look.color, look.line.shadowColor) : Styled(coverage, layers);
        if (!Upload(t, std::move(image), std::move(key))) return false;
        t.halo = static_cast<float>(margin);
        return true;
    }

    bool NameplateRenderer::PrepareOrnament(PlateTexture& t, uint32_t color, int pixelHeight, int width, const LineStyle& shadowLine)
    {
        TextureKey key{"", pixelHeight, width, TextLook{shadowLine, color}};
        if (t.texture != nullptr && t.key == key) return true;
        if (!RedrawNow(t.texture != nullptr, m_RedrawMs)) return true;
        const Stopwatch stopwatch(m_RedrawMs);
        const float shadow = static_cast<float>(pixelHeight) * kOrnamentShadowPerHeight;
        if (!Upload(t, Ornament(width, pixelHeight, shadow, shadowLine.shadowStrength, color, shadowLine.shadowColor), std::move(key)))
            return false;
        t.halo = static_cast<float>(BlurMargin(shadow));
        return true;
    }

    bool NameplateRenderer::PrepareCursor(PlateTexture& t, uint32_t color, int pixelHeight, const Settings& settings)
    {
        const LineStyle shape{settings.cursorFeather ? "feather" : "arrow", false, ToArgb(settings.textOutline)};
        // Its width is the outline's share, which a texture of the same height must match too.
        TextureKey key{"", pixelHeight, settings.cursorOutline, TextLook{shape, color}};
        if (t.texture != nullptr && t.key == key) return true;
        if (!RedrawNow(t.texture != nullptr, m_RedrawMs)) return true;
        const Stopwatch stopwatch(m_RedrawMs);
        const int radius        = std::max(1, WholePixels(static_cast<float>(pixelHeight) * kOutlinePerHeight *
                                                          static_cast<float>(settings.cursorOutline) / kPercent));
        const Shape& outline    = settings.cursorFeather ? FeatherShape() : ArrowShape();
        const Coverage coverage = ShapeCoverage(outline, pixelHeight, radius);
        if (!Upload(t, Outlined(coverage, radius, color, shape.shadowColor), std::move(key))) return false;
        const float width = static_cast<float>(coverage.width - 2 * radius);
        t.tip             = (static_cast<float>(radius) + ShapeTip(outline) * width) / static_cast<float>(coverage.width);
        return true;
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
        return true;
    }

    void NameplateRenderer::Update(const Tracker& tracker, const GameNames& names, const NameHook& hook, const Settings& settings, float toX,
        float toY, const CursorTargets& targets, double now, uint16_t selfIndex, const std::vector<TimerLine>& selfTimers)
    {
        ++m_Frame;
        m_RedrawMs = 0.0;
        m_Shown.clear();
        m_Quads.clear();
        m_CursorNames.clear();
        const uint32_t iconTint  = ToArgb(settings.iconTint);
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
        // The plates whose names HeadsUp draws, to stack: each one's box and which quads are its.
        struct Stacked
        {
            Plate* plate;
            PlateBox box;
            int rank;
            float depth;
            uint16_t index;
            size_t quads, quadsEnd, cursors, cursorsEnd, shown;
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
                const DrawnName gameName = NameFromFrame(*frame, names.TargetScaleX(), names.TargetScaleY());
                // Checked where our name goes: a seated player's head can be in view with the game's name above the screen.
                ScreenBox anchor = PlaceName(gameName.box, names.SceneCamera(), info->feet, info->pose);
                if (!NameOnScreen(anchor, screenWidth, screenHeight)) continue;
                // The game drawing the name this frame is all a mob's lines wait for.
                const PlateLines lines =
                    ChooseLines(*info, tracker.Player().engaged, settings, targets, m_Icons.Failed());
                const bool timersHere = info->index == selfIndex && !selfTimers.empty();
                if (!lines.Any() && !timersHere) continue;
                Plate& p = m_Plates[info->index];
                rank     = PlateRank(info->index, targets);
                opaque   = PlateAlpha(settings, info->kind, rank, targets.target != 0);
                p.frame  = m_Frame;

                const bool self         = info->index == selfIndex;
                const bool scales       = ScalesWithDistance(settings, info->kind, self);
                p.scale                 = EaseScale(p.scale, DistanceScale(gameName.box.Height(), screenHeight), now - p.scaleTime);
                p.scaleTime             = now;
                const float scale       = scales ? p.scale : 1.0f;
                const float nameShown   = static_cast<float>(NameSize(settings, info->kind, self)) * scale * toY;
                const float labelShown  = static_cast<float>(settings.labelSize) * scale * toY;
                const float timerShown  = static_cast<float>(settings.timerSize) * scale * toY;
                const float iconSize    = static_cast<float>(settings.iconSize) * scale;
                const float cursorShown = static_cast<float>(settings.cursorSize) * (settings.scaleCursor ? p.scale : 1.0f) * toY;
                auto raster             = [&](float shown, int current) {
                    return scales ? RasterHeight(shown, current) : WholePixels(shown);
                };
                p.nameRaster   = raster(nameShown, p.nameRaster);
                p.labelRaster  = raster(labelShown, p.labelRaster);
                p.cursorRaster =
                    settings.scaleCursor ? RasterHeight(cursorShown, p.cursorRaster) : WholePixels(cursorShown);
                const float ornamentShown = static_cast<float>(settings.ornamentThickness) * scale * toY;
                p.ornamentRaster          = raster(ornamentShown, p.ornamentRaster);
                const int ornamentWidth   = WholePixels(static_cast<float>(p.ornamentRaster) *
                                                        static_cast<float>(settings.ornamentWidth) /
                                                        static_cast<float>(settings.ornamentThickness));
                const bool showCursor      = lines.cursor != CursorKind::None;
                const uint32_t cursorColor = CursorColor(lines.cursor, settings);
                const uint32_t nameColor = settings.ownNameColor ? ToArgb(settings.nameColor) : gameName.color;
                const uint32_t labelColor = ToArgb(settings.labelColor[static_cast<int>(info->label.shade)]);
                const PlateStyle style    = StyleFor(*info, lines.label, settings);
                const LineStyle nameLine  = NameLineStyle(settings, info->kind);
                TextLook nameLook{nameLine, nameColor};
                if (style.glow)
                {
                    nameLook.glow         = style.glowColor;
                    nameLook.glowStrength = style.glowStrength;
                    nameLook.glowSize     = style.glowSize;
                }
                TextLook labelLook{LevelLineStyle(settings), labelColor};
                labelLook.markAt    = info->label.markAt;
                labelLook.markColor = Lighter(ToArgb(settings.color[CategoryIndex(Category::Placeholder)]));
                if ((lines.name && !Prepare(p.name, info->name, nameLook, p.nameRaster)) ||
                    (lines.label && !Prepare(p.label, info->label.text, labelLook, p.labelRaster)) ||
                    (style.ornament && !PrepareOrnament(p.ornament, style.ornamentColor, p.ornamentRaster, ornamentWidth, nameLine)) ||
                    (showCursor && !PrepareCursor(p.cursor, cursorColor, p.cursorRaster, settings)))
                    break;
                const float nameFit     = FitOf(nameShown, p.name.key.height);
                const float labelFit    = FitOf(labelShown, p.label.key.height);
                const float cursorFit   = FitOf(cursorShown, p.cursor.key.height);
                const float ornamentFit = FitOf(ornamentShown, p.ornament.key.height);
                for (size_t i = timersHere ? selfTimers.size() : 0; i < p.timers.size(); ++i)
                    ReleaseTexture(p.timers[i].texture);
                p.timers.resize(timersHere ? selfTimers.size() : 0);
                p.timerRaster          = timersHere ? raster(timerShown, p.timerRaster) : 0;
                const uint32_t upColor = ToArgb(settings.color[CategoryIndex(Category::Placeholder)]);
                int timersReady        = 0;
                for (; timersReady < static_cast<int>(p.timers.size()); ++timersReady)
                {
                    const TimerLine& line = selfTimers[timersReady];
                    const TextLook timerLook{labelLook.line, line.up ? upColor : kWhite};
                    if (!Prepare(p.timers[timersReady], line.text.c_str(), timerLook, p.timerRaster)) break;
                }
                const float badgeShown = iconSize * kBadgeShare * toY;
                p.badgeRaster          = lines.infoIconCount > 0 ? raster(badgeShown, p.badgeRaster) : 0;
                std::vector<const PlateTexture*> badges;
                for (int i = 0; i < lines.infoIconCount; ++i)
                {
                    const InfoIcon& item   = info->infoIcons.icons[i];
                    const std::string text = BadgeText(item, settings.showRanks);
                    const TextLook badgeLook{BadgeLineStyle(settings), BadgeColor(item.badge, settings)};
                    PlateTexture& badge = m_Badges.Use(BadgeKey{text, badgeLook.color, p.badgeRaster}, m_Frame);
                    if (!Prepare(badge, text.c_str(), badgeLook, p.badgeRaster)) break;
                    badges.push_back(&badge);
                }
                const float playerIconSize = nameShown / toY * static_cast<float>(settings.playerIconSize) / kPercent;

                LineSizes sizes;
                if (lines.name)
                {
                    sizes.nameWidth  = (p.name.width - 2.0f * p.name.halo) * nameFit / toX;
                    sizes.nameHeight = (p.name.height - 2.0f * p.name.halo) * nameFit / toY;
                }
                if (style.ornament)
                {
                    sizes.ornamentWidth  = (p.ornament.width - 2.0f * p.ornament.halo) * ornamentFit / toX;
                    sizes.ornamentHeight = (p.ornament.height - 2.0f * p.ornament.halo) * ornamentFit / toY;
                }
                if (lines.label)
                {
                    sizes.labelWidth  = (p.label.width - 2.0f * p.label.halo) * labelFit / toX;
                    sizes.labelHeight = (p.label.height - 2.0f * p.label.halo) * labelFit / toY;
                }
                sizes.mobIconCount  = lines.mobIconCount;
                sizes.iconSize      = iconSize;
                sizes.infoIconCount = lines.infoIconCount;
                sizes.detectionLeft = settings.detectionIconSide == IconSide::Left;
                sizes.badgesLeft    = settings.badgeSide == IconSide::Left;
                sizes.iconPlace     = settings.mobIconPlace;
                if (showCursor)
                {
                    sizes.cursorWidth  = p.cursor.width * cursorFit / toX;
                    sizes.cursorHeight = p.cursor.height * cursorFit / toY;
                    sizes.cursorTip    = p.cursor.tip;
                }
                sizes.leftIconCount      = lines.leftIconCount;
                sizes.rightIconCount     = lines.rightIconCount;
                sizes.playerIconSize     = playerIconSize;
                sizes.centerNameAndIcons = settings.centerNameAndIcons;
                sizes.timerCount         = timersReady;
                for (int i = 0; i < timersReady; ++i)
                    sizes.timerHeight = std::max(sizes.timerHeight,
                        (p.timers[i].height - 2.0f * p.timers[i].halo) * FitOf(timerShown, p.timers[i].key.height) / toY);
                if (lines.name)
                {
                    const float raise = static_cast<float>(settings.nameRaise) * scale;
                    anchor.minY -= raise;
                    anchor.maxY -= raise;
                }
                const NameplateLayout layout = LayoutNameplate(anchor, sizes, lines.name);
                const float depth            = gameName.depth;
                const size_t firstQuad = m_Quads.size(), firstCursor = cursorQuads.size();
                auto addText = [&](const PlateTexture& t, float x, float y, float fit) {
                    addQuad(m_Quads, t.texture, x - t.halo * fit / toX, y - t.halo * fit / toY, t.width * fit, t.height * fit, t.u, t.v,
                        depth, kWhite);
                };
                if (lines.name) addText(p.name, layout.nameX, layout.nameY, nameFit);
                if (style.ornament) addText(p.ornament, layout.ornamentX, layout.ornamentY, ornamentFit);
                if (lines.label) addText(p.label, layout.labelX, layout.labelY, labelFit);
                int drawn = 0;
                for (; drawn < lines.mobIconCount; ++drawn)
                {
                    const IconTexture* icon = m_Icons.For(m_Device, info->mobIcons.icons[drawn]);
                    if (icon == nullptr) break;
                    const IconSpot at = IconAt(layout, layout.detection, drawn);
                    addQuad(m_Quads, icon->texture, at.x, at.y, iconSize * toX, iconSize * toY, icon->u, icon->v, depth, iconTint);
                }
                for (int i = 0; i < lines.infoIconCount; ++i)
                {
                    const IconTexture* icon = m_Icons.For(m_Device, info->infoIcons.icons[i].icon);
                    if (icon == nullptr) break;
                    const IconSpot at = IconAt(layout, layout.info, i);
                    addQuad(m_Quads, icon->texture, at.x, at.y, iconSize * toX, iconSize * toY, icon->u, icon->v, depth, iconTint);
                    if (i >= static_cast<int>(badges.size()) || badges[i]->texture == nullptr) continue;
                    const PlateTexture& badge = *badges[i];
                    const float fit           = FitOf(badgeShown, badge.key.height);
                    const IconSpot spot       = BadgeAt(at, iconSize, (badge.width - 2.0f * badge.halo) * fit / toX,
                              (badge.height - 2.0f * badge.halo) * fit / toY);
                    addText(badge, spot.x, spot.y, fit);
                }
                auto addPlayerIcons = [&](const IconSet& row, int count, float x) {
                    for (int i = 0; i < count; ++i)
                    {
                        const Icon kind         = row.icons[i];
                        const IconTexture* icon = m_Icons.For(m_Device, kind);
                        if (icon == nullptr) return;
                        addQuad(m_Quads, icon->texture, x + static_cast<float>(i) * layout.playerIconStep, layout.playerIconsY,
                            playerIconSize * toX, playerIconSize * toY, icon->u, icon->v, depth,
                            kind == Icon::Linkshell ? info->linkshellArgb : kWhite);
                    }
                };
                addPlayerIcons(info->playerIcons.left, lines.leftIconCount, layout.leftIconsX);
                addPlayerIcons(info->playerIcons.right, lines.rightIconCount, layout.rightIconsX);
                if (showCursor)
                {
                    addQuad(cursorQuads, p.cursor.texture, layout.cursor.x, layout.cursor.y + CursorBob(now, sizes.cursorHeight),
                        p.cursor.width * cursorFit, p.cursor.height * cursorFit, p.cursor.u, p.cursor.v, depth, kWhite);
                    m_CursorNames.push_back(CursorName{info->index, gameName.box});
                }
                for (int i = 0; i < sizes.timerCount; ++i)
                {
                    const PlateTexture& t = p.timers[i];
                    const float timerFit  = FitOf(timerShown, t.key.height);
                    addText(t, layout.centerX - (t.width - 2.0f * t.halo) * timerFit / toX / 2.0f,
                        layout.timersY + static_cast<float>(i) * layout.timerStep, timerFit);
                }
                auto shownHeight = [&](bool shown, float pixels) { return shown ? static_cast<int>(std::lround(pixels / toY)) : 0; };
                m_Shown.push_back(Shown{info->index, layout.nameX, layout.nameY, layout.labelX, layout.labelY,
                    lines.mobIconCount > 0 ? IconAt(layout, layout.detection, 0).x : layout.centerX, layout.iconsY, shownHeight(lines.name, nameShown), shownHeight(lines.label, labelShown),
                    static_cast<int>(std::lround(iconSize)), drawn, nameColor});
                // Only a name HeadsUp draws can move: the game's own stays where the game puts it.
                if (settings.stackNames && lines.name)
                    stacked.push_back(Stacked{&p, BoxOf(layout, sizes, true), rank, depth, info->index, firstQuad,
                        m_Quads.size(), firstCursor, cursorQuads.size(), m_Shown.size() - 1});
                else
                    p.rise = 0.0f;
            }
        }
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
        for (size_t i = 0; i < stacked.size(); ++i)
        {
            const Stacked& s = stacked[i];
            Plate& p         = *s.plate;
            p.rise           = EaseRise(p.rise, rises[i], now - p.riseTime);
            p.riseTime       = now;
            const float up   = std::round(p.rise * toY);
            for (size_t q = s.quads; q < s.quadsEnd; ++q)
                m_Quads[q].gpu.y -= up;
            for (size_t q = s.cursors; q < s.cursorsEnd; ++q)
                cursorQuads[q].gpu.y -= up;
            Shown& shown = m_Shown[s.shown];
            shown.nameY -= p.rise;
            shown.labelY -= p.rise;
            shown.iconsY -= p.rise;
        }

        // A lone cursor is drawn at exactly its own size and on top: there is no name to scale by or to sit behind walls with.
        if (NameplatesOn(settings) && !m_NamesFailed && m_Device != nullptr)
        {
            std::vector<uint16_t> withNameplateCursor;
            for (const CursorName& c : m_CursorNames)
                withNameplateCursor.push_back(c.index);
            for (const LoneCursor& lone : LoneCursors(targets, settings, withNameplateCursor))
            {
                Plate& p                = m_Plates[lone.index];
                p.frame                 = m_Frame;
                const float cursorShown = static_cast<float>(settings.cursorSize) * toY;
                p.cursorRaster          = WholePixels(cursorShown);
                if (!PrepareCursor(p.cursor, CursorColor(lone.kind, settings), p.cursorRaster, settings)) break;
                const float fit     = FitOf(cursorShown, p.cursor.key.height);
                const float height  = p.cursor.height * fit / toY;
                const CursorSpot at = CursorAtAnchor(lone.x, lone.y, p.cursor.width * fit / toX, height, p.cursor.tip);
                addQuad(cursorQuads, p.cursor.texture, at.x, at.y + CursorBob(now, height), p.cursor.width * fit,
                    p.cursor.height * fit, p.cursor.u, p.cursor.v, 0.0f, kWhite);
                m_CursorNames.push_back(CursorName{lone.index, ScreenBox{}});
            }
        }

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

    void NameplateRenderer::ReleasePlate(Plate& plate)
    {
        ReleaseTexture(plate.name.texture);
        ReleaseTexture(plate.label.texture);
        ReleaseTexture(plate.cursor.texture);
        ReleaseTexture(plate.ornament.texture);
        for (PlateTexture& timer : plate.timers)
            ReleaseTexture(timer.texture);
        plate.timers.clear();
    }

    void NameplateRenderer::Clear()
    {
        m_Quads.clear();
        m_Shown.clear();
        m_CursorNames.clear();
    }

    void NameplateRenderer::ReleasePlates()
    {
        for (auto& [index, plate] : m_Plates)
            ReleasePlate(plate);
        m_Plates.clear();
        for (PlateTexture& badge : m_Badges.TakeAll())
            ReleaseTexture(badge.texture);
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
