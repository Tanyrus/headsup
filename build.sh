#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/build"
PLUGINS_DIR="${HEADSUP_PLUGINS_DIR:-$HOME/Games/PhoenixXI/plugins}"
PURE_SOURCES=(con.cpp cursor_file.cpp fonts.cpp utf16.cpp mobdata.cpp ph_timers.cpp settings.cpp tracker.cpp outline_math.cpp labels.cpp look.cpp check.cpp nameplate.cpp icons.cpp plate_image.cpp player_status.cpp pose.cpp commands.cpp game_glyphs.cpp game_cursor.cpp native_locate.cpp name_frame.cpp copy_state.cpp)
PLUGIN_SOURCES=("${PURE_SOURCES[@]}" d3d_util.cpp game_names.cpp outline.cpp pointer_swap.cpp nameplate_render.cpp menu.cpp plugin.cpp native_hook.cpp)
# A developer's tools, kept beside the repository and out of it: when they are there, they are built in and plugin.cpp's
# HEADSUP_DEV hooks call them.
DEV_DIR="$ROOT/../HeadsUp-dev"
DEV_SOURCES=()
DEV_FLAGS=()
if [[ -d "$DEV_DIR" ]]; then
    DEV_SOURCES=("$DEV_DIR"/*.cpp)
    DEV_FLAGS=(-DHEADSUP_DEV -I"$DEV_DIR")
fi

generate() {
    python3 "$ROOT/tools/gen_mobdata.py"
    python3 "$ROOT/tools/gen_rules.py"
    python3 "$ROOT/tools/gen_icons.py"
    python3 "$ROOT/tools/gen_fonts.py"
    python3 "$ROOT/tools/gen_shapes.py"
}

unit_tests() {
    mkdir -p "$OUT"
    python3 -m unittest discover -s "$ROOT/tests" -p 'test_*.py'
    g++ -std=c++20 -O1 -Wall -Wextra -Werror -I"$ROOT/src" -o "$OUT/unit_tests" \
        "${PURE_SOURCES[@]/#/$ROOT/src/}" "$ROOT"/tests/*.cpp
    "$OUT/unit_tests"
}

abi_check() { python3 "$ROOT/tools/abi_check.py" "$DEV_DIR"; }

plugin() {
    mkdir -p "$OUT"
    # The SDK's Registry.h passes an int* where Windows wants an LPDWORD, which only -fpermissive accepts.
    i686-w64-mingw32-g++ -std=c++20 -O2 -shared \
        -isystem "$ROOT/shim" -isystem "$ROOT/third_party/ashita-sdk" -I"$ROOT/src" "${DEV_FLAGS[@]}" \
        -Wall -Wextra -Werror -fpermissive \
        -static -static-libgcc -static-libstdc++ -Wl,--kill-at \
        -o "$OUT/headsup.dll" "${PLUGIN_SOURCES[@]/#/$ROOT/src/}" "${DEV_SOURCES[@]}" -lpsapi -lgdi32
    echo "built $OUT/headsup.dll"
}

install_plugin() {
    # Write a new file and rename it over the old one: overwriting in place would corrupt a copy Wine has mapped.
    cp "$OUT/headsup.dll" "$PLUGINS_DIR/headsup.dll.tmp"
    mv -f "$PLUGINS_DIR/headsup.dll.tmp" "$PLUGINS_DIR/headsup.dll"
    echo "installed $PLUGINS_DIR/headsup.dll"
}

case "${1:-install}" in
    test) generate; unit_tests ;;
    plugin) generate; unit_tests; abi_check; plugin ;;
    install) generate; unit_tests; abi_check; plugin; install_plugin ;;
    *) echo "usage: $0 [test|plugin|install]" >&2; exit 2 ;;
esac
