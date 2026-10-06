#!/usr/bin/env bash
# Regenerate data/phoenix_mobs.tsv from Phoenix's own map server (spec: docs/specs/2026-10-06-aggroglow-v2-design.md,
# section 1). Builds Phoenix at tools/phoenix/PHOENIX_COMMIT with every module plus tools/phoenix/aggroglow_dump,
# loads its database into a private MariaDB, runs xi_map until the dump module writes every loaded mob, then
# compacts and validates the dump. data/ is only written when validation passes.
#
#   AGGROGLOW_PHOENIX_DIR   work folder, default ~/.cache/aggroglow-phoenix (about 6 GB; never under /tmp)
#   AGGROGLOW_PHOENIX_DB_PORT  MariaDB port, default 3307
#   AGGROGLOW_PHOENIX_DUMPS    server runs to snapshot, default 5, about a minute each (a mob counts as aggressive
#                              if any run saw it attack)
#   JOBS                    parallel build jobs, default nproc
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="${AGGROGLOW_PHOENIX_DIR:-$HOME/.cache/aggroglow-phoenix}"
PORT="${AGGROGLOW_PHOENIX_DB_PORT:-3307}"
DUMPS="${AGGROGLOW_PHOENIX_DUMPS:-5}"
COMMIT="$(tr -d '[:space:]' < "$ROOT/tools/phoenix/PHOENIX_COMMIT")"
PHOENIX_URL=https://github.com/phoenixffxi/Phoenix.git
SERVER="$WORK/server"
BUILD="$WORK/build"
VENV="$WORK/venv"
DB="$WORK/db"
SOCKET="${XDG_RUNTIME_DIR:-$WORK}/aggroglow-phoenix.sock" # under 108 characters, unlike a path inside $WORK
STAGE="$WORK/out"

log() { printf '[refresh] %s\n' "$*"; }
fail() { printf '[refresh] FAILED: %s\n' "$*" >&2; exit 1; }

case "$WORK" in /tmp | /tmp/*) fail "/tmp is a small RAM disk here; set AGGROGLOW_PHOENIX_DIR to a folder on disk" ;; esac
for tool in git cmake ninja g++ python3 mariadbd mariadb-install-db mariadb mariadb-admin timeout; do
    command -v "$tool" > /dev/null || fail "missing tool: $tool"
done
mkdir -p "$WORK/tmp"
export TMPDIR="$WORK/tmp"

# 1. Phoenix source at the pinned commit, with the submodules the server needs.
if [ ! -d "$SERVER/.git" ]; then
    git init -q "$SERVER"
    git -C "$SERVER" remote add origin "$PHOENIX_URL"
fi
if [ "$(git -C "$SERVER" rev-parse -q --verify HEAD || true)" != "$COMMIT" ]; then
    log "checking out Phoenix $COMMIT"
    git -C "$SERVER" fetch -q --depth 1 origin "$COMMIT"
    git -C "$SERVER" checkout -q -f --detach FETCH_HEAD
fi
log "updating submodules (phoenix_ac, ximeshes, navmeshes)"
git -C "$SERVER" submodule update -q --init --checkout --depth 1 modules/phoenix_ac ximeshes navmeshes

# 2. Live runs with the temporary patches: two enabled fishing modules need them, and one of them changes the
#    YAML merge. Skip a patch that is already applied.
for patch in "$SERVER"/modules/temp_patch/*.patch; do
    [ -e "$patch" ] || continue
    if git -C "$SERVER" apply --reverse --check "$patch" 2> /dev/null; then continue; fi
    log "applying ${patch##*/}"
    git -C "$SERVER" apply "$patch"
done

# 3. The dump module, loaded after every other module. Copy only on change so the build stays incremental.
mkdir -p "$SERVER/modules/aggroglow_dump"
cmp -s "$ROOT/tools/phoenix/aggroglow_dump/aggroglow_dump.cpp" "$SERVER/modules/aggroglow_dump/aggroglow_dump.cpp" ||
    cp "$ROOT/tools/phoenix/aggroglow_dump/aggroglow_dump.cpp" "$SERVER/modules/aggroglow_dump/aggroglow_dump.cpp"
grep -qx 'aggroglow_dump/' "$SERVER/modules/init.txt" ||
    printf '\n# aggroglow: dump every loaded mob (tools/phoenix/refresh.sh)\naggroglow_dump/\n' >> "$SERVER/modules/init.txt"

# 4. Build xi_map.
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
cmake --build "$BUILD" -j "${JOBS:-$(nproc)}" > "$WORK/build.log" 2>&1 || fail "build; see $WORK/build.log"
[ -x "$SERVER/xi_map" ] || fail "no xi_map after the build"

# 5. A private MariaDB holding Phoenix's database. Always stopped on exit.
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
log "starting MariaDB on 127.0.0.1:$PORT"
mariadbd --no-defaults --datadir="$DB/data" --socket="$SOCKET" --port="$PORT" --bind-address=127.0.0.1 \
    --pid-file="$DB/mariadb.pid" --log-error="$DB/error.log" --tmpdir="$TMPDIR" --character-set-server=utf8mb4 \
    --collation-server=utf8mb4_general_ci --max-allowed-packet=256M &
for _ in $(seq 120); do
    [ -S "$SOCKET" ] && break
    sleep 0.5
done
[ -S "$SOCKET" ] || fail "MariaDB did not start; see $DB/error.log"
mariadb --no-defaults -S "$SOCKET" -u root -e "CREATE DATABASE IF NOT EXISTS xidb CHARACTER SET utf8mb4 \
    COLLATE utf8mb4_general_ci; CREATE USER IF NOT EXISTS 'xi'@'127.0.0.1' IDENTIFIED BY 'xi'; \
    GRANT ALL PRIVILEGES ON xidb.* TO 'xi'@'127.0.0.1'; FLUSH PRIVILEGES;"
[ -f "$SERVER/tools/config.yaml" ] ||
    printf -- "- mysql_bin: /usr/bin/\n- auto_backup: 0\n- auto_update_client: false\n- db_ver: ''\n" > "$SERVER/tools/config.yaml"
export XI_NETWORK_SQL_HOST=127.0.0.1 XI_NETWORK_SQL_PORT="$PORT" XI_NETWORK_SQL_LOGIN=xi XI_NETWORK_SQL_PASSWORD=xi \
    XI_NETWORK_SQL_DATABASE=xidb
log "loading the database (dbtool update full)"
(cd "$SERVER" && "$VENV/bin/python" tools/dbtool.py update full < /dev/null > "$WORK/dbtool.log" 2>&1) ||
    fail "dbtool; see $WORK/dbtool.log"

# 6. Run the map server until the dump module writes every loaded mob and exits, several times: some mobs change
#    aggression while the server runs (elementals, the Ghrah forms), and compact.py counts a mob as aggressive if any
#    run saw it attack.
rm -f "$WORK"/aggroglow_mobs*.json
dumps=()
for run in $(seq "$DUMPS"); do
    dump="$WORK/aggroglow_mobs_$run.json"
    log "running xi_map until dump $run of $DUMPS is written"
    (cd "$SERVER" && AGGROGLOW_DUMP_PATH="$dump" timeout 1200 ./xi_map --ip 127.0.0.1 --port 54230 < /dev/null \
        > "$WORK/map.log" 2>&1) || fail "xi_map; see $WORK/map.log"
    [ -s "$dump" ] || fail "xi_map wrote no dump; see $WORK/map.log"
    dumps+=("$dump")
done

# 7. Compact and validate into a staging folder; only then replace data/.
rm -rf "$STAGE"
python3 "$ROOT/tools/phoenix/compact.py" "${dumps[@]}" "$STAGE" "$COMMIT"
python3 "$ROOT/tools/phoenix/validate.py" "$STAGE/phoenix_mobs.tsv" || fail "validation; data/ left unchanged"
mkdir -p "$ROOT/data"
cp "$STAGE/phoenix_mobs.tsv" "$STAGE/phoenix_mobs.meta" "$ROOT/data/"
log "data/phoenix_mobs.tsv and data/phoenix_mobs.meta updated"
