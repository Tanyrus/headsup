#include "icons.h"

#include "game_font.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <vector>

namespace headsup
{
    namespace
    {
#include "generated/icons.inc"

        constexpr double kSdtPerPercent = 100.0;
        constexpr Icon kSdtIcons[kSdtCount] = {Icon::Slashing, Icon::Piercing, Icon::Impact, Icon::H2H, Icon::Fire, Icon::Ice,
            Icon::Wind, Icon::Earth, Icon::Lightning, Icon::Water, Icon::Light, Icon::Dark};
        struct Status
        {
            Icon icon;
            uint32_t immunity;
        };
        constexpr Status kRankStatuses[kStatusRankCount] = {{Icon::Paralyze, kImmuneParalyze}, {Icon::Bind, kImmuneBind},
            {Icon::Silence, kImmuneSilence}, {Icon::Slow, kImmuneSlow}, {Icon::Poison, kImmunePoison},
            {Icon::LightSleep, kImmuneLightSleep}, {Icon::DarkSleep, kImmuneDarkSleep}, {Icon::Blind, kImmuneBlind},
            {Icon::Stun, kImmuneStun}, {Icon::Gravity, kImmuneGravity}};
        // MobDB's order after sleep, which comes first.
        constexpr Status kImmunities[] = {{Icon::Gravity, kImmuneGravity}, {Icon::Bind, kImmuneBind}, {Icon::Stun, kImmuneStun},
            {Icon::Silence, kImmuneSilence}, {Icon::Paralyze, kImmuneParalyze}, {Icon::Blind, kImmuneBlind},
            {Icon::Slow, kImmuneSlow}, {Icon::Poison, kImmunePoison}, {Icon::Elegy, kImmuneElegy},
            {Icon::Requiem, kImmuneRequiem}, {Icon::Petrify, kImmunePetrify}};
        constexpr uint32_t kBothSleeps = kImmuneLightSleep | kImmuneDarkSleep;

        bool Weak(int rank, int cutoff) { return cutoff != kNoWeakRank && rank < 0 && rank <= cutoff; }
        bool Resists(int rank, int cutoff) { return cutoff != kNoResistRank && rank > 0 && rank >= cutoff; }

        void HighestFirst(std::vector<InfoIcon>& icons) { std::ranges::stable_sort(icons, std::greater{}, &InfoIcon::value); }
        void LowestFirst(std::vector<InfoIcon>& icons) { std::ranges::stable_sort(icons, std::less{}, &InfoIcon::value); }

        // Keeps at most `most` of the two lists together, the ranks that change magic evasion most first and then a
        // weakness, then the element order; each list keeps its own order.
        void KeepFurthest(std::vector<InfoIcon>& weak, std::vector<InfoIcon>& resist, int most)
        {
            if (std::ssize(weak) + std::ssize(resist) <= most) return;
            struct Pick
            {
                double distance;
                bool resist;
                size_t index;
            };
            std::vector<Pick> picks;
            for (size_t i = 0; i < weak.size(); ++i)
                picks.push_back({std::abs(EvasionMultiplierOf(weak[i].value) - 1), false, i});
            for (size_t i = 0; i < resist.size(); ++i)
                picks.push_back({std::abs(EvasionMultiplierOf(resist[i].value) - 1), true, i});
            std::ranges::stable_sort(picks, std::greater{}, &Pick::distance);
            picks.resize(static_cast<size_t>(std::max(most, 0)));
            std::vector<bool> keepWeak(weak.size()), keepResist(resist.size());
            for (const Pick& pick : picks)
                (pick.resist ? keepResist : keepWeak)[pick.index] = true;
            auto keep = [](std::vector<InfoIcon>& icons, const std::vector<bool>& kept) {
                std::vector<InfoIcon> left;
                for (size_t i = 0; i < icons.size(); ++i)
                    if (kept[i]) left.push_back(icons[i]);
                icons = std::move(left);
            };
            keep(weak, keepWeak);
            keep(resist, keepResist);
        }

        // Light and dark sleep with the same badge become one Sleep badge in light sleep's place.
        void MergeSleeps(std::vector<InfoIcon>& icons)
        {
            const auto light = std::ranges::find(icons, Icon::LightSleep, &InfoIcon::icon);
            const auto dark  = std::ranges::find(icons, Icon::DarkSleep, &InfoIcon::icon);
            if (light == icons.end() || dark == icons.end() || light->value != dark->value) return;
            light->icon = Icon::Sleep;
            icons.erase(dark);
        }
    }

    DetectionIcon DetectionOf(Icon icon)
    {
        switch (icon)
        {
        case Icon::Link: return DetectionIcon::Link;
        case Icon::Sight: return DetectionIcon::Sight;
        case Icon::TrueSight: return DetectionIcon::TrueSight;
        case Icon::Sound: return DetectionIcon::Sound;
        case Icon::Scent: return DetectionIcon::Scent;
        case Icon::Magic: return DetectionIcon::Magic;
        case Icon::Ability: return DetectionIcon::Ability;
        case Icon::Blood: return DetectionIcon::Blood;
        default: return DetectionIcon::Aggro;
        }
    }

    IconSet IconsFor(const MobRecord* mob, uint32_t detectionShown)
    {
        IconSet set;
        if (mob == nullptr) return set;
        auto add = [&](Icon icon) {
            if ((detectionShown & DetectionBit(DetectionOf(icon))) != 0) set.icons[set.count++] = icon;
        };

        const bool notorious = (mob->flags & kMobNotorious) != 0;
        if (IsAggressive(*mob))
            add(notorious ? Icon::AggroHQ : Icon::AggroNQ);
        else
            add(notorious ? Icon::PassiveHQ : Icon::PassiveNQ);
        if (mob->flags & kMobLink) add(Icon::Link);

        const bool trueDetection = (mob->flags & kMobTrueDetection) != 0;
        if ((mob->detects & kDetectSight) && !trueDetection) add(Icon::Sight);
        if (trueDetection) add(Icon::TrueSight);
        if (mob->detects & kDetectHearing) add(Icon::Sound);
        if (mob->detects & kDetectScent) add(Icon::Scent);
        if (mob->detects & kDetectMagic) add(Icon::Magic);
        if (mob->detects & kDetectAbility) add(Icon::Ability);
        if (mob->detects & kDetectLowHp) add(Icon::Blood);
        return set;
    }

    const char* IconTag(Icon icon)
    {
        switch (icon)
        {
        case Icon::AggroNQ:
        case Icon::AggroHQ: return "Aggressive";
        case Icon::PassiveNQ:
        case Icon::PassiveHQ: return "Passive";
        case Icon::Link: return "Links";
        case Icon::Sight: return "Sight";
        case Icon::TrueSight: return "True sight";
        case Icon::Sound: return "Sound";
        case Icon::Scent: return "Scent";
        case Icon::Magic: return "Magic";
        case Icon::Ability: return "Job abilities";
        case Icon::Blood: return "Low HP";
        default: return "";
        }
    }

    InfoIconSet InfoIconsFor(const MobResistances* mob, const BadgeRules& rules)
    {
        InfoIconSet set;
        if (mob == nullptr) return set;
        std::vector<InfoIcon> weak, weakRank, resist, resistRank, statusWeak, statusResist;
        for (int i = 0; i < kSdtCount; ++i)
        {
            const int percent = SdtPercent(mob->sdt[i]);
            if (percent > 0) weak.push_back({kSdtIcons[i], BadgeKind::Weak, percent});
            if (percent < 0) resist.push_back({kSdtIcons[i], BadgeKind::Resist, -percent});
            if (i < kFirstElementSdt || percent != 0) continue;
            const int rank = mob->elementRanks[i - kFirstElementSdt];
            if (Weak(rank, rules.elementWeakRank)) weakRank.push_back({kSdtIcons[i], BadgeKind::WeakRank, rank});
            if (Resists(rank, rules.elementResistRank)) resistRank.push_back({kSdtIcons[i], BadgeKind::ResistRank, rank});
        }
        KeepFurthest(weakRank, resistRank, rules.maxElements);
        HighestFirst(weak);
        LowestFirst(weakRank);
        HighestFirst(resist);
        HighestFirst(resistRank);
        for (int i = 0; i < kStatusRankCount; ++i)
        {
            const StatusBadgeRule& rule = rules.status[i];
            const int rank              = mob->statusRanks[i];
            if (!rule.show || (mob->immunity & kRankStatuses[i].immunity) != 0) continue;
            if (Weak(rank, rule.weakRank)) statusWeak.push_back({kRankStatuses[i].icon, BadgeKind::StatusWeak, rank});
            if (Resists(rank, rule.resistRank)) statusResist.push_back({kRankStatuses[i].icon, BadgeKind::StatusRank, rank});
        }
        MergeSleeps(statusWeak);
        MergeSleeps(statusResist);
        LowestFirst(statusWeak);
        HighestFirst(statusResist);
        auto add = [&](const InfoIcon& icon) {
            if (set.count < kMaxInfoIcons) set.icons[set.count++] = icon;
        };
        for (const std::vector<InfoIcon>* part : {&weak, &weakRank, &resist, &resistRank, &statusWeak, &statusResist})
            for (const InfoIcon& icon : *part)
                add(icon);
        const uint32_t sleeps = mob->immunity & kBothSleeps;
        if (sleeps == kBothSleeps) add({Icon::Sleep, BadgeKind::Immune, 0});
        else if (sleeps == kImmuneLightSleep) add({Icon::LightSleep, BadgeKind::Immune, 0});
        else if (sleeps == kImmuneDarkSleep) add({Icon::DarkSleep, BadgeKind::Immune, 0});
        for (const Status& status : kImmunities)
            if ((mob->immunity & status.immunity) != 0) add({status.icon, BadgeKind::Immune, 0});
        return set;
    }

    InfoIconSet InfoIconsOf(const MobResistances* mob, const BadgeRules& rules, BadgeGroup group)
    {
        const InfoIconSet all = InfoIconsFor(mob, rules);
        InfoIconSet set;
        for (int i = 0; i < all.count; ++i)
        {
            BadgeGroup of = BadgeGroup::Immune;
            switch (all.icons[i].badge)
            {
            case BadgeKind::Weak:
            case BadgeKind::WeakRank: of = BadgeGroup::Weak; break;
            case BadgeKind::Resist:
            case BadgeKind::ResistRank: of = BadgeGroup::Resist; break;
            case BadgeKind::StatusWeak:
            case BadgeKind::StatusRank: of = BadgeGroup::Status; break;
            case BadgeKind::Immune: break;
            }
            if (of == group) set.icons[set.count++] = all.icons[i];
        }
        return set;
    }

    std::string PercentText(int percent) { return (percent > 0 ? "+" : "") + std::to_string(percent) + "%"; }

    std::optional<MagicTarget> MagicTargetOf(const MobRecord& mob, int checkedLevel)
    {
        const int level        = checkedLevel > 0 ? checkedLevel - mob.expLevelMod : mob.maxLevel;
        const MobStats* stats  = StatsAt(mob, level);
        if (stats == nullptr) return std::nullopt;
        return MagicTarget{level, *stats, ResistancesOf(mob)};
    }

    std::string ElementRankText(Element element, int rank, const RankNumbers& numbers)
    {
        return RankNumberText(numbers.display, numbers.countAccuracy, rank, NukeOf(element), numbers.caster, numbers.target);
    }

    std::string StatusRankText(StatusRank status, int rank, const RankNumbers& numbers)
    {
        return RankNumberText(numbers.display, numbers.countAccuracy, rank, StatusSpellOf(status, numbers.caster),
            numbers.caster, numbers.target);
    }

    std::string BadgeText(const InfoIcon& icon, const RankNumbers& numbers)
    {
        switch (icon.badge)
        {
        case BadgeKind::Weak: return PercentText(icon.value);
        case BadgeKind::Resist: return PercentText(-icon.value);
        case BadgeKind::Immune: return kTimesSign;
        case BadgeKind::WeakRank:
        case BadgeKind::ResistRank:
        {
            const auto sdt = std::ranges::find(kSdtIcons, icon.icon) - std::begin(kSdtIcons);
            return ElementRankText(static_cast<Element>(sdt - kFirstElementSdt), icon.value, numbers);
        }
        case BadgeKind::StatusWeak:
        case BadgeKind::StatusRank: break;
        }
        // One Sleep badge for both stands for dark sleep's spell.
        const auto status = icon.icon == Icon::Sleep ? static_cast<std::ptrdiff_t>(StatusRank::DarkSleep)
                                                     : std::ranges::find(kRankStatuses, icon.icon, &Status::icon) - std::begin(kRankStatuses);
        return StatusRankText(static_cast<StatusRank>(status), icon.value, numbers);
    }

    Icon SdtIcon(int sdt) { return kSdtIcons[sdt]; }

    int SdtPercent(int16_t sdt) { return static_cast<int>(std::lround(sdt / kSdtPerPercent)); }

    Icon StatusIcon(int status) { return kRankStatuses[status].icon; }

    uint32_t StatusImmunity(int status) { return kRankStatuses[status].immunity; }

    const IconBitmap& IconImage(Icon icon)
    {
        return kIcons[static_cast<size_t>(icon)];
    }

    namespace
    {
        constexpr size_t kBytes = 4, kChannels = 3, kAlpha = 3;
        constexpr float kFullByte = 255.0f;
        constexpr float kVisibleShare = 0.5f; // a pixel of the game's at least this covered is part of its pearl
        constexpr float kSolidShare   = 0.9f; // and its color its own: the game's clasp and rim are paler and partly clear
        constexpr int kAllRings       = std::numeric_limits<int>::max();
        // XIUI's edge is all or nothing at its 59 pixels: blurred this much and traced at kTraceSamples to a pixel, its
        // steps are gone and its clasp's notch is not.
        constexpr unsigned char kShapeSolidAlpha = 128;
        constexpr float kEdgeBlur                = 0.8f;
        constexpr int kTraceSamples              = 4;
        constexpr int kTraceMargin               = 2; // past the shape's edge, where the cubic reads
        // XIUI's outline: 295 of its pixels along 172 of edge, near black (2 over its body's 127).
        constexpr float kXiuiOutline   = 1.7f;
        constexpr float kOutlineShade  = 0.02f;
        constexpr int kClearBleedRings = 2; // the clear pixels a linear filter reads beside the edge
        constexpr float kFarAway       = 1e20f;

        // A plane of floats over a picture, read clamped to its edge.
        struct Plane
        {
            int width = 0, height = 0;
            std::vector<float> values;
            float At(int x, int y) const
            {
                return values[static_cast<size_t>(std::clamp(y, 0, height - 1) * width + std::clamp(x, 0, width - 1))];
            }
        };

        // The plane smoothed by 1, 2, 1 across and down: DXT gives each block of 4 x 4 four colors, which the
        // pearl's sheen shows as a checker when drawn larger.
        Plane Smoothed(const Plane& plane)
        {
            Plane across = plane, out = plane;
            for (int y = 0; y < plane.height; ++y)
                for (int x = 0; x < plane.width; ++x)
                    across.values[static_cast<size_t>(y * plane.width + x)] =
                        (plane.At(x - 1, y) + 2.0f * plane.At(x, y) + plane.At(x + 1, y)) / 4.0f;
            for (int y = 0; y < plane.height; ++y)
                for (int x = 0; x < plane.width; ++x)
                    out.values[static_cast<size_t>(y * plane.width + x)] =
                        (across.At(x, y - 1) + 2.0f * across.At(x, y) + across.At(x, y + 1)) / 4.0f;
            return out;
        }

        // The plane blurred by a Gaussian of this radius across and down, nothing past its edges.
        Plane Blurred(const Plane& plane, float radius)
        {
            const auto at = [](const Plane& p, int x, int y) { return x < 0 || y < 0 || x >= p.width || y >= p.height ? 0.0f : p.At(x, y); };
            const int reach = static_cast<int>(std::ceil(3.0f * radius));
            std::vector<float> weights;
            float total = 0.0f;
            for (int d = -reach; d <= reach; ++d)
            {
                weights.push_back(std::exp(-static_cast<float>(d * d) / (2.0f * radius * radius)));
                total += weights.back();
            }
            Plane across = plane, out = plane;
            for (int y = 0; y < plane.height; ++y)
                for (int x = 0; x < plane.width; ++x)
                {
                    float sum = 0.0f;
                    for (int d = -reach; d <= reach; ++d)
                        sum += weights[static_cast<size_t>(d + reach)] * at(plane, x + d, y);
                    across.values[static_cast<size_t>(y * plane.width + x)] = sum / total;
                }
            for (int y = 0; y < plane.height; ++y)
                for (int x = 0; x < plane.width; ++x)
                {
                    float sum = 0.0f;
                    for (int d = -reach; d <= reach; ++d)
                        sum += weights[static_cast<size_t>(d + reach)] * at(across, x, y + d);
                    out.values[static_cast<size_t>(y * plane.width + x)] = sum / total;
                }
            return out;
        }

        // The planes with each pixel not known given the mean of its known neighbors, a ring at a time for so many
        // rings, so a clear pixel's color, which is no one's, never reaches the pearl.
        void Bleed(std::array<Plane, kChannels>& planes, std::vector<bool> known, int rings)
        {
            const int width = planes[0].width, height = planes[0].height;
            for (bool grew = true; grew && rings-- > 0;)
            {
                grew = false;
                std::vector<bool> next = known;
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x)
                    {
                        const size_t at = static_cast<size_t>(y * width + x);
                        if (known[at]) continue;
                        std::array<float, kChannels> sum{};
                        int count = 0;
                        for (int ny = std::max(0, y - 1); ny <= std::min(height - 1, y + 1); ++ny)
                            for (int nx = std::max(0, x - 1); nx <= std::min(width - 1, x + 1); ++nx)
                            {
                                const size_t n = static_cast<size_t>(ny * width + nx);
                                if (!known[n]) continue;
                                for (size_t c = 0; c < kChannels; ++c)
                                    sum[c] += planes[c].values[n];
                                ++count;
                            }
                        if (count == 0) continue;
                        for (size_t c = 0; c < kChannels; ++c)
                            planes[c].values[at] = sum[c] / static_cast<float>(count);
                        next[at] = grew = true;
                    }
                known = std::move(next);
            }
        }

        // Catmull-Rom's weights for the four pixels around a point t of the way past the second.
        std::array<float, 4> CubicWeights(float t)
        {
            const float t2 = t * t, t3 = t2 * t;
            return {(-t3 + 2.0f * t2 - t) / 2.0f, (3.0f * t3 - 5.0f * t2 + 2.0f) / 2.0f, (-3.0f * t3 + 4.0f * t2 + t) / 2.0f,
                (t3 - t2) / 2.0f};
        }

        // The plane at a point in pixels, the first pixel's middle at 0.5.
        float Cubic(const Plane& plane, float x, float y)
        {
            x -= 0.5f, y -= 0.5f;
            const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
            const std::array<float, 4> wx = CubicWeights(x - static_cast<float>(ix)), wy = CubicWeights(y - static_cast<float>(iy));
            float sum = 0.0f;
            for (int j = 0; j < 4; ++j)
                for (int i = 0; i < 4; ++i)
                    sum += wx[static_cast<size_t>(i)] * wy[static_cast<size_t>(j)] * plane.At(ix + i - 1, iy + j - 1);
            return sum;
        }

        // The plane with margin pixels of nothing around it.
        Plane Padded(const Plane& plane, int margin)
        {
            Plane out{plane.width + 2 * margin, plane.height + 2 * margin, {}};
            out.values.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height), 0.0f);
            for (int y = 0; y < plane.height; ++y)
                std::copy_n(plane.values.begin() + y * plane.width, plane.width, out.values.begin() + (y + margin) * out.width + margin);
            return out;
        }

        // Each cell's squared distance to the nearest cell of 0, the others kFarAway, in cells: along each line the
        // lower envelope of the parabolas from its cells (Felzenszwalb and Huttenlocher), across and then down.
        void SquaredDistances(std::vector<float>& grid, int width, int height)
        {
            const size_t longest = static_cast<size_t>(std::max(width, height));
            std::vector<float> f(longest), d(longest), z(longest + 1);
            std::vector<int> v(longest);
            const auto line = [&](int n, auto cell) {
                for (int q = 0; q < n; ++q)
                    f[static_cast<size_t>(q)] = cell(q);
                const auto meet = [&](int q, int p) {
                    return (f[static_cast<size_t>(q)] + static_cast<float>(q * q) - f[static_cast<size_t>(p)] - static_cast<float>(p * p)) /
                           static_cast<float>(2 * (q - p));
                };
                size_t k = 0;
                v[0]     = 0;
                z[0] = -kFarAway, z[1] = kFarAway;
                for (int q = 1; q < n; ++q)
                {
                    float s = meet(q, v[k]);
                    while (s <= z[k])
                        s = meet(q, v[--k]);
                    ++k;
                    v[k] = q;
                    z[k] = s, z[k + 1] = kFarAway;
                }
                k = 0;
                for (int q = 0; q < n; ++q)
                {
                    while (z[k + 1] < static_cast<float>(q))
                        ++k;
                    const int p               = v[k];
                    d[static_cast<size_t>(q)] = static_cast<float>((q - p) * (q - p)) + f[static_cast<size_t>(p)];
                }
                for (int q = 0; q < n; ++q)
                    cell(q) = d[static_cast<size_t>(q)];
            };
            for (int y = 0; y < height; ++y)
                line(width, [&](int x) -> float& { return grid[static_cast<size_t>(y * width + x)]; });
            for (int x = 0; x < width; ++x)
                line(height, [&](int y) -> float& { return grid[static_cast<size_t>(y * width + x)]; });
        }
    }

    std::vector<unsigned char> PaintedIcon(const IconBitmap& icon, uint32_t color)
    {
        constexpr double kHalfStrength = 255.0 / 2.0; // a color channel at this leaves the texture as it is
        const size_t pixels = static_cast<size_t>(icon.width) * icon.height;
        std::vector<unsigned char> out(icon.bgra, icon.bgra + pixels * kBytes);
        for (size_t c = 0; c < kChannels; ++c)
        {
            const double gain = static_cast<double>(color >> (8 * c) & 0xFF) / kHalfStrength;
            for (size_t p = 0; p < pixels; ++p)
            {
                unsigned char& channel = out[p * kBytes + c];
                channel = static_cast<unsigned char>(std::min<long>(255, std::lround(channel * gain)));
            }
        }
        return out;
    }

    std::vector<unsigned char> GameLitPearl(const ColorPicture& game, const IconBitmap& shape, int outlinePercent)
    {
        const size_t pixels = static_cast<size_t>(game.width) * static_cast<size_t>(game.height);
        if (game.width <= 0 || game.height <= 0 || game.bgra.size() != pixels * kBytes || shape.width == 0 || shape.height == 0)
            return {};
        // The game's alpha stands at half: twice it is the pearl's coverage.
        std::array<Plane, kChannels> colors;
        colors.fill(Plane{game.width, game.height, std::vector<float>(pixels)});
        std::vector<bool> solid(pixels);
        int left = game.width, top = game.height, right = 0, bottom = 0;
        for (size_t p = 0; p < pixels; ++p)
        {
            for (size_t c = 0; c < kChannels; ++c)
                colors[c].values[p] = static_cast<float>(game.bgra[p * kBytes + c]);
            const float share = 2.0f * static_cast<float>(game.bgra[p * kBytes + kAlpha]) / kFullByte;
            solid[p]          = share >= kSolidShare;
            if (share < kVisibleShare) continue;
            const int x = static_cast<int>(p % static_cast<size_t>(game.width)), y = static_cast<int>(p / static_cast<size_t>(game.width));
            left = std::min(left, x), top = std::min(top, y), right = std::max(right, x + 1), bottom = std::max(bottom, y + 1);
        }
        if (right <= left || std::find(solid.begin(), solid.end(), true) == solid.end()) return {};
        Bleed(colors, solid, kAllRings);
        for (Plane& plane : colors)
            plane = Smoothed(plane);

        // The shape's edge traced smooth: its solid pixels blurred, read at kTraceSamples a pixel and cut at half,
        // inside a ring of samples that are outside, so the shape's own edge is an edge too.
        const int width = static_cast<int>(shape.width), height = static_cast<int>(shape.height);
        const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
        Plane shapeSolid{width, height, std::vector<float>(count)};
        for (size_t p = 0; p < count; ++p)
            shapeSolid.values[p] = shape.bgra[p * kBytes + kAlpha] >= kShapeSolidAlpha ? 1.0f : 0.0f;
        const Plane soft      = Padded(Blurred(shapeSolid, kEdgeBlur), kTraceMargin);
        const int samplesWide = width * kTraceSamples + 2, samplesTall = height * kTraceSamples + 2;
        std::vector<float> distance(static_cast<size_t>(samplesWide) * static_cast<size_t>(samplesTall), 0.0f);
        for (int sy = 1; sy < samplesTall - 1; ++sy)
            for (int sx = 1; sx < samplesWide - 1; ++sx)
            {
                const float x = static_cast<float>(kTraceMargin) + (static_cast<float>(sx - 1) + 0.5f) / kTraceSamples;
                const float y = static_cast<float>(kTraceMargin) + (static_cast<float>(sy - 1) + 0.5f) / kTraceSamples;
                if (Cubic(soft, x, y) >= 0.5f) distance[static_cast<size_t>(sy * samplesWide + sx)] = kFarAway;
            }
        SquaredDistances(distance, samplesWide, samplesTall);
        // A sample is on the outline within its width of the edge, which lies halfway to the nearest outside sample.
        const float reach  = static_cast<float>(outlinePercent) / kPercent * kXiuiOutline * kTraceSamples + 0.5f;
        const float reach2 = outlinePercent > 0 ? reach * reach : 0.0f;

        const int side      = std::max(right - left, bottom - top);
        const float scaleX  = static_cast<float>(side) / static_cast<float>(width);
        const float scaleY  = static_cast<float>(side) / static_cast<float>(height);
        const float originX = static_cast<float>(left) - static_cast<float>(side - (right - left)) / 2.0f;
        const float originY = static_cast<float>(top) - static_cast<float>(side - (bottom - top)) / 2.0f;
        std::array<Plane, kChannels> lit;
        lit.fill(Plane{width, height, std::vector<float>(count)});
        std::vector<unsigned char> alpha(count);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                int inside = 0, outline = 0;
                for (int j = 0; j < kTraceSamples; ++j)
                    for (int i = 0; i < kTraceSamples; ++i)
                    {
                        const float d = distance[static_cast<size_t>((y * kTraceSamples + j + 1) * samplesWide + x * kTraceSamples + i + 1)];
                        inside += d > 0.0f;
                        outline += d > 0.0f && d < reach2;
                    }
                const size_t p    = static_cast<size_t>(y * width + x);
                alpha[p]          = static_cast<unsigned char>(std::lround(kFullByte * static_cast<float>(inside) / (kTraceSamples * kTraceSamples)));
                const float shade = inside > 0 ? 1.0f - (1.0f - kOutlineShade) * static_cast<float>(outline) / static_cast<float>(inside) : 1.0f;
                const float sx = originX + (static_cast<float>(x) + 0.5f) * scaleX, sy = originY + (static_cast<float>(y) + 0.5f) * scaleY;
                for (size_t c = 0; c < kChannels; ++c)
                    lit[c].values[p] = Cubic(colors[c], sx, sy) * shade;
            }
        // Drawn smaller, a clear pixel's color mixes into the edge beside it: it takes the edge's.
        std::vector<bool> seen(count);
        for (size_t p = 0; p < count; ++p)
            seen[p] = alpha[p] > 0;
        Bleed(lit, seen, kClearBleedRings);
        std::vector<unsigned char> bgra(count * kBytes);
        for (size_t p = 0; p < count; ++p)
        {
            for (size_t c = 0; c < kChannels; ++c)
                bgra[p * kBytes + c] = static_cast<unsigned char>(std::lround(std::clamp(lit[c].values[p], 0.0f, kFullByte)));
            bgra[p * kBytes + kAlpha] = alpha[p];
        }
        return bgra;
    }
}
