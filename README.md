# HeadsUp

An Ashita v4 plugin for FFXI (PhoenixXI). It outlines nearby monsters with a colored border showing whether they
will attack you, and shows their level, con and XIUI's aggro and detection icons above their names. It can also
replace the game's mob nameplates with its own. It never targets or checks anything itself.

| Border | Meaning |
|---|---|
| Red | Aggressive, and not Too Weak for your level. Too Weak mobs still count while you are resting or sitting. Mobs Phoenix marks "always aggro" count at any level. |
| Green | Not aggressive, or Too Weak for your level |
| Gold-orange | A notorious monster that will attack you |
| Gold | A notorious monster that won't |
| Gray | Not in the Phoenix mob data |

## Use

- `/load headsup`
- `/headsup` or `/hu` opens the settings window, laid out like XIUI's in Phoenix's colors: an ON/OFF chip for
  outlines and nameplates, a sidebar of pages with the status under it, and `settings` and `color settings` tabs.
  Every setting has a `(?)` that explains it and every slider a box to type its value in:
  - Outlines: thickness, smoothness, max distance and which mob types to outline; their colors
  - Nameplates: replace the game's, level and con, icons, replace the target cursor, hide behind walls, font, bold,
    sizes, scale with distance, the feather cursor; own name color, text outline, icon tint, cursor colors and one
    color per con
  - Debug: what outlines and nameplates did last frame, and a button for `/hu debug`
- `/hu on` and `/hu off` turn outlines and nameplates on and off. `/hu help` lists the commands.
- `/hu debug` writes what every mob with a nameplate shows (data, label, icons, name color, positions) to
  `logs/headsup/debug-<time>.txt`, then adds 120 frames of nameplate data to the same file.

Settings are saved to `config/headsup/settings.ini`.

## Nameplates

Above every mob's name, whether or not it is outlined:

1. a row of icons (`Show icons`)
2. its level and con (`Show level and con above names`)

With `Replace mob names` on, the game's own mob names are hidden and HeadsUp draws the name too, in the color the
game uses for it (so claim status stays visible), with the icons and level above it. Dead mobs keep just their name.
`Replace player names` and `Replace NPC names` do the same for players (you included) and NPCs, which get only their
name. A name whose kind is not replaced is never touched. With `Show player icons` on, a replaced player name gets
the game's icons beside it in XIUI's HQ versions: seeking party, bazaar, linkshell (in the linkshell's color), away,
mentor, new adventurer and GM. Seeking party, bazaar and linkshell come from the flags the game keeps for every player
(Render.Flags1 bits 20 and 27, Flags2 bit 9, and the linkshell color), so they show as soon as HeadsUp loads; away,
mentor, new adventurer and GM come from the server's player updates (packets 0x00D and 0x037) once seen. The name
and icons are centered together over the player, as the game does; with `Center name and icons` off, the name alone is
centered and the icons hang to its left.

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
get none. With `Hide behind walls` on (the default), nameplates are drawn into the 3D scene at the depth of the game's
name, so walls and terrain in front of a name cover it like the game's own, and the game's menus sit on top.

With `Replace target cursor` on, your target's nameplate gets a bobbing arrow on top: white for your target, purple
while you are locked on and gold for what you are picking with the sub-target cursor, each color in the menu, and
`Phoenix feather cursor` swaps the arrow for Phoenix's feather icon in the same colors. The
game's own cursor over it is hidden: the small quad the game draws in its UI layer, centered just above the name, while
that entity is being drawn. It works for mobs, players and NPCs alike; a target whose name the game does not draw
keeps the game's cursor.

## Level and con

The level and con line:

| Label | Meaning |
|---|---|
| `Lv 20-23 EP-DC` | Not checked: the level range from the data, and the con at each end |
| `Lv 22 DC` | You checked it: the exact level and con the server reported |
| `Lv ? ??` | The level is set by a script when the mob spawns, or the mob is not in the data |

Cons: TW Too Weak, IEP Incredibly Easy Prey, EP Easy Prey, DC Decent Challenge, EM Even Match, T Tough, VT Very
Tough, IT Incredibly Tough. Each label is colored like `/check`, using the harder end of a range.

## Checks

When you `/check` a mob, its label shows the exact level and con the server reported, until the mob dies or its
respawn time passes (10 minutes when the data has none). The plugin reads the server's reply and never sends a check
itself.

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
- **Drawing nameplates:** the text is drawn with Windows GDI into a texture per mob and line, redrawn only when its
  text, font, size or colors change; the icons are decoded from their PNGs at build time. The game draws its 3D scene
  into an off-screen image and copies it to the back buffer; with `Hide behind walls` on, nameplates are drawn into
  that image just before the copy, depth-tested at the name's depth, otherwise on top at the end of the frame.
- **Replacing nameplates:** in replace mode the game's mob name letters are measured, then blocked from drawing. A
  letter is hidden only where HeadsUp drew that mob's name the frame before, only if it is the size of that name's
  letters, and never inside a player's or NPC's own name. Our name appears once the game's is hidden, so the two never
  overlap. The name color is read from the hidden letters.

## Mob data

`data/phoenix_mobs.tsv` lists every mob Phoenix's map server loads (`data/phoenix_mobs.meta` names the Phoenix
commit). `tools/phoenix/refresh.sh` regenerates it:

1. clones Phoenix at `tools/phoenix/PHOENIX_COMMIT` with its submodules and temporary patches
2. builds the map server with every module plus `tools/phoenix/headsup_dump`
3. loads Phoenix's database into a private MariaDB
4. runs the map server until the dump module writes every loaded mob, five times (`HEADSUP_PHOENIX_DUMPS`)
5. merges and validates the dumps, then updates `data/`

Some mobs change aggression while the server runs (elementals, the Aw'ghrah and Eo'ghrah forms), so one dump is one
snapshot. A mob counts as aggressive if any of the runs saw it attack. Every run's dumps are kept in
`~/.cache/headsup-snapshots/<commit>/` (`HEADSUP_PHOENIX_SNAPSHOTS`) and each refresh merges all of them, so
coverage only grows (`dumps` in the meta file counts them).

It needs git, cmake, ninja, g++, Python 3, MariaDB (server and client library), LuaJIT, ZeroMQ, OpenSSL, libdwarf
and binutils, and about 6 GB in `~/.cache/headsup-phoenix` (`HEADSUP_PHOENIX_DIR`). The first run takes about
15 minutes. To follow a Phoenix update, change `PHOENIX_COMMIT`, run the script, and commit `data/`.

Records are matched by server ID. A record whose name differs from the client's (ignoring case and punctuation)
counts as missing, so stale data shows gray instead of wrong.

## Build

Requirements: `i686-w64-mingw32-g++`, `g++`, Python 3.

```bash
./build.sh test      # Python and native unit tests
./build.sh plugin    # tests, ABI check, build/headsup.dll
./build.sh install   # plugin, then install into ~/Games/PhoenixXI/plugins (override with HEADSUP_PLUGINS_DIR)
```

`tools/abi_check.py` fails the build if the code calls an Ashita interface method whose vtable slot differs between
MinGW and MSVC.

## Limitations

- **Data drift:** the data matches `PHOENIX_COMMIT`. If live runs a newer Phoenix, refresh the data.
- **Runtime aggression:** a mob whose aggression changes while the server runs, and that no refresh run saw attack,
  can show green while it is aggressive.
- **Not handled:** level-modifying effects. Detection is shown as icons, but outlines do not depend on it.
- **Walls:** outlines are hidden behind walls, just like the mob.

## Credits and licenses

- **Mob data:** generated from Phoenix (phoenixffxi/Phoenix, GPL-3.0) by its own map server.
- **Experience tables and aggro rules:** ported from Phoenix (phoenixffxi/Phoenix, GPL-3.0).
- **Ashita SDK:** `third_party/ashita-sdk`, AshitaXI/Ashita-v4beta at commit 4171c74.
- **Icons:** `third_party/mobdb-icons`, from ThornyFFXI/mobdb (MIT License), the set XIUI uses.
- **Player icons:** `third_party/xiui-icons`, from XIUI (tirem/xiui, MIT License).
- **Feather cursor:** `third_party/phoenix-feather`, Phoenix's feather icon from the phoenix-platform repository
  (phoenix-icon.svg on the phoenix-xi.com media page). It has no license file; see its `SOURCE.md`.
- **Settings window colors:** the Phoenix palette of KiplingFFXI/cadence's settings window, the colors of
  phoenix-xi.com.
