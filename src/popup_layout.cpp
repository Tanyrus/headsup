#include "popup_layout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>

namespace headsup
{
    namespace
    {
        constexpr float kTitleIcon   = 1.6f;  // the title icon, in popupText
        constexpr float kCellIcon    = 1.1f;
        constexpr float kMark        = 0.7f;  // the box an immune mark's ink is centered in
        constexpr float kIconGap     = 0.3f;
        constexpr float kStar        = 0.85f; // a starred cell's star, about as tall as a capital
        constexpr float kCellGap     = 0.6f;  // between cells of a line
        constexpr float kColumnGap   = 0.8f;
        constexpr float kPairGap     = 1.2f;  // the least between a pair's label and its value
        constexpr float kTagPadX     = 0.6f, kTagPadY = 0.4f, kTagGap = 0.3f; // from a tag's ink to its frame
        constexpr float kCardPad     = 0.55f; // a card's contents from its edges
        constexpr float kCardGap     = 0.45f; // between cards, and between the columns of them
        constexpr float kMinWidth    = 12.0f; // popupText, so a short popup is not a sliver
        constexpr float kFoldArrow   = 0.6f;  // a fold row's arrow
        constexpr float kSocketPad   = 0.35f; // above and below the rows in an open fold's socket
        constexpr size_t kMostSideBySide  = 4; // groups of a tall grid's lines set side by side before a card goes on
        constexpr size_t kSideBySideAbove = 6; // the lines a grid has before it is set in groups
        constexpr size_t kSocketLines     = 10; // the lines a socket shows, the rest scrolling
        constexpr float kScrollbar        = 0.25f; // a scrollbar's width

        std::string Upper(std::string text)
        {
            for (char& c : text)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return text;
        }

        Span HeadingSpan(const std::string& heading, bool ornate)
        {
            return Span{ornate ? heading : Upper(heading), Tone::Heading, 0};
        }

        // Its ink from the cell's top: the icon's, and its text's font's.
        TextSize CellSize(const PopupCell& cell, float unit, SpanMeasure measure)
        {
            TextSize size;
            if (cell.icon) size = TextSize{unit * kCellIcon, unit * kCellIcon, 0.0f, unit * kCellIcon};
            bool inked = cell.icon.has_value();
            for (const Span& span : cell.text)
            {
                const TextSize t = measure(span, PopupFont::Text);
                size.width += t.width + (cell.icon && &span == &cell.text.front() ? unit * kIconGap : 0.0f);
                size.height    = std::max(size.height, t.height);
                size.inkTop    = inked ? std::min(size.inkTop, t.inkTop) : t.inkTop;
                size.inkBottom = inked ? std::max(size.inkBottom, t.inkBottom) : t.inkBottom;
                inked          = true;
            }
            if (cell.star) size.width += unit * (kIconGap + kStar);
            return size;
        }

        // From a tag's ink to its frame, in whole pixels: the frame and its text, rounded to pixels alike when drawn,
        // keep all of it.
        float TagPadX(float unit) { return std::round(unit * kTagPadX); }
        float TagPadY(float unit) { return std::round(unit * kTagPadY); }

        // A tag's frame around its cell's ink, with where that ink is in it.
        TextSize Framed(const TextSize& cell, float unit)
        {
            const float ink = cell.inkBottom - cell.inkTop, padY = TagPadY(unit);
            return TextSize{cell.width + 2.0f * TagPadX(unit), ink + 2.0f * padY, padY, ink + padY};
        }

        // A section laid out on its own, from its card's top left: its pieces, the width its rows need and its
        // height, and which pieces are pair values to move to the card's right edge once its width is known.
        struct Block
        {
            std::vector<PopupPiece> pieces;
            float width = 0.0f, height = 0.0f; // the card's, its padding included
            float pad   = 0.0f;                // 0 for the rows under the title, which have no card
            struct Value
            {
                size_t first, last; // pieces [first, last)
                float right;        // where the value ends now
                float inset;        // how far in from the card's padding it ends: a socket's room on its right
            };
            std::vector<Value> values;
            struct Fold
            {
                std::string key;
                float y, height;   // its row, with half the gap to each neighbour
                float x = -1.0f;   // a tab's chip, from the card's left; below 0 for the whole row across the card
                float width = 0.0f;
                std::string tab{};
            };
            std::vector<Fold> folds;
            std::vector<size_t> sockets; // pieces to widen to the card's inside once its width is known
            struct Anchor
            {
                size_t piece;
                float fromRight; // its left edge, from the card's right
            };
            std::vector<Anchor> anchors; // pieces placed from the card's right once its width is known
            struct Scroll
            {
                std::string key;
                float y, height;
                size_t most;              // lines it scrolls
                float trackY, track, thumb; // its scrollbar
                float grip;               // the right padding's width, the scrollbar in it
            };
            std::vector<Scroll> scrolls;
            bool card     = false;
            float reserve = 0.0f; // its height with its tallest tab and its folds closed, which sets its column
        };

        Block LayoutSection(const PopupSection& section, const BarMetrics& metrics, bool ornate, SpanMeasure measure)
        {
            const float unit = metrics.popupText;
            Block b;
            b.card = !section.heading.empty();
            b.pad  = b.card ? std::round(unit * kCardPad) : 0.0f;
            float y = b.pad, contentWidth = 0.0f;
            if (b.card)
            {
                Span heading     = HeadingSpan(section.heading, ornate);
                const TextSize t = measure(heading, PopupFont::Heading);
                contentWidth     = t.width + t.overhang;
                b.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, b.pad, y, t.width, t.height, std::move(heading),
                    PopupFont::Heading});
                y += t.height + metrics.rowGap;
            }
            // The section's rows, then its picked tab's.
            std::vector<PopupRow> rows = section.rows;
            if (!section.tabs.empty())
            {
                const size_t picked = std::min(section.tab, section.tabs.size() - 1);
                rows.insert(rows.end(), section.tabs[picked].rows.begin(), section.tabs[picked].rows.end());
                // The card as wide as its widest tab's rows, whichever is picked, its chips wrapping at that.
                for (const PopupTab& tab : section.tabs)
                    contentWidth = std::max(contentWidth, LayoutSection(PopupSection{"", tab.rows}, metrics, ornate, measure).width);
                const float gap = unit * kTagGap;
                float x = b.pad, lineHeight = 0.0f;
                for (size_t i = 0; i < section.tabs.size(); ++i)
                {
                    const TextSize size   = CellSize(Cell(section.tabs[i].name), unit, measure);
                    const TextSize framed = Framed(size, unit);
                    if (x > b.pad && x + framed.width > b.pad + contentWidth)
                    {
                        x = b.pad;
                        y += lineHeight + gap;
                        lineHeight = 0.0f;
                    }
                    PopupPiece chip{PopupPiece::Kind::Tab, x, y, framed.width, framed.height, {}};
                    chip.open = i == picked;
                    b.pieces.push_back(std::move(chip));
                    b.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, x + TagPadX(unit), y + framed.inkTop - size.inkTop,
                        size.width, size.height, section.tabs[i].name});
                    b.folds.push_back(Block::Fold{section.heading, y, framed.height, x - b.pad, framed.width, section.tabs[i].name.text});
                    contentWidth = std::max(contentWidth, x + framed.width - b.pad);
                    x += framed.width + gap;
                    lineHeight = std::max(lineHeight, framed.height);
                }
                y += lineHeight + metrics.rowGap;
            }
            // Grid rows one after another with as many columns line them up: a grid carried on across rows, or cards.
            std::vector<std::vector<float>> runColumns(rows.size());
            for (size_t first = 0, last = 0; first < rows.size(); first = std::max(last, first + 1))
            {
                const PopupRow& head = rows[first];
                last = first;
                if (head.kind != RowKind::Grid || head.columns <= 0) continue;
                std::vector<float> widths(static_cast<size_t>(head.columns), 0.0f);
                for (; last < rows.size() && rows[last].kind == RowKind::Grid && rows[last].columns == head.columns; ++last)
                    for (size_t i = 0; i < rows[last].cells.size(); ++i)
                        widths[i % widths.size()] =
                            std::max(widths[i % widths.size()], CellSize(rows[last].cells[i], unit, measure).width);
                for (size_t r = first; r < last; ++r)
                    runColumns[r] = widths;
            }
            std::vector<TextSize> cells;
            std::vector<float> columns;
            const float socketPad = std::round(unit * kSocketPad), inset = std::round(unit * (kFoldArrow + kIconGap));
            // A row's lines that scroll: a grid's but for its heads, which stay; any other row is one.
            const auto linesOf = [](const PopupRow& row) -> size_t {
                if (row.kind != RowKind::Grid || row.columns <= 0) return 1;
                const auto columns = static_cast<size_t>(row.columns);
                const size_t head  = row.header ? columns : 0;
                return row.cells.size() > head ? (row.cells.size() - head + columns - 1) / columns : 0;
            };
            // The open socket, for the nested rows now being laid out: which of its lines show, and where it scrolls.
            struct Socket
            {
                size_t piece;
                std::string key;
                size_t lines = 0, shown = 0, from = 0, at = 0; // its lines, how many show, the first shown, the next
                float top  = 0.0f;                             // its first row's
                size_t bar = 0;                                // its scrollbar's track, then its thumb, in the pieces
            };
            std::optional<Socket> socket;
            const auto closeSocket = [&] {
                if (!socket) return;
                if (socket->shown < socket->lines)
                {
                    // A scrollbar down the right padding beside its rows, its thumb the share of them shown.
                    const float bar   = std::max(1.0f, std::round(unit * kScrollbar));
                    const float track = y - metrics.rowGap - socket->top;
                    const float thumb = track * static_cast<float>(socket->shown) / static_cast<float>(socket->lines);
                    const float down  = static_cast<float>(socket->from) / static_cast<float>(socket->lines - socket->shown);
                    const float right = b.pad + (inset + bar) / 2.0f;
                    socket->bar       = b.pieces.size();
                    b.anchors.push_back(Block::Anchor{b.pieces.size(), right});
                    b.pieces.push_back(PopupPiece{PopupPiece::Kind::ScrollTrack, 0.0f, socket->top, bar, track, {}});
                    b.anchors.push_back(Block::Anchor{b.pieces.size(), right});
                    b.pieces.push_back(
                        PopupPiece{PopupPiece::Kind::ScrollThumb, 0.0f, socket->top + (track - thumb) * down, bar, thumb, {}});
                    const Span where{std::to_string(socket->from + 1) + " to " + std::to_string(socket->from + socket->shown) +
                                         " of " + std::to_string(socket->lines) + kDot + "scroll for more",
                        Tone::Dim, 0};
                    const TextSize t = measure(where, PopupFont::Text);
                    b.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, b.pad + inset, y, t.width, t.height, where});
                    contentWidth = std::max(contentWidth, 2.0f * inset + t.width);
                    y += t.height + metrics.rowGap;
                }
                PopupPiece& ground = b.pieces[socket->piece];
                ground.height      = y - metrics.rowGap + socketPad - ground.y;
                y                  = ground.y + ground.height + metrics.rowGap;
                b.sockets.push_back(socket->piece);
                if (socket->shown < socket->lines)
                {
                    const PopupPiece& track = b.pieces[socket->bar], &thumb = b.pieces[socket->bar + 1];
                    b.scrolls.push_back(Block::Scroll{socket->key, ground.y, ground.height, socket->lines - socket->shown,
                        track.y, track.height, thumb.height, inset});
                }
                socket.reset();
            };
            // Whether a socket's line shows, taking the next of its lines; heads always show.
            const auto showLine = [&](const PopupRow& row, bool heads) {
                if (!socket || !row.nested || heads) return true;
                const size_t line = socket->at++;
                return line >= socket->from && line < socket->from + socket->shown;
            };
            for (size_t r = 0; r < rows.size(); ++r)
            {
                const PopupRow& row = rows[r];
                if (!row.nested && !row.hidden) closeSocket();
                if (row.cells.empty()) continue;
                const bool grid = row.kind == RowKind::Grid && row.columns > 0;
                const bool tags = row.kind == RowKind::Tags;
                const bool pair = row.kind == RowKind::Pair && row.cells.size() == 2;
                const float gap = unit * (tags ? kTagGap : pair ? kPairGap : kCellGap);
                const bool fold = !row.fold.empty();
                // A fold row's cells after its arrow, and the rows it holds lined up with them, as far in from the
                // socket's right as from its left.
                const float indent = fold || row.nested ? inset : 0.0f;
                cells.clear();
                columns = grid ? runColumns[r] : std::vector<float>{};
                float rowWidth = indent + (row.nested ? inset : 0.0f);
                for (size_t i = 0; i < row.cells.size(); ++i)
                {
                    const TextSize s = cells.emplace_back(CellSize(row.cells[i], unit, measure));
                    if (!grid) rowWidth += (tags ? Framed(s, unit) : s).width + (i > 0 ? gap : 0.0f);
                }
                for (const float c : columns)
                    rowWidth += c;
                if (grid) rowWidth += unit * kColumnGap * static_cast<float>(columns.size() - 1);
                contentWidth = std::max(contentWidth, rowWidth);
                if (row.hidden) continue;
                if (row.nested && !socket)
                {
                    socket = Socket{b.pieces.size(), row.scroll};
                    for (size_t n = r; n < rows.size() && rows[n].nested; ++n)
                        if (!rows[n].hidden) socket->lines += linesOf(rows[n]);
                    socket->shown = std::min(socket->lines, kSocketLines);
                    socket->from  = std::min(row.scrolled, socket->lines - socket->shown);
                    b.pieces.push_back(PopupPiece{PopupPiece::Kind::Socket, b.pad, y, 0.0f, 0.0f, {}});
                    y += socketPad;
                    socket->top = y;
                }

                const size_t perLine = grid ? columns.size() : row.cells.size();
                float lineHeight = 0.0f, x = b.pad + indent;
                const float top  = y;
                bool shown       = showLine(row, grid && row.header);
                if (fold)
                {
                    const float s = unit * kFoldArrow;
                    PopupPiece arrow{PopupPiece::Kind::Fold, b.pad, y + (cells[0].inkTop + cells[0].inkBottom - s) / 2.0f,
                        s, s, {Text("", Tone::Dim)}};
                    arrow.open = row.open;
                    b.pieces.push_back(std::move(arrow));
                }
                for (size_t i = 0; i < row.cells.size(); ++i)
                {
                    if (i > 0 && i % perLine == 0)
                    {
                        if (shown) y += lineHeight + metrics.rowGap;
                        lineHeight = 0.0f;
                        x          = b.pad + indent;
                        shown      = showLine(row, false);
                    }
                    if (!shown) continue;
                    const PopupCell& cell = row.cells[i];
                    TextSize size         = cells[i];
                    float cx = x, cy = y;
                    const size_t first = b.pieces.size();
                    if (tags)
                    {
                        // The cell's ink centered in its frame: the text's line box can reach past it.
                        const TextSize framed = Framed(size, unit);
                        b.pieces.push_back(PopupPiece{PopupPiece::Kind::Tag, x, y, framed.width, framed.height, {}});
                        cx += TagPadX(unit);
                        cy += framed.inkTop - size.inkTop;
                        size = framed;
                    }
                    if (cell.icon)
                    {
                        const float s = unit * kCellIcon, mark = unit * kMark;
                        b.pieces.push_back(PopupPiece{PopupPiece::Kind::Icon, cx, cy, s, s, {}, PopupFont::Text, *cell.icon});
                        if (cell.immune)
                            b.pieces.push_back(PopupPiece{PopupPiece::Kind::ImmuneMark, cx + s - mark, cy + s - mark, mark,
                                mark, {kTimesSign, Tone::Immune, 0}});
                        cx += s + (cell.text.empty() ? 0.0f : unit * kIconGap);
                    }
                    for (const Span& text : cell.text)
                    {
                        const TextSize t = measure(text, PopupFont::Text);
                        b.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, cx, cy, t.width, t.height, text});
                        cx += t.width;
                    }
                    if (cell.star)
                    {
                        const float s = unit * kStar;
                        cx += unit * kIconGap;
                        b.pieces.push_back(PopupPiece{PopupPiece::Kind::Star, cx, cy + cells[i].inkTop, s, s, {}});
                        cx += s;
                    }
                    if (pair && i == 1)
                    {
                        b.values.push_back(Block::Value{first, b.pieces.size(), cx, row.nested ? inset : 0.0f});
                        for (size_t v = first; v < b.pieces.size(); ++v)
                            b.pieces[v].right = b.pieces[v].kind == PopupPiece::Kind::Text;
                    }
                    lineHeight = std::max(lineHeight, size.height);
                    x += grid ? columns[i % columns.size()] + unit * kColumnGap : size.width + gap;
                }
                if (fold)
                    b.folds.push_back(Block::Fold{row.fold, top - metrics.rowGap / 2.0f, y + lineHeight - top + metrics.rowGap});
                if (shown) y += lineHeight + metrics.rowGap;
            }
            closeSocket();
            b.width  = contentWidth + 2.0f * b.pad;
            b.height = std::max(y - metrics.rowGap, b.pad) + b.pad;
            return b;
        }

        // Moves a block's pieces to x, y and its pair values to its right edge at width; a card's ground goes first.
        void Place(Block& b, float x, float y, float width, size_t line, PopupLayout& l)
        {
            for (const Block::Value& v : b.values)
                for (size_t i = v.first; i < v.last; ++i)
                    b.pieces[i].x += width - b.pad - v.inset - v.right;
            for (const size_t socket : b.sockets)
                b.pieces[socket].width = width - 2.0f * b.pad;
            for (const Block::Anchor& anchor : b.anchors)
                b.pieces[anchor.piece].x = width - anchor.fromRight;
            if (b.card) l.pieces.push_back(PopupPiece{PopupPiece::Kind::Card, x, y, width, b.height, {}, PopupFont::Text,
                            Icon::AggroNQ, line});
            for (PopupPiece& piece : b.pieces)
            {
                piece.x += x;
                piece.y += y;
                piece.line = line;
                l.pieces.push_back(std::move(piece));
            }
            for (Block::Fold& fold : b.folds)
                l.folds.push_back(PopupFold{std::move(fold.key), fold.x < 0.0f ? x : x + b.pad + fold.x, y + fold.y,
                    fold.x < 0.0f ? width : fold.width, fold.height, line, std::move(fold.tab)});
            for (Block::Scroll& scroll : b.scrolls)
                l.scrolls.push_back(PopupScroll{std::move(scroll.key), x + b.pad, y + scroll.y, width - 2.0f * b.pad,
                    scroll.height, line, scroll.most,
                    BarRect{x + width - b.pad - scroll.grip, y + scroll.trackY, scroll.grip, scroll.track}, scroll.thumb});
            l.lines.push_back(PopupLine{y + b.height});
        }
        // A card as LayoutSection lays it out, with its reserve: its height with each tab picked in turn and its fold
        // rows closed, the tallest of them.
        Block LayoutCard(const PopupSection& section, const BarMetrics& metrics, bool ornate, SpanMeasure measure)
        {
            Block b = LayoutSection(section, metrics, ornate, measure);
            PopupSection closed = section;
            std::erase_if(closed.rows, [](const PopupRow& row) { return row.nested; });
            for (size_t tab = 0; tab < std::max<size_t>(section.tabs.size(), 1); ++tab)
            {
                closed.tab = tab;
                b.reserve  = std::max(b.reserve, LayoutSection(closed, metrics, ornate, measure).height);
            }
            return b;
        }

        // A grid's lines in groups side by side, down each group before the next, with an empty column between groups;
        // a header line heads each group.
        PopupRow SideBySide(const PopupRow& row, size_t groups)
        {
            const auto columns  = static_cast<size_t>(row.columns);
            const size_t head   = row.header ? columns : 0;
            const size_t lines  = (row.cells.size() - head + columns - 1) / columns;
            const size_t down   = (lines + groups - 1) / groups;
            const size_t across = (lines + down - 1) / down;
            const auto cellOf   = [&](size_t index) { return index < row.cells.size() ? row.cells[index] : PopupCell{}; };
            PopupRow out = row;
            out.cells.clear();
            out.columns = static_cast<int>(columns * across + across - 1);
            // One line across the groups: each group's cells from first, or empty past the end, an empty one between.
            const auto addLine = [&](auto first) {
                for (size_t group = 0; group < across; ++group)
                {
                    const std::optional<size_t> from = first(group);
                    for (size_t at = 0; at < columns; ++at)
                        out.cells.push_back(from ? cellOf(*from + at) : PopupCell{});
                    if (group + 1 < across) out.cells.emplace_back();
                }
            };
            if (row.header) addLine([](size_t) { return std::optional<size_t>(0); });
            for (size_t step = 0; step < down; ++step)
                addLine([&](size_t group) {
                    const size_t line = group * down + step;
                    return line < lines ? std::optional<size_t>(head + line * columns) : std::nullopt;
                });
            return out;
        }

        // A card that would run taller than tall: its long grids' lines set side by side in it, as few groups as fit;
        // failing that, cut into parts that fit between its rows, a grid's lines counting as rows, each part keeping
        // the heading. A row taller than tall alone is a part of its own.
        std::vector<PopupSection> Fitted(const PopupSection& section, float tall, const BarMetrics& metrics, bool ornate,
                                         SpanMeasure measure)
        {
            // As tall as it can be: with its tallest tab picked, so picking another never sets it differently.
            const auto tallest = [&](PopupSection variant) {
                float most = 0.0f;
                for (size_t tab = 0; tab < std::max<size_t>(variant.tabs.size(), 1); ++tab)
                {
                    variant.tab = tab;
                    most        = std::max(most, LayoutSection(variant, metrics, ornate, measure).height);
                }
                return most;
            };
            if (tallest(section) <= tall) return ListOf(section);
            PopupSection wide = section;
            for (size_t groups = 2; groups <= kMostSideBySide; ++groups)
            {
                wide     = section;
                bool set = false;
                const auto widen = [&](std::vector<PopupRow>& rows) {
                    for (PopupRow& row : rows)
                    {
                        const auto columns = static_cast<size_t>(std::max(row.columns, 0));
                        if (row.kind != RowKind::Grid || columns == 0 ||
                            row.cells.size() <= (kSideBySideAbove + (row.header ? 1 : 0)) * columns)
                            continue;
                        row = SideBySide(row, groups);
                        set = true;
                    }
                };
                widen(wide.rows);
                for (PopupTab& tab : wide.tabs)
                    widen(tab.rows);
                if (!set) break;
                if (tallest(wide) <= tall) return ListOf(wide);
            }
            // A card of tabs stays whole, as wide as it can be set, for the cut to take what is past the room.
            if (!section.tabs.empty()) return ListOf(wide);
            std::vector<PopupRow> rows;
            for (const PopupRow& row : section.rows)
            {
                const auto columns = static_cast<size_t>(std::max(row.columns, 0));
                if (row.kind != RowKind::Grid || columns == 0 || row.cells.size() <= columns)
                {
                    rows.push_back(row);
                    continue;
                }
                for (size_t i = 0; i < row.cells.size(); i += columns)
                {
                    PopupRow line = row;
                    line.cells.assign(row.cells.begin() + static_cast<ptrdiff_t>(i),
                        row.cells.begin() + static_cast<ptrdiff_t>(std::min(i + columns, row.cells.size())));
                    rows.push_back(std::move(line));
                }
            }
            // A card's height is its empty card's and each row's, a gap before each.
            const float empty = LayoutSection(PopupSection{section.heading, {}}, metrics, ornate, measure).height;
            std::vector<PopupSection> parts{PopupSection{section.heading, {}}};
            float height = empty;
            for (PopupRow& row : rows)
            {
                const float more = metrics.rowGap + LayoutSection(PopupSection{"", ListOf(row)}, metrics, ornate, measure).height;
                if (!parts.back().rows.empty() && height + more > tall)
                {
                    parts.push_back(PopupSection{section.heading, {}});
                    height = empty;
                }
                parts.back().rows.push_back(std::move(row));
                height += more;
            }
            return parts;
        }
    }

    PopupLayout LayoutPopup(const PopupContent& content, const BarMetrics& metrics, BarShape shape, bool pinned,
                            SpanMeasure measure, float room)
    {
        const float unit = metrics.popupText, pad = metrics.popupPadding, cardGap = std::round(unit * kCardGap);
        const bool ornate = shape == BarShape::Ornate;
        PopupLayout l;
        // The title row: its icon, its text and the pinned mark, which goes to the right edge once it is known.
        float x = pad, rowHeight = 0.0f;
        if (content.icon)
        {
            l.pieces.push_back(PopupPiece{PopupPiece::Kind::Icon, x, pad, unit * kTitleIcon, unit * kTitleIcon, {},
                PopupFont::Text, *content.icon});
            x += unit * kTitleIcon + unit * kIconGap;
            rowHeight = unit * kTitleIcon;
        }
        for (const Span& s : content.title)
        {
            const TextSize t = measure(s, PopupFont::Title);
            l.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, x, pad, t.width, t.height, s, PopupFont::Title});
            x += t.width;
            rowHeight = std::max(rowHeight, t.height);
        }
        float titleWidth = x - pad;
        std::optional<size_t> pinMark;
        if (pinned)
        {
            const Span mark{"pinned", Tone::Heading, 0};
            const TextSize t = measure(mark, PopupFont::Pin);
            pinMark          = l.pieces.size();
            l.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, 0.0f, pad, t.width, t.height, mark, PopupFont::Pin});
            titleWidth += unit * kCellGap + t.width;
        }
        l.lines.push_back(PopupLine{pad + rowHeight});
        float top = pad + rowHeight + metrics.rowGap;

        // The rows under the title, then the cards.
        std::vector<Block> loose;
        for (const PopupSection& section : content.sections)
            if (section.heading.empty()) loose.push_back(LayoutSection(section, metrics, ornate, measure));
        float width = std::max(unit * kMinWidth, titleWidth);
        float cardsTop = top;
        for (const Block& b : loose)
        {
            width = std::max(width, b.width);
            cardsTop += b.height + metrics.rowGap;
        }
        // The most a column of cards may run, with the padding under it: a card taller goes on in another column.
        const float tall = room - cardsTop - pad;
        std::vector<Block> cards;
        for (const PopupSection& section : content.sections)
            if (!section.heading.empty())
                for (const PopupSection& part : Fitted(section, tall, metrics, ornate, measure))
                    cards.push_back(LayoutCard(part, metrics, ornate, measure));

        // One column, or two when one runs tall: the split whose taller column is shortest, in their order. When that
        // runs past the room, as many columns as fill it in order. Each card counts as its reserve, so a tab picked or
        // a fold opened makes it grow down where it is.
        float stacked = 0.0f;
        for (const Block& b : cards)
            stacked += b.reserve + cardGap;
        std::vector<size_t> starts{0}; // each column's first card
        if (cards.size() >= 2 && stacked > unit * kTwoColumnsAbove)
        {
            float best = stacked, left = 0.0f;
            size_t split = cards.size();
            for (size_t i = 1; i < cards.size(); ++i)
            {
                left += cards[i - 1].reserve + cardGap;
                if (const float taller = std::max(left, stacked - left); taller < best)
                {
                    best  = taller;
                    split = i;
                }
            }
            if (split < cards.size()) starts.push_back(split);
            if (best - cardGap > tall)
            {
                starts = {0};
                float column = 0.0f;
                for (size_t i = 0; i < cards.size(); ++i)
                {
                    if (i > starts.back() && column + cards[i].reserve > tall)
                    {
                        starts.push_back(i);
                        column = 0.0f;
                    }
                    column += cards[i].reserve + cardGap;
                }
            }
        }
        const auto columnOf = [&](size_t card) {
            return static_cast<size_t>(std::upper_bound(starts.begin(), starts.end(), card) - starts.begin() - 1);
        };
        std::vector<float> columnWidths(starts.size(), 0.0f);
        for (size_t i = 0; i < cards.size(); ++i)
            columnWidths[columnOf(i)] = std::max(columnWidths[columnOf(i)], cards[i].width);
        float all = cardGap * static_cast<float>(columnWidths.size() - 1);
        for (const float w : columnWidths)
            all += w;
        if (all < width) columnWidths.front() += width - all; // a wide title widens the first column
        width = std::max(width, all);

        for (Block& b : loose)
        {
            Place(b, pad, top, width, l.lines.size(), l);
            top += b.height + metrics.rowGap;
        }
        std::vector<float> columnX(starts.size(), pad), columnY(starts.size(), top);
        for (size_t c = 1; c < starts.size(); ++c)
            columnX[c] = columnX[c - 1] + columnWidths[c - 1] + cardGap;
        for (size_t i = 0; i < cards.size(); ++i)
        {
            const size_t c = columnOf(i);
            Place(cards[i], columnX[c], columnY[c], columnWidths[c], l.lines.size(), l);
            columnY[c] += cards[i].height + cardGap;
        }
        const float lowest = *std::max_element(columnY.begin(), columnY.end());
        const float bottom = std::max(cards.empty() ? top - metrics.rowGap : lowest - cardGap, l.lines[0].bottom);
        l.width            = width + 2.0f * pad;
        l.height           = bottom + pad;
        if (pinMark) l.pieces[*pinMark].x = l.width - pad - l.pieces[*pinMark].width;
        return l;
    }

    void CutPopup(PopupLayout& layout, float height, float padding)
    {
        if (layout.height <= height) return;
        std::vector<bool> kept(layout.lines.size());
        float bottom = padding;
        for (size_t i = 0; i < layout.lines.size(); ++i)
        {
            kept[i] = i == 0 || layout.lines[i].bottom <= height - padding;
            if (kept[i]) bottom = std::max(bottom, layout.lines[i].bottom);
        }
        std::erase_if(layout.pieces, [&](const PopupPiece& piece) { return !kept[piece.line]; });
        std::erase_if(layout.folds, [&](const PopupFold& fold) { return !kept[fold.line]; });
        std::erase_if(layout.scrolls, [&](const PopupScroll& scroll) { return !kept[scroll.line]; });
        layout.height = bottom + padding;
    }
}
