#!/usr/bin/env python3
"""Read title names from a Phoenix checkout into data/.

Usage: titles.py SERVER_DIR OUT_DIR. Phoenix keeps titles only as identifiers in scripts/enum/title.lua, so each name
is its identifier in words; the identifiers drop apostrophes ("BEHEMOTHS_BANE" for the game's "Behemoth's Bane").
"""
import pathlib
import re
import sys

SMALL_WORDS = {'of', 'the', 'and', 'in', 'to', 'a', 'an', 'for', 'on', 'at', 'by', 'with', 'from'}


class TitleDataError(ValueError):
    pass


def display_name(identifier: str) -> str:
    words = identifier.lower().split('_')
    return ' '.join(word if i > 0 and word in SMALL_WORDS else word.capitalize() for i, word in enumerate(words))


def parse_titles(text: str) -> list:
    rows, seen = [], set()
    for identifier, value in re.findall(r'^\s+([A-Za-z0-9_]+)\s*=\s*([^,\s]+)\s*,', text, re.M):
        if not value.isdigit():
            raise TitleDataError(f'title.lua gives {identifier} {value}, not a number')
        title = int(value)
        if title in seen:
            raise TitleDataError(f'title.lua gives title {title} again, to {identifier}')
        seen.add(title)
        rows.append((title, display_name(identifier)))
    if not rows:
        raise TitleDataError('title.lua lists no titles')
    return sorted(rows)


def build(server: pathlib.Path) -> list:
    return parse_titles((server / 'scripts' / 'enum' / 'title.lua').read_text(encoding='utf-8'))


def write(out: pathlib.Path, rows) -> None:
    out.mkdir(parents=True, exist_ok=True)
    lines = ['id\tname'] + [f'{title}\t{name}' for title, name in rows]
    (out / 'phoenix_titles.tsv').write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main(argv) -> int:
    if len(argv) != 3:
        print('usage: titles.py SERVER_DIR OUT_DIR', file=sys.stderr)
        return 2
    try:
        rows = build(pathlib.Path(argv[1]))
    except (OSError, TitleDataError) as error:
        print(f'titles: {error}', file=sys.stderr)
        return 1
    write(pathlib.Path(argv[2]), rows)
    print(f'titles: {len(rows)} titles -> {argv[2]}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
