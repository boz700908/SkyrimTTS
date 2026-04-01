# SkyrimNVDA — First Steps Guide

A step-by-step walkthrough for blind players using SkyrimNVDA and the Accessibility mod by Dio Kyrie. This guide covers the game from the very beginning through the Golden Claw quest, explaining every step with accessibility features in context.

---

## Before You Start

Make sure you have:
- NVDA running before launching the game
- The game launched through `skse64_loader.exe` (not the normal Skyrim launcher)
- The SkyrimNVDA plugin installed
- Skyrim Accessibility mod by Dio Kyrie installed — download the latest release at https://github.com/DioKyrie-Git/SkyrimAccessibility/releases

When the game starts, NVDA will automatically begin reading menus and HUD elements.

---

## Quick Reference — Key Controls

### Navigation & Scanner
| Key | Action |
|-----|--------|
| Page Down / Page Up | Next / previous object in scanner |
| Shift + Page Down / Up | Change scanner category |
| End | Cycle subcategories (e.g., locked/unlocked doors) |
| Home | Announce current object details + orient camera toward it |
| Shift + Home | Start / stop autowalk toward selected object |
| Alt + Home | Teleport to selected scanner object (last resort) |
| Numpad 5 | Scan objects in current category |

### Combat
| Key | Action |
|-----|--------|
| X | Lock onto nearest enemy (announces name + distance) |
| Hold attack (bow) | Auto-aim activates — beep when target is in sight |

### Menus
| Key | Action |
|-----|--------|
| Tab | Open cross menu (Up=Skills, Down=Map, Left=Magic, Right=Inventory) |
| H | Announce vitals (in game) or gold/weight (in inventory) |
| Up / Down | Navigate items, spells, quests |
| Left / Right | Change category or side (inventory/container) |

### Map
| Key | Action |
|-----|--------|
| Page Down / Up | Navigate map markers |
| End | Cycle filters (All, Discovered, Undiscovered, Quest Targets) |
| Enter (twice) | Fast travel to selected marker |
| Home | Announce marker details |

### Character Creation
| Key | Action |
|-----|--------|
| Ctrl + Left / Right | Switch tab (Race, Body, Head...) |
| Up / Down | Navigate options within tab |
| Left / Right | Adjust slider value or change sex |
| R | Confirm / validate |

---

## Part 1 — The Cart Ride and Character Creation

The game opens with your character waking up in a horse-drawn prison cart, hands bound, heading toward the town of Helgen. You are a prisoner alongside other captives, including Ralof (a Stormcloak rebel) and Ulfric Stormcloak (leader of the rebellion).

**You have no control during this sequence.** Just listen to the dialogue — it sets up the story. The soldiers are escorting prisoners to Helgen for execution.

When the cart arrives at Helgen, an imperial soldier named Hadvar calls out names. One prisoner tries to run and is immediately shot down by archers. You are then led to the executioner's block — and this is where the **character creation screen** opens.

---

## Part 2 — Character Creation

NVDA announces: **"Character creation"** followed by navigation instructions. You must create your character before the story continues.

### Important — How Character Creation Works

The options in the character creation menu are **not selectable items** — you don't need to press Enter or confirm each one individually. Simply browse through the tabs, move your cursor to the option you want, and adjust it. Nothing is "locked in" until you press **R**. When you are happy with your choices across all tabs, press **R once** to confirm everything at once.

### How to Navigate

- **Ctrl + Right / Left arrow**: Switch between tabs (Race, Body, Head, and sub-categories like Eyes, Mouth, Scars...)
- **Up / Down arrows**: Navigate through options within the current tab
- **Left / Right arrows**: Adjust slider values or change selections (like sex: Male/Female)
- **R**: Confirm your entire character when you're done with all tabs

### What to Choose

**Race tab**: Pick your race using Up/Down arrows. NVDA reads each race name and its description. All races are viable — pick whatever sounds interesting. Nords, Bretons, and Redguards are solid choices for beginners.

**Body tab**: The first option here is your sex (Male/Female), changed with Left/Right arrows. NVDA announces "Male" or "Female". The other sliders control appearance details like weight and skin tone — these are purely visual and don't affect gameplay, so you can skip them.

**Head tab and sub-categories**: Controls facial features like hair, eyes, scars, etc. These are also purely visual. The values are numbers (0, 1, 2...) representing different visual presets.

**Name**: After confirming with R, you'll enter name input mode. Type your character's name and press Enter to confirm.

### Tip
Don't spend too long here — none of the appearance options affect gameplay. Just pick a race, set your sex, type a name, and confirm with R.

---

## Part 3 — The Dragon Attack and Escaping Helgen

Once you confirm your character, a massive dragon — **Alduin** — attacks the town. Chaos erupts with fire and destruction everywhere. **Do not touch anything** — wait until the dragon attack scene plays out and your character is free to move.

### Skipping the Intro (Recommended)

The intro sequence requires jumping from roof to roof and navigating through collapsing buildings, which is very difficult without sight. **We strongly recommend skipping it.**

Once you can move, press **L** to open Dio's accessibility menu, then select **"Walkthroughs"** → **"Main Quest Start"**. The mod will automatically walk your character through the entire intro sequence — you don't need to do anything, just wait. At the end, a dialog box will ask you to choose between **Hadvar** (Imperial) or **Ralof** (Stormcloak). Both paths are nearly identical and the choice doesn't affect the main story significantly. Select one and the walkthrough will guide you to the right place.

If you don't skip, you'll need to navigate the scripted escape sequence manually, which involves following NPCs through burning buildings — this is extremely challenging for blind players.

### Choosing Your Companion

After the intro (or after skipping it), follow your chosen companion.

**Using the Scanner**: Press **Page Down / Page Up** to find NPCs near you. Use **Shift + Page Down** to switch to the **NPCs** category. Home key will orient your camera toward the selected NPC. Press **Shift + Home** to autowalk toward them.

### Entering the Keep

Follow your companion into Helgen Keep. Inside, they cut your bonds.

### Your First Equipment

Your companion tells you to search a nearby body for equipment. Here's how:

1. Press **Shift + Page Down** until you hear **"Corpses"** category
2. Press **Page Down** to find the nearest corpse
3. Press **Home** to face it, then **Shift + Home** to walk to it
4. When close enough, press **E** to interact — this opens the container menu
5. NVDA reads: **"Container open"**
6. Use **Up / Down** to browse items (weapons, armor)
7. Press **E** or **Enter** to take items

Now open your inventory (**Tab**, then **Right** for Inventory) and equip your new weapon and armor.

### First Combat — Learning to Fight

Shortly after, you encounter your first enemies — two soldiers.

**Melee Combat basics:**
- **Left click**: Attack
- **Right click (hold)**: Block
- **X key**: Turns your camera toward the nearest hostile enemy — NVDA announces the enemy name and distance. **Shift + X** toggles permanent camera lock onto the enemy. Press **Shift + X** again to unlock.
- Walk toward the enemy and attack. Your companion fights alongside you.

**Tip**: The X key is your best friend in combat. It tells you who the nearest hostile enemy is and turns your camera toward them. Press it repeatedly during a fight to stay oriented.

### Navigating the Keep

The keep is a series of rooms and corridors. Use the scanner to navigate:

The simplest approach is often to **follow your companion** (Ralof or Hadvar) using the scanner (NPCs category → Home → Shift+Home). If they stop moving, look for the quest objective in the **Quests** category of the scanner and follow it with autowalk. You can also use the **audio navigation** — a regular ticking sound that guides you toward the objective. Press **O** to toggle this audio guidance on or off.

To open doors:
1. **Doors category** (Shift + Page Down until "Doors"): Find the next door
2. **Home**: Orient toward the door
3. **Shift + Home**: Autowalk to the door
4. **E**: Open the door

You'll pass through several areas:
- **A torture chamber** with more enemies
- **Natural caves** with giant Frostbite Spiders (use X to lock on, attack from distance if you have a bow)
- **A sleeping bear** — your companion suggests sneaking past. Crouch with **Left Ctrl** and walk slowly past it. NVDA will announce "Sneaking". If detected, NVDA says "Detected" — just run past.

### Exiting the Cave

You finally reach the cave exit and emerge outside. The quest "Unbound" completes. You're free to explore Skyrim.

---

## Part 4 — Riverwood

### Getting There

Your companion suggests heading to Riverwood, a small town nearby. You can:
- Follow your companion using the scanner (NPCs category → Home → Shift+Home)
- Or use the **Quests** category in the scanner to find the quest marker direction

### Dio's Navigation Audio (V Menu)

Press **V** to access Dio's accessibility menu. This provides additional navigation tools:
- **Clairvoyance sound**: An audio trail that guides you toward your current quest objective. Follow the sound — it gets louder as you face the right direction.
- **O key**: Toggle the navigation audio on/off

The clairvoyance audio and the autowalk complement each other well. When autowalk can't find a path (steep terrain, complex areas), use the audio cue to walk manually in the right direction.

### Arriving in Riverwood

When you arrive in Riverwood, NVDA announces the location name. This is your first safe town.

**Important locations** (use the scanner to find them):
- **Alvor's Smithy** — A blacksmith with crafting stations (forge, grindstone, workbench)
- **Riverwood Trader** — A general store run by Lucan Valerius
- **Sleeping Giant Inn** — An inn where you can rest

### Your First Crafting (Optional — recommended later)

Crafting requires materials that you won't have at this stage of the game. We mention it here so you know where to find the stations, but it's best to come back later once you've collected resources. At the blacksmith, you can use crafting stations. When you activate one (press E while facing it), NVDA announces the crafting menu.

**Forge** (creates new items):
- Use **Ctrl + Left/Right** to switch categories (Weapons, Armor, etc.)
- Use **Up/Down** to browse recipes
- NVDA reads: recipe name, quantity produced, required materials, and damage/armor stats

**Grindstone** (improves weapons):
- Simple list — use **Up/Down** to browse your weapons
- Press Enter to improve the selected weapon

**Workbench** (improves armor):
- Same as grindstone but for armor pieces

---

## Part 5 — Bleak Falls Barrow (Dragonstone and Golden Claw)

This dungeon is tied to two quests that you complete at the same time:
- **Main quest**: the mage Farengar at Dragonsreach (Whiterun) asks you to retrieve the **Dragonstone** from Bleak Falls Barrow
- **The Golden Claw**: Lucan Valerius at the Riverwood Trader asks you to retrieve his stolen golden claw, which is in the same dungeon

You can pick up both quests before going, or just follow the main quest. Either way, you'll collect both items while exploring the dungeon.

### Getting the Quests

**Main quest (Dragonstone)**: After speaking with Jarl Balgruuf at Dragonsreach, he sends you to his mage **Farengar Secret-Fire**. Farengar asks you to retrieve the Dragonstone from Bleak Falls Barrow.

**Optional quest (Golden Claw)**: Enter the **Riverwood Trader** (use scanner → Doors category to find it). Talk to **Lucan Valerius**. He explains that thieves stole a golden claw ornament. The thief, Arvel the Swift, fled to Bleak Falls Barrow. Lucan offers a reward if you retrieve it.

Both quests will appear in your journal (**Tab → Down for Map, or J** to open directly).

### Traveling to Bleak Falls Barrow

From Riverwood, head northwest up the mountain. You can:
- Use **Quests** category in the scanner to find the quest direction
- Use **Shift + Home** to autowalk toward the quest marker
- If autowalk struggles on the mountain path, use the **clairvoyance audio** (O key) and walk manually
- The path goes uphill — if you hear "Can't reach target", try walking manually following the audio cue, then re-engage autowalk

### Outside the Barrow — Bandits

At the entrance, **3 bandits** guard the area.
1. Press **X** to lock onto the nearest enemy
2. If you have a bow, draw it (hold attack) — the auto-aim beep guides your shot
3. For melee, press **X** then walk toward the enemy and attack
4. The **kill sound** (3 descending beeps) confirms each kill

### Inside — First Rooms

Enter the barrow. More bandits inside, typically 2 in the first large room. Use X to find them and fight.

As you go deeper, you'll find a dead bandit near a trap. There are poisoned dart traps triggered by a lever — be careful.

### The Pillar Puzzle

You'll reach a room with **three rotating stone pillars** and a locked gate with a lever.

**Important**: If you installed the recommended **Puzzle Pillar Auto-Solve** mod, the pillars are already set correctly. Just pull the lever (find it with scanner → Activators category → E to pull).

If you don't have the mod, the solution is: **Snake, Snake, Whale** (left to right). Use the scanner's Activators category to find each pillar. Our mod reads the current symbol in parentheses (e.g., "Pillar (Snake)"). Activate the pillar with E to rotate it until it shows the correct symbol.

### The Spider Boss

Deeper in, you encounter a large **Frostbite Spider** — a mini-boss. It has trapped someone in its webs.

1. Press **X** to lock onto the spider
2. Use ranged attacks if possible (bow with auto-aim works great here)
3. Keep distance — the spider can poison you
4. The kill sound confirms when it's dead

### Finding the Golden Claw

After killing the spider, you find **Arvel the Swift** — the thief. He may be dead or you may need to fight him. Either way, search his body:

1. Scanner → **Corpses** category → find Arvel
2. **E** to open and loot — take the **Golden Claw**

### Draugr Enemies

As you go deeper, you encounter **Draugr** — undead Nordic warriors. They use melee weapons and some can shout.

- Press **X** to lock onto each draugr
- Fight them one at a time when possible
- Use healing items between fights (open inventory → navigate to potions)

### The Golden Claw Door

You'll reach a door with three rotating rings and a keyhole shaped like a claw.

**If you have the Dragon Claws Auto-Unlock mod**: Just activate the door and it opens automatically since you have the claw in your inventory.

**Without the mod**: The solution is carved on the claw itself. The symbols from top to bottom on the Golden Claw are: **Bear, Moth, Owl**. Our scanner reads the ring symbols (e.g., "Inner ring (Bear)"). Activate each ring to rotate it to the correct symbol, then activate the keyhole.

### The Word Wall and Final Boss

After the claw door, you enter a large chamber with a **Word Wall** — an ancient stone wall covered in glowing runes.

1. Use the scanner → **Activators** category to find the Word Wall
2. Walk toward it — as you approach, you automatically learn the Word of Power "Fus" (Force), the first word of the Unrelenting Force shout
3. This triggers the **final boss**: a **Draugr Overlord** bursts from a coffin

**Fighting the Draugr Overlord:**
- Press **X** immediately to lock onto it
- Attack quickly while it's still rising from the coffin
- Beware: it can use the Unrelenting Force shout to knock you down
- Use healing potions liberally
- The kill sound confirms victory

4. Loot the Overlord's body — it carries the **Dragonstone** (important for the main quest later)

### Exiting the Barrow

After the boss, follow the path to find an exit. Use scanner → Doors to find the way out. You'll emerge on the mountainside.

---

## Part 6 — Returning the Claw and Dragonstone

### Back to Riverwood (Golden Claw)

Travel back to Riverwood:
- Open the **Map** (Tab → Down)
- Use **Page Down/Up** to find "Riverwood"
- Press **Enter** twice to fast travel there

Enter the Riverwood Trader and talk to **Lucan Valerius**. He's thrilled to have the claw back and rewards you with gold. Golden Claw quest complete!

---

## Part 7 — To Whiterun and Beyond

### Next Destination: Whiterun

Your main quest now points you toward **Whiterun**, the large city to the northeast. You need to warn the Jarl about the dragon attack on Helgen.

Travel there:
- Open the **Map** → find "Whiterun" (or nearby "Whiterun Stables")
- **Enter** twice to fast travel
- Or walk there using the quest marker (Quests category in scanner)

### Dragonsreach

### Returning the Dragonstone

Travel to Whiterun and head to **Dragonsreach**. Talk to **Farengar Secret-Fire** and hand over the Dragonstone. The main quest advances.

**Congratulations!** You've completed the opening arc of Skyrim. From here, the main quest continues with dragon encounters and the discovery that you are the Dragonborn — but the entire world of Skyrim is open for you to explore.

---

## General Tips

### Autowalk and Audio Navigation
- **Autowalk** (Shift + Home) works great on flat terrain and inside buildings
- When autowalk says "Can't reach target", switch to **manual walking with audio navigation** (O key for clairvoyance sound)
- The two systems complement each other — use autowalk for easy paths and audio navigation for complex terrain

### Scanner Teleport
- **Alt + Home** teleports you directly to the selected scanner object — use this as a last resort when you are stuck in a dungeon or cannot reach an objective
- In interiors, teleportation only works within the same area (no teleporting through loading doors)
- In exteriors, limited to 3000 units range
- **Warning**: teleporting past quest triggers, locked doors, or scripted events may break quest progression — if something goes wrong, reload a previous save

### Combat Tips
- Always press **X** before engaging enemies to know who and where they are
- For ranged combat, equip a bow — the **auto-aim** system handles targeting automatically
- The auto-aim **beep** tells you when you have line of sight — release your arrow when you hear it
- **Dragons**: The auto-aim prioritizes dragons over other enemies, and you'll hear "Dragon in flight" / "Dragon landed" announcements
- **Kill sound**: 3 descending beeps confirm any enemy kill

### Exploration Tips
- Use the scanner categories strategically: NPCs to find people, Doors to navigate, Items to find loot
- The **Locations** category shows nearby discovered map markers — useful for finding towns
- Press **H** anytime to check your Health, Magicka, and Stamina
- In inventory, press **H** for your gold and carry weight

### Crafting Tips
- Visit a forge to create new weapons and armor from raw materials
- Use a grindstone to sharpen weapons (increases damage)
- Use a workbench to improve armor (increases armor rating)
- Recipes show required materials — you need to have them in your inventory

### Map and Fast Travel
- Open the map with **Tab → Down** or **M**
- Navigate markers with **Page Down / Up**
- Use **End** to filter: All, Discovered only, Undiscovered only, or Quest Targets only
- Press **Enter** twice on a discovered location to fast travel there
- You can only fast travel to locations you've already visited
- Use **Shift + Home** to set a reference point and measure distances between markers
