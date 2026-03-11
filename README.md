# SkyrimNVDA — Player Guide

Accessibility plugin for Skyrim Special/Anniversary Edition. Automatically vocalizes game menus via NVDA.

---

## Requirements

- Skyrim Special Edition or Anniversary Edition
- SKSE64
- NVDA (running before launching the game)

---

## Getting Started — Character Creation

When you start the game, after the intro cinematic, you arrive at the **character creation menu**.

NVDA announces: *"Character creation"*

### Navigating character creation

| Key | Action |
|-----|--------|
| Numpad 5 / 8 | Switch tab (Race, Sex, Appearance…) |
| Up / Down | Navigate options within a tab |
| Left / Right | Adjust a slider value |
| R | Confirm / validate |

---

## Tween Menu (Tab key)

NVDA announces: *"Cross menu"*

| Key | Destination |
|-----|-------------|
| Up | Magic |
| Down | Inventory |
| Left | Skills |
| Right | Journal |

---

## Inventory

NVDA announces: *"Inventory open"*

- Up/Down: change item → name, value, weight vocalized
- Left/Right (or Q/E): change category
- **H**: announces gold and current carry weight / maximum

---

## Container (chest, body…)

NVDA announces: *"Container open"*

- Up/Down: change item
- Left/Right: switch between your inventory and the container
- **H**: announces gold and carry weight

---

## Magic Menu

NVDA announces: *"Magic menu open"*

- Up/Down: change spell → name, effects, cost vocalized
- Left/Right: change category (Destruction, Restoration…)

---

## Journal (J key)

NVDA announces: *"Journal open"*

- Up/Down: change quest or entry
- **Numpad 5 / 8**: switch tab (Quests, Inventory, Skills, Magic)

---

## Skills Menu (from the Tween menu)

- Up/Down/Left/Right: navigate the skill tree
- The selected skill description is vocalized automatically
- Perks are vocalized with their description and requirements

---

## Favorites (Q key)

NVDA announces: *"Favorites"*

- Up/Down: change item or spell

---

## Dialogue

The NPC's dialogue line is vocalized automatically.
Up/Down to choose your response.

---

## HUD (in game)

| Situation | Vocalization |
|-----------|-------------|
| Object/NPC/door in crosshair | Name + action (e.g. "Open door") vocalized automatically |
| Notification (quest, level…) | Vocalized automatically |
| Subtitle | Vocalized automatically |
| New location discovered | Vocalized automatically |
| **H** (in game) | Current Health / Magicka / Stamina |

---

## Main Menu

NVDA announces: *"Main menu open"*

Up/Down to navigate New Game, Continue, Load, Settings, Quit.

---

## Level Up

NVDA announces: *"Level gained! Choose your improvement."*

Left/Right to choose between Health, Magicka or Stamina.
**Enter** to confirm.

---

## Message Box

Game messages (confirmations, warnings) are vocalized automatically.
Up/Down to navigate buttons, **Enter** to confirm.

---

## Not yet vocalized

- Merchants (buy/sell)
- Crafting stations (forge, enchanting table…)
- Map menu

---

## Keyboard shortcuts summary

| Key | Action |
|-----|--------|
| H | Health/Magicka/Stamina (in game) or Gold/Weight (inventory/container) |
| Tab | Open/close the Tween menu |
| J | Journal |
| Q | Favorites |

---

*Plugin developed by Pyrhame. Requires NVDA.*
