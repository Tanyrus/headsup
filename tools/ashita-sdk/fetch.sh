#!/usr/bin/env bash
# Fetches the Ashita SDK headers HeadsUp builds against into third_party/ashita-sdk, checked against files.sha256:
# AshitaXI/Ashita-v4beta's plugins/sdk at commit 4171c74 (Ashita v4.3.2.1, interface 4.30), LGPL-3.0-or-later. Its
# d3d8/includes are the Microsoft DirectX 8 SDK headers as Ashita's SDK ships them, Microsoft copyright.
# Usage: tools/ashita-sdk/fetch.sh   (does nothing when every file is already there as it should be)
set -euo pipefail

TOOLS="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$TOOLS/../.." && pwd)"
DIR="$ROOT/third_party/ashita-sdk"
COMMIT=4171c74c8ddb2ca2a31654f199e6c1cee40d7256
URL="https://raw.githubusercontent.com/AshitaXI/Ashita-v4beta/$COMMIT/plugins/sdk"
SUMS="$TOOLS/files.sha256"

if [[ -d "$DIR" ]] && (cd "$DIR" && sha256sum --check --status "$SUMS"); then
    exit 0
fi
# Fetched beside it and moved into place once all are checked, so a failed fetch never leaves half an SDK.
mkdir -p "$(dirname "$DIR")"
fetched="$(mktemp -d "$(dirname "$DIR")/.ashita-sdk.XXXXXX")"
trap 'rm -rf "$fetched"' EXIT
chmod 755 "$fetched"
while read -r _ file; do
    mkdir -p "$fetched/$(dirname "$file")"
    curl -sSfL -o "$fetched/$file" "$URL/$file"
done < "$SUMS"
(cd "$fetched" && sha256sum --check --quiet "$SUMS")
rm -rf "$DIR"
mv "$fetched" "$DIR"
trap - EXIT
echo "fetched the Ashita SDK into $DIR"
