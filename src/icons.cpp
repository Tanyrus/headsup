#include "icons.h"

#include "game_font.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
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
        constexpr float kVisibleShare = 0.5f; // a pixel at least this covered is part of the pearl, and its color its own
        constexpr float kPearlDetailRadius = 2.0f; // XIUI's rim and clasp are finer than this, its shading broader
        constexpr float kMaxDetail         = 2.0f;
        constexpr float kLeastWeight       = 1e-3f;
        constexpr float kRed = 0.299f, kGreen = 0.587f, kBlue = 0.114f;

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

        // The planes with each pixel not known given the mean of its known neighbors, a ring at a time, so a clear
        // pixel's color, which is no one's, never reaches the pearl.
        void Bleed(std::array<Plane, kChannels>& planes, std::vector<bool> known)
        {
            const int width = planes[0].width, height = planes[0].height;
            for (bool grew = true; grew;)
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

    std::vector<unsigned char> GameLitPearl(const ColorPicture& game, const IconBitmap& shape)
    {
        const size_t pixels = static_cast<size_t>(game.width) * static_cast<size_t>(game.height);
        if (game.width <= 0 || game.height <= 0 || game.bgra.size() != pixels * kBytes || shape.width == 0 || shape.height == 0)
            return {};
        // The game's alpha stands at half: twice it is the pearl's coverage.
        std::array<Plane, kChannels> colors;
        colors.fill(Plane{game.width, game.height, std::vector<float>(pixels)});
        std::vector<bool> known(pixels);
        int left = game.width, top = game.height, right = 0, bottom = 0;
        for (size_t p = 0; p < pixels; ++p)
        {
            for (size_t c = 0; c < kChannels; ++c)
                colors[c].values[p] = static_cast<float>(game.bgra[p * kBytes + c]);
            known[p] = 2.0f * static_cast<float>(game.bgra[p * kBytes + kAlpha]) / kFullByte >= kVisibleShare;
            if (!known[p]) continue;
            const int x = static_cast<int>(p % static_cast<size_t>(game.width)), y = static_cast<int>(p / static_cast<size_t>(game.width));
            left = std::min(left, x), top = std::min(top, y), right = std::max(right, x + 1), bottom = std::max(bottom, y + 1);
        }
        if (right <= left) return {};
        Bleed(colors, known);
        for (Plane& plane : colors)
            plane = Smoothed(plane);

        // The shape's detail: its light over its light blurred, weighed by its alpha so the clear around it counts for nothing.
        const int width = static_cast<int>(shape.width), height = static_cast<int>(shape.height);
        const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
        Plane alpha{width, height, std::vector<float>(count)}, light = alpha, lit = alpha;
        for (size_t p = 0; p < count; ++p)
        {
            const unsigned char* px = shape.bgra + p * kBytes;
            alpha.values[p]         = static_cast<float>(px[kAlpha]) / kFullByte;
            light.values[p]         = kBlue * px[0] + kGreen * px[1] + kRed * px[2];
            lit.values[p]           = light.values[p] * alpha.values[p];
        }
        const Plane broadLit = Blurred(lit, kPearlDetailRadius), broadAlpha = Blurred(alpha, kPearlDetailRadius);

        const int side         = std::max(right - left, bottom - top);
        const float scaleX     = static_cast<float>(side) / static_cast<float>(width);
        const float scaleY     = static_cast<float>(side) / static_cast<float>(height);
        const float originX    = static_cast<float>(left) - static_cast<float>(side - (right - left)) / 2.0f;
        const float originY    = static_cast<float>(top) - static_cast<float>(side - (bottom - top)) / 2.0f;
        std::vector<unsigned char> out(count * kBytes);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                const size_t p     = static_cast<size_t>(y * width + x);
                const float broad  = broadAlpha.values[p] > kLeastWeight ? broadLit.values[p] / broadAlpha.values[p] : 0.0f;
                const float detail = broad > kLeastWeight ? std::clamp(light.values[p] / broad, 0.0f, kMaxDetail) : 1.0f;
                const float sx = originX + (static_cast<float>(x) + 0.5f) * scaleX, sy = originY + (static_cast<float>(y) + 0.5f) * scaleY;
                unsigned char* pixel = out.data() + p * kBytes;
                for (size_t c = 0; c < kChannels; ++c)
                    pixel[c] = static_cast<unsigned char>(std::lround(std::clamp(Cubic(colors[c], sx, sy) * detail, 0.0f, kFullByte)));
                pixel[kAlpha] = shape.bgra[p * kBytes + kAlpha];
            }
        return out;
    }
}
