# Système AutoWalk — Spécification pour Skyrim

Basé sur l'implémentation de Fallout 4 Access. Ce document décrit comment reproduire le système d'autowalk pour Skyrim SE/AE.

---

## Vue d'ensemble

Le système autowalk a deux parties principales :
1. **Scanner d'objets** (plugin C++ SKSE) — Scanne les objets proches, permet de les parcourir par catégorie, annonce le nom + la distance
2. **AutoWalk** (Papyrus + Creation Kit) — Déplace le joueur automatiquement vers une cible sélectionnée en utilisant le pathfinding du jeu (navmesh)

Le plugin C++ gère le scan, les touches et les annonces vocales. Le Creation Kit fournit la structure de quête qui active le déplacement automatique. Les scripts Papyrus font le lien entre les deux.

---

## Partie 1 : Scanner d'objets (côté C++)

### Catégories

Le scanner organise les objets proches en catégories. L'utilisateur navigue entre elles.

| Catégorie | Ce qui est scanné | Equivalent Skyrim |
|-----------|-------------------|-------------------|
| Tout | Tout ce qui est proche | Pareil |
| PNJ | Acteurs vivants | Pareil |
| Portes | Références de portes | Pareil |
| Conteneurs | Coffres, tonneaux, etc. | Pareil |
| Objets | Armes, armures, potions, livres, clés, ingrédients, divers | Pareil |
| Activateurs | Meubles, stations d'artisanat | Forges, tables d'enchantement, laboratoires d'alchimie |
| Marqueurs de quête | Objectifs de quête actifs | Pareil (utiliser les données de la boussole) |
| Cadavres | Acteurs morts | Pareil |
| Lieux | Marqueurs de carte découverts sur la boussole | Pareil |
| Compagnons | Acteurs qui nous suivent | Pareil (vérifier IsPlayerTeammate()) |

**Catégories spécifiques à FO4 NON nécessaires pour Skyrim :**
- Armure assistée (n'existe pas dans Skyrim)
- Atelier/Colonies (n'existe pas dans Skyrim)

### Sous-catégories

Certaines catégories ont des sous-catégories que l'utilisateur peut parcourir avec une touche dédiée :

| Catégorie | Sous-catégorie A | Sous-catégorie B |
|-----------|-----------------|-----------------|
| Conteneurs | Non vides | Vides |
| Portes | Verrouillées | Portes de cellule (mènent vers d'autres zones) |
| Cadavres | Non pillés | Pillés |
| Activateurs | Meubles | Autres |

### Touches

Voici les touches utilisées dans FO4 Access. À adapter si besoin pour Skyrim.

| Touche | Action |
|--------|--------|
| **Pavé numérique 5** | Scanner : cherche les objets proches dans la catégorie actuelle, annonce le nombre + le plus proche |
| **Page Bas** | Objet suivant dans la catégorie |
| **Page Haut** | Objet précédent dans la catégorie |
| **Shift + Page Bas** | Catégorie suivante (saute les catégories vides) |
| **Shift + Page Haut** | Catégorie précédente (saute les catégories vides) |
| **Début (Home)** | Annonce l'objet actuel (nom, distance, état) + tourne la caméra vers lui |
| **Shift + Début** | Active/désactive l'autowalk : commence à marcher vers l'objet sélectionné, ou arrête si déjà en marche |
| **Fin (End)** | Change de sous-catégorie (portes verrouillées seulement, conteneurs vides seulement, etc.) |

### Comment le scan fonctionne

1. Quand l'utilisateur appuie sur Pavé 5, on scanne toutes les cellules chargées autour du joueur
2. Pour chaque référence trouvée, on vérifie son type et on l'ajoute à la bonne catégorie
3. Les résultats sont triés par distance (le plus proche en premier)
4. En intérieur : les objets au même étage (moins de 256 unités de différence en Z) sont prioritaires
5. Re-scan automatique si le joueur bouge de plus de 100 unités depuis le dernier scan

### Ce qui est collecté par objet

- Référence (pour marcher vers lui plus tard)
- Nom affiché
- Distance depuis le joueur (3D)
- Différence d'élévation (au-dessus/en-dessous)
- État : verrouillé, vide, pillé, etc.

### Annonces vocales

**Au scan (Pavé 5) :**
```
"5 Portes. Porte en fer, 234 unités, en dessous"
"3 Conteneurs. Coffre, 89 unités"
"Aucun PNJ à proximité"
```

**À la navigation (Page Haut/Bas) :**
```
"Porte en fer, verrouillée, 234 unités, au dessus"
"Coffre, vide, 100 unités"
```

**Au changement de catégorie (Shift + Page Haut/Bas) :**
```
"Portes"
"Marqueurs de quête"
"Conteneurs"
```

**Au changement de sous-catégorie (Fin) :**
```
"Verrouillées seulement"
"Non vides seulement"
"Tout"
```

**Format de la distance :**
- Valeur entière en unités de jeu : `"234 unités"`
- Si l'objet est à plus de 256 unités au-dessus : on ajoute `", au dessus"`
- Si l'objet est à plus de 256 unités en-dessous : on ajoute `", en dessous"`

**Suffixes d'état ajoutés au nom :**
- Portes verrouillées : `"Porte en fer, verrouillée"`
- Conteneurs vides : `"Coffre, vide"`
- Cadavres pillés : `"Bandit, pillé"`

---

## Partie 2 : AutoWalk (Creation Kit + Papyrus)

### Ce que le Creation Kit doit fournir

Il faut créer un fichier ESP contenant une quête avec une structure précise. Voici exactement quoi créer :

#### 1. Quête

| Propriété | Valeur |
|-----------|--------|
| EditorID | `SkyrimTTS_AutoWalkQuest` |
| Flags | Start Game Enabled (démarre au lancement du jeu) |
| Priorité | 50 (par défaut) |
| Script | Attacher le script Papyrus (voir plus bas) |

#### 2. Alias de référence (dans la quête)

**Alias 1 : DstMarker**
- Type : Reference Alias
- Remplissage : Aucun (sera rempli au moment de l'exécution par Papyrus)
- Rôle : Stocke la cible de destination. Le Travel Package pointe ici.

**Alias 2 : Traveler**
- Type : Reference Alias
- Remplissage : Specific Reference → le joueur (PlayerRef)
- Rôle : L'acteur qui sera déplacé (le joueur). La Scène utilise cet alias.

#### 3. Scène

| Propriété | Valeur |
|-----------|--------|
| EditorID | `SkyrimTTS_WalkScene` |
| Flags | Override Behavior, Repeat |

La scène contient UNE phase avec UN package :

#### 4. Package d'IA (dans la Scène)

| Propriété | Valeur |
|-----------|--------|
| Type | Travel |
| Cible | Alias DstMarker |
| Vitesse | Run (course) ou Walk (marche) selon préférence |
| Autoriser la nage | Oui |

C'est la pièce clé : quand la scène démarre, le Travel Package fait marcher le joueur le long du navmesh vers DstMarker. Le moteur du jeu gère tout le pathfinding automatiquement.

#### 5. Propriétés du script (à configurer dans le CK)

Après avoir attaché le script Papyrus à la quête, définir ces propriétés :

| Propriété | Type | Valeur |
|-----------|------|--------|
| DstMarker | ReferenceAlias | → pointer vers l'alias DstMarker |
| Traveler | ReferenceAlias | → pointer vers l'alias Traveler |
| WalkScene | Scene | → pointer vers la scène |
| PlayerRef | Actor | → Game.GetPlayer() / PlayerRef |

### Résumé de la structure CK

```
Quête : SkyrimTTS_AutoWalkQuest (Start Game Enabled)
├── Script : SkyrimTTS_AutoWalk.psc
├── Alias : DstMarker (ReferenceAlias, vide, rempli à l'exécution)
├── Alias : Traveler (ReferenceAlias, rempli avec PlayerRef)
└── Scène : SkyrimTTS_WalkScene (Override Behavior, Repeat)
    └── Package : Travel vers DstMarker (vitesse course, nage autorisée)
```

---

## Partie 3 : Déroulement de l'autowalk

### Quand l'utilisateur appuie sur Shift+Home

1. Le plugin C++ détecte la touche
2. Il envoie le FormID de l'objet sélectionné au script Papyrus
3. Papyrus convertit le FormID en référence d'objet
4. Papyrus place la cible dans l'alias DstMarker
5. Papyrus active `Game.SetPlayerAIDriven(true)` → le joueur passe en mode IA
6. Papyrus démarre la Scène → le Travel Package fait marcher le joueur
7. Toutes les 0.25 secondes, Papyrus vérifie si le joueur est arrivé

### Quand le joueur arrive

1. La distance est inférieure au seuil d'arrêt (environ 120 unités)
2. Le joueur regarde vers la cible
3. La scène s'arrête
4. Le mode IA se désactive → le joueur reprend le contrôle

### Quand le joueur annule (ZQSD/Échap/Espace)

1. Le plugin C++ détecte les touches de mouvement via `GetAsyncKeyState()` (nécessaire car l'input normal est bloqué en mode IA)
2. Il appelle Papyrus pour arrêter
3. La scène s'arrête, le mode IA se désactive

### Quand un combat commence

1. Le script Papyrus détecte l'événement `OnCombatStateChanged`
2. L'autowalk s'arrête automatiquement
3. Le joueur reprend le contrôle pour combattre

---

## Partie 4 : Notes importantes

### Adaptations spécifiques à Skyrim

1. **SetPlayerAIDriven** — Dans Skyrim, l'équivalent est `PlayerCharacter::SetAIDriven(bool)`. À vérifier que ça fonctionne pareil.

2. **Marqueurs de quête** — Skyrim utilise une structure de données différente de FO4. Les positions des marqueurs de quête devront être lues depuis le système de boussole ou les données d'objectifs de quête.

3. **Lieux** — Les marqueurs de carte Skyrim fonctionnent différemment. Les lieux découverts sont stockés dans les données MapMarker des références.

4. **Compagnons** — Utiliser `Actor::IsPlayerTeammate()` au lieu du `IsFollowing()` de FO4.

5. **Précision des FormID** — Quand on passe un FormID du C++ vers Papyrus, toujours utiliser un int32, jamais un float. Le float perd en précision sur les grands FormID.

### Version minimale viable

Pour une première version, se concentrer sur :
1. Catégorie Marqueurs de quête (le plus utile pour la navigation)
2. Catégorie Portes (naviguer dans les donjons)
3. Catégorie PNJ (trouver les donneurs de quête)
4. AutoWalk vers l'objet sélectionné

Ajouter les autres catégories (Objets, Conteneurs, Cadavres, etc.) progressivement.

---

## Résumé : qui fait quoi

| Composant | Qui | Outil |
|-----------|-----|-------|
| Scanner d'objets (scan, navigation, annonces) | Pyrhame | C++ / plugin SKSE |
| Quête + Scène + Package + Alias | Dio | Creation Kit |
| Script Papyrus | L'un ou l'autre | Éditeur texte (compiler avec CK ou Champollion) |
| Intégration & tests | Les deux | En jeu |
