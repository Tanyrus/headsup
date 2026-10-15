#!/usr/bin/env python3
"""Generate src/generated/views.inc from data/window_views.json: the views of the game's windows (the windows it
opens together, where it placed them with HeadsUp unloaded) that HeadsUp moves under its panel as one.

With --from-sweep MENUS BAND_ON BAND_OFF DIR [DIR ...], write data/window_views.json instead from the views.json of
each sweep DIR (the windows the game's own list held in each view, a 1920 x 1080 menu), naming each view by its way
through MENUS (the game menus' contract) and keeping the panel's band with and without the help bar it was taken at.
"""
import json
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from gen_mobdata import c_string  # noqa: E402

MENU = [1920, 1080]
HIDDEN = 0x0E                                 # a window's type while it is hidden or closing
STRIP = ('helpwind', 'titlewin', 'netstat')   # the game's strip, which moves by its own rules
KEY = re.compile(r'[!-~]{1,8}')               # a window's short name: up to 8 of the name's printable letters
KEY_LENGTH = 8
REST = 'rest'
REST_NAME = 'No Menu Open'
NAME_JOIN = ' > '


class DataError(ValueError):
    pass


def view_windows(windows: list, screen_bottom: int) -> list:
    """The shown windows that hang from the top (above the screen's middle), the strip left out."""
    return [{'key': w['short'], 'rect': w['rect']} for w in windows
            if w['type'] != HIDDEN and not w['closed'] and not w['virtual'] and w['short'] not in STRIP
            and w['rect'][1] < screen_bottom // 2]


def name_of(node: str, labels: dict) -> str:
    if node == REST:
        return REST_NAME
    parts = node.split('/')
    return NAME_JOIN.join(labels.get('/'.join(parts[:i + 1]), parts[i]) for i in range(len(parts)))


def from_sweep(sweeps: list, menus: dict, reference: dict) -> dict:
    labels = {n['id']: n['label'] for n in menus['nodes']}
    views, seen = [], set()
    for sweep in sweeps:
        for node, view in sweep.items():
            windows = view_windows(view['windows'], reference['menu'][1])
            shape = tuple(sorted((w['key'], tuple(w['rect'])) for w in windows))
            if not windows or shape in seen:
                continue
            seen.add(shape)
            views.append({'id': node, 'name': view.get('name') or name_of(node, labels), 'windows': windows})
    return {'reference': reference, 'views': views}


def check(data: dict) -> None:
    ids = set()
    for view in data['views']:
        if view['id'] in ids:
            raise DataError(f'view {view["id"]} is there twice')
        ids.add(view['id'])
        if not view['windows']:
            raise DataError(f'view {view["id"]} has no windows')
        for w in view['windows']:
            if not KEY.fullmatch(w['key']):
                raise DataError(f'view {view["id"]}: {w["key"]} is not a window\'s short name')
            left, top, right, bottom = w['rect']
            if not (0 <= left < right and 0 <= top < bottom):
                raise DataError(f'view {view["id"]}: {w["key"]} has no area at {w["rect"]}')


def key_bytes(key: str) -> str:
    return '{' + ', '.join(f"'{c}'" for c in key.ljust(KEY_LENGTH)) + '}'


def generate(data: dict) -> str:
    check(data)
    reference = data['reference']
    lines = ['// From data/window_views.json by tools/gen_views.py; do not edit.',
             f'constexpr int kReferenceBandHelpOn = {reference["bandHelpOn"]};',
             f'constexpr int kReferenceBandHelpOff = {reference["bandHelpOff"]};',
             'inline constexpr WindowKey kViewWindows[] = {']
    starts, at = [], 0
    for view in data['views']:
        starts.append(at)
        for w in view['windows']:
            lines.append(f'    {key_bytes(w["key"])},')
            at += 1
    lines += ['};', 'inline constexpr WindowView kWindowViews[] = {']
    for view, start in zip(data['views'], starts):
        top = min(w['rect'][1] for w in view['windows'])
        lines.append(f'    {{{c_string(view["id"])}, {c_string(view["name"])}, {top}, '
                     f'{{kViewWindows + {start}, {len(view["windows"])}}}}},')
    lines.append('};')
    return '\n'.join(lines) + '\n'


def main(argv) -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    data_path = root / 'data' / 'window_views.json'
    try:
        if argv[1:2] == ['--from-sweep']:
            if len(argv) < 6:
                print('usage: gen_views.py [--from-sweep MENUS BAND_ON BAND_OFF DIR ...]', file=sys.stderr)
                return 2
            menus = json.loads(pathlib.Path(argv[2]).read_text(encoding='utf-8'))
            reference = {'menu': MENU, 'bandHelpOn': int(argv[3]), 'bandHelpOff': int(argv[4])}
            sweeps = [json.loads((pathlib.Path(d) / 'views.json').read_text(encoding='utf-8')) for d in argv[5:]]
            data = from_sweep(sweeps, menus, reference)
            check(data)
            data_path.write_text(json.dumps(data, indent=1) + '\n', encoding='utf-8')
            print(f'views: {len(data["views"])} views -> {data_path}')
            return 0
        if len(argv) != 1:
            print('usage: gen_views.py [--from-sweep MENUS BAND_ON BAND_OFF DIR ...]', file=sys.stderr)
            return 2
        data = json.loads(data_path.read_text(encoding='utf-8'))
        text = generate(data)
    except (OSError, ValueError, KeyError) as error:
        print(f'gen_views: {error}', file=sys.stderr)
        return 1
    out = root / 'src' / 'generated' / 'views.inc'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding='utf-8')
    print(f'views: {len(data["views"])} views -> {out}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
