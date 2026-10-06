# AggroGlow

An Ashita v4 plugin for FFXI (PhoenixXI) that outlines nearby monsters with a colored border showing whether they
will attack you. It never targets anything.

| Border | Meaning |
|---|---|
| Red | Aggressive, and not Too Weak for your level. Too Weak mobs still count while you are resting or sitting. |
| Green | Not aggressive, or Too Weak for your level |
| Gray | Not in the bundled MobDB data |

## Use

- `/load aggroglow`
- `/aggroglow` or `/ag` opens the settings window:
  - thickness and smoothness
  - max distance
  - per-category show/hide and colors
  - con table (era or modern)
  - a status line with frame time
- `/ag on` and `/ag off` toggle outlines. `/ag help` lists the commands.

Settings are saved to `config/aggroglow/settings.ini`.

## How it works

- **Finding mob meshes:** FFXI skins character models on the CPU, so their draw calls have an identity world
  matrix. The plugin finds a draw's owner from the entity actor pointer on the call stack.
- **Drawing the border:** it marks each mob's pixels in the stencil buffer, then draws solid-color copies of the
  mesh shifted a few pixels around the silhouette.
- **Deciding the color:** the aggro flag and level range come from MobDB. The Too Weak rule uses Phoenix's
  exp tables.

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

- **Server data drift:** the mob data comes from MobDB, not Phoenix. Mobs that PhoenixXI has changed may be colored wrong.
- **Not handled:** the server-only "always aggro" modifier and level-modifying effects.
- **Walls:** outlines are hidden behind walls, just like the mob.

## Credits and licenses

- **MobDB zone data:** ThornyFFXI/mobdb, MIT License (`third_party/mobdb/LICENSE`).
- **Experience tables and aggro rules:** ported from Phoenix (phoenixffxi/Phoenix, GPL-3.0).
- **Ashita SDK:** `third_party/ashita-sdk`, AshitaXI/Ashita-v4beta at commit 4171c74.
