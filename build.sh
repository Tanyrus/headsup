#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/build"
PLUGINS_DIR="${HEADSUP_PLUGINS_DIR:-$HOME/Games/PhoenixXI/plugins}"
# RmlUi and FreeType as tools/rmlui/build.sh builds them: static, for the plugin and for the unit tests.
RMLUI_DIR="${HEADSUP_RMLUI_DIR:-$HOME/.cache/headsup-rmlui}"
RMLUI_LIBS=(-lrmlui_debugger -lrmlui -lfreetype)
PURE_SOURCES=(address_space.cpp xilinux_report.cpp game_font.cpp window_layout.cpp json_text.cpp window_file.cpp friend_list.cpp auto_check.cpp dash_mark.cpp cursor_picture.cpp rml_pure.cpp markup.cpp bar_document.cpp form.cpp menu_form.cpp menu_document.cpp rml_menu.cpp con.cpp cursor_file.cpp fonts.cpp pointer_keys.cpp utf16.cpp mobdata.cpp npcdata.cpp magic_chance.cpp ph_timers.cpp settings.cpp tracker.cpp outline_math.cpp labels.cpp look.cpp check.cpp nameplate.cpp icons.cpp plate_image.cpp player_status.cpp pose.cpp commands.cpp game_glyphs.cpp game_cursor.cpp native_locate.cpp window_views.cpp strip_text.cpp code_patch.cpp name_frame.cpp copy_state.cpp zones.cpp vanadiel.cpp bar_theme.cpp bar_look.cpp bar_content.cpp bar_server_time.cpp bar_fps.cpp bar_job.cpp bar_connection.cpp bar_party.cpp bar_signet.cpp bar_game_help.cpp bar_widgets.cpp bar_placement.cpp bar_layout.cpp popup_layout.cpp weather.cpp game_packets.cpp game_state.cpp titles.cpp currencies.cpp menu_state.cpp plate_settings.cpp entity_state.cpp plate_template.cpp plate_compose.cpp plate_layout.cpp ini_text.cpp)
PLUGIN_SOURCES=("${PURE_SOURCES[@]}" rml_system.cpp rml_render.cpp rml_host.cpp rml_bar.cpp d3d_util.cpp game_names.cpp outline.cpp pointer_swap.cpp gpu.cpp nameplate_render.cpp installed_fonts.cpp debug_report.cpp plugin.cpp native_hook.cpp)
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
    python3 "$ROOT/tools/gen_npcs.py"
    python3 "$ROOT/tools/gen_rules.py"
    python3 "$ROOT/tools/gen_icons.py"
    python3 "$ROOT/tools/gen_fonts.py"
    python3 "$ROOT/tools/gen_shapes.py"
    python3 "$ROOT/tools/gen_zones.py"
    python3 "$ROOT/tools/gen_titles.py"
    python3 "$ROOT/tools/gen_window.py"
    python3 "$ROOT/tools/gen_views.py"
}

# compile DIR COMPILER [FLAGS...] -- SOURCES...: each source to an object of its own in DIR, as many at a time as there
# are cores, the objects in OBJECTS. Each source's messages print whole once all are done, as side by side -Werror's
# would interleave mid-line, and it fails if any source did.
compile() {
    local dir="$1" cores source object failed=0
    local command=()
    shift
    while [[ "$1" != -- ]]; do
        command+=("$1")
        shift
    done
    shift
    cores="$(nproc)"
    rm -rf "$dir"
    mkdir -p "$dir"
    OBJECTS=()
    for source in "$@"; do
        object="$dir/$(basename "$(dirname "$source")")-$(basename "$source" .cpp)"
        OBJECTS+=("$object.o")
        while (( $(jobs -rp | wc -l) >= cores )); do wait -n; done
        { "${command[@]}" -c -o "$object.o" "$source" > "$object.log" 2>&1 || touch "$object.failed"; } &
    done
    wait
    for object in "${OBJECTS[@]%.o}"; do
        cat "$object.log" >&2
        if [[ -e "$object.failed" ]]; then failed=1; fi
    done
    return "$failed"
}

unit_tests() {
    mkdir -p "$OUT"
    python3 -m unittest discover -s "$ROOT/tests" -p 'test_*.py'
    compile "$OUT/unit" g++ -std=c++20 -O1 -Wall -Wextra -Werror -I"$ROOT/src" -isystem "$RMLUI_DIR/linux/include" \
        -DRMLUI_STATIC_LIB -- "${PURE_SOURCES[@]/#/$ROOT/src/}" "$ROOT"/tests/*.cpp
    g++ -o "$OUT/unit_tests" "${OBJECTS[@]}" -L"$RMLUI_DIR/linux/lib" "${RMLUI_LIBS[@]}"
    "$OUT/unit_tests"
}

abi_check() { python3 "$ROOT/tools/abi_check.py" "$DEV_DIR"; }

plugin() {
    mkdir -p "$OUT"
    # The SDK's Registry.h passes an int* where Windows wants an LPDWORD, which only -fpermissive accepts. x87 math made
    # the glow's blur five to eight times slower than SSE2; the game and Ashita call in with the stack only 4-byte aligned.
    # 32-bit Windows' malloc aligns to 8 bytes, where GCC takes plain new to give 16 (tools/rmlui/build.sh).
    compile "$OUT/plugin" i686-w64-mingw32-g++ -std=c++20 -O2 -msse2 -mfpmath=sse -mincoming-stack-boundary=2 \
        -faligned-new=8 \
        -isystem "$ROOT/shim" -isystem "$ROOT/third_party/ashita-sdk" -isystem "$RMLUI_DIR/mingw/include" \
        -DRMLUI_STATIC_LIB -I"$ROOT/src" "${DEV_FLAGS[@]}" \
        -Wall -Wextra -Werror -fpermissive -- "${PLUGIN_SOURCES[@]/#/$ROOT/src/}" "${DEV_SOURCES[@]}"
    i686-w64-mingw32-g++ -shared -static -static-libgcc -static-libstdc++ -Wl,--kill-at \
        -o "$OUT/headsup.dll" "${OBJECTS[@]}" -L"$RMLUI_DIR/mingw/lib" "${RMLUI_LIBS[@]}" -lpsapi -lgdi32
    # The static libstdc++ brings 11 MB of debug sections, which the game would map too. The DLL ships without them and
    # headsup.debug beside it keeps them and the symbols, for reading a crash's addresses.
    i686-w64-mingw32-objcopy --only-keep-debug "$OUT/headsup.dll" "$OUT/headsup.debug"
    i686-w64-mingw32-strip --strip-all "$OUT/headsup.dll"
    i686-w64-mingw32-objcopy --add-gnu-debuglink="$OUT/headsup.debug" "$OUT/headsup.dll"
    echo "built $OUT/headsup.dll"
}

install_plugin() {
    # Write a new file and rename it over the old one: overwriting in place would corrupt a copy Wine has mapped.
    cp "$OUT/headsup.dll" "$PLUGINS_DIR/headsup.dll.tmp"
    mv -f "$PLUGINS_DIR/headsup.dll.tmp" "$PLUGINS_DIR/headsup.dll"
    echo "installed $PLUGINS_DIR/headsup.dll"
}

for target in linux mingw; do
    if [[ ! -f "$RMLUI_DIR/$target/lib/librmlui.a" ]]; then
        echo "RmlUi is not built for $target: run tools/rmlui/build.sh first" >&2
        exit 1
    fi
done

case "${1:-install}" in
    test) generate; unit_tests ;;
    plugin) generate; unit_tests; abi_check; plugin ;;
    install) generate; unit_tests; abi_check; plugin; install_plugin ;;
    *) echo "usage: $0 [test|plugin|install]" >&2; exit 2 ;;
esac
