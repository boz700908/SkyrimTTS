# AutoWalk System — Specification for Skyrim

Based on the Fallout 4 Access implementation. This document describes how to reproduce the autowalk system for Skyrim SE/AE.

---

## Overview

The autowalk system has two main parts:
1. **Object Scanner** (C++ SKSE plugin) — Scans nearby objects, lets the user browse them by category, announces name + distance
2. **AutoWalk** (Papyrus + Creation Kit) — Moves the player automatically toward a selected target using the game's built-in pathfinding (navmesh)

The C++ plugin handles scanning, user input, and announcements. The Creation Kit provides the quest structure that enables AI-driven player movement. Papyrus scripts bridge the two.

---

## Part 1: Object Scanner (C++ side)

### Categories

The scanner organizes nearby objects into categories. The user cycles through them.

| Category | What it scans | Skyrim equivalent |
|----------|--------------|-------------------|
| All | Everything nearby | Same |
| NPCs | Living actors | Same |
| Doors | Door references | Same |
| Containers | Chests, barrels, etc. | Same |
| Items | Weapons, armor, potions, books, keys, ingredients, misc | Same |
| Activators | Furniture, crafting stations | Forges, enchanting tables, alchemy labs |
| Quest Markers | Active quest objectives | Same (use compass data) |
| Corpses | Dead actors | Same |
| Locations | Discovered map markers on compass | Same |
| Companions | Following actors | Same (check IsPlayerTeammate()) |

**FO4-specific categories NOT needed for Skyrim:**
- Power Armor (doesn't exist in Skyrim)
- Workshop/Settlement (doesn't exist in Skyrim)

### Subcategories

Some categories have subcategories the user can cycle through with a dedicated key:

| Category | Subcategory A | Subcategory B |
|----------|--------------|---------------|
| Containers | Non-empty | Empty |
| Doors | Locked | Cell doors (doors leading to other areas) |
| Corpses | Unlooted | Looted |
| Activators | Furniture | Other |

### Key Bindings

These are the keys used in FO4 Access. Adapt as needed for Skyrim.

| Key | Action |
|-----|--------|
| **Numpad 5** | Scan: searches nearby objects in current category, announces count + nearest |
| **Page Down** | Next object in current category |
| **Page Up** | Previous object in current category |
| **Shift + Page Down** | Next category (skips empty categories) |
| **Shift + Page Up** | Previous category (skips empty categories) |
| **Home** | Announce current object (name, distance, status) + turn camera toward it |
| **Shift + Home** | Toggle autowalk: start walking to selected object, or stop if already walking |
| **End** | Cycle subcategory (locked doors only, empty containers only, etc.) |

### Scanning Details

**How scanning works:**
1. When user presses Numpad 5, scan all loaded cells around the player
2. For each reference found, check its type and add to the matching category
3. Sort results by distance (nearest first)
4. In interiors: objects on the same floor level (within 256 units Z difference) are prioritized
5. Auto-rescan if player moves more than 100 units from last scan position

**What data is collected per object:**
- Reference pointer (to walk to it later)
- Display name
- Distance from player (3D Euclidean)
- Elevation difference (above/below)
- Status: locked, empty, looted, etc.

### Announcements

**On scan (Numpad 5):**
```
"5 Doors. Iron Door, 234 units, below"
"3 Containers. Chest, 89 units"
"No NPCs nearby"
```

**On navigation (Page Up/Down):**
```
"Iron Door, locked, 234 units, above"
"Chest, empty, 100 units"
```

**On category change (Shift + Page Up/Down):**
```
"Doors"
"Quest Markers"
"Containers"
```

**On subcategory change (End):**
```
"Locked only"
"Non-empty only"
"All"
```

**Distance format:**
- Integer value in game units: `"234 units"`
- If object is more than 256 units above player: append `", above"`
- If object is more than 256 units below player: append `", below"`

**Status suffixes added to object name:**
- Locked doors: `"Iron Door, locked"`
- Empty containers: `"Chest, empty"`
- Looted corpses: `"Bandit, looted"`

---

## Part 2: AutoWalk (Creation Kit + Papyrus)

### What the Creation Kit needs to provide

You need to create one ESP file containing a quest with specific structure. Here is exactly what to create:

#### 1. Quest Record

| Property | Value |
|----------|-------|
| EditorID | `SkyrimTTS_AutoWalkQuest` |
| Flags | Start Game Enabled |
| Priority | 50 (default) |
| Script | Attach a Papyrus script (see below) |

#### 2. Reference Aliases (inside the quest)

**Alias 1: DstMarker**
- Type: Reference Alias
- Fill Type: None (will be filled at runtime by Papyrus)
- Purpose: Stores the destination target. The Travel package points here.

**Alias 2: Traveler**
- Type: Reference Alias
- Fill Type: Specific Reference → Player
- Purpose: The actor that will be moved (the player). The Scene uses this.

#### 3. Scene

| Property | Value |
|----------|-------|
| EditorID | `SkyrimTTS_WalkScene` |
| Flags | Override Behavior, Repeat |

The scene contains ONE phase with ONE package:

#### 4. AI Package (inside the Scene)

| Property | Value |
|----------|-------|
| Type | Travel |
| Target | DstMarker alias |
| Preferred Speed | Run (or Walk, depending on preference) |
| Allow Swimming | Yes |

This is the key piece: when the scene starts, the Travel package makes the player walk along the navmesh toward DstMarker. The game engine handles all pathfinding automatically.

#### 5. Script Properties (set in CK)

After attaching the Papyrus script to the quest, set these properties:

| Property | Type | Value |
|----------|------|-------|
| DstMarker | ReferenceAlias | → point to DstMarker alias |
| Traveler | ReferenceAlias | → point to Traveler alias |
| WalkScene | Scene | → point to the scene |
| PlayerRef | Actor | → Game.GetPlayer() / PlayerRef |

### Summary of CK structure

```
Quest: SkyrimTTS_AutoWalkQuest (Start Game Enabled)
├── Script: SkyrimTTS_AutoWalk.psc
├── Alias: DstMarker (ReferenceAlias, empty, filled at runtime)
├── Alias: Traveler (ReferenceAlias, filled with PlayerRef)
└── Scene: SkyrimTTS_WalkScene (Override Behavior, Repeat)
    └── Package: Travel to DstMarker (Run speed, Allow Swimming)
```

---

## Part 3: Papyrus Script

The Papyrus script bridges C++ and Creation Kit. Here is the full script logic:

```papyrus
ScriptName SkyrimTTS_AutoWalk extends Quest

; === Properties (set in Creation Kit) ===
ReferenceAlias Property DstMarker Auto
ReferenceAlias Property Traveler Auto
Scene Property WalkScene Auto
Actor Property PlayerRef Auto

; === Internal state ===
ObjectReference CurrentTarget
float fStopDistance = 100.0
bool IsWalking = false
int TimerArrivalCheck = 1
float CheckInterval = 0.25

; === Called from C++ via DispatchMethodCall ===
Function OnWalkToTarget(int aiFormID, float afStopDistance)
    Form targetForm = Game.GetForm(aiFormID)
    if targetForm == None
        return
    endIf

    ObjectReference targetRef = targetForm as ObjectReference
    if targetRef == None
        return
    endIf

    ; Stop any current walk first
    if IsWalking
        StopWalkingInternal(false)
    endIf

    fStopDistance = afStopDistance
    CurrentTarget = targetRef

    ; Check if already close enough
    float dist = PlayerRef.GetDistance(CurrentTarget)
    if dist <= fStopDistance
        ; Already there
        return
    endIf

    ; Set destination
    DstMarker.ForceRefTo(CurrentTarget)

    ; Register for combat detection
    RegisterForRemoteEvent(PlayerRef, "OnCombatStateChanged")

    ; Enable AI-driven movement
    Game.SetPlayerAIDriven(true)

    ; Start the walking scene (Travel package kicks in)
    WalkScene.Start()

    IsWalking = true

    ; Start polling for arrival
    StartTimer(CheckInterval, TimerArrivalCheck)
EndFunction

; === Called from C++ to stop walking ===
Function OnStopWalking()
    if IsWalking
        StopWalkingInternal(true)
    endIf
EndFunction

; === Timer: check if we arrived ===
Event OnTimer(int aiTimerID)
    if aiTimerID == TimerArrivalCheck
        CheckArrival()
    endIf
EndEvent

Function CheckArrival()
    if CurrentTarget == None || CurrentTarget.IsDeleted()
        StopWalkingInternal(true)
        return
    endIf

    float dist = PlayerRef.GetDistance(CurrentTarget)
    if dist <= fStopDistance + 20.0
        ; Arrived!
        PlayerRef.SetLookAt(CurrentTarget, true)
        StopWalkingInternal(false)
    else
        ; Keep checking
        StartTimer(CheckInterval, TimerArrivalCheck)
    endIf
EndFunction

; === Combat auto-stop ===
Event Actor.OnCombatStateChanged(Actor akSender, Actor akTarget, int aeCombatState)
    if akSender == PlayerRef && aeCombatState == 1
        if IsWalking
            StopWalkingInternal(true)
        endIf
    endIf
EndEvent

; === Internal stop ===
Function StopWalkingInternal(bool abNotify)
    CancelTimer(TimerArrivalCheck)
    WalkScene.Stop()
    UnregisterForRemoteEvent(PlayerRef, "OnCombatStateChanged")
    Game.SetPlayerAIDriven(false)
    PlayerRef.EvaluatePackage()
    DstMarker.Clear()
    CurrentTarget = None
    IsWalking = false
EndFunction
```

---

## Part 4: C++ Plugin Integration

The C++ plugin (SKSE) does the following:

### Triggering autowalk from C++

```cpp
// 1. Find the quest at plugin load
auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SkyrimTTS_AutoWalkQuest");

// 2. When user presses Shift+Home with a selected object:
auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
// Get VM handle for the quest
// Call DispatchMethodCall with:
//   Script: "SkyrimTTS_AutoWalk"
//   Method: "OnWalkToTarget"
//   Args: formID (as int32), stopDistance (as float)

// 3. When user presses WASD/Escape/Space during walk:
// Call DispatchMethodCall with:
//   Method: "OnStopWalking"
```

### Cancel detection during walk

While autowalk is active, poll movement keys using Windows API:
```cpp
if (GetAsyncKeyState('W') || GetAsyncKeyState('A') ||
    GetAsyncKeyState('S') || GetAsyncKeyState('D') ||
    GetAsyncKeyState(VK_ESCAPE) || GetAsyncKeyState(VK_SPACE)) {
    StopAutoWalk();
}
```

This is necessary because normal input is blocked during `SetPlayerAIDriven(true)`.

---

## Part 5: Important Notes

### Skyrim-specific adaptations needed

1. **Game.SetPlayerAIDriven()** — In Skyrim, the equivalent is `PlayerCharacter::SetAIDriven(bool)`. Verify it works the same way.

2. **Quest Markers** — Skyrim uses a different data structure than FO4's PipboyDataManager. Quest marker positions may need to be read from the compass system or from quest objective data.

3. **Locations** — Skyrim map markers work differently. Discovered locations are stored on MapMarker extra data on references.

4. **No Power Armor / Workshop** — These categories don't exist in Skyrim, skip them.

5. **Companions** — Use `Actor::IsPlayerTeammate()` instead of FO4's `IsFollowing()`.

6. **FormID precision** — When passing FormID from C++ to Papyrus, always use int32, never float. Float loses precision on large FormIDs.

### Minimum viable version

For a first version, focus on:
1. Quest Markers category (most useful for navigation)
2. Doors category (navigate dungeons)
3. NPCs category (find quest givers)
4. AutoWalk to selected object

Add other categories (Items, Containers, Corpses, etc.) incrementally.

---

## Summary: Who does what

### Pyrhame (C++ SKSE plugin)

Everything below is done in C++ within the existing SkyrimNVDA plugin — same codebase, same tools, same workflow as the current menu vocalization:

- **Object Scanner**: scan nearby objects in loaded cells, sort by distance, organize by category
- **Input handling**: detect all key presses (Numpad 5, Page Up/Down, Home, End, Shift combos)
- **Announcements**: vocalize object names, distances, categories, statuses via NVDA
- **Category/subcategory system**: cycle through categories, filter by subcategory
- **AutoWalk trigger**: when user presses Shift+Home, send the target FormID to the Papyrus script
- **AutoWalk cancel**: detect WASD/Escape/Space via GetAsyncKeyState() during walk and call Papyrus to stop
- **Camera look-at**: turn the player camera toward the selected object on Home key

### Dio (Creation Kit)

These items require the Creation Kit GUI and cannot be done elsewhere:

1. **Create a Quest** named `SkyrimTTS_AutoWalkQuest`
   - Flag: `Start Game Enabled`
   - Attach the Papyrus script `SkyrimTTS_AutoWalk` (provided in this doc, ready to use)

2. **Create 2 Reference Aliases** inside the quest:
   - `DstMarker` — empty, will be filled at runtime (the walk destination)
   - `Traveler` — filled with PlayerRef (the player)

3. **Create a Scene** named `SkyrimTTS_WalkScene`
   - Flags: `Override Behavior`, `Repeat`
   - Add one phase with a **Travel package** targeting the `DstMarker` alias
   - Speed: Run, Allow Swimming: Yes

4. **Set script properties** on the quest:
   - `DstMarker` → point to alias DstMarker
   - `Traveler` → point to alias Traveler
   - `WalkScene` → point to the scene
   - `PlayerRef` → the player reference

5. **Compile the Papyrus script** (the source is provided in this doc)

6. **Save as ESP** and distribute with the mod

The Papyrus script source code is ready to copy-paste from Part 3 of this document.
