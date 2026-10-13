#!/usr/bin/env bash
# Builds RmlUi and FreeType as static libraries for the plugin (i686 MinGW) and for the unit tests (this machine)
# into $HEADSUP_RMLUI_DIR (~/.cache/headsup-rmlui): <dir>/mingw and <dir>/linux each hold include/ and lib/.
# Usage: tools/rmlui/build.sh [mingw|linux|all]   (all when not given)
set -euo pipefail

TOOLS="$(cd "$(dirname "$0")" && pwd)"
DIR="${HEADSUP_RMLUI_DIR:-$HOME/.cache/headsup-rmlui}"
FREETYPE_VERSION=2.15.0
FREETYPE_SHA256=93cd47eb719c325107f2d3cf087d06299e944c3b48a40b55537302304dea1dd5
RMLUI_VERSION=6.3
RMLUI_SHA256=d977298bb6147610e5984d5db85ddf284020d655a8713913f6982074f1dbdede
# The plugin's own code generation flags, so the libraries share its stack alignment and SSE math. -g: the plugin's
# build strips the DLL and keeps the symbols beside it, where a crash's addresses are read.
MINGW_FLAGS="-g -O2 -msse2 -mfpmath=sse -mincoming-stack-boundary=2"
# 32-bit Windows' malloc aligns to 8 bytes, where GCC takes plain new to give 16: without this, RmlUi's layout pool
# (16-aligned chunks from plain new) faults on its first aligned store.
MINGW_CXX_FLAGS="$MINGW_FLAGS -faligned-new=8"

fetch() {
    local name=$1 url=$2 sha=$3 archive="$DIR/downloads/$1"
    mkdir -p "$DIR/downloads"
    if [[ ! -f "$archive" ]]; then
        curl -sSL -o "$archive" "$url"
    fi
    echo "$sha  $archive" | sha256sum --check --quiet
}

unpack() {
    local archive="$DIR/downloads/$1" into="$DIR/src/$2"
    if [[ ! -d "$into" ]]; then
        mkdir -p "$DIR/src"
        tar -xf "$archive" -C "$DIR/src"
    fi
}

build() {
    local target=$1
    shift
    local prefix="$DIR/$target"
    local common=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF "-DCMAKE_INSTALL_PREFIX=$prefix"
        -DCMAKE_C_FLAGS_RELEASE=-DNDEBUG -DCMAKE_CXX_FLAGS_RELEASE=-DNDEBUG "$@")
    cmake -S "$DIR/src/freetype-$FREETYPE_VERSION" -B "$DIR/build/$target/freetype" "${common[@]}" \
        -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
    cmake --build "$DIR/build/$target/freetype"
    cmake --install "$DIR/build/$target/freetype"
    cmake -S "$DIR/src/RmlUi-$RMLUI_VERSION" -B "$DIR/build/$target/rmlui" "${common[@]}" \
        "-DCMAKE_PREFIX_PATH=$prefix" -DRMLUI_SAMPLES=OFF -DRMLUI_FONT_ENGINE=freetype
    cmake --build "$DIR/build/$target/rmlui"
    cmake --install "$DIR/build/$target/rmlui"
}

fetch "freetype-$FREETYPE_VERSION.tar.xz" \
    "https://download.savannah.gnu.org/releases/freetype/freetype-$FREETYPE_VERSION.tar.xz" "$FREETYPE_SHA256"
fetch "RmlUi-$RMLUI_VERSION.tar.gz" "https://github.com/mikke89/RmlUi/archive/refs/tags/$RMLUI_VERSION.tar.gz" \
    "$RMLUI_SHA256"
unpack "freetype-$FREETYPE_VERSION.tar.xz" "freetype-$FREETYPE_VERSION"
unpack "RmlUi-$RMLUI_VERSION.tar.gz" "RmlUi-$RMLUI_VERSION"
# RmlUi 6.3 compares a transform's text with each keyword over the keyword's length, past the end of a shorter
# text (PropertyParserTransform::Scan): the patch stops at the text's end. Applied once.
if ! patch -d "$DIR/src/RmlUi-$RMLUI_VERSION" -p1 -R -s -f --dry-run < "$TOOLS/transform-scan.patch" > /dev/null 2>&1; then
    patch -d "$DIR/src/RmlUi-$RMLUI_VERSION" -p1 -s < "$TOOLS/transform-scan.patch"
fi

case "${1:-all}" in
    mingw) build mingw "-DCMAKE_TOOLCHAIN_FILE=$TOOLS/mingw-i686.cmake" "-DCMAKE_C_FLAGS=$MINGW_FLAGS" \
        "-DCMAKE_CXX_FLAGS=$MINGW_CXX_FLAGS" ;;
    linux) build linux -DCMAKE_C_FLAGS=-O2 -DCMAKE_CXX_FLAGS=-O2 ;;
    all) "$0" mingw && "$0" linux ;;
    *) echo "usage: $0 [mingw|linux|all]" >&2; exit 2 ;;
esac
