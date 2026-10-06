#!/usr/bin/env bash
# Build, test and install aggroglow.dll.
#   ./build.sh test      generate data, run the Python and native unit tests
#   ./build.sh plugin    test, check the Ashita ABI, cross-compile build/aggroglow.dll
#   ./build.sh install   plugin, then install it into the game's plugins folder (the default)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/build"
PLUGINS_DIR="${AGGROGLOW_PLUGINS_DIR:-$HOME/Games/PhoenixXI/plugins}"
PURE_SOURCES=(con.cpp mobdata.cpp classifier.cpp settings.cpp tracker.cpp outline_math.cpp labels.cpp examine.cpp nameplate.cpp icons.cpp)
PLUGIN_SOURCES=("${PURE_SOURCES[@]}" outline.cpp nameplate_render.cpp menu.cpp plugin.cpp)

generate() {
    python3 "$ROOT/tools/gen_mobdata.py"
    python3 "$ROOT/tools/gen_icons.py"
}

unit_tests() {
    mkdir -p "$OUT"
    python3 -m unittest discover -s "$ROOT/tests" -p 'test_*.py'
    g++ -std=c++20 -O1 -Wall -Wextra -Werror -I"$ROOT/src" -I"$ROOT/tests" -o "$OUT/unit_tests" \
        "${PURE_SOURCES[@]/#/$ROOT/src/}" "$ROOT"/tests/*.cpp
    "$OUT/unit_tests"
}

abi_check() { python3 "$ROOT/tools/abi_check.py"; }

plugin() {
    mkdir -p "$OUT"
    i686-w64-mingw32-g++ -std=c++20 -O2 -shared \
        -I"$ROOT/shim" -I"$ROOT/third_party/ashita-sdk" -I"$ROOT/src" \
        -fpermissive -Wno-unknown-pragmas -Wno-attributes \
        -static -static-libgcc -static-libstdc++ -Wl,--kill-at \
        -o "$OUT/aggroglow.dll" "${PLUGIN_SOURCES[@]/#/$ROOT/src/}" -lpsapi
    echo "built $OUT/aggroglow.dll"
}

install_plugin() {
    # Write a new file and rename it over the old one: overwriting in place would corrupt a copy Wine has mapped.
    cp "$OUT/aggroglow.dll" "$PLUGINS_DIR/aggroglow.dll.tmp"
    mv -f "$PLUGINS_DIR/aggroglow.dll.tmp" "$PLUGINS_DIR/aggroglow.dll"
    echo "installed $PLUGINS_DIR/aggroglow.dll"
}

case "${1:-install}" in
    test) generate; unit_tests ;;
    plugin) generate; unit_tests; abi_check; plugin ;;
    install) generate; unit_tests; abi_check; plugin; install_plugin ;;
    *) echo "usage: $0 [test|plugin|install]" >&2; exit 2 ;;
esac
