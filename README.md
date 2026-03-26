# SkyrimNVDA — Player Guide

Accessibility plugin for Skyrim Special/Anniversary Edition. Automatically vocalizes game menus, adds an object scanner, autowalk, auto-aim, and quest tracking via NVDA.

---

## Requirements

- Skyrim Special Edition or Anniversary Edition
- SKSE64 (Skyrim Script Extender)
- NVDA screen reader (must be running before launching the game)

---

## Installation

Your Skyrim installation folder is usually located at:
C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\

### Step 1: Install SKSE64

If you haven't already, download and install SKSE64 from https://skse.silverlock.org/. Follow its installation instructions. You must launch the game through skse64_loader.exe instead of the normal Skyrim launcher.

### Step 2: Copy the plugin files

From the release archive, copy the following files to the correct locations inside your Skyrim installation folder:

- SkyrimNVDA.dll: copy to Data\SKSE\Plugins\SkyrimNVDA.dll
- nvdaControllerClient.dll: copy to Data\SKSE\Plugins\nvdaControllerClient.dll
- SkyrimTTS_AutoWalk.esp: copy to Data\SkyrimTTS_AutoWalk.esp
- SkyrimTTS_AutoWalk.pex: copy to Data\Scripts\SkyrimTTS_AutoWalk.pex

If the folders SKSE\Plugins\ or Scripts\ do not exist inside Data\, create them.

### Step 3: Activate the ESP

The file SkyrimTTS_AutoWalk.esp must be activated in your load order. You can do this in two ways:

Using a mod manager (Vortex, Mod Organizer 2): the ESP should appear in your plugin list. Make sure it is enabled (checked).

Manually: open the file Data\plugins.txt (or %LOCALAPPDATA%\Skyrim Special Edition\plugins.txt) and add the line:
*SkyrimTTS_AutoWalk.esp

### Step 4: Launch the game

1. Start NVDA
2. Launch the game through skse64_loader.exe (not the normal Skyrim launcher)
3. NVDA should start reading menus automatically

---

## First Steps — Walkthrough Guide

New to Skyrim? Read our detailed **[First Steps Guide](GUIDE.md)** — a complete walkthrough from character creation through the Golden Claw quest, with every step explained using accessibility features.

---

## Getting Started — Character Creation

When you start the game, after the intro cinematic, you arrive at the character creation menu.

NVDA announces: "Character creation"

- Ctrl + Left / Right: switch tab (Race, Body, Head, and sub-categories)
- Up / Down: navigate options within a tab
- Left / Right: adjust a slider value or change sex (Male/Female)
- R: confirm / validate

---

## Tween Menu (Tab key)

NVDA announces: "Cross menu"

- Up: Skills
- Down: Map
- Left: Magic
- Right: Inventory

---

## Inventory

NVDA announces: "Inventory open"

- Up / Down: change item (name, value, weight vocalized)
- Left / Right (or Q / E): change category
- H: announces gold and current carry weight / maximum

---

## Container (chest, body...)

NVDA announces: "Container open"

- Up / Down: change item
- Left / Right: switch between your inventory and the container
- H: announces gold and carry weight

---

## Magic Menu

NVDA announces: "Magic menu open"

- Up / Down: change spell (name, effects, cost vocalized)
- Left / Right: change category (Destruction, Restoration...)

---

## Journal (J key)

NVDA announces: "Journal open"

- Up / Down: change quest or entry
- Enter: activate or deactivate the selected quest (NVDA announces "active" or "inactive")
- Ctrl + Left / Right: switch tab (Quests, System, etc.)

When you activate or deactivate a quest in the journal, it is reflected in the scanner's Quests category.

---

## Skills Menu

- Up / Down / Left / Right: navigate the skill tree
- The selected skill description is vocalized automatically
- Perks are vocalized with their description and requirements

---

## Favorites (Q key)

NVDA announces: "Favorites"

- Up / Down: change item or spell

---

## Dialogue

The NPC's dialogue line is vocalized automatically.
Up / Down to choose your response.

---

## HUD (in game)

- Object, NPC, or door in crosshair: name and action vocalized automatically (e.g. "Open door")
- Notifications (quest updates, level up, skill increases): vocalized automatically
- Quest objective updates: the specific objective text is read (e.g., "Find the Golden Claw"), not just "Quest updated"
- Item pickup messages: "Gold added", "Item added", etc.
- Subtitles: vocalized automatically
- New location discovered: vocalized automatically
- Stealth status: announces Hidden / Detected / Caution when sneaking
- Crouch toggle: announces "Sneaking" / "Standing"
- Camera view: announces "First person" / "Third person" when pressing F
- Arrow info: arrow type and count announced when equipping a bow
- H: announces current Health / Magicka / Stamina
- Tutorial hints: beginning-of-game hints vocalized with key names

---

## Main Menu

NVDA announces: "Main menu open"

Up / Down to navigate New Game, Continue, Load, Settings, Quit.

---

## Level Up

NVDA announces: "Level gained! Choose your improvement."

Left / Right to choose between Health, Magicka or Stamina. Enter to confirm.

---

## Message Box

Game messages (confirmations, warnings) are vocalized automatically.
Up / Down to navigate buttons, Enter to confirm.

---

## Barter Menu (Merchant)

NVDA announces: "Barter menu open"

- Up / Down: change item (name, value, weight, damage, armor, description vocalized)
- Left / Right: change category or switch between vendor/player side
- H: announces player gold, vendor gold, and carry weight

---

## Gift Menu (Companion Exchange)

Full vocalization for giving/taking items with companions — same controls as container.

---

## Crafting Stations

All crafting stations are fully accessible:
- **Forge** and **Tanning Rack**: category navigation with Ctrl + Left/Right, items with Up/Down
- **Grindstone** and **Workbench**: simple list with Up/Down
- **Smelter**, **Enchanting Table**, **Alchemy Lab**: full vocalization

Recipe name, quantity produced, required materials, and damage/armor stats are all vocalized.

---

## Book Menu

When opening a book, NVDA reads: the title, the full content, and any spell or skill learned.

---

## Training Menu

When talking to a trainer, NVDA reads: skill name, trainer level, training count, cost per session, and your current gold. Updated after each training session.

---

## Sleep / Wait Menu

NVDA reads: the question (rest or wait?), current time, and hours selected as you adjust the slider.

---

## Loading Screen

Loading tips and hints are vocalized automatically during loading screens.

---

## Developer Console (~)

The developer console is now accessible. Typed text and command results are read by NVDA. Useful for advanced commands like `setstage`.

---

## Soul Gems

Soul gems in inventory/container/barter display their soul level (e.g., "Grand", "Common"). Empty gems show no soul level.

---

## Object Scanner

The scanner lets you detect and navigate all objects around you: NPCs, doors, items, containers, quest targets, locations, and more.

### Scanner keyboard shortcuts (outside menus)

- Numpad 5: scan all objects around you
- Page Down: next object in the current category
- Page Up: previous object in the current category
- Shift + Page Down: next category
- Shift + Page Up: previous category
- Home: announce current object with distance and orient your camera toward it
- End: cycle subcategories (e.g. locked/unlocked doors, looted/unlooted corpses)

### Scanner categories

- All: every detected object
- NPCs: living characters (friendly and hostile)
- Doors: all doors, with destination name for cell doors
- Containers: chests, barrels, etc. (shows "empty" if looted)
- Items: weapons, potions, books, gold, etc.
- Activators: levers, buttons, beds, ore veins, etc. (puzzle pillars show their current symbol)
- Corpses: dead bodies
- Companions: your followers
- Quests: active quest objectives with distance to the target (follows compass direction for targets in other cells)
- Locations: nearby discovered map markers with their type (City, Cave, Fort, Nordic Ruins...)

### Puzzle support

When scanning puzzle pillars or dragon claw door rings, the scanner shows the current symbol in parentheses (e.g. "Pillar (Snake)", "Inner ring (Bear)"). For pillars, the direction (north, south...) is also shown to help identify which pillar is which.

When you activate a pillar or ring, NVDA automatically announces the new symbol.

---

## Autowalk

Autowalk lets you walk automatically toward a selected scanner object or quest target.

### Autowalk keyboard shortcuts

- Shift + Home: start autowalk toward the selected scanner object
- Shift + Home again: stop autowalk
- W / A / S / D / Escape / Space: stop autowalk immediately

### How it works

- The player automatically runs toward the target using the game's AI pathfinding system
- For quest objectives in another cell (e.g. inside a dungeon), the autowalk follows the compass to find the correct entrance door
- If the player gets stuck for 20 seconds, autowalk stops with "Can't reach target"
- Autowalk stops automatically when you arrive within 200 units of the target

---

## Enemy Lock (X key)

Press X to rotate your camera toward the nearest hostile enemy. NVDA announces the enemy name and distance. Press **Shift + X** to toggle permanent camera lock onto the enemy. Press **Shift + X** again to unlock.

- Works only during combat
- Ignores dead enemies, companions, and disabled actors
- Targets the center of the enemy's body for accurate melee fighting

---

## Auto-Aim (Bow)

When you draw your bow (hold the attack button), the plugin automatically:

1. Finds the nearest hostile enemy
2. Locks onto it and aims at the center of its body
3. Compensates for arrow gravity based on distance (uses real projectile physics data from the game)
4. Predicts enemy movement to aim where the target will be when the arrow arrives
5. Plays a beep sound (1000 Hz) when you have line of sight to the target
6. Re-aims every 500ms to track moving targets

When you release the bow, tracking stops. When any enemy is killed by the player (bow, melee, magic), a kill sound plays (3 descending beeps).

### Dragon combat

- Auto-aim prioritizes hostile dragons over closer non-dragon enemies (deer, foxes, etc.)
- High-pitched beep when your arrow hits a dragon
- Automatic voice announcements: "Dragon in flight" / "Dragon landed" during dragon combat

---

## Map Menu (M key)

The map is fully accessible with keyboard navigation.

### Map keyboard shortcuts

- Page Down: next map marker
- Page Up: previous map marker
- Home: announce full details of current marker (type, distance, direction, fast travel availability)
- Shift + Home: set current marker as reference point (all distances recalculated from this marker instead of the player). Press again to clear the reference
- End: cycle filters (All, Discovered, Undiscovered, Quest Targets)
- Enter (twice): fast travel to selected marker (first press asks for confirmation, second confirms)

### Map features

- All map markers are listed with name, type (City, Cave, Fort...), distance and direction (north, south, east...)
- Mouse hover: when you move the mouse over a marker on the map, NVDA reads its name
- Filters let you see only discovered or undiscovered locations, or quest targets only
- Quest targets filter shows active quest objectives as map markers
- Fast travel via Enter key — no need to click on the map visually
- Reference point: set any marker as reference to measure distances between locations

---

## Quest Tracking

Quest objectives appear in the scanner's Quests category. Only quests activated in your journal are shown.

### How it works

- Activate or deactivate quests in the journal (J key, then Enter on a quest)
- Active quests appear in the Quests category of the scanner with distance and direction
- For targets in another cell (dungeon, building), the scanner follows the compass direction and shows the distance to the door you need to take
- Use Home to orient toward the quest target
- Use Shift + Home to autowalk toward the quest target

### Word Walls

Word Walls (where you learn dragon shouts) appear in the Activators category of the scanner. Walk near them to learn the word automatically.

---

## Recommended mods for accessibility

- [Puzzle Pillar Auto-Solve](https://www.nexusmods.com/skyrimspecialedition/mods/125875) — All pillars are pre-solved, just pull the lever
- [Dragon Claws Auto-Unlock](https://www.nexusmods.com/skyrimspecialedition/mods/47329) — Claw doors open automatically when you have the claw

---

## Keyboard shortcuts summary

### In game (no menu open)

- H: Health / Magicka / Stamina
- Numpad 5: scan objects
- Page Down: next object
- Page Up: previous object
- Shift + Page Down: next category
- Shift + Page Up: previous category
- Home: announce current object and orient camera
- Shift + Home: start / stop autowalk
- End: cycle subcategories
- X: lock nearest enemy (combat only)
- Draw bow: auto-aim activates automatically

### In inventory / container

- H: gold and carry weight
- Up / Down: change item
- Left / Right: change category or switch inventory/container

### In map (M key)

- Page Down: next marker
- Page Up: previous marker
- Home: marker details
- Shift + Home: set / clear reference point
- End: cycle filters (All, Discovered, Undiscovered, Quest Targets)
- Enter (twice): fast travel

### In journal (J key)

- Up / Down: change quest
- Enter: activate / deactivate quest

### Character creation

- Ctrl + Left / Right: switch tab
- Up / Down: navigate options
- Left / Right: adjust slider / change sex
- R: confirm

---

*Plugin developed by Pyrhame. Requires NVDA.*
