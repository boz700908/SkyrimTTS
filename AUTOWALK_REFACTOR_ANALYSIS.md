# Analyse approfondie : Autowalk SkyrimNVDA vs f4access

**Date** : 2026-04-20
**Contexte** : Le système autowalk de SkyrimNVDA souffre d'un crash persistant `BSShaderAccumulator` (rax=0 à +0xF4, pendant la passe render) qui peut corrompre les sauvegardes. Malgré des mois d'investigation, la cause précise reste insaisissable. Le plugin sœur f4access (Fallout 4 accessibility, dans `c:\Users\marcd\source\repos\f4access\`) implémente la même fonctionnalité sans jamais crasher. Ce document consigne l'analyse comparative exhaustive faite par plusieurs agents pour identifier la cause et proposer un refactor.

---

## ÉTAT ACTUEL (2026-04-21) — refactor majoritairement exécuté

Le refactor f4access-style a été appliqué sur la branche Pyrhame. Résultat :

- **Étapes complétées** : 1, 2, 3, 4, 5 (parties 1 et 2), 8, 9
- **Étapes non faites** (reportées) : 6 (Scene.Start/Stop Papyrus), 7 (mode mounted réel)
- **autowalk.h** : passé de **2531 à ~1555 lignes** (-39%)
- **Zéro mutation d'acteur C++** : plus aucun `SetAIDriven`, `EvaluatePackage`, création de ref, delete de ref
- **Zéro thread de polling** : le monitor jthread et le crash-diag jthread sont supprimés
- **Cancel input event-driven** via `AutoWalkInputUpdate()` dans `InputListener::ProcessEvent` (style f4access)
- **Arrivée détectée côté Papyrus** via ModEvent `SkyrimNVDA_AutoWalkArrived`, orientation via `Actor.SetLookAt` natif
- **Tests** : objet jeté au sol fonctionne, autowalk général stable, pas de régression observée
- **Crash BSShaderAccumulator** : éliminé par construction (la classe de bug causée par mutation d'acteur pendant render pass n'est plus possible)

**Commits principaux** :
- `0608a3b` — refactor principal (étapes 1-3, 5, 9)
- `b8a7597` — étape 4 (suppression CreateTempMarkerAt)

**Éléments C++ restants qui touchent à l'acteur** (conservés volontairement) :
- `kTryStep` / `kCanJump` sur le char controller (flags physiques Havok, pas de l'AI — pas d'API Papyrus équivalente et pas impliqué dans la classe de crash)
- `SetActorValue(kSpeedMult, base)` en pre-start (corrige un bug de vitesse lente post-load)

Si l'étape 6 (Scene) est un jour faite, la pureté C++-messager sera parfaite. L'étape 7 (mounted) est une feature distincte, pas un refactor.

---

## Table des matières

1. [Rapport exhaustif f4access](#1-rapport-exhaustif-f4access)
2. [Rapport exhaustif SkyrimNVDA](#2-rapport-exhaustif-skyrimnvda)
3. [Comparaison et plan de refactor](#3-comparaison-et-plan-de-refactor)
4. [Synthèse finale et recommandations](#4-synthèse-finale-et-recommandations)

---

# 1. Rapport exhaustif f4access

## 1.1 Architecture d'ensemble

f4access utilise une **architecture hybride C++/Papyrus** où C++ est un dispatcher mince et toute la logique de mouvement vit dans un **Papyrus Quest avec un Scene qui exécute un Travel package sur le joueur via un ReferenceAlias**. C++ ne touche **jamais** à `SetAIDriven`, n'assigne **jamais** de packages, ne fait **jamais** d'injection d'input brut.

Le C++ se limite à :
1. Chercher le quest form par EditorID
2. Valider la cible
3. Appeler une fonction Papyrus via `RE::GameVM::DispatchMethodCall`, passant la cible FormID en `int32` (pas `float` — évite la perte de précision FormID sur les DLC)
4. Poller à chaque event input pour détecter les touches d'annulation

**Fichiers sources** :
- `c:\Users\marcd\source\repos\f4access\src\AutoWalk.h` (58 lignes)
- `c:\Users\marcd\source\repos\f4access\src\AutoWalk.cpp` (187 lignes)
- `c:\Users\marcd\source\repos\f4access\Data\Scripts\Source\User\F4A\AutoWalk.psc` (240 lignes)
- `c:\Users\marcd\source\repos\f4access\src\ObjectScanner.cpp` (intégration L1849-1908)
- `c:\Users\marcd\source\repos\f4access\src\main.cpp:29` (init wire-up)
- `c:\Users\marcd\source\repos\f4access\docs\plans\new-vegas-port-feasibility.md:372-470` (doc design)

## 1.2 Côté C++

### État (AutoWalk.h:44-56)

**Pas d'atomics, pas de threads, pas de timers.** Juste des pointeurs plain :

```cpp
RE::TESQuest* m_quest = nullptr;
RE::TESObjectREFR* m_targetRef = nullptr;   // non-null == "walking"
float m_stopDistance = 100.0f;
std::function<void()> m_onArrival = nullptr;
bool m_initialized = false;
static constexpr float ARRIVAL_TOLERANCE = 30.0f;
```

`IsWalking()` est défini comme `m_targetRef != nullptr` — l'état dérive de la cible stockée, pas d'un flag séparé.

### Init — deferred à kGameDataReady

`main.cpp:27-29` :

```cpp
if (a_msg->type == F4SE::MessagingInterface::kGameDataReady) {
    AutoWalk::GetSingleton()->Initialize();
```

`AutoWalk.cpp:16-30` fait uniquement `GetFormByEditorID<RE::TESQuest>("F4A_AutoWalkQuest")` et warne si l'ESP manque. **Graceful degradation** au lieu de crash.

### Start — dispatch via VM (AutoWalk.cpp:32-100)

Pattern critique : récupérer le handle policy, obtenir le handle depuis le quest form, puis `DispatchMethodCall` :

```cpp
auto& handlePolicy = vm->GetObjectHandlePolicy();
auto handle = handlePolicy.GetHandleForObject(
    RE::BSScript::GetVMTypeID<RE::TESQuest>(), m_quest);

if (handle == handlePolicy.EmptyHandle()) { /* error + clear state */ return; }

bool dispatched = vm->DispatchMethodCall(
    handle,
    RE::BSFixedString("F4A:AutoWalk"),
    RE::BSFixedString("OnWalkToTarget"),
    nullptr,
    static_cast<std::int32_t>(a_target->GetFormID()),  // <-- int32, PAS float
    a_stopDistance);
```

Early-out à `AutoWalk.cpp:52-57` si déjà à stop distance, appelle le callback arrival immédiatement — pas de dispatch.

### Update — appelé depuis le hook input-event, pas de thread

`ObjectScanner.cpp:77-80` :

```cpp
bool ObjectScanner::ShouldHandleEvent(const RE::InputEvent* a_event) {
    AutoWalk::GetSingleton()->Update();
```

Donc la cadence est "à chaque input event" — effectivement à chaque frame pendant l'activité. **Zéro background thread, zéro std::atomic, zéro Win32 timer, zéro F4SE task dispatch.**

`Update` à `AutoWalk.cpp:133-162` :

```cpp
if (!m_targetRef) return;
// poll WASD / Esc / Space via GetAsyncKeyState (vk codes)
if (GetAsyncKeyState(0x57) & 0x8000 || ... ) { Stop(); return; }
if (m_targetRef->IsDeleted() || m_targetRef->IsDisabled()) { Stop(); return; }
CheckArrival();
```

**`GetAsyncKeyState` est utilisé explicitement parce que AI-driven mode bloque les input events normaux** (commentaire AutoWalk.cpp:139-140). C'est un point crucial.

### Arrival (côté C++, AutoWalk.cpp:164-186)

Check distance pur. Le callback est copié localement avant que l'état soit nettoyé, pour survivre à une mutation réentrante :

```cpp
if (distance <= m_stopDistance + ARRIVAL_TOLERANCE) {
    auto callback = m_onArrival;   // copy before clearing
    m_targetRef = nullptr;
    m_onArrival = nullptr;
    if (callback) callback();
}
```

**Note importante** : Papyrus fait son **propre** check d'arrivée indépendant et stoppe le scene. Le check C++ existe **uniquement pour fire le callback C++** (ObjectScanner l'utilise pour tourner le joueur face à l'objet).

### Stop (AutoWalk.cpp:102-131)

Dispatch `OnStopWalking` à Papyrus puis null son propre état. No-op si pas en train de walker. **Ne call jamais SetAIDriven depuis C++.**

### Pas de AddTask / main-thread dispatching

Il n'y a **pas** d'équivalent `SKSE::GetTaskInterface()` utilisé. Tout le C++ tourne sur le thread input-handler, et tout le travail actor-mutating est poussé dans Papyrus, qui tourne sur le thread VM. **C'est le trick de robustesse clé.**

## 1.3 Côté Papyrus — le vrai moteur

`Data\Scripts\Source\User\F4A\AutoWalk.psc` — `Scriptname F4A:AutoWalk extends Quest conditional`.

### Properties (psc:15-34)

```
ReferenceAlias DstMarker       ; destination alias -- target gets forced here
ReferenceAlias Traveler        ; player alias (PlayerRef) used by Scene
Scene WalkScene                ; the scene that runs the Travel package
Actor PlayerRef
Idle IdleStop                  ; optional stop animation
```

### Entry point depuis C++ (psc:71-87)

```papyrus
Function OnWalkToTarget(int aiFormID, float afStopDistance)
    Form targetForm = Game.GetForm(aiFormID)
    ObjectReference targetRef = targetForm as ObjectReference
    fStopDistance = afStopDistance
    StartWalkingTo(targetRef)
```

### StartWalkingTo — la séquence critique (psc:100-147)

```papyrus
DstMarker.ForceRefTo(akTarget)                             ; 1. stuff target in alias
RegisterForRemoteEvent(PlayerRef, "OnCombatStateChanged")  ; 2. combat abort
Game.SetPlayerAIDriven(true)                               ; 3. hand player to AI
WalkScene.Start()                                          ; 4. scene runs Travel package
IsWalking = true
StartTimer(CheckInterval, TimerArrivalCheck)               ; 5. 0.25s tick
```

Le Scene contient un Travel package dont la cible est `DstMarker` (l'alias). Donc **la cible du package est alias-filled, PAS assignée dynamiquement** — le mécanisme de Fallout 4 (alias fill + scene-start) est la machinerie moteur qui rend ça robuste. Le package est cuit dans la form database ; Papyrus ne fait que toggler quelles refs vont dans l'alias.

### Arrival loop (psc:149-191) — timer auto-polling Papyrus

```papyrus
Event OnTimer(int aiTimerID)
    if aiTimerID == TimerArrivalCheck && IsWalking
        CheckArrival()

Function CheckArrival()
    if CurrentTarget.IsDeleted() || CurrentTarget.IsDisabled()
        StopWalking(false); return
    if distance <= stopDist + 20.0
        StopWalking(false)
        PlayerRef.SetLookAt(CurrentTarget, true)
    else
        StartTimer(CheckInterval, TimerArrivalCheck)   ; re-arm
```

Cadence : 0.25s. Le timer est **single-shot et ré-armé depuis l'intérieur du handler** — idiome classique qui évite les bugs de timers empilés.

### Interruption combat (psc:58-67)

```papyrus
Event Actor.OnCombatStateChanged(Actor akSender, ..., int aeCombatState)
    if akSender == PlayerRef && aeCombatState == 1 && IsWalking
        StopWalking(true)
```

Abort automatique à la détection d'ennemi — le joueur n'est pas locké en AI-driven pendant un combat.

### StopWalkingInternal (psc:203-229) — l'ordre compte

```papyrus
CancelTimer(TimerArrivalCheck)
WalkScene.Stop()
UnregisterForRemoteEvent(PlayerRef, "OnCombatStateChanged")
Game.SetPlayerAIDriven(false)
PlayerRef.EvaluatePackage()        ; force package re-eval après release
if IdleStop
    PlayerRef.PlayIdle(IdleStop)
DstMarker.Clear()                  ; alias cleared LAST
CurrentTarget = None
IsWalking = false
```

**L'ordre est délibéré** : timer d'abord (plus de ticks), puis scene, puis event unreg, puis AIDriven off, puis `EvaluatePackage` pour remettre le joueur dans son package normal avant de clearer l'alias. `DstMarker.Clear()` **après** que AI soit released évite que la cible du package disparaisse pendant que le package est encore actif.

## 1.4 Différences vs approche Skyrim SetAIDriven+Travel

Avantages clés du design f4access qui manquent probablement dans un port Skyrim naïf :

1. **Pas de package ad-hoc.** Le package n'est pas créé/attaché/assigné au runtime — c'est un form pré-authored dans l'ESP dont la cible est un alias. Papyrus fait juste `ForceRefTo` et `Scene.Start()`. La machinerie moteur package n'est jamais appelée à faire quelque chose de non-standard.

2. **Scene-driven, pas package-driven directement.** `WalkScene.Start()` est ce qui active le package sur l'actor aliasé. Stopper le scene retire proprement le package sans uninstall manuel. Les scenes ont un lifecycle start/stop bien défini que l'add-remove de package n'a pas.

3. **Toute la mutation actor est sur le thread VM Papyrus.** C++ n'appelle jamais `SetAIDriven`, `EvaluatePackage`, `ForceRefTo`, ou `SetLookAt`. Ces opérations tournent dans Papyrus où la VM serialize l'accès à l'état actor. Les plugins SKSE qui appellent `SetActorAIDriven` off-main/VM-thread sont une source de crash bien connue.

4. **FormID passé en int32 via `DispatchMethodCall`.** Évite la perte de précision sur les FormIDs avec les octets de poids fort set (DLC/mods). CHANGELOG.md:29 note explicitement `Fix autowalk failing on DLC/mod objects due to float FormID precision loss`.

5. **L'ownership de "is walking" vit à deux endroits mais dérive de sources distinctes.** C++ owns `m_targetRef` pour la comptabilité callback ; Papyrus owns `IsWalking` pour l'état moteur. Ils peuvent se désynchroniser et chaque côté tolère ça (C++ `Stop` est idempotent et no-op quand pas de target ; Papyrus `OnStopWalking` check `IsWalking`).

6. **`GetAsyncKeyState` polling pour cancel.** Parce que AI-driven mode swallow les input events normaux, f4access poll l'état clavier Win32 directement depuis le hook input-event (`AutoWalk.cpp:141-146`). Une version Skyrim qui se fie à `OnKeyDown` ou InputEvent silencieusement échouera à cancel.

7. **`EvaluatePackage()` après avoir release AI.** Important pour ré-asseoir le package normal (no-op) du joueur. Le skipper peut laisser le joueur dans un état stupéfié.

8. **Remote event combat state** comme interrupt dur, enregistré **uniquement pendant le walk** (évite l'overhead listener permanent).

Doc design (`docs/plans/new-vegas-port-feasibility.md:418`) précise qu'aucune injection input brut ou calcul d'angle manuel n'est fait — le pathfinding navmesh du jeu gère les obstacles gratuitement. Si une version Skyrim walk en calculant un heading et en pushant WASD, c'est la divergence fondamentale.

## 1.5 Indicateurs de stabilité

- **Graceful degradation quand ESP manquant** (`AutoWalk.cpp:22-26`) : warn, disable feature, pas de crash.
- **Validation à chaque entry** : `IsDeleted() || IsDisabled()` check dans `WalkTo` (L40), chaque `Update` (L154), Papyrus `CheckArrival` (L165), Papyrus `StartWalkingTo` (L103).
- **Handle policy empty-check** (`AutoWalk.cpp:75`) : si la VM ne peut pas faire un handle pour le quest, l'état est rolled back au lieu de continuer.
- **Dispatch return-value check** (`AutoWalk.cpp:90`) : l'état est rolled back si dispatch fail.
- **Callback copy-before-clear** (`AutoWalk.cpp:176-184`) : défend contre les callbacks qui pourraient re-entrer dans `WalkTo`/`Stop`.
- **Papyrus `StartWalkingTo` re-entry** (`psc:108-111`) : appelle `StopWalkingInternal()` first si déjà walking — pas de scenes nestés.
- **CHANGELOG.md** mentionne des fixes pour : précision float FormID, crash null-display-name sur walk vers quest-marker, use-after-free dans Pip-Boy (non lié mais montre que le projet connaît les UAF actor).
- **Pas de commentaire "DO NOT CALL THIS"**, pas de garde known-crash au-delà des checks de validation. La robustesse du design vient de pousser le travail vers Papyrus, pas de fix-ups défensifs autour des bugs moteur.

## 1.6 Signatures Papyrus (pour couche de compat)

```papyrus
Scriptname F4A:AutoWalk extends Quest conditional

Function OnWalkToTarget(int aiFormID, float afStopDistance)  ; called from C++
Function OnStopWalking()                                      ; called from C++
Function StartWalkingTo(ObjectReference akTarget)
Function StopWalking(bool abNotify = true)
Function StopWalkingInternal()
Function CheckArrival()
Function ForceStop()  ; external/console entry point

Event OnTimer(int aiTimerID)
Event Actor.OnCombatStateChanged(Actor akSender, Actor akTarget, int aeCombatState)

; Properties filled in Creation Kit
ReferenceAlias DstMarker
ReferenceAlias Traveler
Scene WalkScene
Actor PlayerRef
Idle IdleStop
bool IsWalking       ; conditional/hidden
ObjectReference CurrentTarget
float CheckInterval = 0.25
int TimerArrivalCheck = 1 const
```

## 1.7 Recommandations pour port Skyrim

1. **Déplacer toute la mutation actor dans Papyrus.** Si le C++ actuel appelle `SetActorAIDriven` ou fonctions d'assignment de package directement, c'est la cause racine la plus probable du crash. Miroir du modèle f4access : C++ fait uniquement `DispatchMethodCall`.
2. **Utiliser un Travel package pré-authored derrière un Scene**, avec la cible dans un ReferenceAlias qu'on `ForceRefTo` au start. Pas de package dynamique.
3. **Passer FormIDs en int32**, pas float (les FormIDs Skyrim ont le même problème de précision pour Dawnguard/Dragonborn/mod loads).
4. **`EvaluatePackage` au stop**, après avoir release AI driven, avant de clearer l'alias.
5. **Poll cancel keys via `GetAsyncKeyState` depuis un hook input-event**, pas via InputEvent (AI-driven mode mange les events).
6. **Armer le timer d'arrival dans Papyrus**, pas depuis un C++ thread. Single-shot, ré-armé depuis le handler OnTimer.
7. **Register OnCombatStateChanged comme remote event uniquement pendant walking**, unregister au stop.
8. **Valider `IsDeleted/IsDisabled`** à chaque arrival tick et chaque entry point — la cible peut être pickup, kill, despawn mid-walk.

---

# 2. Rapport exhaustif SkyrimNVDA

## 2.1 Entry points

### C++ user-triggered

- **Keyboard** — `Shift + g_keyAnnounce` (MCM-configurable, default probable `B`) : `plugin.cpp:1744-1748` → `ToggleAutoWalk()`.
- **Gamepad** — `LB + Primary (A/Cross)` : `plugin.cpp:1284-1285` → `ToggleAutoWalk()`. Seulement hors map mode ; map mode réutilise la même combo comme `MapFastTravel()` (`plugin.cpp:1228`).
- **Keyboard fast-travel** — `Enter` dans map menu : `plugin.cpp:1699-1702` → `MapFastTravel()` (dispatch `OnFastTravel` côté Papyrus via `SkyrimTTS_AutoWalk.psc:325-345`).

### C++ programmatique

- `ToggleAutoWalk()` (`autowalk.h:1854-1865`) — wrapper public ; stop si déjà walking, sinon route vers `ToggleAutoWalkImpl()` sur le main thread via `SKSE::GetTaskInterface::AddTask`.
- `StartAutoWalk(FormID, stopDist, x, y, z)` (`autowalk.h:902-1311`) — dispatcher core.
- `StopAutoWalk()` (`autowalk.h:1314-1389`) — stop threads puis dispatch `OnStopWalking`.

### Papyrus entry (invoqué depuis C++ via `DispatchMethodCall`)

- `OnWalkToTarget(aiFormID, afStopDistance, afX, afY, afZ)` — `SkyrimTTS_AutoWalk.psc:27`
- `OnWalkToTargetMounted(...)` — `psc:71` **(déclaré mais JAMAIS appelé depuis C++)**
- `OnStopWalking()` — `psc:170`
- `OnFastTravel(aiFormID)` — `psc:325`
- `OnLoadGameReset()` — `psc:311` (delayed 3s via `AutoWalkSafetyReset`)
- `OnPlaySound / OnPlayLoopSound / OnStopLoopSound` — `psc:353, 374, 394` (helpers son, pas autowalk per se)

## 2.2 Core flow (StartAutoWalk → movement)

1. **Anti-bounce** — `autowalk.h:905-915` : rejette le dispatch si < 500 ms depuis le dernier (`kAutoWalkMinDispatchGapMs`, `g_autoWalkLastDispatchMs`).
2. **State stash** — `g_autoWalkTargetID`, `g_autoWalkStopDist`, `g_autoWalkTargetPos` set (`autowalk.h:920-925`).
3. **Main-thread AddTask** (`autowalk.h:927-1310`) :
   - Pre-start SpeedMult normalisation (`SetActorValue(kSpeedMult, base)`) et forced `SetAIDriven(false)` + `EvaluatePackage` pour clear le résidu.
   - Set char controller flags `kTryStep` + `kCanJump` sur `GetCharController()`.
   - Appelle `FindAutoWalkQuest()` (`autowalk.h:7-66`, tente EditorID, puis `LookupForm` 0x800 sur `SkyrimTTS_AutoWalk.esp`, puis resolve via `compileIndex` / light FE slot).
   - Récupère le VM handle via `ObjectHandlePolicy`.
   - Appelle `AutoWalkRunCrashDiag(..., "PRE-DISPATCH")` (`autowalk.h:541-836`) — dump énorme (player 3D, char controller, process/package, fade node, scene graph children, camera, cell, UI stack, ProcessLists, calendar).
   - `vm->DispatchMethodCall("SkyrimTTS_AutoWalk", "OnWalkToTarget", args)`.
   - Au success : `g_autoWalking=true`, lance `StartAutoWalkMonitor()` et `StartAutoWalkCrashDiagPoll()`.
4. **Papyrus `OnWalkToTarget`** (`psc:27-63`) :
   - FormID ≠ 0 → `Game.GetForm` → cast `ObjectReference` → `StartWalkToRef`.
   - FormID = 0 → `PlayerRef.PlaceAtMe(Game.GetForm(0x10), 1, true, true)` crée un XMarkerHeading → `SetPosition(x,y,z)` → `StartWalkToRef`.
5. **`StartWalkToRef`** (`psc:256-295`) : check early-arrival → `PlayerRef.StopCombat/StopCombatAlarm` → `DstMarker.ForceRefTo(target)` → `Game.SetPlayerAIDriven(true)` → `PlayerRef.EvaluatePackage()` → `IsWalking=true` → `RegisterForSingleUpdate(0.25)`.
6. Le Travel package (attaché via ALPC sur l'alias `Traveler` dans `SkyrimTTS_AutoWalk.esp`) lit l'alias `DstMarker` et drive le joueur.

## 2.3 Monitor / watchdog loop — `StartAutoWalkMonitor` (`autowalk.h:227-532`)

- Dédié `std::jthread g_autoWalkMonitor`, **cadence 250 ms** (`sleep_for(milliseconds(250))`).
- Le jthread ne fait que sleep + `IsMovementInputActive` poll clavier. Toutes les lectures gameplay dispatched sur main thread via `AddTask`.
- **Chaque tick lit** : position joueur, état mount (`IsOnMount`), position cible (via `LookupByID` ou `g_autoWalkTargetPos` caché), running package, char controller state, parent cell.
- **Conditions de pause** (reset `g_autoWalkStuckTimer` + `g_autoWalkLastPos`, return) : `LoadingMenu::MENU_NAME`, `"Fader Menu"`, ou `ui->GameIsPaused()` (`autowalk.h:302-309`).
- **Escalation pour on-foot stuck** (`autowalk.h:488-527`) :
  - **4 s** — gentle : re-apply `kTryStep`/`kCanJump`, `EvaluatePackage` (gated par `g_autoWalkEnableStuckRecovery`).
  - **6 s** — hard : mêmes flags + `SetAIDriven(false) → EvaluatePackage → SetAIDriven(true) → EvaluatePackage`.
  - **10 s** — give up : `Speak("Can't reach target")`, `SetAIDriven(false)`, restore `SpeedMult`, `EvaluatePackage`.
  - Diagnostic à 4 s (once, `s_diagLogged`) : log pos, target cell, running package, path speed, char state (`autowalk.h:400-465`).
- **Mounted stuck** (`autowalk.h:376-385, 468-474`) : track `s_lastDistMounted` ; reset timer uniquement si distance dropped d'au moins 5 u ; timeout 30 s → stop.
- **Arrival** : `dist <= g_autoWalkStopDist + 50.0f` → `Speak("Arrived at " + name)`, reset AIDriven/SpeedMult, ré-oriente yaw/pitch joueur vers target (`autowalk.h:337-373`).

## 2.4 Stop paths

| Trigger | Location |
|---|---|
| Left-stick gamepad pendant autowalk | `plugin.cpp:921-925` |
| Keyboard `W/A/S/D` | `plugin.cpp:1759-1768` |
| Escape / Space (via `IsMovementInputActive`) | `autowalk.h:136-147, 255-274` |
| Arrival (dist ≤ stopDist+50) | `autowalk.h:337-373` |
| C++ stuck 10 s (on-foot) | `autowalk.h:516-527` |
| C++ stuck 30 s (mounted) | `autowalk.h:469-474` |
| Papyrus arrival (`dist ≤ fStopDistance + 20.0`) | `psc:199-205` via `OnUpdate` @ 0.25 s |
| Papyrus target invalid (`IsDeleted`) | `psc:185-189` |
| Player death pendant autowalk | `plugin.cpp:1980-1984` (DeathListener) |
| Blocking menu open (Dialogue / Crafting / Barter / Container / Gift / Training / SleepWait / Book) | `plugin.cpp:239-251` |
| Re-toggling quand déjà walking | `autowalk.h:1856-1860` |
| `OnLoadGameReset` (après crash-in-save) | `autowalk.h:171-224`, `psc:311-322` |
| `OnStopWalking` explicit dispatch | `autowalk.h:1360-1367`, `psc:170-175` |

## 2.5 Safety / cleanup logic

- **Pre-dispatch guards** (`autowalk.h:1870-1896`) :
  - `g_autoWalkUnsafeUntilMs` cooldown → speak "Please wait, game still loading".
  - `LoadingMenu` ou `Fader Menu` open → reject.
- **`AutoWalkSafetyReset()`** (`autowalk.h:171-224`) : step 1 immediate `SetAIDriven(false)` + `SpeedMult` reset ; step 2 fire un **`std::thread` detached** qui sleep 3 s puis `AddTask` → dispatch `OnLoadGameReset`. Appelé depuis `plugin.cpp:2394` à `kPostLoadGame`.
- **`AutoWalkArmSafetyCooldown(ms, reason)`** (`autowalk.h:120-128`) : stocke MAX du courant et du nouveau cooldown (garde la race 5s-écrase-10s).
  - 10 s à `kPostLoadGame` (`plugin.cpp:2398`).
  - 5 s à `LoadingMenu` **close** (`plugin.cpp:407`) — **pas** sur `TESCellFullyLoadedEvent` malgré ce que le commentaire à `autowalk.h:115` dit (aucun tel sink n'existe — voir §8).
- **Rollback flags** (`autowalk.h:80-81`) :
  - `g_autoWalkEnableStuckRecovery` — disable 4 s/6 s recovery si `false`.
  - `g_autoWalkEnableXMarkerDelete` — disable C++ temp-marker `Disable+Delete` dans `StopAutoWalk` (`autowalk.h:1372-1384`).
- **Post-dispatch diag** — `StartAutoWalkCrashDiagPoll` (`autowalk.h:849-887`) : cadence 100 ms les 3 premières s, puis 500 ms, jusqu'à 60 s ; log chaque state via `AutoWalkRunCrashDiag("T+XXXms")`.
- **`StopAutoWalk` cleanup** (`autowalk.h:1314-1389`) : stop les deux jthreads (`request_stop` + `join`), reset `AIDriven`/`SpeedMult`, dispatch `OnStopWalking`, delete C++ temp marker, clear `g_autoWalkTarget`.
- **Papyrus `StopWalkingInternal`** (`psc:208-253`) : `UnregisterForUpdate`, `DstMarker.Clear`, restore alias Traveler si mounted, `SetPlayerAIDriven(false)`, `EvaluatePackage`, delete temp XMarker dont la base est `0x10`.

## 2.6 Papyrus script architecture

- **Quest** : `SkyrimTTS_AutoWalkQuest` (editorID), FormID `0x800` dans `SkyrimTTS_AutoWalk.esp`.
- **Properties** (`psc:4-7`) : `ReferenceAlias DstMarker`, `ReferenceAlias Traveler`, `Scene WalkScene`, `Actor PlayerRef`.
- **Internal state** (`psc:18-22`) : `ObjectReference CurrentTarget`, `float fStopDistance=100`, `bool IsWalking`, `bool MountedMode`, `float CheckInterval=0.25`, `int currentLoopInstance` (son).
- **C++-callable functions** : `OnWalkToTarget`, `OnWalkToTargetMounted`, `OnStopWalking`, `OnLoadGameReset`, `OnFastTravel`, `OnPlaySound`, `OnPlayLoopSound`, `OnStopLoopSound`, `ForceStop`.
- **OnUpdate loop** (`psc:178-205`) : `CheckArrival` toutes les `CheckInterval=0.25 s`, seuil d'arrivée `fStopDistance + 20.0` (côté Papyrus) vs `stopDist + 50.0` (côté C++ — deux seuils différents, race possible).
- **Travel package** : attaché dans `SkyrimTTS_AutoWalk.esp` à l'alias **`Traveler` via ALPC**. Lit l'alias `DstMarker` comme cible pathfinding. Mounted mode (`psc:146-167`) rebind `Traveler` sur l'acteur mount pour que le cheval (qui a l'AI native) exécute le package sans avoir besoin de `SetPlayerAIDriven`.

## 2.7 Pain points connus (depuis commentaires)

- **BSShaderAccumulator crash** (`autowalk.h:100-106, 664-668, 1125-1130, 2274-2278`) : `and [rax+0xF4]` avec `rax=0` pendant passe render/cull ; offset `0xF4` est `NiAVObject::flags`. Stack montre `BSFadeNode "Skeleton.nif" + BSShaderAccumulator + NiCamera`. Trigger par `SetAIDriven` dans la fenêtre fragile après load / cell change pendant que le skeleton est en reconstruction.
- **Save corruption après crash** (`autowalk.h:155-160, psc:304-310`) : un crash pendant autowalk bake `IsWalking=true`, `AIDriven=true`, `DstMarker` filled, `Traveler` pointing at a mount dans la save. `OnLoadGameReset` hard-reset tout ça.
- **Slow walk stuck après stop** (`psc:285-286`) : legacy v1.3.1 bug — forcer `SpeedMult` à 250 causait 2.5× speed ; explicitement **pas** touché maintenant.
- **Target 3D pas loaded** (`autowalk.h:1113, 2287-2293`, `psc:186`) : risque de crash si dispatch FormID hit une ref sans 3D ; géré via fallback coord-mode quand dist > 30000 u.
- **Walled cities routing** (`autowalk.h:2347-2409`) : Whiterun/Solitude/etc sont des sub-worldspaces ; pathfinding s'arrête aux walls, donc C++ scan grid + persistent cells pour une exit door dont le worldspace destination diffère.
- **Sticky `IsInCombat`** (`psc:191-195, 272-278`) : combat flag peut rester true ~30 s après fin du combat, forçant slow walk ; explicitement PAS check et `StopCombat/StopCombatAlarm` sont appelés.
- **Target-cell null quand player interior** — non-FF cases 1/2/3 cascade dans `ToggleAutoWalkImpl` (`autowalk.h:2215-2527`) incluant compass-hop XMarker, NAM0/MNAM validation.

## 2.8 Cosmetic / dead code / red flags

- **Dead code** : énorme bloc `#if 0 … #endif` à `autowalk.h:1004-1285` duplicating `AutoWalkRunCrashDiag` inline. ~280 lignes, devrait être supprimé.
- **Rollback markers** : `autowalk.h:69-82` ("DEBUG FLAGS - Tests de rollback anti-crash BSShaderAccumulator (2026-04-20)") — toujours flaggé TRUE, commentaire mentionne `ROLLBACK test` lignes dans le `.psc` mais **le `.psc` ne contient plus aucun marker `ROLLBACK`** (grep : aucun) — commentaires out-of-date.
- **`OnWalkToTargetMounted`** (`psc:71-117`) est **jamais dispatched depuis C++** (grep de `OnWalkToTargetMounted` dans `src/` renvoie 0 hits). Dead Papyrus entry point — `MountedMode` peut seulement être `true` si un autre mod ou debug path le set. Le monitor a néanmoins une branche mounted complète (`autowalk.h:376-385, 468-474`).
- **Cooldown comment vs réalité** : `autowalk.h:115` dit que le cooldown 5s est armé depuis `TESCellFullyLoadedEvent`, mais seul le close `LoadingMenu` l'arme réellement — grep de `TESCellFullyLoadedEvent` trouve seulement le commentaire.
- **Deux seuils arrivée race** — C++ `stopDist + 50.0` (`autowalk.h:337`) vs Papyrus `fStopDistance + 20.0` (`psc:199`). Celui qui fire d'abord gagne ; les deux dispatch `SetAIDriven(false)` / `EvaluatePackage`, donc si les deux fire dans un tick tu as des resets AI redondants.
- **Race conditions / red flags de complexité** :
  - `std::thread` detached dans `AutoWalkSafetyReset` (`autowalk.h:196-223`) n'a pas de join / cancellation — si le plugin unload en moins de 3 s la task fire contre une VM stale.
  - Deux jthreads (`g_autoWalkMonitor`, `g_autoWalkCrashDiag`) plus Papyrus `RegisterForSingleUpdate` chassent le même state (`IsWalking` / `g_autoWalking` / `CurrentTarget` / `DstMarker`) sans ordering explicite ; `StopAutoWalk` fait confiance à `g_autoWalking.store(false)` + `request_stop` + `join` mais `g_autoWalking` est lu à plein d'endroits sans membar au-delà des defaults `std::atomic`.
  - `IsMovementInputActive` poll global keyboard state avec `GetAsyncKeyState` ; fire même quand la window n'a pas le focus.
  - `ToggleAutoWalkImpl` fait 670+ lignes dans une fonction (`autowalk.h:1867-2529`) avec lambdas nested, trois code paths différents pour lire le compass (deux lectures GFx quasi-dupliquées à `autowalk.h:2009-2065` et `2423-2458`), quatre fallbacks de routing (3.0 walled / 3a near / 3b compass-hop / 3c NAM0 / 3d last-resort).
  - `g_autoWalkTempMarker` ownership split entre C++ (créé par `CreateTempMarkerAt`, deleted dans `StopAutoWalk`) **et** Papyrus (markers créés via `PlaceAtMe` dans `OnWalkToTarget` sont deleted dans `StopWalkingInternal`) — deux deletion paths indépendants, pas de registry partagé.
  - `AutoWalkRunCrashDiag` dump ~100 fields par tick — à cadence 100 ms pendant 3 s ça fait environ 3 000 lignes de log par autowalk start juste pour le diag.
  - Location-routing (`autowalk.h:1485-1849`) reach dans `PlayerCharacter` à des offsets hardcodés `0x580`/`0x588` via `REL::RelocateMemberIfNewer` — casse sur tout runtime hors SE 1.5.97 / AE 1.6.629.

---

# 3. Comparaison et plan de refactor

## 3.1 Executive summary

Les deux plugins implémentent la même feature high-level mais habitent des univers différents en termes de threading, ownership du state, et qui poke les subsystems actor/AI/3D du moteur. **f4access est un thin C++ dispatcher de ~190 lignes** qui délègue chaque morceau de mutation actor (SetPlayerAIDriven, Scene.Start, EvaluatePackage) à un Scene Papyrus pré-authored avec un seul timer self-rearming. **SkyrimNVDA est un monstre de ~2530 lignes** dans lequel C++ appelle directement `player->SetAIDriven()` et `player->EvaluatePackage()` depuis un worker-thread-driven `AddTask`, fait tourner un monitor dupliqué à 250ms, un crash-diag poll à 100ms, un safety thread détaché de 3 secondes, plus une boucle `OnUpdate` Papyrus avec un *second* seuil d'arrivée.

Le crash render-thread `BSShaderAccumulator rax=0 at +0xF4` est **presque certainement produit par les appels C++-initiés `SetAIDriven`/`EvaluatePackage`** qui firent dans le moteur pendant des frames où le 3D/skeleton du joueur est en mid-rebuild (fade, cell load, mount change, save-load). Le refactor collapse SkyrimNVDA à la forme f4access : kill le monitor thread, le crash-diag thread, le safety thread détaché, et chaque call non-read C++ sur le PlayerCharacter ; move toute la mutation actor dans Papyrus derrière un Scene+Travel package pré-authored ; keep seulement la logique location-routing *read-only* en C++ (pour décider *quoi* passer à Papyrus) et le poll cancel `GetAsyncKeyState`.

**Delta estimé** : `-1900 / +150` lignes dans `autowalk.h`, `+80` lignes dans `SkyrimTTS_AutoWalk.psc`, plus un changement ESP (ajouter un `WalkScene` qui start/stop réellement).

## 3.2 Divergences architecturales (rankées par impact sur le crash BSShaderAccumulator)

| # | Aspect | f4access | SkyrimNVDA | Impact |
|---|---|---|---|---|
| 1 | **Qui appelle `SetAIDriven` / `EvaluatePackage`** | Exclusivement Papyrus (`Game.SetPlayerAIDriven`, `PlayerRef.EvaluatePackage`). C++ ne mute jamais le PlayerCharacter. | C++ appelle `player->SetAIDriven` et `player->EvaluatePackage` à **sept** endroits : `StartAutoWalk` pre-flight (autowalk.h:942-943), monitor input cancel (autowalk.h:264-270), monitor target-lost (autowalk.h:327), monitor arrival (autowalk.h:342-348), monitor stuck recovery 4s/6s (autowalk.h:499, 511-514), monitor 10s give-up (autowalk.h:520-526), `StopAutoWalk` (autowalk.h:1335-1343), `AutoWalkSafetyReset` (autowalk.h:179). Plus Papyrus *aussi* le fait dans `StartWalkToRef`/`StopWalkingInternal`/`OnLoadGameReset`. | **HIGH** |
| 2 | **Thread qui initie la mutation actor** | VM (Papyrus) thread — le contexte d'exécution synchronisé du moteur pour l'état actor. | SKSE `TaskInterface` (= main-thread AddTask) drivé par un worker `std::jthread`. Le timing main-thread est correct mais l'*ordering* est faux : AddTask fire quand le main thread drain sa queue, ce qui n'est pas gaté sur des frames render-safe. Un SetAIDriven posté au frame boundary d'une transition fade-in peut tourner entre le tear-down du 3D model et le re-attach. | **HIGH** |
| 3 | **Duplication du seuil d'arrivée** | Single source of truth dans Papyrus (distance ≤ stopDist, dans `TimerArrivalCheck`). C++ ne mesure jamais la distance. | Deux seuils indépendants qui racent : monitor C++ à `stopDist + 50.0` (autowalk.h:337) et Papyrus à `fStopDistance + 20.0` (SkyrimTTS_AutoWalk.psc:199). Celui qui fire d'abord dispatch `SetAIDriven(false) + EvaluatePackage` ; l'autre fire ~25-50ms plus tard contre un actor en mid-cleanup. Double-EvaluatePackage sur un actor en transition est un candidat fort pour la race. | **HIGH** |
| 4 | **Driver du timer** | Papyrus `StartTimer(0.25, TimerArrivalCheck)` ré-armé depuis `OnTimer`. Zéro threads C++. | `std::jthread g_autoWalkMonitor` (autowalk.h:88, 249-531) *et* Papyrus `RegisterForSingleUpdate(0.25)` (.psc:166, 203, 294). Les deux tournent. | **HIGH** |
| 5 | **Scene vs dynamic package** | Papyrus start un Scene pré-authored (`Scene.Start()`) qui owns le lifecycle du Travel package. | `WalkScene Property` existe (SkyrimTTS_AutoWalk.psc:6) mais est **jamais démarré**. À la place Papyrus fill l'alias DstMarker et toggle `Game.SetPlayerAIDriven(true)` directement (StartWalkToRef, .psc:282-290). Pas de Scene lifecycle → rien ne garantit que le Travel package est fully deactivé au stop. | **MEDIUM** |
| 6 | **Stuck recovery** | Aucune. L'AI soit arrive, soit l'utilisateur cancel. | 4-tier escalation (4s gentle / 6s AIDriven toggle / 10s give-up / 30s mounted). Chaque tier fire `SetAIDriven(true)→(false)→(true)` et `EvaluatePackage`. Le hard-toggle 6s à autowalk.h:511-514 est une surface repeated trigger pour le crash. | **MEDIUM** |
| 7 | **Safety reset au load** | Aucun. L'état est éphémère — pas de globals C++ à reset, l'état Papyrus survive aux saves naturellement et re-evaluate quand le package re-tick. | `AutoWalkSafetyReset` (autowalk.h:171-224) : immediate C++ `SetAIDriven(false)` à T=0s de `kPostLoadGame` (fenêtre dangereuse), **plus** un `std::thread` détaché sleeping 3s puis dispatching `OnLoadGameReset`. Le thread détaché n'a pas de `join`, pas de stop token — si le plugin unload pendant la fenêtre 3s, il accède une VM torn-down. | **MEDIUM** |
| 8 | **Pre-dispatch diagnostics** | Aucun. | `AutoWalkRunCrashDiag` dump ~100 fields à PRE-DISPATCH, puis un `std::jthread g_autoWalkCrashDiag` poll toutes les 100ms pendant 3s puis 500ms pendant 60s (autowalk.h:847-887). Chaque dump AddTask'd touche `player->Get3D()`, `body3D->AsFadeNode()->GetRuntimeData()`, itère `body3D->AsNode()->GetChildren()`. Ces reads racent *aussi* avec le render/cull thread. Le diag lui-même peut contribuer à la surface de crash qu'il essaie de diagnostiquer. | **MEDIUM-LOW** |
| 9 | **Temp-marker ownership** | Single-authority XMarker Papyrus. Alias cleared LAST dans stop sequence. | Deux creation paths (`CreateTempMarkerAt` en C++ autowalk.h:1396-1440 et `PlayerRef.PlaceAtMe(Game.GetForm(0x10))` en Papyrus .psc:59). Deux deletion paths (`StopAutoWalk` autowalk.h:1372-1384 et `StopWalkingInternal` .psc:241-248). Pas de registry partagé → un marker créé par C++ peut être deleted par le path Papyrus ou vice versa, ou leaked sur un crash. | **MEDIUM-LOW** |
| 10 | **Ordering de la stop sequence** | Ordre canonique documenté : CancelTimer → Scene.Stop → UnregisterRemoteEvent → SetPlayerAIDriven(false) → EvaluatePackage → DstMarker.Clear → CurrentTarget=None → IsWalking=false. Alias cleared LAST. | Papyrus .psc:209-253 clear DstMarker FIRST, puis SetAIDriven(false), puis EvaluatePackage, puis delete temp marker, puis zero state. Si le Travel package est encore actif quand DstMarker.Clear() fire, le package re-evaluate contre un alias vide avant que AIDriven soit released → état pathing undefined. | **MEDIUM-LOW** |
| 11 | **Type paramètre FormID** | int32 (matches f4access report). | int32 côté Skyrim aussi (autowalk.h:984), donc OK. | NONE |
| 12 | **Cancel-input detection** | `GetAsyncKeyState` poll à chaque event input. | `GetAsyncKeyState` poll toutes les 250ms depuis le jthread monitor (autowalk.h:255) ET hook SKSE input-event à plugin.cpp:1759-1766 (qui call StopAutoWalk). Deux paths peuvent racer. | LOW |
| 13 | **Combat interrupt** | `RegisterForRemoteEvent(PlayerRef, "OnCombatStateChanged")` register uniquement pendant walking. | `StopCombat()` + `StopCombatAlarm()` appelé au start (.psc:278-279) pour clear flag sticky. Pas de listener combat-start — se fie au monitor qui détecte le movement input du joueur attaquant. | LOW |
| 14 | **Dead code** | None reported. | Bloc `#if 0` 280-line à autowalk.h:1004-1285 duplicating AutoWalkRunCrashDiag. `OnWalkToTargetMounted`, `StartWalkToRefMounted` sont dans `.psc` mais jamais dispatched depuis C++ (MountedMode jamais triggered). | NONE (noise, pas crash-relevant) |

## 3.3 Root cause hypothesis

L'instruction crash `and dword ptr [rax+0xF4], ...` avec `rax=0`, stack `BSFadeNode "Skeleton.nif" + BSShaderAccumulator + NiCamera`, offset `0xF4 = NiAVObject::flags`, nous dit : le render thread est dans la phase d'écriture scenegraph-flag de `BSShaderAccumulator::AccumulateShape` (ou culling) quand un parent's child-pointer est soudainement null. Le BSFadeNode dans la stack names `Skeleton.nif` — le skeleton biped du joueur. La cible d'écriture est `NiAVObject::flags` sur un 3D node qui a été detached entre load-time et store-time.

### Hypothèses rankées

**H1 (forte) — C++ SetAIDriven pendant fenêtre de rebuild 3D.** `PlayerCharacter::SetAIDriven` de Skyrim touche finalement l'animation/movement subsystem qui partage ownership de certains node flags (`kSelectiveUpdate`, `kForceUpdate`) sur les enfants skeleton BSFadeNode. Le moteur assume que `SetAIDriven` tourne sur le thread game-tick *entre* animation update et render submission. L'appeler depuis un callback SKSE `AddTask` (qui tourne early dans la main frame, avant animation tick) pendant un fade peut causer l'animation subsystem à swap out un child node pendant que le render thread est mid-accumulator. **f4access ne fait jamais ça** — seul le VM Papyrus le fait, et la VM tourne synchronisée avec le game tick.

**Cheap test** : commenter chaque C++ `player->SetAIDriven(...)` et `player->EvaluatePackage()` call dans `autowalk.h` (lignes 179, 264-270, 327, 342-348, 499, 511-514, 520-526, 942-943, 1335-1343) et remplacer par dispatches Papyrus ou no-ops. Si le crash disparaît sur 2 semaines de playtesting, H1 est confirmé.

**H2 (forte) — Double arrival dispatch.** Quand C++ (à `stopDist+50`) et Papyrus (à `fStopDistance+20`) détectent arrival dans ~250ms l'un de l'autre, deux séquences `SetAIDriven(false) + EvaluatePackage` tournent. La première détruit le Travel package, la deuxième tourne contre un actor idle-package-only, et `EvaluatePackage` pendant un flip ragdoll/fade actif peut toucher le skeleton.

**Cheap test** : retirer le check arrival C++ entièrement (autowalk.h:337-373). Laisser Papyrus être la seule authority d'arrival. Si le crash drop en fréquence mais persiste, H2 est un contributor mais pas la seule cause.

**H3 (moderate) — Crash-diag poll pulling 3D reads dans la mauvaise frame.** `AutoWalkRunCrashDiag` lit `body3D->AsFadeNode()->GetRuntimeData()` et itère `AsNode()->GetChildren()` (autowalk.h:675-709) toutes les 100ms pendant 3 seconds. Ces reads sont lock-free et racent avec le render thread. Ils ne *write* pas les flags, donc ne peuvent pas causer directement `rax=0`, mais ils peuvent observer un torn state et *logger* alors que le render thread pick up le même tear et crash. Le tear *causal* est toujours H1, mais H3 means que le diag tool n'est pas neutral — il ajoute du load et des timing changes.

**Cheap test** : disable `StartAutoWalkCrashDiagPoll` (autowalk.h:1305) et verify que le crash arrive toujours au même rate. Si le rate drop, le diag était partie du trigger.

**H4 (faible) — Double-free temp-marker.** C++ mark `g_autoWalkTempMarker` et le delete dans `StopAutoWalk` ; Papyrus `StartWalkToRef` peut recevoir ce même marker comme `target`, puis `StopWalkingInternal` run le check `baseForm->GetFormID() == 0x10` et re-delete. Un double `Disable()+Delete()` sur un TESObjectREFR qui est actuellement parented dans le scenegraph pourrait null-out un child pointer dans le skeleton *si* le marker avait été attaché via hand animation ou dialogue. Unlikely étant donné que le marker est un XMarker disembodied — il n'est pas parented au skeleton. Toujours, c'est un bug de lifecycle worth cleaning up.

**H5 (faible) — Thread safety détaché accédant VM après plugin unload.** Le `std::thread([]{...}).detach()` à autowalk.h:196-223 causerait un crash différent (heap-use-after-free dans VM) pas un scenegraph `rax=0`. Non lié à BSShaderAccumulator mais c'est un path de save-corruption au plugin swap/reload.

### Root cause le plus probable

**H1.** Les preuves :
1. f4access ne fait zero C++ actor mutation et a zero crashes ; SkyrimNVDA fait sept classes de C++ actor mutation et a le crash.
2. La signature crash implicate spécifiquement `NiAVObject::flags` writes pendant rendering — exactement la mémoire touchée quand `SetAIDriven` cause l'animation subsystem à pause/resume actor locomotion nodes.
3. Le crash est décrit comme rare et correlé avec des "fragile windows" (post-load, post-cell-change), qui est quand le skeleton BSFadeNode est en mid-rebuild. Le cooldown post-load 10s dans `g_autoWalkUnsafeUntilMs` papers over le worst offender mais les *autres* C++ `SetAIDriven` call sites (stuck recovery à 6s, arrival, monitor input cancel) ne sont pas gatés par ce cooldown — ils fire à n'importe quel moment pendant walking.

H2 est la cause second-order : même avec le cooldown, la race arrival double le nombre d'`EvaluatePackage` calls. H1 + H2 ensemble expliquent pourquoi le crash est rare (a besoin de double-dispatch et d'une frame render-unsafe) mais persistent.

## 3.4 Refactor plan (étapes concrètes ordonnées)

Big-bang est tentant mais risky pour un plugin blind-player en production. Le plan est **incremental-first** (étapes 1-4 land indépendamment et sont individuellement revertibles), puis **big-bang** pour la removal de threads (étape 5) parce que le monitor est trop entangled pour partially gut.

### Étape 1 — Delete les deux arrival thresholds, keep only Papyrus arrival

**Files** : `src/autowalk.h` (monitor block autowalk.h:337-373), `autowalk/SkyrimTTS_AutoWalk.psc` (CheckArrival, .psc:184-205).

**Actions** :
- Dans `autowalk.h`, retirer la branche arrival dans le monitor AddTask (le bloc entier `if (dist <= g_autoWalkStopDist + 50.0f)`, lignes 337-373). Laisser la stuck-detection pour l'instant.
- Dans `SkyrimTTS_AutoWalk.psc`, changer le predicate d'arrival de `dist <= fStopDistance + 20.0` à `dist <= fStopDistance` (retirer le buffer +20 — la distance MCM est ce que l'utilisateur a demandé).
- Retirer le state C++ `g_autoWalkStopDist` si rien d'autre ne le lit.

**Add** : rien.
**Migration** : incremental, land comme single commit.
**LOC** : -45 / +2.
**Regression risk** : LOW. Arrival devient légèrement later de ~50 units mais Papyrus est authoritative. Verify que l'arrival TTS fire toujours via le côté Papyrus — on peut need d'ajouter un `Debug.Notification` ou un `SendModEvent` que C++ listen pour l'annonce "Arrived at X" voice (actuellement à autowalk.h:338).

### Étape 2 — Move l'annonce "Arrived at X" TTS vers un event Papyrus → C++

**Files** : `src/autowalk.h` (add un SKSE ModEvent sink pour `SkyrimTTS_AutoWalkArrived`), `autowalk/SkyrimTTS_AutoWalk.psc` (`StopWalkingInternal`).

**Actions** :
- Dans Papyrus `StopWalkingInternal`, quand `abNotify == false` (qui est le path arrival), call `SendModEvent("SkyrimTTS_AutoWalkArrived", CurrentTarget.GetDisplayName())` avant de clearer l'alias.
- Dans C++, register un ModEvent sink via SKSE `GetModCallbackEventSource` qui appelle `Speak(L"Arrived at " + name)`.

**Add** : ~20 lignes C++, 2 lignes Papyrus.
**Migration** : land avec Étape 1 pour que l'arrival s'annonce toujours.
**LOC** : +22 / -0.
**Regression risk** : LOW. ModEvents sont le channel standard Papyrus→SKSE.

### Étape 3 — Delete le crash-diag poll entirely

**Files** : `src/autowalk.h`.

**Actions** :
- Retirer `g_autoWalkCrashDiag` (autowalk.h:847), `StartAutoWalkCrashDiagPoll` (autowalk.h:849-887), call site à autowalk.h:1305, stop call à autowalk.h:1324-1327.
- Retirer `AutoWalkRunCrashDiag` (autowalk.h:541-836) — c'est uniquement appelé par le poll et depuis `PRE-DISPATCH` (autowalk.h:1003). Retirer le PRE-DISPATCH call aussi.
- Retirer le bloc `#if 0` dead à autowalk.h:1004-1285.

**Migration** : land seul. Si le crash rate change (up ou down), on apprend quelque chose sur H3.
**LOC** : -580 / +0.
**Regression risk** : NONE. Pure diagnostic removal.

### Étape 4 — Consolidate le path temp-marker : Papyrus owns it

**Files** : `src/autowalk.h` (removal of `CreateTempMarkerAt`, `g_autoWalkTempMarker`), `autowalk/SkyrimTTS_AutoWalk.psc` (a déjà le path `PlaceAtMe`).

**Actions** :
- Delete `CreateTempMarkerAt` (autowalk.h:1396-1440), `g_autoWalkTempMarker` (autowalk.h:153), tous callers (autowalk.h:2173, 2466-2478, 1372-1384 deletion block).
- Dans `ToggleAutoWalkImpl`, chaque call site qui fait actuellement `CreateTempMarkerAt(pos) + StartAutoWalk(markerID, 150, x, y, z)` devient juste `StartAutoWalk(0, 150, x, y, z)` — pass FormID=0 et coords, le Papyrus `OnWalkToTarget` crée déjà un XMarker via `PlaceAtMe` dans la branche coords (.psc:59).

**Migration** : incremental. Papyrus support déjà coords mode ; le path marker C++ était un duplicate.
**LOC** : -90 / +5.
**Regression risk** : LOW. Le XMarker Papyrus via `PlaceAtMe` est ce que le Creation Kit officially support pour destinations dynamiques. Le `CreateTempMarkerAt` C++ use `IFormFactory::Create()` + manual `Load3D(false)` qui est fragile.

### Étape 5 — Big-bang : replace le jthread monitor avec un Papyrus timer + un SKSE input hook

**Files** : `src/autowalk.h` (massive), `src/plugin.cpp` (input cancel hook), `autowalk/SkyrimTTS_AutoWalk.psc` (a déjà `OnUpdate`, extend it).

**Actions** :
- **Remove** : `g_autoWalkMonitor` (autowalk.h:88), `StartAutoWalkMonitor` (autowalk.h:227-532) en entirety, incluant tous les C++ `SetAIDriven`/`EvaluatePackage` embedded. La stuck recovery cascade part avec.
- **Remove** : les globals stuck-detection (`g_autoWalkLastPos`, `g_autoWalkStuckTimer`, `g_autoWalkHasRepathed` à autowalk.h:131-133).
- **Replace input cancel** : Papyrus-side `OnUpdate` tick déjà toutes les 0.25s. Add à `CheckArrival` : si C++ signal cancel via un mod event `SkyrimTTS_AutoWalkCancel`, call `StopWalkingInternal(true)`. Dans `src/plugin.cpp:1759-1766` keep l'existing `InputEvent` poll qui call `StopAutoWalk` (celui-là est safe — il dispatch uniquement une méthode Papyrus, pas de mutation actor C++). Delete le `GetAsyncKeyState` poll dupliqué inside le monitor thread.
- **Keep `IsMovementInputActive()`** — call depuis le SKSE `InputEventSink` directly (déjà à plugin.cpp input path), pas depuis un background thread. Dispatch `OnStopWalking` au match.
- **Keep `g_autoWalking` atomic** — C++ need toujours savoir "est-ce qu'un autowalk est en progress" pour le gating du poll input GetAsyncKeyState et pour le gamepad stick-cancel à plugin.cpp:920-924.

**Migration** : big-bang within autowalk.h. Must land comme un single coherent commit parce que le monitor touche plein de functions.
**LOC** : -310 / +20.
**Regression risk** : MEDIUM. Stuck recovery is gone — players dans dungeons bloqués vont devoir cancel manually et retry. C'est le contrat f4access et acceptable ; le crash rare est worse qu'un autowalk stuck rare.

### Étape 6 — Move toute la remaining mutation actor C++ dans Papyrus

**Files** : `src/autowalk.h` (StartAutoWalk pre-flight, StopAutoWalk, AutoWalkSafetyReset).

**Actions** :
- **`StartAutoWalk` (autowalk.h:902-1311)** : delete le pre-flight block à lignes 928-952 (SpeedMult fix, SetAIDriven(false), EvaluatePackage, kTryStep/kCanJump flags). Move le SpeedMult reset dans Papyrus `StartWalkToRef` (une ligne extra). Delete les kTryStep/kCanJump — f4access ne les a pas ; si les escaliers break, add them to Papyrus via un new SKSE native ou live without.
- **`StopAutoWalk` (autowalk.h:1314-1389)** : delete le AddTask block à lignes 1332-1344 (SetAIDriven, SpeedMult reset, EvaluatePackage). Le Papyrus `OnStopWalking` fait déjà tout ça. La fonction réduit à : set `g_autoWalking.store(false)`, dispatch `OnStopWalking` to Papyrus, done.
- **`AutoWalkSafetyReset` (autowalk.h:171-224)** : delete Step 1 (immediate C++ reset à lignes 176-191). Keep seulement le dispatch 3s-delayed de `OnLoadGameReset`, **mais** replace le `std::thread` détaché avec un SKSE `RegisterForUpdate` ou un proper `std::jthread` avec un stop token tracked pour plugin-unload. Ou simpler : move le 3-second wait dans Papyrus (`RegisterForSingleUpdate(3.0)` inside `OnInit` after detecting un dirty state) et avoir C++ just dispatch `OnLoadGameReset` immediately at kPostLoadGame, laissant Papyrus self-delay.

**Add** : `Function OnLoadGameReset` existe déjà (.psc:311-322). Add to it : `Utility.Wait(3.0)` au top *ou* use un pattern `RegisterForSingleUpdate(3.0)`. Le cleaner move est : C++ call un new `OnLoadGameResetDeferred` qui schedule le real reset via timer Papyrus.
**Migration** : incremental (land après Étape 5).
**LOC** : -80 / +15.
**Regression risk** : MEDIUM. L'immediate Step-1 reset était "belt and braces" — si un dirty save load avec AIDriven=true, le propre `OnInit` de Papyrus plus `OnLoadGameReset` doivent cover it. Test spécifiquement avec une save qui était force-killed mid-autowalk.

### Étape 7 — Add un vrai Scene.Start / Scene.Stop lifecycle

**Files** : `autowalk/SkyrimTTS_AutoWalk.psc`, et l'ESP (`SkyrimTTS_AutoWalk.esp`) dans Creation Kit.

**Actions** :
- Dans Creation Kit : sur `WalkScene`, author un Scene qui owns le Travel package via une Phase avec une actor-alias action targeting `Traveler`. Pattern décrit dans le rapport f4access.
- Dans `StartWalkToRef` (.psc:256-295) : replace le pair `Game.SetPlayerAIDriven(true) + PlayerRef.EvaluatePackage()` avec `WalkScene.Start()`. Keep `DstMarker.ForceRefTo(target)` (le Scene's Travel phase lit l'alias).
- Dans `StopWalkingInternal` (.psc:208-253) : replace `Game.SetPlayerAIDriven(false) + PlayerRef.EvaluatePackage()` avec `WalkScene.Stop()`. Reorder per f4access : `CancelTimer → WalkScene.Stop() → Game.SetPlayerAIDriven(false) → EvaluatePackage → DstMarker.Clear → CurrentTarget=None → IsWalking=false`. Note : même avec un Scene, `SetPlayerAIDriven(false)` est toujours appelé comme belt-and-braces, vu que le system Scene Skyrim peut ne pas *toujours* release AIDriven (une différence quirk-Papyrus vs FO4).

**Add** : ~15 lignes Papyrus ; 1 Scene asset dans ESP.
**Migration** : big-bang pour ce file. ESP change require Creation Kit — donner à l'utilisateur des instructions claires plutôt que tenter automated ESP patching.
**LOC** : +15 Papyrus / -4 Papyrus.
**Regression risk** : MEDIUM. Scene authoring est error-prone. Test : Scene doit restart cleanly si le joueur trigger deux autowalks back-to-back (le re-entry check `if IsWalking: StopWalkingInternal(false)` cover ça).

### Étape 8 — Handle location-based routing (keep read-only, move dispatch to Papyrus)

**Files** : `src/autowalk.h` (keep the C++ routing helpers read-only, use only for routing *decision*).

**Actions** :
- La routing logic à autowalk.h:1485-2527 (ResolveActiveQuestTargetRef, FindDungeonRootLocation, FindDungeonEntranceRef, TryFindQuestDoorByLocation, la cascade 4-fallback dans ToggleAutoWalkImpl) est toute **read-only** — elle compute *which FormID to dispatch*, pas mutating the player. **Keep it in C++.**
- La seule mutation dans ce block est `CreateTempMarkerAt` — déjà removed dans Étape 4. Après Étape 4, chaque path finit dans `StartAutoWalk(formID, dist, x, y, z)` qui est un pure Papyrus dispatch.

**Add** : rien.
**Migration** : no-op une fois Étapes 1-6 landed.
**LOC** : 0.
**Regression risk** : NONE.

### Étape 9 — GetAsyncKeyState vs InputEvent pour cancel

**Decision** : keep both paths, but each in its correct place.
- **GetAsyncKeyState poll** : le pattern f4access est "poll à chaque input event." Dans Skyrim, l'equivalent est : dans le `InputEventSink::ProcessEvent` de `src/plugin.cpp` (autour de plugin.cpp:1759), check `g_autoWalking.load()` et si true, call `IsMovementInputActive()` sur WASD/Space/Esc/gamepad stick. Si any pressed, dispatch `OnStopWalking`. C'est déjà partiellement present. Pas de background thread needed.
- **Delete le 250ms GetAsyncKeyState poll** inside le monitor (autowalk.h:255-274) — il part avec Étape 5 quand le monitor est removed.
- Pour gamepad stick cancel à plugin.cpp:920-924, keep as-is.

**Migration** : part of Étape 5.
**LOC** : already counted in Étape 5.

### Summary des changements

| File | LOC removed | LOC added |
|---|---|---|
| `src/autowalk.h` | ~1900 | ~50 |
| `src/plugin.cpp` | ~5 | ~25 (input cancel hook + ModEvent sink) |
| `autowalk/SkyrimTTS_AutoWalk.psc` | ~10 | ~80 (Scene.Start/Stop, cancel ModEvent handler, arrival ModEvent, deferred load reset) |
| ESP (Creation Kit) | — | 1 Scene authoring |

**Final `autowalk.h` shape** : `FindAutoWalkQuest`, `StartAutoWalk` (pure dispatch, ~30 lines), `StopAutoWalk` (pure dispatch, ~20 lines), `ToggleAutoWalkImpl` (les ~670 lines de location routing stay), ModEvent sink (~20 lines), `AutoWalkArmSafetyCooldown` + `OnLoadGameReset` dispatch (~30 lines). Target : **~800 lines, down from 2531**.

## 3.5 Ce qu'on garde de SkyrimNVDA

### 1. Location-based quest routing (`ResolveActiveQuestTargetRef`, `TryFindQuestDoorByLocation`, dungeon-hierarchy walker)

Located at autowalk.h:1485-1820. C'est **strictly better que f4access** — il lit `BGSInstancedQuestObjective` + `BGSLocation::specialRefs` + `parentLoc` pour trouver la correct dungeon door plutôt que guessing par compass angle. Les quêtes FO4 sont usually single-worldspace donc f4access ne need pas ça, mais les dungeons Skyrim sont des labyrinthes multi-cell où ça est essential.

**Survit au refactor** unchanged — c'est pure read-only engine introspection feeding FormIDs dans `StartAutoWalk(formID, ...)`. Après Étape 4 retire `CreateTempMarkerAt`, l'output du code routing feed le coords-mode XMarker creation path de Papyrus, qui est stable.

### 2. MCM-configurable stop distance

Lives in `SkyrimTTS_MCM.psc` / `SkyrimTTS_MCM_Native.psc`. Le côté C++ read from MCM et pass to `StartAutoWalk(stopDistance=...)`. f4access hardcode 150 units ; SkyrimNVDA let l'utilisateur set per-category distances (par exemple closer pour NPCs, further pour landmarks extérieurs).

**Survit** — le parameter `stopDistance` est déjà la single source of truth après Étape 1. Pas de change needed au-delà de retirer le buffer `+20` dans le CheckArrival Papyrus.

### 3. Scanner integration avec auto-target (`ScannerGetCurrentFormID`, `ScannerGetCurrentName`, `g_scannedFiltered`)

autowalk.h:1906-1913 + integration scanner.h. Le scanner build une liste catégorisée ordonnée de nearby things (NPCs, items, doors, quest targets) et autowalk pick `g_scanIndex` automatically ou l'utilisateur cycle. f4access n'a pas d'equivalent — il require un external "look at this" mechanism.

**Survit** unchanged — scanner est pure-read, feed un FormID dans dispatch.

### 4. Mounted support (`OnWalkToTargetMounted`, `StartWalkToRefMounted`)

Actuellement dead code (.psc:71-167) — jamais dispatched depuis C++. Le design est conceptually sound : redirect l'alias `Traveler` vers le mount actor au lieu du joueur, let l'AI native du mount execute le Travel package, pas de `SetPlayerAIDriven` needed. Ça *evite* le crash H1 entirely en mode cheval.

**Survit** — durant le refactor, wire up le dispatch C++ : `ToggleAutoWalkImpl` check `player->IsOnMount()` (autowalk.h:311 déjà le fait pour d'autres purposes), fetch le mount actor, et dispatch `OnWalkToTargetMounted` au lieu de `OnWalkToTarget`. ~15 lignes de C++. Le path mounted est *stable* parce que le mount a sa propre AI et il n'y a pas de call SetAIDriven sur le joueur. **Keep ça dans le refactor — c'est une strict improvement sur f4access.**

### 5. Map fast-travel via autowalk dispatcher (`OnFastTravel`)

.psc:325-345, dispatched depuis `src/menu_map.h:970`. Unified entry point : quand l'utilisateur select un map marker dans le TTS-driven map menu, il dispatch `OnFastTravel` au même quest script, qui call `Game.FastTravel(targetRef)`. Cleaner que f4access qui a map travel comme un separate code path.

**Survit** unchanged — pure dispatch Papyrus, pas de mutation actor C++.

### 6. Sound playback dispatcher (`OnPlaySound`, `OnPlayLoopSound`, `OnStopLoopSound`)

.psc:353-400 + common.h:1078-1083. Unified Sound FormID dispatch pour TTS feedback beeps (aim, confirm, error). Nice pattern — keep it.

**Survit** unchanged.

### 7. Translation infrastructure (localisation française via `translations/`)

Les calls `Speak(L"...")` throughout sont des strings qui passent through le translation layer. Keep every `Speak` call site's string identical pendant le refactor — les translations sont keyed off le exact C++ string literal.

**Survit** en ne changeant pas les string literals pendant le refactor. Les quatre `Speak` calls removed (messages de stuck recovery du monitor) sont English-only et n'affectent pas la localisation.

### 8. Death listener / menu listener pour auto-stop

plugin.cpp:1976-1983 (DeathListener) et plugin.cpp:236-249 (MenuListener stopping autowalk on Dialogue/Crafting/Barter/Container/Gift/Training/SleepWait/Book). Ceux-là catch les game states autowalk-incompatible qui laisseraient AIDriven stuck.

**Survit** — les deux use déjà `StopAutoWalk()` qui après Étape 6 est un pure dispatch Papyrus. Pas de changes.

### 9. Post-load / post-cell-change safety cooldown (`g_autoWalkUnsafeUntilMs`, `AutoWalkArmSafetyCooldown`)

autowalk.h:107-128, armé à plugin.cpp:407 (cell change) et plugin.cpp:2398 (kPostLoadGame). Block autowalk starts pendant la fenêtre de rebuild skeleton du moteur. **Même après le refactor, keep ça.** C'est cheap (un atomic int64 compare) et correctly prevent le dispatch Papyrus de fire dans un scenegraph still-rebuilding. f4access ne l'a pas parce que le pipeline de load FO4 est different ; la fenêtre BSFadeNode fade-in Skyrim est un hazard connu.

**Survit** unchanged.

### Critical Files for Implementation

- `c:\Users\marcd\source\repos\SkyrimNVDA\src\autowalk.h`
- `c:\Users\marcd\source\repos\SkyrimNVDA\autowalk\SkyrimTTS_AutoWalk.psc`
- `c:\Users\marcd\source\repos\SkyrimNVDA\src\plugin.cpp`
- `c:\Users\marcd\source\repos\SkyrimNVDA\autowalk\SkyrimTTS_AutoWalk.esp` (Creation Kit : add a real WalkScene with Travel-package phase bound to the Traveler alias)
- `c:\Users\marcd\source\repos\SkyrimNVDA\src\menu_map.h` (verify `OnFastTravel` dispatch is unaffected)

---

# 4. Synthèse finale et recommandations

## 4.1 Les différences en chiffres

### Taille du code autowalk
- **f4access** : 187 lignes C++ + 240 lignes Papyrus = ~430 lignes total
- **SkyrimNVDA** : 2531 lignes C++ + 440 lignes Papyrus = **~2970 lignes total, 7 fois plus gros**

### Philosophie radicalement différente

**f4access (qui marche)** :
- C++ = **simple dispatcher** qui fait UN appel `DispatchMethodCall` et c'est tout
- Toute la logique moteur est dans Papyrus
- **AUCUN** `SetAIDriven()` ni `EvaluatePackage()` en C++
- **AUCUN** thread C++, **AUCUN** atomic, **AUCUN** timer C++
- Papyrus utilise un Scene pré-authored qui start/stop le Travel package via son cycle de vie
- Cancel détecté par `GetAsyncKeyState` **dans le callback input-event** (pas dans un thread)

**SkyrimNVDA (qui crashe)** :
- C++ = **monstre de 2500 lignes** qui orchestre tout
- `SetAIDriven()` appelé en C++ à **7 endroits différents**
- **2 jthread** + **1 std::thread detached** (sans join !)
- Scene déclaré dans Papyrus (`WalkScene Property`) mais **jamais utilisé** — on préfère `SetPlayerAIDriven(true)` direct
- **Deux seuils d'arrivée** qui se font la course (C++ +50u vs Papyrus +20u)
- **280 lignes de code mort** dans un `#if 0`
- `OnWalkToTargetMounted` défini en Papyrus mais **jamais appelé** depuis C++

## 4.2 Root cause probable du crash BSShaderAccumulator

**Hypothèse H1 (forte)** : **C++ appelle `SetAIDriven` pendant que le squelette du joueur est en reconstruction** (fade-in après load, cell change, montage cheval).

Preuves :
1. f4access ne fait **jamais** ça → pas de crash
2. Le crash `rax=0 à +0xF4` pointe vers `NiAVObject::flags` — exactement la mémoire que touche le subsystem animation quand `SetAIDriven` pause/resume les nodes de locomotion
3. Le moteur suppose que `SetAIDriven` tourne **sur le game-tick thread entre la passe animation et la soumission render**. Nous l'appelons depuis `AddTask` qui s'exécute **plus tôt** dans la frame, potentiellement **pendant** la passe render d'un autre thread

**Hypothèse H2 (contribuante)** : **Double dispatch d'arrivée** — quand le C++ et le Papyrus détectent l'arrivée à ~250ms d'écart, deux séquences `SetAIDriven(false) + EvaluatePackage` tournent, la deuxième sur un actor en pleine transition

## 4.3 Plan de refactor en 9 étapes (du plus simple au plus risqué)

1. **Supprimer le seuil d'arrivée C++** (-45 lignes) — Papyrus seule autorité
2. **Remplacer l'annonce "Arrived" par un ModEvent** Papyrus → C++ (+22 lignes)
3. **Supprimer tout le crash-diag poll** (-580 lignes) — pur diagnostic inutile après refactor
4. **Unifier le temp-marker : Papyrus seul propriétaire** (-90 lignes)
5. **Big-bang : supprimer le jthread monitor + stuck recovery** (-310 lignes / +20)
6. **Déplacer tous les `SetAIDriven` C++ dans Papyrus** (-80 / +15)
7. **Ajouter un vrai Scene.Start/Stop** (Creation Kit requis, +15 Papyrus)
8. **Location routing : rien à changer** (déjà read-only)
9. **Cancel par InputEventSink uniquement** (intégré dans étape 5)

**Résultat** : `autowalk.h` passe de **2531 à ~800 lignes** (−68%). Le Papyrus gonfle un peu (+80 lignes) mais contient la logique qui doit y être.

## 4.4 Ce qu'on garde et qui est mieux qu'f4access

1. **Location-based routing** (dungeons multi-cellules) — f4access n'en a pas besoin, FO4 a rarement des donjons multi-worldspace
2. **Scanner auto-target** — f4access n'a pas d'équivalent
3. **MCM stop distance configurable**
4. **Support mounted (à activer)** — si on dispatch vraiment `OnWalkToTargetMounted`, le crash H1 disparaît en mode cheval (pas de SetAIDriven sur le player)
5. **Map fast-travel unifié** via le même quest script
6. **Safety cooldown post-load** — f4access ne l'a pas car FO4 a un autre pipeline de chargement

## 4.5 Recommandation finale

**Le crash H1 est quasi-certain.** f4access nous donne la preuve empirique : architecture identique en concept, mais **sans les appels C++ actor mutation**, donc **pas de crash**.

Ordre d'attaque recommandé :

- **Étape 3 d'abord** (suppression crash-diag, -580 lignes, risque zéro) pour gagner en lisibilité
- **Étape 1+2 ensuite** (suppression double seuil d'arrivée, -45/+22 lignes) pour éliminer H2
- **Puis étape 5+6** (gros morceau : enlever les SetAIDriven C++) pour tuer H1
- Les étapes 4, 7, 8, 9 suivent naturellement

Chaque étape est **indépendamment testable** (sauf 5 qui doit partir en big-bang vu l'entanglement).

---

*Document généré le 2026-04-20 par synthèse de trois agents d'analyse (Explore, Explore, Plan) sur les bases de code f4access et SkyrimNVDA. À conserver pour consultation ultérieure avant lancement du refactor.*
