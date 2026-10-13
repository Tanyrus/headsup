# HeadsUp

An Ashita v4 plugin for Final Fantasy XI on PhoenixXI. It outlines nearby monsters in a color that tells you whether
they will attack you, and replaces the game's nameplates with clearer ones: names, level and con, how a monster
detects you, player status icons and a target cursor. It only reads what the game already shows and the server
already sends, apart from three requests the game's own menus make, which the top bar sends for you: the one the game
sends when you open Region Info, while the bar's Zone widget is on, and the two it sends when you open the
Currencies menu, while the Gil widget is on ([Top bar](#top-bar) says when). It never targets or checks anything
itself.

Download `headsup.dll` from the latest release on the [Releases](https://github.com/Tanyrus/headsup/releases) page and
put it in Ashita's `plugins` folder. Load it with `/load headsup`; `/hu` opens its settings, and `/hu menu <page>`
opens them at a page, such as `/hu menu entities`, `/hu menu mobs` or `/hu menu topbar`.

> **About the data:** HeadsUp's monster data (levels, aggression, links and detection) is built from Phoenix's own
> server code: the map server is compiled from Phoenix's public source and records every monster it loads. The top
> bar's zone names, regions, weather forecasts and player titles come from the same source. All of it matches the
> Phoenix version it was built from, so a monster changed on the live server since then can show the wrong color,
> level or icons, a changed zone the wrong name, region or forecast, and a changed title the wrong title.

![Settings window](resources/settings.png)

## Settings

`/hu` opens the settings window. `/hu menu <page>` opens it at a page: `entities`, `mobs`, `players`, `self`, `npcs`,
`pets`, `userinterface`, `topbar`, `mousecursor`, `configmenu` or `debug`. The pages, in the order of the window's list:

| Page | Holds |
|---|---|
| Entities | What every kind shares: the nameplate and outline styles, the target cursor and the five color palettes. Under it, a page for each kind: Mobs, Players, Self, NPCs and Pets |
| User Interface | The Window Groups and Game Windows cards, which move the game's windows. Under it, the Top Bar page |
| Mouse Cursor | The chocobo pointer and the pointer fixes |
| Config Menu | The settings window's own font and size, and the button that puts every setting back to its default |
| Debug | Switches HeadsUp off as a whole, and writes a report for bug reports |

Settings that belong to a feature that is off are greyed out and cannot be changed until it is on.

## Outlines and nameplates

Each kind of entity has its own page: Mobs, Players, Self, NPCs and Pets. A page starts with a Visibility card. Each
row of it is a group of entities (for mobs: Aggressive, Passive, No Data, Aggressive NM, Passive NM, NM Placeholder,
Party Claim and Others' Claim; for players: Players, Party Members, Friends and Linkshell Co-Members; Self, NPCs and
Pets have one row each). A row has two choices: what its nameplate does and what its outline does. Giving the claim rows a
look of their own helps in crowded places such as Dynamis.

| Nameplate | What it does |
|---|---|
| Hidden | No name at all |
| Shown | HeadsUp's nameplate |
| When Targeted | HeadsUp's nameplate only while the entity is your target |
| Expanding | Its name alone until you target it, then the whole nameplate |
| Vanilla | The game's own name, and nothing of HeadsUp's |

| Outline | What it does |
|---|---|
| Disabled | No outline |
| Shown | An outline |
| When Targeted | An outline only while the entity is your target |
| By Nature | On the Party Claim, Others' Claim and NM Placeholder rows only: the mob keeps the outline it would have had otherwise, unclaimed, or by whether it would attack you |

Outlines reach up to 50 yalms, as far as the game draws mobs; the Outline Style card sets their thickness, smoothness
and range. Outlines are hidden behind walls, just like the mob.

### Outline colors

The Outline Design card gives each row's outline a color: one you pick, or one from a palette by the entity's state. The
palettes are on the Entities page:

| Palette | Colors by |
|---|---|
| Aggression | Whether a mob would attack you, with notorious monsters, their placeholders and mobs with no data apart |
| Toughness | How a mob cons to you, from Too Weak to Very Tough, and a color for an unknown level |
| Claim | Unclaimed, claimed by someone else or claimed by you |
| Target Status | Not targeted, targeted, locked on, the sub-target you are picking, and a sub-target out of range |
| Target Type | A mob, an NPC or object, a pet, a player, a friend, a party member or a linkshell co-member |

A text color can come from a palette, from the game's own color for the name (which shows claim status), or be one you
pick.

The Aggression palette starts with these colors:

| Color | The mob |
|---|---|
| Red | Will attack you |
| Green | Will not: it is passive, or aggressive but Too Weak for you |
| Orange | Is a notorious monster that will attack you |
| Gold | Is a notorious monster that will not |
| Purple | Is a lottery placeholder, whose death can pop a notorious monster |
| Gray | Is not in HeadsUp's data |

A mob's outline, the glow of its name and the ornament over its name all start on this palette. The Passive and No Data
rows start without an outline, so green and gray show at first only in a name's glow and ornament; set either row's
outline to Shown to outline those mobs too.

### The nameplate

The Nameplate Design card holds three lines of text. You type in a line as you would anywhere, and put tags in it: a
tag is a dollar sign and a word, such as `$name`, and it draws something. Click a tag to put it where you last typed, or
drag it onto a line. Anything else you type shows as typed, and `$$` is one dollar sign. For example, `$level $con` on
the first line and `$name` on the second shows a monster's level and con over its name.

| Tag | Kinds | Draws |
|---|---|---|
| `$name` | All | The name |
| `$level` | Mobs, Players, Self | A mob's level or range, such as 20-23, and its exact level after you `/check` it; a player's main job level when known |
| `$con` | Mobs | The con at each end of the level range, such as EP-DC, or one con after a `/check`. A tag has no color of its own: its line's Color colors it, and the default level line takes the Toughness palette, which colors it like `/check` |
| `$detect` | Mobs | Whether it is aggressive or passive, whether it links, and how it detects you: sight, true sight, sound, scent, magic, job abilities, low HP. The Detection card picks which |
| `$id` | Mobs | The last three hex digits of its ID, which players name placeholders by, such as [106] |
| `$serverid` | Mobs | Its whole ID in decimal |
| `$ph` | Mobs | [PH] on a lottery placeholder, whose death can pop a notorious monster |
| `$weak`, `$resist` | Mobs | The damage types and elements it is weak to or resists, with percents or ranks |
| `$status` | Mobs | The statuses it is weak to or resists, as the Statuses card picks |
| `$immune` | Mobs | The statuses it cannot take |
| `$job` | Players, Self | Jobs, such as WAR/MNK: yours, your party's, and another player's once you have checked them |
| `$gm`, `$mentor`, `$new`, `$sync`, `$away` | Players, Self | The GM, mentor, new adventurer, level sync and away icons |
| `$linkshell`, `$bazaar`, `$seeking` | Players, Self | The linkshell icon in its color, the bazaar icon and the seeking party icon |
| `$owner` | Pets | The pet's owner |

The icons are XIUI's HQ ones for players and MobDB's for mobs. An icon shows only when it applies to that player. Away, mentor, new adventurer and GM appear once the server has told
HeadsUp about the player, and the names of players sitting, resting or in a chair are lowered to their heads.

Weaknesses, resistances, statuses and immunities are shown as small badges on icons. The Badges card sets their font,
colors and whether they show ranks; the Weaknesses and Statuses cards pick what is worth a badge.

After the design come a Line Style card, with a tab for each line, and an Ornament card. A line has these settings:

- **Show:** always, or only when any checked condition holds: when targeted, unclaimed, your claim, others' claim,
  engaged in combat, too weak, aggressive, passive, party member, friend or linkshell co-member (the ones that fit the
  kind)
- **Alignment:** left, center or right against the plate's widest line
- **Font:** a line's own font and bold, or the Default font from the Entities page
- **Text size and icon size:** a percent of the base font size set on the Entities page; the sliders show the pixels too
- **Color:** a color of your own, the game's, or from a palette
- **Shadow, glow and their colors:** the soft dark shadow behind the letters, and a colored halo, such as a monster's
  name glowing in its outline color
- **Sync line style:** borrows every setting from another line, so the two always match

The Ornament is a diamond rule between lines 1 and 2, or 2 and 3, drawn while both lines show; its width, height and
color are set in its card.

![A Goblin Leecher's red outline and nameplate](resources/nameplate_and_outline.png)

The Nameplate Style card, on the Entities page, sets what every nameplate shares: a vertical offset, a base font size,
the Default font (any font installed in Windows), and whether names scale with distance, between a smallest and a
largest size. With Scale Your Own Name off, your own nameplate stays one size. With Stack Names on, names that would
cover each other rise one over another, while your target's keeps its place. The Fading card on the Mobs page makes the
other mobs' names fainter while you have a target, and its Glow Off While Fighting switch takes a mob's glow away once
you or your party has claimed it.

![A player's nameplate with their icons and the feather cursor](resources/player_nameplate.png)

### Friends and linkshell co-members

Friends are a list of character names on the Players page: type a name and press Enter, and click a name to remove it.
A Friend row and the Friend colors then apply to them, unless they are in your party. A linkshell co-member is someone in
your linkshell who is neither in your party nor a friend. The game does not say which linkshell a player wears until you
`/check` them, so a co-member is known only after you have checked them.

### Placeholder timers

The Self page has a Placeholder Timers card. With Respawn Timers on, a countdown appears above your name for each
placeholder you see die, such as Bigmouth Billy [11A] 3:15; it reads "up" when the placeholder is due.

### Target cursor

Your target gets a bobbing arrow, or Phoenix's feather, in place of the game's cursor. The Target Cursor card on the
Entities page switches it, and sets its size and the thickness of its dark edge. Its colors come from the Target Status
palette: one for your target, another while locked on, another for the sub-target you are picking and another when that
sub-target is out of range.

### Screenshot mode

`/hu screenshot` hides every name, the top bar, HeadsUp's menu and every addon window, until you run it again. Bind it
to a key with Ashita's `/bind`, for example:

```
/bind F12 /hu screenshot
```

The top bar also hides itself during cutscenes and NPC dialogs.

## Mouse pointer

The Mouse Cursor page puts PlayOnline's chocobo back as your mouse pointer, the one the PlayOnline Viewer had. It also
holds a fix for the game's pointer after a resize and over Ashita's windows, and the keys that do not make the game hide
its pointer.

## Top bar

A bar across the top of the screen, on by default, with these widgets from left to right in its default layout:

| Widget | Shows | Hover for |
|---|---|---|
| Zone | The zone's name, its region and who holds it | The zone's ID and region; in a conquest region, each nation's and the beastmen's share of this week's influence there, the largest first, with a star beside whoever holds it, and the time to the tally |
| Weather | The weather now, with its element, and today's chance of it | A five-day forecast, starting today |
| Vana'diel | The weekday in its element's color, the time and moon | The date, and when the day and moon change |
| Job | Your jobs and levels, and the EXP to your next level (your merits at 75), over a bar of it | EXP per hour, the time to level, limit points |
| Party | How many are in your party, with the lowest HP and MP in your zone | Each member's jobs, levels, HP, MP and TP |
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
"...", and hovering it shows all of it. Its switch is the Help Bar card's own, on the Top Bar page, and "Hide Game
Help" in that card hides the game's own copy.

Game windows like the equipment window are moved straight down through the game's own move, so the mouse follows them:
all by the same amount, from under the game's own help bar to under HeadsUp's, so windows that open together stay
together. The Game Windows card on the User Interface page lists the windows HeadsUp has a name for and any other that
has opened since it loaded, each with an Under Bar switch and an X and Y offset, in the game's menu resolution, that
moves it from where the game puts it; the equipment window and those that open with it start under the bar. The Window
Groups card above it moves several windows together by one offset. The help bar has an X and Y offset of its own, in
screen pixels, in its card. Unloading HeadsUp puts every open window it moved back where the game put it.

Hover a widget to open its details. Click it to keep them open; click it again to let them close once the pointer leaves
the widget and its popup. Left and right clicks on the bar or an open popup never reach the game in windowed modes.

Under Target, a monster in Phoenix's mob data has a popup; a player you have checked shows their jobs and levels, or
? ? ? when anonymous, with a popup of the check; any other player or NPC shows just its name. The weather widget reads the
weather the game keeps; if HeadsUp cannot find it, it goes by the server's weather changes and shows the day's usual
weather until one comes. While the Gil widget is on, HeadsUp asks the server for your currencies with the two requests
the game's Currencies menu sends: once after loading, once per zone and each minute while the widget's popup is open.

Game help's bar, Job, Connection, FPS and Distance start on, and Server time, Party and Signet start off. The connection's S and R come from the game's own readout, which
the bar covers and the "Hide Readout" switch stops the game drawing; they show "-" if HeadsUp
cannot find them in the game's memory. The latency is HeadsUp's own measure, from a packet sent to the server's next
reply, rounded to a frame.

After each zone change (and after loading) HeadsUp asks the server for the conquest standings, as opening Region Info
does, while the Zone widget is on, so the zone's owner and the influence in its region show without opening that menu.
It asks again each minute while the widget's popup is open, as the influence changes through the week. Whoever holds
the largest share when the tally comes takes the region. Your conquest points and Imperial Standing are among the Gil
widget's currencies.

The bar stays off until your character is in game, so it does not draw over the login screens.

A bar under 36 px is too short for two lines of text. There each widget's second line goes after its title, dimmed, as
in "27/30 Bags"; One Line can put it before the title instead, or leave it off.

The Top Bar page of the settings (`/hu menu topbar`) has three cards: Top Bar, Help Bar and Widgets.

The Top Bar card has the bar's switch, a theme, a separators choice, a title font and a text font (the theme's own by
default), a height from 24 to 64 px (45 by default), a text size, One Line, a popup size, and "Hide Readout", which
hides the game's connection readout. The Help Bar card has Game help's switch, "Hide Game Help", the help bar's
position and whether it has a background.

In the Widgets card, Left, Center and Right are trays side by side, each holding its widgets as chips in bar order,
with a lit dot on each one that is on. A chip drags to another place in its tray or into another tray, an empty one
too, and "Reset Layout" puts every widget back in its first place. A click on a chip shows that widget's options below
the trays: Enabled, its section, its second line and icons where it has them, and its own options (Target's details,
Gil's currency, the clocks' 24-hour and seconds switches and Server time's time zone, Job's EXP bar under or beside the
text or off and its job names, Connection's latency).

Separators picks diamonds or none between neighbouring widgets in any theme, and starts at the theme's own. "Text Size"
and "Popup Size" set those sizes in pixels or leave them on Auto, which follows the bar height; a bar size too large for
the height is drawn as large as fits, and the menu says what that is. Bar text runs from 9 to 32 px and popup text from
10 to 32 px. The themes are:

| Theme | Look |
|---|---|
| Phoenix Gold | Ornate: an edge line with diamonds, and a framed target. The default. |
| Umbra Dark | Each part in a dark rounded pill |
| Phoenix Red, Ornate | The ornate look in red |
| Phoenix Red, Pills | The pill look in red |

The game's help bar is the strip across the top that shows a menu's title and help text while a menu is open. HeadsUp's
bar covers its top, and the rest of it shows below the bar unless the bar is taller. The switch, off by default, stops
the game drawing it, and Game help's bar, while it is on, shows the same text under HeadsUp's bar. The game's connection readout (S, R and %) at the top right is under the bar
too: covered, or its lower part showing below a shorter bar. Its own switch, also off by default, stops the game drawing
it and leaves every other window in place.

The weakness, resistance, status and immunity badges appear only if a line of a mob's nameplate holds their tags. The
target popup shows the same data. On the Mobs page, the Detection, Badges, Weaknesses and Statuses cards set what those
tags show.
