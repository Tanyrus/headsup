# HeadsUp

An Ashita v4 plugin for Final Fantasy XI on PhoenixXI. It outlines nearby monsters in a color that tells you whether
they will attack you, and replaces the game's nameplates with clearer ones: names, level and con, how a monster
detects you, player status icons and a target cursor. It only reads what the game already shows and the server
already sends, apart from one request of its own: the conquest request the game sends when you open Region Info, sent
once after loading or zoning while the top bar's Zone or Conquest widget is on. It never targets or checks anything
itself.

Download `headsup.dll` from the latest release on the [Releases](https://github.com/Tanyrus/headsup/releases) page and
put it in Ashita's `plugins` folder. Load it with `/load headsup`; `/hu` opens its settings, and `/hu menu <page>`
opens them at a page, such as `/hu menu widgets` or `/hu menu mobs colors`.

> **About the data:** HeadsUp's monster data (levels, aggression, links and detection) is built from Phoenix's own
> server code: the map server is compiled from Phoenix's public source and records every monster it loads. The top
> bar's zone names, regions, weather forecasts and player titles come from the same source. All of it matches the
> Phoenix version it was built from, so a monster changed on the live server since then can show the wrong color,
> level or icons, a changed zone the wrong name, region or forecast, and a changed title the wrong title.

![Settings window](resources/settings.png)

## Aggro outlines

Each monster gets a colored border, from Phoenix's own mob data and your level:

| Border | Meaning |
|---|---|
| Red | Will attack you |
| Green | Won't attack you, or is too weak to bother you |
| Gold-orange | A notorious monster that will attack you |
| Gold | A notorious monster that won't |
| Purple | A lottery placeholder for a notorious monster |
| Gray | Not in the mob data |

Green and gray are off until you turn them on in the settings. Mobs claimed by your party, and mobs claimed by anyone
else, can each take a color of their own, which helps in crowded places such as Dynamis. Both are off by default.
Outlines reach up to 50 yalms, as far as the game draws mobs.

## Nameplates

Above a monster's name: a row of icons (aggressive or passive, links, and how it detects you: sight, true sight,
sound, scent, magic, job abilities, low HP) and its level and con, colored like `/check`. After you `/check` it, the
line shows its exact level for that spawn. Mob IDs show as the last three hex digits players name placeholders by, the
whole ID in decimal, or both. With them on, a placeholder shows `[PH]` on its level line, and an optional timer above
your name counts down to the respawn of each placeholder you see die. The icons can sit at the ends of the level line,
together over its middle, or on the line itself, and while you have a target the other mobs' plates can fade. Where
several names would cover each other they can stack, each rising over the one in front, while your target's keeps its
place: "Stack names" on the Global page turns it on.

Names are drawn in a bundled serif font, Marcellus SC by default, with Cinzel and Cormorant SC also included and every
font installed in Windows available too. Mobs, players and NPCs each have their own name font. Badge numbers use the bundled Barlow Condensed, plain enough to read at a few
pixels tall. A monster's name glows in its border color, with a small ornament between the
name and its level line, so a glance tells you whether it will attack.

![A Goblin Leecher's red outline and nameplate](resources/nameplate_and_outline.png)

Players get XIUI's HQ status icons beside their names: seeking party, bazaar, linkshell in its color, away, mentor,
new adventurer, GM and level sync. Each icon can sit left or right of the name, or be hidden, and they can be put in
any order, with a limit on how many show. Icons can share a slot, which shows the first of them a player has. GM always
shows, nearest the name on its left. The names of players
sitting, resting or in a chair are lowered to their heads.

Your target gets a bobbing arrow, or Phoenix's feather, in place of the game's cursor: one color for your target,
another while locked on, and another for the sub-target you are picking. Its dark outline can be made thinner or
thicker.

![A player's nameplate with their icons and the feather cursor](resources/player_nameplate.png)

It can also put PlayOnline's chocobo back as your mouse pointer, the one the PlayOnline Viewer had.

Every part has its own switch, and fonts, sizes and colors are in the settings. Settings that belong to a feature that
is off are greyed out and cannot be changed until it is on. The Global page's Reset card puts every setting back to its
default.

## Top bar

A bar across the top of the screen, on by default, with these widgets from left to right in its default layout:

| Widget | Shows | Hover for |
|---|---|---|
| Zone | The zone's name, its region and who holds it | The region, who holds it and the zone's ID |
| Weather | The weather now, with its element, and today's chance of it | A five-day forecast, starting today |
| Vana'diel | The weekday in its element's color, the time and moon | The date, and when the day and moon change |
| Job | Your jobs and levels, and the EXP to your next level (your merits at 75), over a bar of it | EXP per hour, the time to level, limit points |
| Party | How many are in your party, with the lowest HP and MP in your zone | Each member's jobs, levels, HP, MP and TP |
| Conquest | The time to the weekly tally and your conquest points | Your Imperial Standing and the tally in Vana'diel days |
| Signet | The time left on Signet or Sanction, only while one is on | What it gives and whether it works where you are |
| Server time | The time in Japan, which Phoenix keeps, or another time zone's | The time to the daily reset and the conquest tally |
| Target | A monster's name in its border color, its level and con | Weaknesses, resistances, immunities, tags, spawn, drops at Treasure Hunter 0 and what can be stolen; for a player you checked, their jobs, master level, linkshell and title, as of your check |
| Gil | Your gil, or the currency picked in its section | Gil and every currency you have |
| Bags | Your inventory's used and maximum space | Every bag you have |
| Connection | The game's own S and R, bytes sent and received a second; amber when no data arrives, red once the game's meter drops, with the latency if switched on | Latency, the last server data, this session's totals |
| FPS | Frames per second | The frame time and the slowest recent frame |
| Clock | The time on your computer | The date |
| Distance | How far your target is, in yalms | Nothing |

Under the bar at the screen's left, Game help has a thin bordered box of its own while the game has help to show, all
on one line: the open menu's name, a bag's slots beside a bag's name, then the selected entry's help, a target prompt
or a checked player's jobs. The box is as wide as its text, up to half the screen. Help longer than that ends in
"...", and hovering it shows all of it. Its switch is "Help bar" in the Help Bar section, and the game's own copy can be hidden with "Hide
game help".

Game windows like the equipment window are moved straight down through the game's own move, so the mouse follows them:
all by the same amount, from under the game's own help bar to under HeadsUp's, so windows that open together stay
together. Under "Game Windows" on the Top Bar page, each window that has opened since HeadsUp loaded is listed with an
Under Bar switch and an X and Y offset, in the game's menu resolution, that moves it from where the game puts it;
the equipment window and those that open with it start under the bar. The help bar has an X and Y offset of its own, in
screen pixels, in its card. Unloading HeadsUp puts every open window it moved back where the game put it.

Hover a widget to open its details. Click it to keep them open; click it again to let them close once the pointer leaves
the widget and its popup. Left and right clicks on the bar or an open popup never reach the game in windowed modes.

Under Target, a monster in Phoenix's mob data has a popup; a player you have checked shows their jobs and levels, or
? ? ? when anonymous, with a popup of the check; any other player or NPC shows just its name. The weather widget reads the
weather the game keeps; if HeadsUp cannot find it, it goes by the server's weather changes and shows the day's usual
weather until one comes. HeadsUp asks the server for your currencies as the game's Currencies menu does: once after
loading, once per zone and each minute while the Gil widget's popup is open.

Game help's bar, Job, Connection, FPS and Distance start on, and Server time, Party, Conquest and Signet start off. The connection's S and R come from the game's own readout, which
the bar covers and the "Hide readout" switch stops the game drawing; they show "-" if HeadsUp
cannot find them in the game's memory. The latency is HeadsUp's own measure, from a packet sent to the server's next
reply, rounded to a frame.

After each zone change (and after loading) HeadsUp asks the server for the conquest standings, as opening Region Info
does, while the Zone or Conquest widget is on, so the zone's owner and your conquest points show without opening that
menu.

The bar stays off until your character is in game, so it does not draw over the login screens.

A bar under 36 px is too short for two lines of text. There each widget's second line goes after its title, dimmed, as
in "27/30 Bags"; One Line can put it before the title instead, or leave it off.

The Top Bar page of the settings (`/hu`) has a switch for the bar, a theme, a separators choice, a height from 24 to 64
px (45 by default), a title font and a text font (the theme's own by default), a text size and a popup size, and one
that hides the game's connection readout. Under Widgets,
Left, Center and Right are wells side by side, each holding its widgets' chips in bar order; a chip drags within its
well or into another, an empty one too, and a click switches it on or off; "Reset" puts them back. Below come the Help
Bar and Game Windows sections, then a section for each widget in bar order: its switch, its second line and icons
where it has them, and its own options (Target's details, Gil's currency, the clocks' 24-hour and seconds switches and Server time's
zone, Job's EXP bar under or beside the text or off and its job names, Connection's latency). Separators picks
diamonds or none between neighbouring widgets in any theme, and starts at the theme's own. "Text size" and "Popup
size" set those sizes in pixels or leave them on Auto, which follows the bar height; a bar size too large for the height is drawn as large as fits, and the menu says
what that is. Bar text runs from 9 to 32 px and popup text from 10 to 32 px. The themes are:

| Theme | Look |
|---|---|
| Phoenix gold | Ornate: an edge line with diamonds, and a framed target. The default. |
| Umbra dark | Each part in a dark rounded pill |
| Phoenix red, ornate | The ornate look in red |
| Phoenix red, pills | The pill look in red |

The game's help bar is the strip across the top that shows a menu's title and help text while a menu is open. HeadsUp's
bar covers its top, and the rest of it shows below the bar unless the bar is taller. The switch, off by default, stops
the game drawing it, and Game help's bar, while it is on, shows the same text under HeadsUp's bar. The game's connection readout (S, R and %) at the top right is under the bar
too: covered, or its lower part showing below a shorter bar. Its own switch, also off by default, stops the game drawing
it and leaves every other window in place.

The nameplate badges (the weaknesses, resistances and immunities in a monster's icon row) are off by default. The target
popup shows the same data. On the Mobs page, Detection, Weaknesses, Statuses and Immunities each have a card with the
switch in its title and their own settings, and the Icons card places them all.
