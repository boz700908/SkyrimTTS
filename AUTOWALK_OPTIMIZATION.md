# Autowalk Optimization Guide

Toutes les options disponibles pour améliorer le comportement de l'autowalk (escaliers, pentes, terrain difficile) sans faire un pathfinding custom.

## 1. bhkCharacterController — Flags physiques Havok (PRIORITÉ HAUTE)

Le flag `kTryStep` est LA cause principale des blocages dans les escaliers. Il permet au personnage de monter les marches automatiquement.

### Accès en C++
```cpp
auto* charCtrl = player->GetCharController();
if (charCtrl) {
    charCtrl->flags.set(RE::CHARACTER_FLAGS::kTryStep);   // monter les marches
    charCtrl->flags.set(RE::CHARACTER_FLAGS::kCanJump);   // autoriser le saut
}
```

### Flags disponibles (RE/B/bhkCharacterController.h)
| Flag | Bit | Effet |
|------|-----|-------|
| `kTryStep` | 1 << 2 | **CRUCIAL** — Monte les marches automatiquement |
| `kCanJump` | 1 << 10 | Autorise le saut |
| `kAllowJumpNoContact` | 1 << 4 | Autorise le saut sans contact au sol |
| `kNoGravityOnGround` | 1 << 1 | Pas de gravité au sol (évite de glisser sur les pentes) |
| `kNotPushable` | 1 << 14 | Ne peut pas être poussé par des PNJ/objets |
| `kOnStairs` | 1 << 24 | **Lecture seule** — indique qu'on est sur un escalier |
| `kSwimAtWaterSurface` | 1 << 31 | Nage à la surface |

### Détection de pente/glissade
```cpp
// hkpSurfaceInfo::SupportedState
// kUnsupported = 0 (en l'air)
// kSliding = 1 (glisse sur une pente)
// kSupported = 2 (sol normal)
auto state = charCtrl->surfaceInfo.supportedState;
bool onStairs = charCtrl->flags.any(RE::CHARACTER_FLAGS::kOnStairs);
```

### État Havok du personnage (hkpCharacterContext)
```cpp
auto state = charCtrl->context.currentState;
// 0 = kOnGround, 1 = kJumping, 2 = kInAir, 3 = kClimbing, 4 = kFlying, 5 = kSwimming
```

---

## 2. PACKAGE_DATA — Flags du package Travel (PRIORITÉ HAUTE)

### Comment modifier le package actif en C++
```cpp
auto* process = player->currentProcess;
if (process) {
    auto* pkg = process->GetRunningPackage();
    if (pkg) {
        pkg->packData.packFlags.set(
            RE::PACKAGE_DATA::GeneralFlag::kAllowSwimming,       // traverser l'eau
            RE::PACKAGE_DATA::GeneralFlag::kMaintainSpeedAtGoal,  // pas de ralentissement
            RE::PACKAGE_DATA::GeneralFlag::kUnlocksDoorsAtPackageStart, // ouvrir les portes
            RE::PACKAGE_DATA::GeneralFlag::kMustComplete,         // ne pas interrompre
            RE::PACKAGE_DATA::GeneralFlag::kIgnoreCombat          // ignorer le combat
        );
        // Forcer la course
        pkg->packData.packFlags.set(RE::PACKAGE_DATA::GeneralFlag::kPreferredSpeed);
        pkg->packData.maxSpeed = RE::PACKAGE_DATA::PreferredSpeed::kRun;
    }
}
```

### Flags disponibles (RE/T/TESPackage.h, lignes 95-148)
| Flag | Effet |
|------|-------|
| `kAllowSwimming` (1 << 18) | Autorise la nage — résout les blocages près de l'eau |
| `kMaintainSpeedAtGoal` (1 << 3) | Maintient la vitesse même en approchant de la destination |
| `kUnlocksDoorsAtPackageStart` (1 << 6) | Ouvre les portes au début |
| `kUnlocksDoorsAtPackageEnd` (1 << 7) | Ouvre les portes à la fin |
| `kContinueIfPCNear` (1 << 9) | Continue même si le joueur est proche |
| `kPreferredSpeed` (1 << 13) | Active le champ maxSpeed |
| `kMustComplete` (1 << 2) | Le package ne peut pas être interrompu |
| `kIgnoreCombat` (1 << 20) | Ignore le combat pendant le déplacement |
| `kNoCombatAlert` (1 << 27) | Pas d'alerte de combat |
| `kAlwaysSneak` (1 << 17) | Force le sneak |

### PreferredSpeed (quand kPreferredSpeed est actif)
| Valeur | Constante |
|--------|-----------|
| 0 | `kWalk` |
| 1 | `kJog` |
| 2 | `kRun` |
| 3 | `kFastWalk` |

---

## 3. Récupération de blocage — Forcer un saut (PRIORITÉ MOYENNE)

Quand le joueur est bloqué, au lieu d'abandonner, essayer un saut :

```cpp
// Via animation graph
player->NotifyAnimationGraph("JumpStandingStart");

// S'assurer que le saut est autorisé
auto* charCtrl = player->GetCharController();
if (charCtrl) {
    charCtrl->flags.set(RE::CHARACTER_FLAGS::kCanJump);
}
```

### Autres événements d'animation utiles
```cpp
player->NotifyAnimationGraph("sprintStart");  // forcer le sprint
player->NotifyAnimationGraph("sprintStop");
```

---

## 4. Détection de blocage améliorée (PRIORITÉ MOYENNE)

### Via HighProcessData (RE/H/HighProcessData.h)
```cpp
auto* high = player->currentProcess->high;
if (high) {
    float desiredSpeed = high->pathingDesiredMovementSpeed.Length();
    float actualSpeed = high->pathingCurrentMovementSpeed.Length();
    if (desiredSpeed > 0.1f && actualSpeed < 0.1f) {
        // L'IA veut bouger mais n'y arrive pas = BLOQUÉE
    }
}
```

Champs disponibles :
- `pathingCurrentMovementSpeed` (NiPoint3)
- `pathingCurrentRotationSpeed` (NiPoint3)
- `pathingDesiredPosition` (NiPoint3)
- `pathingDesiredOrientation` (NiPoint3)
- `pathingDesiredMovementSpeed` (NiPoint3)
- `pathingDesiredRotationSpeed` (NiPoint3)

### Via CachedValues (RE/A/AIProcess.h)
```cpp
auto* cache = player->currentProcess->cachedValues;
if (cache) {
    float runSpeed = cache->cachedRunSpeed;
    bool cantRun = cache->booleanValues.any(
        RE::CachedValues::BooleanValue::kConditionPreventsRun);
}
```

---

## 5. Re-évaluation du package en cas de blocage (PRIORITÉ MOYENNE)

Quand le joueur est bloqué, recalculer le chemin :

```cpp
player->SetAIDriven(false);
player->EvaluatePackage();
// petit délai (1-2 frames)
player->SetAIDriven(true);
player->EvaluatePackage();
```

---

## 6. Animation Graph Variables (PRIORITÉ BASSE)

### Lire/écrire des variables d'animation
```cpp
player->SetGraphVariableFloat("Speed", 100.0f);
player->SetGraphVariableFloat("Direction", 0.0f);   // 0 = avant
player->SetGraphVariableBool("bAllowRotation", true);

float speed;
player->GetGraphVariableFloat("Speed", speed);
```

Variables de mouvement :
- `Speed` — vitesse de l'animation
- `SpeedMult` — multiplicateur
- `Direction` — 0=avant, 1=droite, 2=arrière, 3=gauche
- `IsRunning` — en train de courir
- `IsSprinting` — en train de sprinter

---

## 7. ActorValues liés au mouvement

| ActorValue | Index | Effet |
|-----------|-------|-------|
| `kSpeedMult` | 30 | Multiplicateur de vitesse global |
| `kJumpingBonus` | 62 | Bonus de saut |
| `kCarryWeight` | 32 | Poids transportable (surcharge = lent) |

---

## 8. PackageLocation — Rayon de destination

Le champ `rad` sur PackageLocation contrôle le rayon de la destination. Un rayon plus grand aide l'IA à trouver un chemin plus facilement.

---

## Plan d'implémentation recommandé

### Étape 1 — Flags de base (impact immédiat)
Au lancement de l'autowalk, dans le code C++ après le `SetAIDriven(true)` :
1. Activer `kTryStep` sur le bhkCharacterController
2. Activer les flags du package (swim, doors, run, combat)
3. Remettre ces flags à l'arrêt

### Étape 2 — Récupération intelligente
Dans le monitor de l'autowalk (le thread de polling) :
1. Détecter le blocage via `pathingDesiredMovementSpeed` vs `pathingCurrentMovementSpeed`
2. Si bloqué < 3 secondes : essayer un saut (`NotifyAnimationGraph`)
3. Si bloqué < 6 secondes : réévaluer le package
4. Si bloqué > 10 secondes : abandonner

### Étape 3 — Feedback audio
Annoncer "Stairs" quand `kOnStairs` est détecté, "Sliding" quand `kSliding`.

### Important
Ces modifications sont côté C++ (autowalk.h), pas Papyrus. Le Papyrus ne peut pas accéder à ces API Havok/Package.

### Note sur la compilation
Le script Papyrus `SkyrimTTS_AutoWalk.psc` doit être compilé avec le projet SkyrimCK-MCP car notre compilateur local produit un .pex incompatible.
