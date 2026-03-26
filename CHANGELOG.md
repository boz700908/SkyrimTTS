# Changelog

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
- Map: camera now moves to the selected marker's world position when navigating
- Map: quest targets in interiors are redirected to the exit door for correct map positioning
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
