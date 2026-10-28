# HeadsUp

An Ashita v4 plugin for Final Fantasy XI on PhoenixXI. It outlines monsters by whether they will attack you, replaces
the game's nameplates with clearer ones, and adds a panel of widgets across the top of the screen.

![Goblins, a sheep, a crab and an NM in Valkurm Dunes, each with HeadsUp's nameplate, and the top panel above](resources/headsup.jpg)

## Install

Download `HeadsUp.dll` from the [Releases](https://github.com/Tanyrus/headsup/releases) page, put it in Ashita's
`plugins` folder (in place of any old `headsup.dll`) and run `/load HeadsUp`. `/hu` opens the settings.

## Outlines and nameplates

<p>
  <img src="resources/nameplate_and_outline.png" alt="A Goblin Leecher's red outline and nameplate" height="300">
  <img src="resources/player_nameplate.png" alt="Your own nameplate with its icons and the feather cursor" height="300">
</p>

| Color | The mob |
|---|---|
| Red | Will attack you |
| Green | Will not: passive, or too weak for you |
| Orange | A notorious monster that will attack you |
| Gold | A notorious monster that will not |
| Purple | A lottery placeholder |
| Gray | Not in HeadsUp's data |

Outlines and text can also be colored by toughness, claim, targeting or entity type. Each nameplate has three lines you
design by dragging tags into them:

| Tag | Shows |
|---|---|
| Name, Level, Difficulty | The name, the level or level range, and the con |
| Detection | Sight, sound, scent, magic, low HP, and whether it links |
| ID, Server ID, Placeholder | The ID players name placeholders by, and [PH] on a placeholder |
| Weaknesses, Resistances, Statuses, Immunities | Badges with how much more or less often your magic lands |
| Job, Job & Level, Title | A player's jobs, level and title |
| GM, Mentor, New Adventurer, Level Sync, Away, Linkshell, Bazaar, Seeking Party | A player's icons |
| Owner | A pet's owner |

| Target cursor | Looks like |
|---|---|
| Vanilla | The game's own |
| Triangle | A plain triangle |
| Phoenix Feather | Phoenix's feather |
| Chocobo | PlayOnline's chocobo pointer |
| Your own | Any `.cur` or `.ani` file in `config/headsup/cursors` |

## Top panel

![The top panel with the Weather widget's popup](resources/top_panel.png)

Hover a widget for its popup, and click to keep it open.

| Widget | Shows | Popup |
|---|---|---|
| Zone | The zone, its region and who holds it | Each nation's influence there |
| Weather | The weather and today's chance of it | A five-day forecast, and which zones can roll each element |
| Vana'diel | The day, the time and the moon | The date and the next day and moon |
| Job | Your jobs, levels and EXP | EXP per hour and the time to level |
| EXP | Your EXP bar | EXP per hour and the time to level |
| Party | Your party's lowest HP and MP | Each member's HP, MP and TP |
| Signet | Signet or Sanction's time left | What it gives |
| Distance | Your target's distance | |
| Conquest | Regions each nation holds | Which regions, and where you can run supplies |
| World Info | The next ship, weather, guild hours, RSE or moon | Ships, the calendar, guild hours and today's guild turn-ins |
| Target | Your target's name, level and con | A mob's stats, weaknesses, spells, drops and spawn; an NPC's shop, trades and quests |
| Gil | Your gil or a currency | Every currency |
| Bags | Your inventory space | Every bag |
| Connection | Bytes sent and received, and your ping | The last server data and your PlayOnline messages |
| Memory | The game's memory left | The frames and LinuxXI's figures |
| FPS | Frames per second | The slowest recent frame |
| Clock | Your time, or a time zone you pick | The daily reset and the conquest tally |

| Theme | Look |
|---|---|
| Phoenix Gold | Ornate, with diamonds and a framed target. The default |
| Umbra Dark | Dark rounded pills |
| Phoenix Red | The ornate or pill look in red |
| Aurora | Slate glass, lit from cyan into violet |

## Settings

![The settings window's Nameplate Design card](resources/settings.png)

| Page | Holds |
|---|---|
| Entities | Nameplates, outlines, the target cursor and the color palettes, with a page for Mobs, Players, Self, NPCs and Pets |
| User Interface | Screenshot mode and moving the game's windows, with the Top Panel, Widgets and Mouse Cursor pages |
| HeadsUp Config | Turning HeadsUp off, a report for bug reports, and the window's own look |

`/hu menu <page>` opens a page, such as `/hu menu mobs`. `/hu screenshot` hides everything for a picture, and
Ashita's `/screenshot hide` does it for you.

## What it sends

HeadsUp reads what the game already shows and the server already sends. It never targets anything. It only sends:

| Request | When |
|---|---|
| Region Info's conquest request | After each zone change, while the Zone widget is on |
| The Currencies menu's two requests | After loading and each zone, while the Gil widget is on |
| A `/check` of nearby monsters | Only with Auto-Check on (off by default). Never a player |
| A ping to the server | Once a second, only while the Connection widget shows your ping |
| Today's guild turn-ins from api.phoenix-xi.com | Once a day, while the World Info widget is on |

Its monster, zone and weather data is built from Phoenix's own server code, so anything changed on the live server
since that build can show wrong.

## Development

HeadsUp is developed with the assistance of AI.
