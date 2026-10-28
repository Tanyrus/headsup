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
            };
            std::vector<Value> values;
            struct Fold
            {
                std::string key;
                float y, height; // its row, with half the gap to each neighbour
            };
            std::vector<Fold> folds;
            std::vector<size_t> sockets; // pieces to widen to the card's inside once its width is known
            bool card = false;
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
            // Grid rows one after another with as many columns line them up: a grid carried on across rows, or cards.
            std::vector<std::vector<float>> runColumns(section.rows.size());
            for (size_t first = 0, last = 0; first < section.rows.size(); first = std::max(last, first + 1))
            {
                const PopupRow& head = section.rows[first];
                last = first;
                if (head.kind != RowKind::Grid || head.columns <= 0) continue;
                std::vector<float> widths(static_cast<size_t>(head.columns), 0.0f);
                for (; last < section.rows.size() && section.rows[last].kind == RowKind::Grid &&
                       section.rows[last].columns == head.columns;
                     ++last)
                    for (size_t i = 0; i < section.rows[last].cells.size(); ++i)
                        widths[i % widths.size()] =
                            std::max(widths[i % widths.size()], CellSize(section.rows[last].cells[i], unit, measure).width);
                for (size_t r = first; r < last; ++r)
                    runColumns[r] = widths;
            }
            std::vector<TextSize> cells;
            std::vector<float> columns;
            const float socketPad = std::round(unit * kSocketPad);
            std::optional<size_t> socket; // the open one, for the nested rows now being laid out
            const auto closeSocket = [&] {
                if (!socket) return;
                PopupPiece& ground = b.pieces[*socket];
                ground.height      = y - metrics.rowGap + socketPad - ground.y;
                y                  = ground.y + ground.height + metrics.rowGap;
                b.sockets.push_back(*socket);
                socket.reset();
            };
            for (size_t r = 0; r < section.rows.size(); ++r)
            {
                const PopupRow& row = section.rows[r];
                if (!row.nested)
                    closeSocket();
                else if (!socket)
                {
                    socket = b.pieces.size();
                    b.pieces.push_back(PopupPiece{PopupPiece::Kind::Socket, b.pad, y, 0.0f, 0.0f, {}});
                    y += socketPad;
                }
                if (row.cells.empty()) continue;
                const bool grid = row.kind == RowKind::Grid && row.columns > 0;
                const bool tags = row.kind == RowKind::Tags;
                const bool pair = row.kind == RowKind::Pair && row.cells.size() == 2;
                const float gap = unit * (tags ? kTagGap : pair ? kPairGap : kCellGap);
                const bool fold = !row.fold.empty();
                // A fold row's cells after its arrow, and the rows it holds lined up with them.
                const float indent = fold || row.nested ? std::round(unit * (kFoldArrow + kIconGap)) : 0.0f;
                cells.clear();
                columns = grid ? runColumns[r] : std::vector<float>{};
                float rowWidth = indent;
                for (size_t i = 0; i < row.cells.size(); ++i)
                {
                    const TextSize s = cells.emplace_back(CellSize(row.cells[i], unit, measure));
                    if (!grid) rowWidth += (tags ? Framed(s, unit) : s).width + (i > 0 ? gap : 0.0f);
                }
                for (const float c : columns)
                    rowWidth += c;
                if (grid) rowWidth += unit * kColumnGap * static_cast<float>(columns.size() - 1);
                contentWidth = std::max(contentWidth, rowWidth);

                const size_t perLine = grid ? columns.size() : row.cells.size();
                float lineHeight = 0.0f, x = b.pad + indent;
                const float top  = y;
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
                        y += lineHeight + metrics.rowGap;
                        lineHeight = 0.0f;
                        x          = b.pad + indent;
                    }
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
                        b.values.push_back(Block::Value{first, b.pieces.size(), cx});
                        for (size_t v = first; v < b.pieces.size(); ++v)
                            b.pieces[v].right = b.pieces[v].kind == PopupPiece::Kind::Text;
                    }
                    lineHeight = std::max(lineHeight, size.height);
                    x += grid ? columns[i % columns.size()] + unit * kColumnGap : size.width + gap;
                }
                if (fold)
                    b.folds.push_back(Block::Fold{row.fold, top - metrics.rowGap / 2.0f, y + lineHeight - top + metrics.rowGap});
                y += lineHeight + metrics.rowGap;
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
                    b.pieces[i].x += width - b.pad - v.right;
            for (const size_t socket : b.sockets)
                b.pieces[socket].width = width - 2.0f * b.pad;
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
                l.folds.push_back(PopupFold{std::move(fold.key), x, y + fold.y, width, fold.height, line});
            l.lines.push_back(PopupLine{y + b.height});
        }
        // A card that would run taller than tall, cut into parts that fit between its rows, a grid's lines counting as
        // rows; each part keeps the heading. A row taller than tall alone is a part of its own.
        std::vector<PopupSection> Fitted(const PopupSection& section, float tall, const BarMetrics& metrics, bool ornate,
                                         SpanMeasure measure)
        {
            if (LayoutSection(section, metrics, ornate, measure).height <= tall) return ListOf(section);
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
                    cards.push_back(LayoutSection(part, metrics, ornate, measure));

        // One column, or two when one runs tall: the split whose taller column is shortest, in their order. When that
        // runs past the room, as many columns as fill it in order.
        float stacked = 0.0f;
        for (const Block& b : cards)
            stacked += b.height + cardGap;
        std::vector<size_t> starts{0}; // each column's first card
        if (cards.size() >= 2 && stacked > unit * kTwoColumnsAbove)
        {
            float best = stacked, left = 0.0f;
            size_t split = cards.size();
            for (size_t i = 1; i < cards.size(); ++i)
            {
                left += cards[i - 1].height + cardGap;
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
                    if (i > starts.back() && column + cards[i].height > tall)
                    {
                        starts.push_back(i);
                        column = 0.0f;
                    }
                    column += cards[i].height + cardGap;
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
        layout.height = bottom + padding;
    }
}
