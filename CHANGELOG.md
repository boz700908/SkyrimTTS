# Changelog

## v1.6 (unreleased)

### New features
- Scanner: containers and corpses the player has already opened are now announced with a `looted` flag, independently of whether they still contain items. Useful to avoid re-visiting a container or corpse you already checked. The flag persists across saves (stored in the SKSE cosave). When the game engine respawns a reference (e.g. dungeon chests refill after ~30 days), the `looted` flag is automatically cleared for that reference, so the scanner correctly announces the re-filled chest as unlooted. A safety fallback also clears any entry older than 30 in-game days in case the engine reset event was missed

## v1.5 (2026-04-19)

### Mod support
- **QuickLoot IE / QuickLoot RE support** — the small loot overlay that lets you pick items directly from a corpse or chest without opening the full container menu is now fully vocalized. Items are announced as you scroll through the list
- **RaceMenu mod support** — the character creation menu now automatically detects whether RaceMenu is installed and switches between RaceMenu-specific paths and vanilla paths. Falls back to vanilla gracefully if RaceMenu is not present
- **Extended Hotkey System (EHS) support** — when EHS is installed in place of the vanilla favorites menu, hotkey assignments via number keys 1-8 and Ctrl+F1-F12 are now announced. EHS stores assignments in its own co-save invisible to the standard menu system, so the plugin maintains its own internal mapping to track and announce shortcuts
- Translation loader: plugin can now load mod-specific translation files from `Interface\translations\<mod>_<LANG>.txt` (with fallback to `_english.txt` if the language isn't translated in your language)

### New features
- Container menu: follower carry weight — when trading items with a follower (companion), the H key now also announces the follower's current carry weight and capacity (e.g. "Lydia: 150 of 300")
- **Autowalk: smarter quest routing in dungeons** — the plugin now uses Skyrim's location data to decide which door to take when following a quest objective, instead of just picking the door whose angle matches the compass marker. Concretely:
  - From outside, autowalk on a quest objective inside a dungeon now reliably picks the official main entrance (the one tagged as the dungeon entrance in the game data), never the back exit even when both lead to the outside world
  - Inside a multi-room dungeon, autowalk picks the door that actually leads toward the objective's room (or its sub-area), not a random door pointing in the right direction through walls
  - When the objective is in another worldspace (e.g. carrying a quest item back to a city), autowalk picks doors that exit the current building toward the outside, not doors that go deeper into adjacent rooms (jail, jarl's quarters, etc.)
  - Falls back to the original compass-angle search if the location data is unavailable (modded dungeons that don't tag their entrance, etc.), so no regression for existing setups
- Scanner: destructible objects detection — spider webs (blocking or egg sacs), wooden barricades, Eldergleam roots and breakable doors are now announced with a "destructible" flag and their current health percentage. They appear in the Activators category with a new sub-filter so you can list them quickly. Useful for the Bleak Falls Barrow spider web and similar obstacles that block progression until destroyed
- Scanner: "Object gone" detection — when you press Home to announce the currently selected object and that object has been picked up, destroyed or despawned since the last scan, the scanner now announces "Object gone" and automatically jumps to the next object in the list
- Autowalk: more reliable targeting on dropped items — when autowalking to an item you dropped from your inventory, the plugin now uses the visible mesh position instead of the physics body position. The physics body can drift far from the visible item after bouncing, so autowalk was sometimes stopping away from the real object
- Map: map filter and fast travel now use the real discovery state — the "Discovered" filter used to also show locations that just had their icon pre-placed on the map (Solitude, Whiterun, Imperial and Stormcloak camps, Fort Dawnguard before their quests enable them). The filter now only lists places you have physically visited, which matches the fast travel rules of the vanilla game. You still see all those places in "Undiscovered" and "All" until you reach them
- Map: quest targets on the map — quest objectives now appear as navigable markers in the map menu, with the same position logic as the in-game scanner (fallback to the quest alias when condition checks fail). Only one marker per objective, no duplicates
- Map: location type sub-filter now also cycles with Alt+Page Down / Alt+Page Up — in addition to the existing Alt+End shortcut. Easier to reach on most keyboards
- Teleport: the scanner teleport (Alt+Home) now allows reaching quest targets up to the configured MCM range (3000 units by default) when the target is in your current cell. The previous hard 1000 unit limit is now only kept for cross-cell quest targets, where teleporting to raw coordinates from another cell could send you into the void

### Bug fixes
- Camera POV announce on F was reversed — pressing F to switch first/third person used to announce the previous view instead of the new one. The plugin now toggles the camera itself (`ForceFirstPerson` / `ForceThirdPerson`) and consumes the key event so the engine does not toggle a second time, which fully eliminates the race that left the announce inverted in some cases
- Gamepad LB / L1 modifier could stay "stuck" after forced dialogue or teleport — when an NPC forced a dialogue while the LB/L1 modifier button was held, or when the player teleported onto an activator (forge, enchanter), the "LB released" event never reached the plugin. The `LB held` state remained set forever, controls stayed masked, and every subsequent button was treated as a LB combo until the player force-quit the game. Two safety nets now prevent this: (1) the plugin resets the LB state immediately when a dialogue, message box, loading screen, fader, console, sleep/wait, tutorial or book menu opens while LB is considered held; (2) a background watcher polls the real controller state every 500 ms via XInput and force-resets if LB has not been physically pressed for two seconds while the plugin still thinks it is held
- Scanner: empty container / looted corpse filter was unreliable — the in-game `GetInventoryCount()` returns "phantom" items even after the player has fully looted a container (e.g. a noble's wardrobe with default clothes still counted as 8 items after taking everything). The plugin now reads the real inventory state via the per-item `countDelta` from the container's change record, with a fallback to the base form's content for untouched containers. The "Empty" sub-filter now correctly lists looted containers and corpses, and the empty status updates properly when scrolling through the list with Page Up / Page Down (not only on first category open)
- **Autowalk crash fix** — fixed a race condition where the autowalk monitor thread could read player or target game data at the same moment the engine was writing it, causing random null-pointer crashes. All game object reads now run on the main thread. Multiple users had reported this crash during long outdoor travels
- Autowalk crash recovery after crossing a cell-change door — fixed secondary race conditions that could corrupt the save after an autowalk crash: autowalk state was persisting across reloads (AI-driven flag stuck on true, invalid travel target), causing the reloaded save to crash again. The plugin now resets the autowalk state three seconds after any save load, stops autowalk when the player dies, and pauses the autowalk monitor whenever the game is paused (menus, loading screens, console) so it can no longer fight the engine during unstable moments
- Autowalk: better recovery when stuck — instead of a single hard reset after 4 seconds, the plugin now tries a gentle recovery first (re-apply movement flags and re-evaluate the AI package), then a full AI-driven toggle at 6 seconds, and only gives up at 10 seconds. This fixes cases where the player was blocked on small obstacles and the old single recovery was either too aggressive (teleporting the camera) or not enough
- Map sub-filter shortcut restored — location type sub-filter (All types → Cities → Towns → Dungeons → Forts → Camps) is cycled with Alt+End again (was briefly broken on laptops after a change that used Home+Arrow, which conflicts with Home being Fn+Arrow on laptop keyboards)
- Crafting menu: opening announce is no longer cut off by the first item readout
- Stats menu: fixed a rare race where pressing the stats shortcut right as the menu was opening could leave the reader stuck
- HUD: safer reading of on-screen messages — prevents a potential crash on heavily modded UIs
- MCM and Journal: removed debug log spam that filled the log file with one line every 80 ms during normal navigation

### Performance
- Speech: the screen reader calls no longer block the keyboard input thread — moved to a dedicated worker thread so navigation stays responsive even when NVDA is busy announcing a long sentence
- Scanner: the outdoor scan (5x5 cells around the player) is significantly faster — script type lookups for activators (word walls, puzzle pillars, etc.) are now cached per object, so each activator is only queried once per game session instead of on every scan. Reduces the small lag some players felt when the scanner refreshed in large exterior areas

### Diagnostics
- Autowalk pre-dispatch state logging — the plugin now logs a full snapshot of the player and target state right before triggering an autowalk call (3D model, character controller, parent cell, position, running AI package, target resolution, etc.). Helps diagnose the rare engine-level crash some players still hit. If you crash during autowalk, please send your `SkyrimNVDA.log` — the pre-dispatch lines will show which field was null or missing

## v1.4 (2026-04-07)

### New features
- MCM: new "Bow auto aim" toggle in the General page — disable bow auto-aim entirely if you want to use vanilla bow combat without the assistance system
- MCM: new "Gamepad" page to remap controller button assignments — for each action (next object, previous object, announce, autowalk/fast travel, teleport, vitals, sneak toggle, POV toggle, lock enemy, set reference on map), pick which button to use from a dropdown of all controller buttons (D-pad, A/B/X/Y, LS/RS click, RB, Start, Back). LB stays fixed as the modifier key.
- Gamepad support: full scanner, autowalk and map control with an Xbox/PlayStation controller using LB as a modifier key
  - **Scanner (in game)**
    - LB + D-pad Down/Up: next/previous scanned object (down = farther, up = closer)
    - LB + D-pad Left: announce current target (Home equivalent)
    - LB + Right stick Left/Right: change category (All, NPCs, Doors, etc.)
    - LB + Right stick Up/Down: change sub-filter
    - LB + A: start/stop autowalk
    - LB + B: teleport to scanned target
    - LB + Y: contextual stats — in game announces health, magicka, stamina; in inventory, container or barter menu announces gold and carry weight (same behavior as H key on keyboard)
    - LB + LS click: toggle sneak (crouch/stand)
    - LB + RS click: toggle first/third person camera
    - RS click alone: lock nearest enemy
  - **Map menu**
    - LB + D-pad Down/Up: next/previous marker in current filter (sorted by distance)
    - LB + D-pad Left: announce marker details
    - LB + D-pad Right: set reference point for distance calculation
    - LB + A: fast travel to selected marker (custom system, more reliable than vanilla)
    - LB + Y: place or remove a custom marker on the currently selected map marker (same as P key on keyboard) — the marker becomes a navigable target in the in-game scanner
    - LB + Right stick Left/Right: cycle location sub-filter (All types → Cities → Towns → Dungeons → Forts → Camps)
    - LB + Right stick Up/Down: cycle main filter (All → Discovered → Undiscovered → Quest Targets)
  - **Automatic remapping**
    - Sprint: remapped from LB to LS click (no manual config needed)
    - Sneak: remapped from LS click to LB+LS (no manual config needed)
    - Map context: A, Y, D-pad Left and any button you assign to scanner combos are disabled in the map context to avoid conflicts with our combos
  - Left stick movement cancels autowalk (same as WASD on keyboard)
- Autowalk: mounted autowalk — you can now launch autowalk while riding a horse and the horse itself will path to the destination instead of forcing you to dismount. Mount your horse manually first, then trigger autowalk like usual (Shift+Home on keyboard or LB+A on gamepad) — the plugin detects that you are riding and transparently switches to mounted mode. The horse paths on the navmesh at normal riding speed with full collision and obstacle handling, which is more reliable than walking on foot for long outdoor trips.
  - **Stuck detection is more patient on horseback** — horses move in bursts while recalculating waypoints, so the normal 3/6/10 second recovery cycle (jump, repath, give up) is replaced by a simple 30 second timeout that only triggers if you make no real progress toward the target
  - **Known limitation on horse controls after stopping** — after a mounted autowalk stops (arrival, user cancel, stuck timeout), the player stays on the horse but the keyboard controls (WASD) for the horse are temporarily lost. Dismount and remount manually (E twice) to regain control of your horse. This is a Skyrim engine quirk that affects every mod using the mounted travel pattern (SkyTrek SE has the same bug); a full night of investigation could not work around it without breaking the travel start itself
- Autowalk: player now automatically faces the target on arrival — the crosshair points directly at the door or object, ready for activation
- Autowalk: better stuck recovery — when blocked, the plugin now simulates a real Space jump (preserves forward velocity, like pressing Space while running) instead of a standing jump
- Autowalk: detailed stuck diagnostic — when stuck for more than 4 seconds, the log now reports player position, target position, AI package state, desired vs current movement speed, character controller state and current cell, to help diagnose why navigation is failing
- Scanner: improved quest target detection — when all quest target conditions fail (e.g. Bleak Falls Barrow's Dragonstone, which is not yet placed in the world during your first visit), the scanner now falls back to resolving the quest alias without checking conditions, so the marker still appears with a usable position. The historical behavior for working quests (multiple waypoints in dungeons like Helgen) is preserved exactly.

### Bug fixes
- Scanner: fixed quest distance inconsistency when pressing Home — `RefreshQuestTarget` now uses the same logic as the main scan (worldLocMarker resolution for cross-cell targets, 2D distance), so the distance announced on Home matches the one shown when navigating with Page Up/Down. Previously the Home key could announce 47000 units instead of the real ~5700 because it used raw interior coordinates.
- Map: location type sub-filter (Home+Arrow Down/Up) — cycle through All types, Cities, Towns, Dungeons, Forts, Camps to narrow down markers within the current filter
- Map: markers are now filtered by current worldspace — Solstheim markers no longer appear when you are in Skyrim and vice versa
- Autowalk: fixed unnatural movement speed — autowalk now runs at normal speed instead of 2.5x
- Autowalk: fixed player stuck in slow walk after autowalk arrival — speed is now properly restored in all stop scenarios (arrival, cancellation, stuck detection)
- Autowalk: no longer stops automatically when entering combat — the player decides when to stop
- Autowalk: starting autowalk now automatically disables aim lock (X) and toggle lock-on (Shift+X) to prevent camera conflicts
- Autowalk: when targeting a quest objective whose reference is not yet spawned in the world (e.g. Dragonstone before defeating the Draugr Overlord), the autowalk now uses the position resolved by the scanner instead of incorrectly trying to find a dungeon entrance via the compass
- Crash fix: protected all GFx UI access with SEH exception handling — fixes crash on startup with heavily modded UIs (e.g. Journals of Jyggalag modlist)
- Crash fix: protected the engine translation table lookup with SEH — prevents random crashes in the main menu when the scrap heap recycles memory while we read translations
- Scanner: quest markers now follow the correct waypoint in dungeons — quests like "Escape Helgen" have multiple invisible waypoints that guide you through corridors and rooms. Previously, the scanner always pointed to the first waypoint (near the entrance). Now it follows the same waypoint as the compass, updating as you progress through the dungeon
- Scanner: pressing Home on a quest target now refreshes the waypoint in real-time — if the quest stage changes (e.g. you pass a trigger in a dungeon), the marker updates immediately without needing to rescan
- Scanner: distances are now recalculated in real-time when navigating with Page Up/Down — previously distances were only updated when pressing Home
- Scanner: fixed inconsistent distances between Home and Page Up/Down — Home was using the 3D mesh center (much higher for tall objects like standing stones) while Page Up/Down used the base position, causing large discrepancies

## v1.3.1 (2026-04-03)

### New features
- Map: location type sub-filter (Home+Arrow Down/Up) — cycle through All types, Cities, Towns, Dungeons, Forts, Camps to narrow down markers within the current filter
- Map: markers are now filtered by current worldspace — Solstheim markers no longer appear when you are in Skyrim and vice versa

### Bug fixes
- Autowalk: fixed unnatural movement speed — autowalk now runs at normal speed instead of 2.5x
- Autowalk: fixed player stuck in slow walk after autowalk arrival — speed is now properly restored in all stop scenarios (arrival, cancellation, stuck detection)
- Autowalk: no longer stops automatically when entering combat — the player decides when to stop
- Autowalk: starting autowalk now automatically disables aim lock (X) and toggle lock-on (Shift+X) to prevent camera conflicts
- Crash fix: protected all GFx UI access with SEH exception handling — fixes crash on startup with heavily modded UIs (e.g. Journals of Jyggalag modlist)
- Scanner: quest markers now follow the correct waypoint in dungeons — quests like "Escape Helgen" have multiple invisible waypoints that guide you through corridors and rooms. Previously, the scanner always pointed to the first waypoint (near the entrance). Now it follows the same waypoint as the compass, updating as you progress through the dungeon
- Scanner: pressing Home on a quest target now refreshes the waypoint in real-time — if the quest stage changes (e.g. you pass a trigger in a dungeon), the marker updates immediately without needing to rescan
- Scanner: distances are now recalculated in real-time when navigating with Page Up/Down — previously distances were only updated when pressing Home
- Scanner: fixed inconsistent distances between Home and Page Up/Down — Home was using the 3D mesh center (much higher for tall objects like standing stones) while Page Up/Down used the base position, causing large discrepancies

## v1.3 (2026-04-02)

### New features
- Scanner teleport (Alt+Home): teleport directly to the object selected in the scanner. Useful for getting unstuck in dungeons or reaching hard-to-access quest objectives. Limitations: in interiors, only works within the same cell (no teleporting through loading doors). Teleport range is configurable via MCM (default 3000 units). Quest targets have a stricter limit of 1000 units to avoid breaking quest progression.
- Improved Vortex compatibility: nvdaControllerClient.dll is now automatically installed to the game root folder via FOMOD installer — no more manual copying required for Vortex users.
- Improved compatibility with modded UIs: GFx value access is now protected with SEH exception handling to prevent crashes with heavily modded setups.
- MCM settings menu (requires SkyUI): configure the plugin directly in-game via Mod Configuration Menu. Open it from the pause menu under "Mod Configuration", then select "SkyrimNVDA". Three pages are available:
  - **General**: toggle stealth announcements (Hidden/Detected/Caution) and scanner teleportation on or off. Adjust scan range and teleport range with sliders. Reset all settings to defaults.
  - **Audio**: adjust the volume of each sound effect (aim, kill, dragon hit) with sliders from 0.0 to 2.0. Changes apply immediately — no restart needed.
  - **Controls**: rebind the scanner keys (including teleport key). Select a key, press Enter, then press the new key you want. Modifier keys (Shift, Alt) still work the same way with your new key. For example, if you change the Announce key from Home to F5, then F5 announces the object, Shift+F5 starts autowalk, and Alt+F5 teleports — same behavior, different key.
- Auto-aim: exact ballistic trajectory calculation — the arrow now follows the mathematically perfect parabolic arc, accounting for projectile speed, gravity, distance, and height difference. Much more accurate at long range.
- Auto-aim: arrow range detection — the aim sound now stops when the target is out of your arrow's effective range. The range is calculated from real projectile physics data (speed, gravity, drop distance). At lock-on, NVDA announces "out of range" if the target is too far, or "obstructed" if there is an obstacle between you and the target.
- Help menu: the Help pages in the System tab now read their full content, including key/button names (e.g. "Press Mouse1 to attack").

### Bug fixes
- Map: fixed quest markers appearing twice when a quest has multiple targets pointing to the same location
- Map: fixed quest marker positions for targets inside interiors — distances are now accurate instead of showing ~50,000 units due to interior coordinates being used on the world map

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
