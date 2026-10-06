# AggroGlow

An Ashita v4 plugin for FFXI (PhoenixXI). It outlines nearby monsters with a colored border showing whether they
will attack you, and shows their level, con and XIUI's aggro and detection icons above their names. It can also
replace the game's mob nameplates with its own. It never targets anything unless you turn on auto-examine.

| Border | Meaning |
|---|---|
| Red | Aggressive, and not Too Weak for your level. Too Weak mobs still count while you are resting or sitting. Mobs Phoenix marks "always aggro" count at any level. |
| Green | Not aggressive, or Too Weak for your level |
| Gold-orange | A notorious monster that will attack you |
| Gold | A notorious monster that won't |
| Gray | Not in the Phoenix mob data |

## Use

- `/load aggroglow`
- `/aggroglow` or `/ag` opens the settings window:
  - thickness and smoothness
  - max distance
  - per-category show/hide and colors
  - nameplates: replace the game's, icons, level and con, font, bold, sizes, scale with distance
  - auto-examine on or off
  - a status line with frame time
- `/ag on` and `/ag off` turn outlines and nameplates on and off. `/ag help` lists the commands.
- `/ag debug` writes what every mob with a nameplate shows (data, label, icons, name color, positions) to
  `logs/aggroglow/debug-<time>.txt`, then adds 120 frames of nameplate data to the same file.

Settings are saved to `config/aggroglow/settings.ini`.

## Nameplates

Above every mob's name, whether or not it is outlined:

1. a row of icons (`Show icons`)
2. its level and con (`Show level and con above names`)

With `Replace game nameplates` on, the game's own mob names are hidden and AggroGlow draws the name too, in the color
the game uses for it (so claim status stays visible), with the icons and level above it. Dead mobs keep just their
name. Player and NPC names are never touched.

| Icon | Meaning |
|---|---|
| Aggro (red) / Passive (blue) | Whether the mob aggros at all (the outline says whether it would attack you). The HQ versions mark notorious monsters |
| Link | Links with its family |
| Sight, True Sight, Sound, Scent | How it detects you. True Sight sees through Invisible or Sneak |
| Magic, JA, Blood | Detects spellcasting, job abilities and weapon skills, or low HP |

Font family (Arial, Tahoma, Verdana, Trebuchet MS, Courier New or Times New Roman), bold, and the name, level and
icon sizes are in the menu. With `Scale with distance` on, they grow and shrink with the game's own name size.

The menu also sets the colors: one for each con on the level line (Easy Prey covers Incredibly Easy Prey, Very Tough
covers Incredibly Tough, and a gray for unknown levels), the text outline, an icon tint (white keeps the icons' own
colors), and an optional name color that replaces the game's.

A nameplate appears only when the camera can see the mob: the game drew its body and its name this frame and the
frame before, and some of the name is on screen. Mobs beyond the draw distance, off screen or with names turned off
get none.

## Level and con

The level and con line:

| Label | Meaning |
|---|---|
| `Lv 20-23 EP-DC` | Not examined: the level range from the data, and the con at each end |
| `Lv 22 DC` | Examined: the exact level and con the server reported |
| `Lv ? ??` | The level is set by a script when the mob spawns, or the mob is not in the data |

Cons: TW Too Weak, IEP Incredibly Easy Prey, EP Easy Prey, DC Decent Challenge, EM Even Match, T Tough, VT Very
Tough, IT Incredibly Tough. Each label is colored like `/check`, using the harder end of a range.

## Auto-examine

Off by default. When on, the plugin sends a `/check` for the mob you target if all of these hold:

- it would give you experience (not Too Weak)
- it is within 45 yalms
- it is not a notorious monster or a battlefield mob
- it has not been checked within its respawn time (10 minutes when the data has none)
- no other automatic check went out in the last second

The game's chat line for an automatic check is hidden. Your own `/check` still prints, and it updates the label too.
An exact level lasts until the mob dies or its respawn time passes.

The `checker` addon prints its own line for automatic checks as well, because it reads the original packet. Unload
`checker` if you don't want those lines.

## How it works

- **Finding mob meshes:** FFXI skins character models on the CPU, so their draw calls have an identity world
  matrix. The plugin finds a draw's owner from the entity actor pointer on the call stack.
- **Drawing the border:** it marks each mob's pixels in the stencil buffer, then draws solid-color copies of the
  mesh shifted a few pixels around the silhouette.
- **Deciding the color:** level range, aggro flags, "always aggro", notorious, link, detection and respawn time come
  from Phoenix's own map server (below). The Too Weak rule and cons use Phoenix's era experience table.
- **Placing labels:** nameplate letters are drawn as pretransformed quads inside the 3D scene (depth between 0 and
  1); the label is centered above the run of letter-sized quads on the name's line. HUD text, such as the target bar,
  is drawn at depth 0 and is ignored, and so are stray or screen-sized quads the game sometimes draws while a mob is
  on the stack. Nameplates are placed when the game finishes drawing the frame's names, so they move with them.
- **Replacing nameplates:** in replace mode the game's mob name letters are measured, then blocked from drawing. A
  letter is hidden only where AggroGlow drew that mob's name the frame before, only if it is the size of that name's
  letters, and never inside a player's or NPC's own name. Our name appears once the game's is hidden, so the two never
  overlap. The name color is read from the hidden letters.

## Mob data

`data/phoenix_mobs.tsv` lists every mob Phoenix's map server loads (`data/phoenix_mobs.meta` names the Phoenix
commit). `tools/phoenix/refresh.sh` regenerates it:

1. clones Phoenix at `tools/phoenix/PHOENIX_COMMIT` with its submodules and temporary patches
2. builds the map server with every module plus `tools/phoenix/aggroglow_dump`
3. loads Phoenix's database into a private MariaDB
4. runs the map server until the dump module writes every loaded mob, five times (`AGGROGLOW_PHOENIX_DUMPS`)
5. merges and validates the dumps, then updates `data/`

Some mobs change aggression while the server runs (elementals, the Aw'ghrah and Eo'ghrah forms), so one dump is one
snapshot. A mob counts as aggressive if any of the runs saw it attack. Every run's dumps are kept in
`~/.cache/aggroglow-snapshots/<commit>/` (`AGGROGLOW_PHOENIX_SNAPSHOTS`) and each refresh merges all of them, so
coverage only grows (`dumps` in the meta file counts them).

It needs git, cmake, ninja, g++, Python 3, MariaDB (server and client library), LuaJIT, ZeroMQ, OpenSSL, libdwarf
and binutils, and about 6 GB in `~/.cache/aggroglow-phoenix` (`AGGROGLOW_PHOENIX_DIR`). The first run takes about
15 minutes. To follow a Phoenix update, change `PHOENIX_COMMIT`, run the script, and commit `data/`.

Records are matched by server ID. A record whose name differs from the client's (ignoring case and punctuation)
counts as missing, so stale data shows gray instead of wrong.

## Build

Requirements: `i686-w64-mingw32-g++`, `g++`, Python 3.

```bash
./build.sh test      # Python and native unit tests
./build.sh plugin    # tests, ABI check, build/aggroglow.dll
./build.sh install   # plugin, then install into ~/Games/PhoenixXI/plugins (override with AGGROGLOW_PLUGINS_DIR)
```

`tools/abi_check.py` fails the build if the code calls an Ashita interface method whose vtable slot differs between
MinGW and MSVC.

## Limitations

- **Data drift:** the data matches `PHOENIX_COMMIT`. If live runs a newer Phoenix, refresh the data.
- **Runtime aggression:** a mob whose aggression changes while the server runs, and that no refresh run saw attack,
  can show green while it is aggressive.
- **Not handled:** level-modifying effects. Detection is shown as icons, but outlines do not depend on it.
- **Walls:** outlines are hidden behind walls, just like the mob.
- **`checker` addon:** it prints automatic checks (see Auto-examine).

## Credits and licenses

- **Mob data:** generated from Phoenix (phoenixffxi/Phoenix, GPL-3.0) by its own map server.
- **Experience tables and aggro rules:** ported from Phoenix (phoenixffxi/Phoenix, GPL-3.0).
- **Ashita SDK:** `third_party/ashita-sdk`, AshitaXI/Ashita-v4beta at commit 4171c74.
- **Icons:** `third_party/mobdb-icons`, from ThornyFFXI/mobdb (MIT License), the set XIUI uses.
