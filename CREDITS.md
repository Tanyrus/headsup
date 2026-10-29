# Credits and licenses

HeadsUp is free software under the GNU General Public License, version 3 or later (see `LICENSE`).

- **Mob data, experience tables, /check rules, zone names, regions, weather calendars and player titles:** generated
  from Phoenix (phoenixffxi/Phoenix, GPL-3.0) by its own map server, except the zone names, regions, weather calendars
  and titles, which are read from its source and SQL files; the aggro and /check logic follows Phoenix's source.
- **The search server's protocol:** the ID list request and answer, and the game's Blowfish variant with its starting
  tables (`src/ffxi_blowfish_tables.inc`), follow LandSandBoat (LandSandBoat/server, GPL-3.0): `src/common/blowfish.cpp`,
  `src/search/search_handler.cpp` and `src/search/packets/search_list.cpp`.
- **The game's model files:** how a model is found from its number and read (the file tables, the race and gear
  tables in `FFXiMain.dll`, skeletons, meshes, pictures and animations) follows Xim by Asura.Aamace (GPL-3.0,
  https://xim.pages.dev) and Vanalytics by Soverance (MIT, https://github.com/Soverance/Vanalytics), checked against
  the game's own files.
- **Ashita SDK:** `third_party/ashita-sdk`, AshitaXI/Ashita-v4beta at commit 4171c74 (LGPL-3.0), with the
  DirectX 8 SDK headers it ships.
- **Fonts:** Montserrat, Marcellus SC, Cinzel, Cormorant SC and Barlow Condensed, bundled in `third_party/fonts` under
  the SIL Open Font License 1.1 (their notices ship beside them).
