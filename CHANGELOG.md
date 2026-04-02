# Changelog

## v1.3 (2026-04-01)

### New features
- Scanner teleport (Alt+Home): teleport directly to the object selected in the scanner. Useful for getting unstuck in dungeons or reaching hard-to-access quest objectives. Limitations: in interiors, only works within the same cell (no teleporting through loading doors). In exteriors, limited to 3000 units range.
- Improved mod manager compatibility: nvdaControllerClient.dll is now automatically installed to the game root folder via FOMOD installer — no more manual copying required.
- Improved compatibility with modded UIs: GFx value access is now protected with SEH exception handling to prevent crashes with heavily modded setups.
- MCM settings menu (requires SkyUI): configure the plugin directly in-game via Mod Configuration Menu. Open it from the pause menu under "Mod Configuration", then select "SkyrimNVDA". Three pages are available:
  - **General**: toggle stealth announcements (Hidden/Detected/Caution) and scanner teleportation on or off.
  - **Audio**: adjust the volume of each sound effect (aim, kill, dragon hit) with sliders from 0.0 to 2.0. Changes apply immediately — no restart needed.
  - **Controls**: rebind the scanner keys. Select a key, press Enter, then press the new key you want. Modifier keys (Shift, Alt) still work the same way with your new key. For example, if you change the Announce key from Home to F5, then F5 announces the object, Shift+F5 starts autowalk, and Alt+F5 teleports — same behavior, different key.

### Important note about scanner teleport
Use scanner teleport with caution. Teleporting past quest triggers, doors, or scripted events may break quest progression. It is meant as a last resort when you are stuck, not as a primary navigation method. If a quest seems broken after teleporting, try reloading a previous save.

## v1.2 (2026-03-30)

### SkyUI compatibility
SkyUI (https://www.nexusmods.com/skyrimspecialedition/mods/12604) replaces the vanilla menus with a new interface. For players who have used Fallout Access, the SkyUI inventory experience is similar: categories on the left, items on the right, with keyboard-driven navigation.

- Inventory, container, barter, magic, gift, and favorites menus now work with SkyUI installed
- SkyUI uses different category names and navigation — categories are on the left panel, use Left/Right arrows to switch between categories and items
- Alt key switches between your inventory and the vendor/container inventory (replaces the vanilla divider system)
- Favorites menu with SkyUI: category filters (All, Gear, Aid, Magic) are read when navigating
- Inventory sorting with keyboard (SkyUI only): press 1 to sort by equipped, 2 by name, 3 by weight, 4 by value — the sort type is announced, followed by the first item in the sorted list
- SkyUI MCM (Mod Configuration Menu): full accessibility — mod list, pages, all option types (toggle, slider, menu, keymap, text, color), value changes read on toggle/slider, key names displayed instead of scan codes
- SkyUI MCM: menu dropdown and slider dialog popups are read when navigating
- The plugin automatically detects whether SkyUI or vanilla UI is installed — no configuration needed

### Fixes
- Autowalk: fixed dynamic objects (dropped items, summoned NPCs) — autowalk now correctly walks to them instead of searching for a door
- Scanner: crosshair now accurately targets dropped items on the ground (uses real 3D mesh position instead of static bounds)
- Scanner: crosshair now accurately targets dead bodies (ragdolls) lying on the ground instead of aiming above them
- Scanner: quest objectives no longer disappear from the list when navigating with Page Up/Down
- Scanner: quest objectives now correctly filtered from game start — no need to open the journal first
- Map: quest markers for targets inside dungeons now point to the dungeon entrance instead of raw interior coordinates — distances and directions are now accurate
- Map/Scanner: quest distances are now consistent between the map and the scanner (both use 2D ground distance)
- Auto-aim: reduced dragon priority range from 30,000 to 5,000 units — dragons are only prioritized over closer enemies when they are nearby

### New features
- Auto-aim: replaced system beep with custom in-game sound that follows Skyrim's volume settings
- Kill sound: replaced system beep with custom in-game sound
- Dragon hit: replaced system beep with custom in-game sound
- NPC dialogue subtitles are no longer read by NVDA (NPCs already have voice acting)
- Translations: plugin now reads translation files directly from BSA archives — no need to extract Translate_FRENCH.txt manually
- Journal: Miscellaneous quests are now navigable individually — scroll to Misc, press Right arrow to enter the list, navigate with Up/Down, press Enter to activate/deactivate individual misc quests
- Map: press P on any map marker to place a custom marker — it appears in the scanner's Quests category with distance and direction, and you can autowalk toward it with Shift+Home
- Barter/Container/Inventory/Gift: quantity slider is now read when buying, selling, dropping, or giving stacked items — announces the quantity on open and reads each change when pressing Up/Down

### Sound volume settings
You can now adjust the volume of each custom sound effect by editing the file `Data/SKSE/Plugins/SkyrimNVDA.ini`. This file is installed alongside the plugin. Open it with any text editor and change the values:

```
[Sounds]
AimVolume=0.2
KillVolume=0.4
DragonHitVolume=1.0
```

- Values range from 0.0 (silent) to 1.0 (full volume)
- AimVolume: the continuous beep when aiming at an enemy with a bow
- KillVolume: the sound that plays when you kill an enemy
- DragonHitVolume: the sound that plays when your arrow hits a dragon
- Setting a value to 0.0 will completely disable that sound
- Changes require a game restart to take effect

## v1.1 (2026-03-26)

### New features
- Barter menu: full vocalization (item name, value, weight, damage, armor, categories, vendor/player side, descriptions)
- Barter menu: H key announces player gold, vendor gold, and carry weight
- HUD: vocalize item pickup messages (Gold added, Item added, etc.)
- HUD: vocalize quest objective updates (the specific objective text, not just "Quest updated")
- HUD: vocalize arrow type and count when equipping a bow
- HUD: vocalize stealth status changes (Hidden / Detected / Caution)
- Scanner: added Locations category (nearby discovered map markers with type)
- Scanner: Word Walls now appear in Activators category
- Scanner: filtered out invisible/technical objects (triggers, "should not be visible")
- Quest tracking: misc quests now supported
- Quest tracking: compass-based door finding for targets in other cells
- Quest tracking: active/inactive toggle reflected from journal
- Map: added reference point system (Shift+Home to measure distances between markers)
- Auto-aim: added line-of-sight check for beep (no beep if wall between you and target)
- Auto-aim: added gravity compensation using real projectile physics data
- Auto-aim: added movement prediction for moving targets
- Auto-aim: faster tracking rate (100ms) and continuous beep when target is in line of sight
- Auto-aim: faster kill sound
- Auto-aim: dragon priority for bow — when a hostile dragon is within range, it is targeted over closer non-dragon enemies (deer, foxes, etc.)
- Dragon combat: high-pitched beep when your arrow hits a dragon
- Dragon combat: automatic "Dragon in flight" / "Dragon landed" voice announcements during combat
- Kill sound: 3 descending beeps on any enemy kill (works with bow, melee, magic)

- Crafting: all stations accessible (forge, grindstone, workbench, tanning rack, smelter, enchanting table, alchemy lab)
- Crafting: recipe name with quantity produced (e.g. "Leather Strips (4)"), materials required, damage/armor stats
- Crafting: category navigation for forge and tanning rack, single list navigation for grindstone/workbench
- Inventory/Container/Barter: reordered item info (damage/armor first, then value, then weight)
- Loading screen: vocalize loading tips and hints text
- Sleep/Wait menu: vocalize question (rest/wait how long?), current time, and hours selected with slider
- Gift menu: full vocalization for companion item exchange (give/take gifts with item details)
- Book menu: reads book title, full content, and spell/skill learned when opening a book
- Training menu: vocalize skill name, trainer level, training count, cost, and gold on open and after each training session
- Auto-aim: uses ProcessLists instead of cell scan to find enemies — dragons in flight are now detected reliably
- Map: added Quest Targets filter — active quest objectives appear as markers with distance and direction
- Map: quest targets in interiors are redirected to the exit door for correct map positioning
- Enemy lock: Shift+X toggles permanent camera lock onto the nearest enemy (press Shift+X again to unlock)
- HUD: announce sneaking/standing when toggling crouch
- HUD: announce first person/third person when switching camera view
- Console: developer console (~) is now accessible — typed text and command results are read by NVDA
- Greybeards quest (MQ105): accessibility for the shout demonstration targets at High Hrothgar
- Map: fast travel now uses Enter key with Papyrus Game.FastTravel() — bypasses unreliable GFx marker selection
- Map: fast travel confirmation dialog (press Enter twice to travel)
- Inventory/Container/Barter/Gift: soul gems now announce their soul level (e.g. "Grand", "Common")

### Bug fixes
- HUD: fixed notifications (quicksave, quest updates) not being re-read when the same message appears again
- Inventory/Container/Barter/Gift: items with zero value no longer announce "value 0"
- HUD: fixed quest objective updates being read in an infinite loop when old and new objectives were displayed simultaneously
- Inventory: fixed weight/damage/armor not being filtered when value is zero (game sometimes returns "000")
- Enemy lock (X): filtered out disabled, deleted, and unloaded ghost actors
- Scanner Home: improved crosshair precision by aiming from eyes at object center (better object activation)
- Doors: fixed destination not showing for interior-to-exterior doors (now shows worldspace name)
- Quest distance: fixed Home key showing direct distance instead of door distance for cross-cell targets
- Map: fixed quest markers for completed/deactivated quests still appearing on the map
- Container/Inventory/Barter/Gift: fixed item not being re-read when returning to a subcategory with a single item
- Container: fixed "container"/"inventory" side prefix not updating correctly when navigating between sides (used wrong GFx index)
- Main menu: removed unnecessary "Main menu closed" announcement
- Auto-aim: fixed bow beep firing through walls and obstacles — now correctly silent when line of sight is blocked

## v1.0 (2026-03-19)

First public release.

- Full menu vocalization: inventory, container, magic, journal, skills, favorites, dialogue, HUD, main menu, level up, message box, character creation
- Object scanner with 10 categories (All, NPCs, Doors, Containers, Items, Activators, Corpses, Companions, Quests, Locations)
- Scanner subcategories (locked/unlocked doors, looted/unlooted corpses)
- Autowalk with AI pathfinding toward any scanner object or quest target
- Auto-aim for bows with automatic target tracking
- Enemy lock (X key) for melee combat orientation
- Accessible map with keyboard navigation, filters, and mouse hover reading
- Quest tracking with journal integration and compass-based navigation
- Puzzle support: pillar and dragon claw ring symbol reading
- Door destination detection (shows where cell doors lead)
- HUD vocalization: crosshair, notifications, subtitles, location names, tutorial hints
- Kill sound notification
- Player vitals (H key): Health/Magicka/Stamina in game, Gold/Weight in inventory
