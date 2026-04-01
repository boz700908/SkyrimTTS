# Recherche avancee : Pathfinding sur navmesh

Document de recherche pour ameliorer le pathfinding A* custom du plugin SkyrimNVDA.
Genere le 2026-03-31.

## Etat actuel du code (pathfinding.h)

Le systeme actuel fait :
1. A* sur graphe de triangles (centroide a centroide)
2. Waypoints = milieu des aretes partagees entre triangles
3. Lissage par raycasting navmesh (desactive actuellement)
4. Suivi de waypoints via Papyrus Travel package
5. Gestion des portes, sauts, rebords, transitions de cellule
6. Detection de blocage avec recuperation progressive

**Problemes identifies :**
- Les waypoints passent par les milieux d'aretes, causant des zigzags
- Le lissage par raycasting est desactive car trop agressif
- Pas de funnel algorithm = chemins non optimaux
- Les couts A* utilisent la distance centroide-centroide (imprecis)

---

## 1. Funnel Algorithm / Simple Stupid Funnel Algorithm (SSFA)

### Concept

Le funnel algorithm est LA technique pour obtenir le chemin le plus court a l'interieur d'un couloir de triangles. Au lieu de passer par les centroides ou les milieux d'aretes, il "tire" le chemin comme une ficelle tendue entre les murs du couloir.

**Avant funnel :** le chemin zigzague de centroide en centroide ou de milieu d'arete en milieu d'arete.  
**Apres funnel :** le chemin est une ligne droite tant que possible, avec des virages uniquement aux coins obligatoires (les "apex").

### Portails (Portals)

Chaque transition entre deux triangles consecutifs cree un **portail** = l'arete partagee entre les deux triangles. Un portail a un point gauche et un point droit (les deux sommets de l'arete partagee).

```
Portail = [leftVertex, rightVertex]
```

La sequence de portails forme un couloir (channel) a travers lequel le chemin optimal doit passer.

**Important pour SkyrimNVDA :** On a deja `PF_SharedEdgeMidpoint()` qui trouve les sommets partages. Il suffit de retourner les DEUX sommets au lieu de leur milieu.

### Determination gauche/droite

Pour chaque portail, il faut determiner quel sommet est "gauche" et quel est "droite" par rapport a la direction de marche. On utilise le produit en croix 2D :

```cpp
// Si cross(direction, sommet) > 0, c'est a gauche
// Si cross(direction, sommet) < 0, c'est a droite
float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
```

### Pseudocode complet du SSFA

Source : Mikko Mononen (auteur de Recast/Detour), digestingduck.blogspot.com

```
Fonction triarea2(a, b, c):
    // Deux fois l'aire signee du triangle (produit en croix 2D)
    ax = b.x - a.x
    ay = b.y - a.y
    bx = c.x - a.x
    by = c.y - a.y
    retourner bx * ay - ax * by
    // > 0 = c est a gauche de AB
    // < 0 = c est a droite de AB
    // = 0 = collineaire
```

```
Fonction FunnelPath(portals[], nportals) -> points[]:
    // portals[0] = (start, start)   <-- point de depart replique
    // portals[last] = (end, end)    <-- point d'arrivee replique
    // portals[i] = (left_i, right_i) pour i dans [1..last-1]

    apex = portals[0].left       // point de depart
    portalLeft = apex
    portalRight = apex
    apexIndex = 0
    leftIndex = 0
    rightIndex = 0
    points = [apex]

    Pour i de 1 a nportals - 1:
        left = portals[i].left
        right = portals[i].right

        // --- Mise a jour du cote DROIT ---
        Si triarea2(apex, portalRight, right) <= 0:
            // Le nouveau point droit est dans le funnel ou le retrecit
            Si apex == portalRight OU triarea2(apex, portalLeft, right) > 0:
                // Le nouveau point ne croise pas le cote gauche
                portalRight = right
                rightIndex = i
            Sinon:
                // Le cote droit croise le cote gauche !
                // Le point gauche actuel devient le nouvel apex
                points.ajouter(portalLeft)
                apex = portalLeft
                apexIndex = leftIndex
                // Reinitialiser le funnel
                portalLeft = apex
                portalRight = apex
                leftIndex = apexIndex
                rightIndex = apexIndex
                i = apexIndex    // REDEMARRER depuis l'apex
                continuer

        // --- Mise a jour du cote GAUCHE ---
        Si triarea2(apex, portalLeft, left) >= 0:
            Si apex == portalLeft OU triarea2(apex, portalRight, left) < 0:
                portalLeft = left
                leftIndex = i
            Sinon:
                points.ajouter(portalRight)
                apex = portalRight
                apexIndex = rightIndex
                portalLeft = apex
                portalRight = apex
                leftIndex = apexIndex
                rightIndex = apexIndex
                i = apexIndex
                continuer

    // Ajouter le point final
    points.ajouter(portals[nportals-1].left)  // = destination
    retourner points
```

### Implementation C++ pour SkyrimNVDA

```cpp
// Fonction utilitaire
static float PF_TriArea2D(const RE::NiPoint3& a, const RE::NiPoint3& b, const RE::NiPoint3& c) {
    float ax = b.x - a.x, ay = b.y - a.y;
    float bx = c.x - a.x, by = c.y - a.y;
    return bx * ay - ax * by;
}

static bool PF_VEqual2D(const RE::NiPoint3& a, const RE::NiPoint3& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return (dx * dx + dy * dy) < 0.001f;
}

struct PF_Portal {
    RE::NiPoint3 left;
    RE::NiPoint3 right;
};

// Extraire les portails d'une sequence de triangles dans le meme navmesh
static std::vector<PF_Portal> PF_ExtractPortals(
    const RE::NavMesh* mesh,
    const std::vector<uint16_t>& triPath,
    const RE::NiPoint3& startPos,
    const RE::NiPoint3& endPos)
{
    std::vector<PF_Portal> portals;
    // Premier portail = point de depart
    portals.push_back({startPos, startPos});

    for (size_t i = 0; i + 1 < triPath.size(); i++) {
        auto& tA = mesh->triangles[triPath[i]];
        auto& tB = mesh->triangles[triPath[i + 1]];

        // Trouver les 2 sommets partages
        RE::NiPoint3 shared[2];
        int count = 0;
        for (int a = 0; a < 3 && count < 2; a++) {
            for (int b = 0; b < 3; b++) {
                if (tA.vertices[a] == tB.vertices[b]) {
                    shared[count++] = mesh->vertices[tA.vertices[a]].location;
                    break;
                }
            }
        }

        if (count == 2) {
            // Determiner gauche/droite par rapport a la direction de marche
            auto center_A = PF_TriangleCentroid(mesh, triPath[i]);
            auto center_B = PF_TriangleCentroid(mesh, triPath[i + 1]);
            float cross = PF_TriArea2D(center_A, center_B, shared[0]);
            if (cross > 0) {
                portals.push_back({shared[0], shared[1]});  // 0=left, 1=right
            } else {
                portals.push_back({shared[1], shared[0]});  // swap
            }
        }
    }

    // Dernier portail = point d'arrivee
    portals.push_back({endPos, endPos});
    return portals;
}

// Funnel algorithm (SSFA)
static std::vector<RE::NiPoint3> PF_FunnelSmooth(const std::vector<PF_Portal>& portals) {
    if (portals.size() < 2) return {};

    std::vector<RE::NiPoint3> path;
    RE::NiPoint3 apex = portals[0].left;
    RE::NiPoint3 pLeft = apex, pRight = apex;
    int apexIdx = 0, leftIdx = 0, rightIdx = 0;
    path.push_back(apex);

    int n = static_cast<int>(portals.size());
    for (int i = 1; i < n; i++) {
        auto& left = portals[i].left;
        auto& right = portals[i].right;

        // Update right
        if (PF_TriArea2D(apex, pRight, right) <= 0.0f) {
            if (PF_VEqual2D(apex, pRight) || PF_TriArea2D(apex, pLeft, right) > 0.0f) {
                pRight = right;
                rightIdx = i;
            } else {
                path.push_back(pLeft);
                apex = pLeft;
                apexIdx = leftIdx;
                pLeft = apex;
                pRight = apex;
                leftIdx = apexIdx;
                rightIdx = apexIdx;
                i = apexIdx;
                continue;
            }
        }

        // Update left
        if (PF_TriArea2D(apex, pLeft, left) >= 0.0f) {
            if (PF_VEqual2D(apex, pLeft) || PF_TriArea2D(apex, pRight, left) < 0.0f) {
                pLeft = left;
                leftIdx = i;
            } else {
                path.push_back(pRight);
                apex = pRight;
                apexIdx = rightIdx;
                pLeft = apex;
                pRight = apex;
                leftIdx = apexIdx;
                rightIdx = apexIdx;
                i = apexIdx;
                continue;
            }
        }
    }

    // Ajouter la destination
    if (path.empty() || !PF_VEqual2D(path.back(), portals.back().left)) {
        path.push_back(portals.back().left);
    }
    return path;
}
```

### Complexite

- O(n) en moyenne, O(n^2) dans le pire cas (rare, uniquement si le funnel se replie souvent)
- n = nombre de portails = nombre de triangles dans le couloir - 1
- Tres rapide, pas de structures de donnees complexes

### Adaptation 3D

Le SSFA classique travaille en 2D (plan XY). Pour Skyrim ou il y a des escaliers et des changements de hauteur :

1. **Projeter en 2D** pour le calcul du funnel (ignorer Z)
2. **Interpoler Z** le long du chemin en utilisant la hauteur du navmesh sous chaque point
3. Pour les cas extremes (spirale, passage au-dessus/en-dessous), on peut "derouler" le funnel en 3D en projetant chaque portail sur un plan perpendiculaire a la direction de marche

---

## 2. String-Pulling vs Funnel

### Difference

- **Funnel algorithm** : necessite un "channel" (sequence ordonnee de triangles adjacents formant un couloir). C'est ce qu'on obtient de A*. Plus efficace.
- **String-pulling** : technique plus generale, fonctionne meme sans channel propre. Moins efficace mais plus robuste.

Pour SkyrimNVDA, le funnel est le bon choix car A* nous donne deja le channel.

### Optimisation supplementaire : Visibilite directe

Apres le funnel, on peut encore optimiser en testant si certains waypoints consecutifs ont une ligne de vue directe (raycasting sur le navmesh). C'est ce que fait `optimizePathVisibility` dans Detour.

```
Pour chaque paire de waypoints (i, j) ou j > i + 1:
    Si on peut marcher en ligne droite de i a j (raycasting navmesh):
        Supprimer les waypoints entre i et j
```

Note : le code actuel `PF_CanWalkStraight()` fait deja ca, mais il echantillonne le long de la ligne et verifie que chaque point est sur un triangle navmesh. C'est correct mais plus lent qu'un vrai raycast navmesh.

---

## 3. Comment Recast/Detour fait les choses

Recast/Detour (par Mikko Mononen) est le standard industriel. Utilise par Unity, Unreal Engine, Godot, et des dizaines de moteurs.

### Architecture en couches

```
1. Recast : genere le navmesh a partir de la geometrie 3D
   (pas pertinent pour nous, on utilise le navmesh de Skyrim)

2. Detour : pathfinding et queries sur le navmesh
   - dtNavMeshQuery::findPath()    -> A* sur polygones -> corridor
   - dtNavMeshQuery::findStraightPath() -> funnel/string-pull -> waypoints
   - dtNavMeshQuery::moveAlongSurface() -> mouvement contraint
   - dtNavMeshQuery::raycast()     -> test de visibilite

3. DetourCrowd : gestion multi-agents
   - dtPathCorridor  -> maintien du couloir en temps reel
   - dtCrowdAgent    -> steering + evitement local
```

### Pipeline typique dans Detour

```
1. findPath(start, goal) -> corridor de polygones [poly0, poly1, ..., polyN]
2. findStraightPath(corridor) -> waypoints lisses [p0, p1, ..., pK]
3. A chaque frame:
   a. findCorners() -> 2-3 prochains waypoints
   b. Calculer velocite desiree vers le prochain coin
   c. RVO/ORCA pour evitement local
   d. moveAlongSurface(velocite) -> nouvelle position contrainte au navmesh
   e. Mettre a jour le corridor (dtPathCorridor::movePosition)
```

### dtPathCorridor - Gestion du couloir

Le corridor est la sequence de polygones entre l'agent et sa destination. Il est maintenu en temps reel :

- **movePosition()** : quand l'agent bouge, appelle `moveAlongSurface()` pour contraindre au navmesh, puis `dtMergeCorridorStartMoved()` pour ajuster le debut du corridor.

- **moveTargetPosition()** : meme logique pour la destination qui bouge.

- **optimizePathVisibility()** : raycasting pour raccourcir le corridor (skip des polygones intermediaires si ligne de vue directe).

- **optimizePathTopology()** : recalcul local A* (max 32 iterations) pour trouver un meilleur chemin dans le voisinage sans tout recalculer.

- **findCorners()** : appelle `findStraightPath()` sur le corridor, puis elimine les coins trop proches (< 0.01 unites) et ceux apres un off-mesh connection.

### Facteur d'echelle heuristique

Detour utilise `H_SCALE = 0.999f` pour l'heuristique A*. Cela fait que l'heuristique sous-estime legerement, garantissant l'optimalite tout en guidant la recherche vers la destination.

### Lecons pour SkyrimNVDA

1. **Separer corridor et waypoints** : d'abord le corridor (A* sur triangles), puis les waypoints (funnel sur portails du corridor)
2. **Optimisation locale** : ne pas tout recalculer a chaque fois, juste ajuster le corridor localement
3. **Mouvement contraint** : au lieu de teleporter vers le waypoint, contraindre le mouvement au navmesh

---

## 4. Mouvement le long du chemin

### Comment les jeux font marcher les personnages

**Approche 1 : Input simulation (ce que fait SkyrimNVDA actuellement)**
- Utilise le Travel package de Skyrim via Papyrus
- Avantage : respect la physique et les collisions
- Inconvenient : le Travel package peut bloquer dans certains cas

**Approche 2 : Steering behaviors**
```
A chaque frame:
    1. Trouver la direction vers le prochain waypoint
    2. Calculer l'angle de rotation necessaire
    3. Tourner le joueur progressivement
    4. Avancer si l'angle est suffisamment aligne
    5. Ralentir dans les virages serres
```

**Approche 3 : Mouvement contraint (style Detour)**
```
A chaque frame:
    1. Calculer la position desiree (vers le prochain waypoint)
    2. moveAlongSurface() : deplacement contraint aux polygones navmesh
    3. Si la position finale est sur un portail, avancer dans le corridor
    4. Si bloque, glisser le long du bord du polygone
```

### Gestion de l'elevation et des escaliers

- **Escaliers** : le navmesh de Skyrim suit deja les escaliers. Le Z des sommets du navmesh encode la hauteur. Interpoler Z entre les sommets donne la bonne hauteur.
- **Sauts** : les edges de type `kLedgeUp` dans extraEdgeInfo indiquent un saut. Detecter et declencher un saut (le code actuel fait deja ca).
- **Chutes** : `kLedgeDown` indique un saut vers le bas. Le joueur tombe naturellement.

### Recommandation pour SkyrimNVDA

Le Travel package via Papyrus reste la meilleure approche pour Skyrim car :
- Il utilise le moteur de mouvement natif du jeu
- Il gere les collisions et la gravite
- Il est deja implemente

L'amelioration cle est de lui donner de MEILLEURS waypoints via le funnel algorithm, et d'utiliser le lissage pour reduire les virages inutiles.

---

## 5. Evitement dynamique d'obstacles

### RVO / ORCA

**RVO (Reciprocal Velocity Obstacles)** est l'algorithme standard pour l'evitement de collision entre agents. ORCA (Optimal Reciprocal Collision Avoidance) est la version amelioree.

Principe :
1. Chaque agent a une vitesse desiree (vers son prochain waypoint)
2. Pour chaque voisin proche, calculer la zone de vitesses interdites
3. Choisir la vitesse la plus proche de la desiree qui evite toutes les collisions

**Pertinence pour SkyrimNVDA :** Faible. Le joueur est le seul agent qu'on controle. Les PNJ ont leur propre IA. L'evitement local n'est pas necessaire.

### Evitement d'obstacles statiques

Plus pertinent pour nous. Detour utilise `moveAlongSurface()` qui glisse automatiquement le long des bords du navmesh. Pour SkyrimNVDA :

- Le Travel package gere deja les collisions statiques
- Le probleme est quand il BLOQUE (pas qu'il ne detecte pas les obstacles)
- Solutions : meilleurs waypoints (funnel), detection de blocage plus precoce, re-routage plus intelligent

---

## 6. Couts et heuristiques de chemin

### Couts actuels dans SkyrimNVDA

- Distance 3D centroide-centroide (A* edge cost)
- Penalite Z x2 dans l'heuristique
- Multiplicateur x3 pour les rebords (ledge up/down)
- Bonus x0.5 pour les triangles preferes

### Couts supplementaires a considerer

| Cout | Description | Implementation |
|------|-------------|----------------|
| **Pente** | Penaliser les pentes raides | `slope = abs(deltaZ / distance2D)`. Si > 0.5 (angle ~27deg), multiplier le cout |
| **Type de terrain** | Eau, boue, neige | Verifier le flag kWater sur les triangles. Penaliser si le joueur ne peut pas nager |
| **Triangles etroits** | Passages serres risquent de bloquer | Penaliser les triangles avec une petite aire |
| **Zone dangereuse** | Pieges, ennemis | Pas de donnees directes dans le navmesh, mais on pourrait cross-referencer avec les references de la cellule |
| **Distance au bord** | Rester au milieu du chemin | Bonus pour les triangles loin des bords du navmesh |
| **Preferred path** | Chemins marques comme preferes dans le CK | Deja implemente via kPreferred |

### Cout plus precis : distance portail a portail

Au lieu de centroide a centroide (imprecis pour les gros triangles), utiliser la distance entre les points de passage reels des portails :

```cpp
// Au lieu de :
float cost = Distance(centroid_A, centroid_B);

// Utiliser :
float cost = Distance(midpoint_shared_edge_AB, midpoint_shared_edge_BC);
```

Encore mieux : utiliser les points de passage du funnel algorithm retroactivement pour ajuster les couts A*. Mais c'est un probleme poule-et-oeuf (il faut le chemin pour avoir les points, et les points pour avoir le chemin). Solution pragmatique : milieu des aretes partagees.

### Flag kPreferred : attention au bug Skyrim

Les triangles marques "Preferred" dans Skyrim ont un bug connu : les PNJ les suivent meme en combat, ce qui les fait prendre des detours absurdes. Pour notre pathfinding joueur, ce n'est pas un probleme (le joueur ne combat pas en autowalk), mais il faut etre conscient que le bonus ne devrait pas etre trop fort.

---

## 7. Pathfinding hierarchique

### Pourquoi

Pour les longues distances (traverser une ville, un donjon entier), A* sur les triangles individuels explose en nombre d'iterations. Le navmesh de Skyrim peut avoir des milliers de triangles par cellule.

### HNA* (Hierarchical Navigation A*)

Architecture en 3 niveaux :

```
Niveau 2 : Graphe de regions (17 noeuds pour une zone entiere)
    |
Niveau 1 : Graphe de clusters (316 noeuds)
    |
Niveau 0 : Graphe de triangles (5000+ noeuds)
```

**Construction (offline / au chargement) :**
1. Partitionner les triangles en clusters par partitionnement k-way
2. Pour chaque cluster, identifier les portails de frontiere (aretes partagees avec d'autres clusters)
3. Pre-calculer les chemins internes (A* intra-cluster entre chaque paire de portails de frontiere)
4. Construire le graphe de haut niveau avec les clusters comme noeuds et les portails comme aretes

**Recherche de chemin :**
1. Connecter start et goal au graphe hierarchique
2. A* au niveau haut (clusters) -> sequence de clusters
3. Pour chaque cluster traverse, extraire le sous-chemin pre-calcule
4. Concatener les sous-chemins
5. Funnel sur le chemin complet

**Performance :** 7-9x plus rapide que A* direct. Jusqu'a 15x avec parallelisation GPU.

### Application a SkyrimNVDA

Pour Skyrim, une hierarchie naturelle existe deja :

```
Niveau haut : Cellules (chaque cellule a son propre navmesh)
    |
Niveau bas : Triangles dans chaque navmesh
```

On peut utiliser `NavMeshInfoMap` et `PrecomputedNavmeshInfoPathMap` (qui existent deja dans CommonLibSSE) pour le niveau haut. Le moteur a deja les chemins pre-calcules entre navmeshes !

```cpp
// NavMeshInfoMap::GetSingleton() fournit :
// - BuildListOfConnectedInfos() : navmeshes connectes a un navmesh donne
// - PrecomputedNavmeshInfoPathMap : chemins pre-calcules entre navmeshes
```

**Implementation recommandee :**
1. Pour les courtes distances (meme cellule) : A* direct sur triangles + funnel
2. Pour les longues distances :
   a. Utiliser NavMeshInfoMap pour trouver la sequence de navmeshes
   b. Pour chaque navmesh, A* local + funnel
   c. Gerer les transitions via les portails inter-navmesh

---

## 8. Implementations open source de reference

### Recast/Detour (C++, MIT)

**Le standard industriel.** Code source tres lisible.

- Repo : https://github.com/recastnavigation/recastnavigation
- Fichiers cles :
  - `Detour/Source/DetourNavMeshQuery.cpp` : findPath, findStraightPath, moveAlongSurface
  - `DetourCrowd/Source/DetourPathCorridor.cpp` : corridor management
  - `DetourCrowd/Source/DetourCrowd.cpp` : multi-agent simulation

Patterns a copier :
- Le funnel dans `findStraightPath()` (portails gauche/droite, test triarea2D)
- Le corridor dans `dtPathCorridor` (merge, optimize, findCorners)
- Le mouvement contraint dans `moveAlongSurface()` (glissement le long des bords)

### HyungseobKim/NavMesh (C++)

- Repo : https://github.com/HyungseobKim/NavMesh
- Generation automatique de navmesh + A* + funnel
- Code plus simple que Detour, bon pour comprendre le funnel

### frapa/nav2d (JavaScript)

- Repo : https://github.com/frapa/nav2d
- Implementation robuste du funnel pour navmesh 2D polygonal
- Bon pour comprendre l'algorithme sans la complexite 3D

### DotRecast (C#, port de Detour)

- Repo : https://github.com/ikpil/DotRecast
- Port C# complet de Recast/Detour
- Utile si on veut lire le code dans un langage plus accessible

### Combat Pathing Revolution (C++, MIT, specifique Skyrim)

- Repo : https://github.com/max-su-2019/CombatPathingRevolution
- Modifie l'IA de combat de Skyrim via SKSE
- Montre comment acceder aux navmeshes at runtime dans Skyrim
- Deja reference dans le code existant

---

## 9. Specificites navmesh de Skyrim

### Structure NVNM

Le format NVNM (enregistrement navmesh dans les fichiers .esp/.esm) contient :
- **Vertices** : tableaux de NiPoint3 (X, Y, Z)
- **Triangles** : 3 indices de sommets + 3 indices de voisins (0xFFFF si pas de voisin) + flags
- **Edge extra info** : portails vers d'autres navmeshes, rebords up/down
- **Door portals** : connexions specifiques aux portes
- **Cover edges** : aretes de couverture pour l'IA

### Flags de triangle importants

| Flag | Signification | Impact pathfinding |
|------|--------------|-------------------|
| kDeleted | Triangle supprime | Ne JAMAIS traverser |
| kPreferred | Chemin prefere | Bonus de cout (mais buggue pour les PNJ en combat) |
| kNoLargeCreatures | Interdit aux grandes creatures | Ignorable pour le joueur |
| kOverlapping | Triangle superpose a un autre | Choisir celui avec le Z le plus proche |
| Water triangle (flag O dans CK) | Zone aquatique | Penaliser si le joueur ne sait pas nager |

### Comportement runtime

- Les PNJ ne marchent PAS sur le navmesh directement. Ils marchent sur la surface de collision mais ne depassent pas les limites du navmesh.
- Les acteurs sont "places sur le navmesh puis laches au sol" (drop to ground).
- Il y a une certaine tolerance entre le navmesh et la geometrie de collision.
- Les portes necessitent un triangle navmesh directement sous le marqueur de porte.

### Problemes connus du navmesh vanilla

Le mod "Navigator - Navmesh Fixes" (Nexus 52641) corrige de nombreux problemes :
- Triangles manquants dans les passages
- Navmeshes non connectes entre cellules
- Portails de porte mal configures
- Triangles superposes causant des comportements etranges

**Recommandation :** conseiller aux joueurs d'installer Navigator pour de meilleurs resultats avec notre pathfinding.

### Triangles superposes (kOverlapping)

Skyrim utilise des triangles superposes pour les zones multi-niveaux (ponts, balcons). Quand on cherche "dans quel triangle est le joueur", il faut comparer le Z du joueur avec le Z moyen du triangle et prendre le plus proche. Le code actuel (`PF_FindTriangle`) fait deja ca correctement.

---

## 10. Plan d'integration dans pathfinding.h

### Phase 1 : Funnel Algorithm (priorite haute)

**Changements necessaires :**

1. **Modifier A* pour stocker le chemin de triangles (pas juste les cles)**
   - En plus de `cameFrom`, garder une map `meshForKey` pour retrouver quel navmesh contient chaque triangle
   - Reconstruire la sequence de triangles, pas juste les waypoints

2. **Ajouter PF_ExtractPortals()**
   - Pour chaque paire de triangles consecutifs dans le meme navmesh, extraire les deux sommets partages
   - Determiner gauche/droite via le produit en croix

3. **Ajouter PF_FunnelSmooth()**
   - Implementer le SSFA tel que decrit ci-dessus
   - Travailler en 2D (XY) et interpoler Z

4. **Modifier le pipeline**
   ```
   AVANT : A* -> milieux d'aretes -> (lissage desactive) -> waypoints
   APRES : A* -> corridor de triangles -> portails -> funnel -> waypoints lisses
   ```

5. **Gerer les transitions inter-navmesh**
   - Le funnel s'arrete aux frontieres de navmesh
   - Appliquer le funnel separement pour chaque segment intra-navmesh
   - Les points de transition inter-navmesh sont des waypoints obligatoires

### Phase 2 : Couts ameliores (priorite moyenne)

1. Utiliser milieu d'arete au lieu de centroide pour les couts A*
2. Ajouter penalite de pente
3. Ajouter penalite pour triangles etroits (aire faible)

### Phase 3 : Pathfinding hierarchique (priorite basse)

1. Utiliser NavMeshInfoMap pour planifier les transitions inter-navmesh
2. A* local par navmesh + funnel
3. Cache des chemins inter-navmesh recemment calcules

### Phase 4 : Optimisation corridor (priorite basse)

1. Optimisation de visibilite (raycasting pour raccourcir)
2. Recalcul local au lieu de tout recalculer (comme dtPathCorridor::optimizePathTopology)

---

## Sources

- [Simple Stupid Funnel Algorithm - Mikko Mononen](https://digestingduck.blogspot.com/2010/03/simple-stupid-funnel-algorithm.html)
- [Constrained Movement Along Path Corridor](http://digestingduck.blogspot.com/2009/12/constrained-movement-along-path.html)
- [Recast Navigation (GitHub)](https://github.com/recastnavigation/recastnavigation)
- [Detour: Pathfinding and Navigation (DeepWiki)](https://deepwiki.com/recastnavigation/recastnavigation/3-detour:-pathfinding-and-navigation)
- [Funnel Algorithm - Yu Shutong's Blog](https://yushutong.wordpress.com/2013/06/16/pathfinding-funnel-algorithm/)
- [Navigation Meshes and Pathfinding - GameDev.net](https://gamedev.net/tutorials/programming/artificial-intelligence/navigation-meshes-and-pathfinding-r4880/)
- [A* with Navigation Meshes - Medium](https://medium.com/@mscansian/a-with-navigation-meshes-246fd9e72424)
- [String Pulling Explained - GameDev.net Forum](https://www.gamedev.net/forums/topic/539575-string-pulling-explained/4484448/)
- [RVO2 Library - UNC](https://gamma.cs.unc.edu/RVO2/)
- [HNA* - Hierarchical Pathfinding for NavMeshes](https://www.sciencedirect.com/science/article/abs/pii/S0097849316300668)
- [HNA* Implementation (GitHub)](https://github.com/educharlie/HNA-Algorithm)
- [Skyrim NVNM Format - UESP](https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/NVNM_Field)
- [Navigator - Navmesh Fixes (Nexus)](https://www.nexusmods.com/skyrimspecialedition/mods/52641)
- [Skyrim Navmesh Clarification (Forum)](http://www.gamesas.com/clarification-navmeshes-t411246.html)
- [Saner Pathing (Nexus)](https://www.nexusmods.com/skyrim/mods/75216)
- [Combat Pathing Revolution (GitHub)](https://github.com/max-su-2019/CombatPathingRevolution)
- [HyungseobKim/NavMesh (GitHub)](https://github.com/HyungseobKim/NavMesh)
- [DotRecast (GitHub)](https://github.com/ikpil/DotRecast)
- [Detour DetourNavMeshQuery.cpp (GitHub)](https://github.com/recastnavigation/recastnavigation/blob/main/Detour/Source/DetourNavMeshQuery.cpp)
- [Detour DetourPathCorridor.cpp (GitHub)](https://github.com/recastnavigation/recastnavigation/blob/main/DetourCrowd/Source/DetourPathCorridor.cpp)
