#!/usr/bin/env bash
# Regenerate data/phoenix_mobs.tsv and data/phoenix_rules.json from Phoenix's own map server; data/ is only written when
# validation passes. It clones Phoenix at PHOENIX_COMMIT with its patches and mesh submodules (not the staff-only
# phoenix_ac), builds the map server with the headsup_dump module, loads Phoenix's database into a private MariaDB, runs
# the server until the module has dumped every loaded mob and the /check and aggro rules HEADSUP_PHOENIX_DUMPS times,
# then merges every dump kept for this commit and validates it.
# A mob counts as aggressive if any run saw it attack: some change aggression while the server runs.
#
#   HEADSUP_PHOENIX_DIR        work folder, default ~/.cache/headsup-phoenix (about 6 GB; never under /tmp)
#   HEADSUP_PHOENIX_DB_PORT    MariaDB port, default 3307
#   HEADSUP_PHOENIX_MAP_PORT   map server port, default 54230
#   HEADSUP_PHOENIX_DUMPS      server runs to snapshot, default 5, about three minutes each
#   HEADSUP_PHOENIX_SNAPSHOTS  where every run's dumps are kept, one folder per Phoenix commit, settings and dump
#                              module, default ~/.cache/headsup-snapshots; each refresh merges all of that folder's dumps
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="${HEADSUP_PHOENIX_DIR:-$HOME/.cache/headsup-phoenix}"
DB_PORT="${HEADSUP_PHOENIX_DB_PORT:-3307}"
MAP_PORT="${HEADSUP_PHOENIX_MAP_PORT:-54230}"
HOST=127.0.0.1
DB_NAME=xidb
DB_USER=xi
DB_PASSWORD=xi
MAP_TIMEOUT=1200 # seconds for one server run to write its dump
VANADIEL_DAY=3456 # real seconds in a Vana'diel day: 24 hours of 144 s
DUMPS="${HEADSUP_PHOENIX_DUMPS:-5}"
COMMIT="$(tr -d '[:space:]' < "$ROOT/tools/phoenix/PHOENIX_COMMIT")"
PHOENIX_URL=https://github.com/phoenixffxi/Phoenix.git
SERVER="$WORK/server"
BUILD="$WORK/build"
VENV="$WORK/venv"
DB="$WORK/db"
# A Unix socket's path must stay under 108 characters, which a long HEADSUP_PHOENIX_DIR could pass.
SOCKET="${XDG_RUNTIME_DIR:-$WORK}/headsup-phoenix.sock"
STAGE="$WORK/out"
# Live Phoenix restricts content to Treasures of Aht Urhgan and before. That picks the era experience table and /check
# curve, and which mobs load, so the server runs with the same settings. Dumps made with other settings, or by another
# version of the dump module, are kept apart.
LIVE_SETTINGS=(XI_MAIN_RESTRICT_CONTENT=1 XI_MAIN_ENABLE_WOTG=0 XI_MAIN_ENABLE_ACP=0 XI_MAIN_ENABLE_AMK=0
    XI_MAIN_ENABLE_ASA=0 XI_MAIN_ENABLE_ABYSSEA=0 XI_MAIN_ENABLE_VOIDWATCH=0 XI_MAIN_ENABLE_SOA=0 XI_MAIN_ENABLE_ROV=0
    XI_MAIN_ENABLE_TVR=0)
DUMP_ID="$(printf '%s\n' "${LIVE_SETTINGS[@]}" | cat - "$ROOT/tools/phoenix/headsup_dump/headsup_dump.cpp" | sha256sum | cut -c1-8)"
SNAPSHOTS="${HEADSUP_PHOENIX_SNAPSHOTS:-$HOME/.cache/headsup-snapshots}/$COMMIT-$DUMP_ID"

log() { printf '[refresh] %s\n' "$*"; }
fail() { printf '[refresh] FAILED: %s\n' "$*" >&2; exit 1; }

for dir in "$WORK" "$SNAPSHOTS"; do
    case "$dir" in /tmp | /tmp/*) fail "/tmp is a small RAM disk here; keep $dir on disk" ;; esac
done
for tool in git cmake ninja g++ python3 mariadbd mariadb-install-db mariadb mariadb-admin timeout faketime; do
    command -v "$tool" > /dev/null || fail "missing tool: $tool"
done
mkdir -p "$WORK/tmp"
export TMPDIR="$WORK/tmp"

# phoenix_ac is staff only: it is never fetched, and its modules are taken out of the module list.
if [ ! -d "$SERVER/.git" ]; then
    git init -q "$SERVER"
    git -C "$SERVER" remote add origin "$PHOENIX_URL"
fi
if [ "$(git -C "$SERVER" rev-parse -q --verify HEAD || true)" != "$COMMIT" ]; then
    log "checking out Phoenix $COMMIT"
    git -C "$SERVER" fetch -q --depth 1 origin "$COMMIT"
    git -C "$SERVER" checkout -q -f --detach FETCH_HEAD
fi
log "updating submodules (ximeshes, navmeshes)"
git -C "$SERVER" submodule update -q --init --checkout --depth 1 ximeshes navmeshes
sed -i '/^phoenix_ac\//d' "$SERVER/modules/init.txt"

# Live runs with the temporary patches: two enabled fishing modules need them, and one of them changes the YAML
# merge.
for patch in "$SERVER"/modules/temp_patch/*.patch; do
    [ -e "$patch" ] || continue
    if git -C "$SERVER" apply --reverse --check "$patch" 2> /dev/null; then continue; fi
    log "applying ${patch##*/}"
    git -C "$SERVER" apply "$patch"
done

# The dump module loads after every other module. Copy it only on change so the build stays incremental.
mkdir -p "$SERVER/modules/headsup_dump"
cmp -s "$ROOT/tools/phoenix/headsup_dump/headsup_dump.cpp" "$SERVER/modules/headsup_dump/headsup_dump.cpp" ||
    cp "$ROOT/tools/phoenix/headsup_dump/headsup_dump.cpp" "$SERVER/modules/headsup_dump/headsup_dump.cpp"
grep -qx 'headsup_dump/' "$SERVER/modules/init.txt" ||
    printf '\n# headsup: dump every loaded mob (tools/phoenix/refresh.sh)\nheadsup_dump/\n' >> "$SERVER/modules/init.txt"

if [ ! -x "$VENV/bin/python" ]; then
    log "creating the Python venv"
    python3 -m venv "$VENV"
fi
"$VENV/bin/pip" install -q -r "$SERVER/tools/requirements.txt" > "$WORK/pip.log" 2>&1 || fail "pip install; see $WORK/pip.log"
log "configuring"
cmake -G Ninja -S "$SERVER" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DTRACY_ENABLE=OFF -DPCH_ENABLE=ON \
    -DWARNINGS_AS_ERRORS=FALSE -DENABLE_CLANG_TIDY=OFF -DPython_EXECUTABLE="$VENV/bin/python" \
    > "$WORK/configure.log" 2>&1 || fail "cmake configure; see $WORK/configure.log"
log "building (the first build takes about 15 minutes)"
cmake --build "$BUILD" > "$WORK/build.log" 2>&1 || fail "build; see $WORK/build.log"
[ -x "$SERVER/xi_map" ] || fail "no xi_map after the build"

stop_db() {
    if [ -S "$SOCKET" ]; then
        mariadb-admin --no-defaults -S "$SOCKET" -u root shutdown > /dev/null 2>&1 || true
        for _ in $(seq 60); do
            [ -S "$SOCKET" ] || break
            sleep 0.5
        done
    fi
}
trap stop_db EXIT
if [ ! -d "$DB/data/mysql" ]; then
    log "creating the database folder"
    mkdir -p "$DB/data"
    mariadb-install-db --no-defaults --datadir="$DB/data" --auth-root-authentication-method=normal --skip-test-db \
        > "$WORK/db-install.log" 2>&1 || fail "mariadb-install-db; see $WORK/db-install.log"
fi
log "starting MariaDB on $HOST:$DB_PORT"
mariadbd --no-defaults --datadir="$DB/data" --socket="$SOCKET" --port="$DB_PORT" --bind-address="$HOST" \
    --pid-file="$DB/mariadb.pid" --log-error="$DB/error.log" --tmpdir="$TMPDIR" --character-set-server=utf8mb4 \
    --collation-server=utf8mb4_general_ci --max-allowed-packet=256M &
for _ in $(seq 120); do
    [ -S "$SOCKET" ] && break
    sleep 0.5
done
[ -S "$SOCKET" ] || fail "MariaDB did not start; see $DB/error.log"
mariadb --no-defaults -S "$SOCKET" -u root -e "CREATE DATABASE IF NOT EXISTS $DB_NAME CHARACTER SET utf8mb4 \
    COLLATE utf8mb4_general_ci; CREATE USER IF NOT EXISTS '$DB_USER'@'$HOST' IDENTIFIED BY '$DB_PASSWORD'; \
    GRANT ALL PRIVILEGES ON $DB_NAME.* TO '$DB_USER'@'$HOST'; FLUSH PRIVILEGES;"
[ -f "$SERVER/tools/config.yaml" ] ||
    printf -- "- mysql_bin: /usr/bin/\n- auto_backup: 0\n- auto_update_client: false\n- db_ver: ''\n" > "$SERVER/tools/config.yaml"
export XI_NETWORK_SQL_HOST="$HOST" XI_NETWORK_SQL_PORT="$DB_PORT" XI_NETWORK_SQL_LOGIN="$DB_USER" \
    XI_NETWORK_SQL_PASSWORD="$DB_PASSWORD" XI_NETWORK_SQL_DATABASE="$DB_NAME"
log "loading the database (dbtool update full)"
(cd "$SERVER" && "$VENV/bin/python" tools/dbtool.py update full < /dev/null > "$WORK/dbtool.log" 2>&1) ||
    fail "dbtool; see $WORK/dbtool.log"

# The runs start at hours spread over one Vana'diel day (faketime shifts the clock), so mobs that sleep at night are
# also seen awake. A run writes to a .part file first, so a run that dies never leaves a partial dump among the
# snapshots.
mkdir -p "$SNAPSHOTS"
stamp="$(date +%Y%m%d-%H%M%S)"
for run in $(seq "$DUMPS"); do
    dump="$SNAPSHOTS/$stamp-$run.json"
    offset=$(((run - 1) * VANADIEL_DAY / DUMPS))
    log "running xi_map until dump $run of $DUMPS is written (clock +${offset}s)"
    status=0
    (cd "$SERVER" && timeout "$MAP_TIMEOUT" faketime -f "+${offset}" env "${LIVE_SETTINGS[@]}" HEADSUP_DUMP_PATH="$dump.part" \
        ./xi_map --ip "$HOST" --port "$MAP_PORT" < /dev/null > "$WORK/map.log" 2>&1) || status=$?
    if [ "$status" -ne 0 ] || [ ! -s "$dump.part" ]; then
        rm -f "$dump.part"
        [ "$status" -eq 124 ] && fail "xi_map wrote no dump within $MAP_TIMEOUT seconds; see $WORK/map.log"
        fail "xi_map exited with status $status and no dump; see $WORK/map.log"
    fi
    mv "$dump.part" "$dump"
done

rm -rf "$STAGE"
python3 "$ROOT/tools/phoenix/compact.py" "$SNAPSHOTS"/*.json "$STAGE" "$COMMIT" ||
    fail "compact; data/ left unchanged"
python3 "$ROOT/tools/phoenix/validate.py" "$STAGE/phoenix_mobs.tsv" "$STAGE/phoenix_rules.json" ||
    fail "validation; data/ left unchanged"
mkdir -p "$ROOT/data"
cp "$STAGE/phoenix_mobs.tsv" "$STAGE/phoenix_mobs.meta" "$STAGE/phoenix_rules.json" "$ROOT/data/"
log "data/phoenix_mobs.tsv, data/phoenix_mobs.meta and data/phoenix_rules.json updated"
