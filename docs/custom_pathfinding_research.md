# Recherche : Pathfinding custom via navmesh (SKSE/C++)

Document de recherche pour implementer un pathfinding intelligent en remplacement du Travel package.
Genere par la conversation SkyrimCK-MCP le 2026-03-31.

## Pourquoi

Le Travel package natif de Skyrim bloque souvent dans les interieurs (escaliers, passages etroits, changements de hauteur). On veut un pathfinding maison qui :
- Detecte les blocages
- Calcule un chemin optimal via le navmesh
- Deplace le joueur point par point
- Gere les portes et transitions entre cellules

## Classes CommonLibSSE disponibles

Toutes les classes sont dans les headers CommonLibSSE-NG deja installes dans le projet (vcpkg).

### BSNavmesh (classe centrale)

Header : `RE/B/BSNavmesh.h`
Herite de : `BSIntrusiveRefCounted`
Taille : 0x118

Membres principaux :
- `BSTArray<BSNavmeshVertex> vertices` (offset 0x010) — tous les sommets
- `BSTArray<BSNavmeshTriangle> triangles` (offset 0x028) — tous les triangles
- `BSTArray<BSNavmeshEdgeExtraInfo> extraEdgeInfo` (offset 0x040) — portails entre navmeshes
- `BSTArray<BSNavmeshTriangleDoorPortal> doorPortals` (offset 0x058) — portails de portes
- `BSTArray<BSNavmeshClosedDoorInfo> closedDoors` (offset 0x070) — info portes fermees
- `BSTArray<BSNavmeshCoverEdge> coverArray` (offset 0x088) — aretes de couverture
- `BSNavmeshGrid meshGrid` (offset 0x0A0) — grille spatiale pour lookup rapide
- `BSTArray<NiPointer<BSNavmeshObstacleUndoData>> obstacles` (offset 0x0D0)
- `BSTSmartPointer<BSPathingCell> parentCell` (offset 0x108)
- Methode virtuelle : `uint32_t QNavmeshID()`

### BSNavmeshVertex (12 bytes)

- `NiPoint3 location` — coordonnees X, Y, Z (3 floats)

### BSNavmeshTriangle (16 bytes) — STRUCTURE CLE

- `uint16_t vertices[3]` — 3 indices de sommets dans le tableau vertices
- `uint16_t triangles[3]` — 3 indices de triangles adjacents (0xFFFF = pas de voisin)
- `TriangleFlag triangleFlags` — flags par arete :
  - kEdge0_Link, kEdge1_Link, kEdge2_Link — aretes connectees
  - kDeleted — triangle supprime
  - kNoLargeCreatures — pas de grandes creatures
  - kOverlapping — triangle superpose
  - kPreferred — chemin prefere
- `TraversalFlag traversalFlags` — info de couverture par arete

### BSNavmeshGrid (grille spatiale, 0x30 bytes)

Permet de trouver rapidement quel triangle est sous une position donnee.

- `uint32_t gridSize` — dimensions de la grille
- `float columnSectionLen` — taille d'une colonne
- `float rowSectionLen` — taille d'une rangee
- `NiPoint3 gridBoundsMin` — coin minimum
- `NiPoint3 gridBoundsMax` — coin maximum
- `SimpleArray<BSTArray<uint16_t>> gridData` — pour chaque cellule de grille, liste des indices de triangles

### BSNavmeshEdgeExtraInfo (portails entre navmeshes)

- `EDGE_EXTRA_INFO_TYPE type` :
  - kPortal — connexion vers un autre navmesh
  - kLedgeUp — rebord montant
  - kLedgeDown — rebord descendant
  - kEnableDisablePortal — portail activable/desactivable
- `BSNavmeshTriangleEdgePortal portal` :
  - `FormID otherMeshID` — ID du navmesh voisin
  - `uint16_t triangle` — index du triangle dans l'autre navmesh
  - `int8_t edgeIndex` — quelle arete

### BSNavmeshTriangleDoorPortal

Connexions specifiques aux portes (quand une porte connecte deux cellules).

### NavMesh (forme TES complete)

Header : `RE/N/NavMesh.h`
Herite de : `TESForm` + `TESChildCell` + `BSNavmesh`
Taille : 0x140

C'est la version "form" du navmesh, identifiable par FormID.

### NavMeshInfoMap (carte globale des navmeshes)

Header : `RE/N/NavMeshInfoMap.h`
Herite de : `TESForm` + `BSNavmeshInfoMap` + `PrecomputedNavmeshInfoPathMap`
Taille : 0xF0

Methodes utiles :
- `BSNavmeshInfo* GetNavmeshInfo(uint32_t id)` — recuperer info par ID
- `BSNavmeshInfo* GetNavMeshInfoFixID(uint32_t id)` — idem avec correction ID
- `void GetAllNavMeshInfo(BSTArray<BSNavmeshInfo*>& results)` — toutes les infos
- `void BuildListOfConnectedInfos(const BSNavmeshInfo* info, BSTArray<BSNavmeshInfo*>& results)` — navmeshes connectes
- `void ForEach(IVisitor* visitor)` — iteration avec visiteur

Membres :
- `BSTHashMap<uint32_t, NavMeshInfo*> infoMap` (offset 0x80) — lookup par ID
- `BSReadWriteLock mapLock` (offset 0xE0)

### PrecomputedNavmeshInfoPathMap

Chemins precalcules entre navmeshes :
- `BSTArray<BSTArray<const BSNavmeshInfo*>*> allPaths` — tous les chemins
- `BSTHashMap<const BSNavmeshInfo*, uint32_t> infoToIndexMap` — index

### ExtraNavMeshPortal

Header : `RE/E/ExtraNavMeshPortal.h`
Extra data sur les references pour les portails navmesh.

### BSPathingCell / PathingCell

Headers : `RE/B/BSPathingCell.h`, `RE/P/PathingCell.h`
- BSPathingCell taille 0x10
- PathingCell taille 0x18, membre `FormID cellID` a offset 0x14

## Comment acceder au navmesh a l'execution

### Via la cellule du joueur (approche principale)

```cpp
#include <RE/Skyrim.h>

RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
RE::TESObjectCELL* cell = player->GetParentCell();

if (cell) {
    auto& runtimeData = cell->GetRuntimeData();
    if (runtimeData.navMeshes) {
        for (auto& navMesh : runtimeData.navMeshes->navMeshes) {
            // navMesh est un BSTSmartPointer<NavMesh>
            // Acces aux donnees :
            auto& verts = navMesh->vertices;     // BSTArray<BSNavmeshVertex>
            auto& tris = navMesh->triangles;      // BSTArray<BSNavmeshTriangle>
            auto& grid = navMesh->meshGrid;       // BSNavmeshGrid
            auto& edges = navMesh->extraEdgeInfo;  // portails
            auto& doors = navMesh->doorPortals;    // portes
        }
    }
}
```

### Via NavMeshInfoMap (pour navigation inter-cellules)

```cpp
// NavMeshInfoMap est un TESForm singleton
// Permet de decouvrir les connexions entre navmeshes de differentes cellules
// Methode BuildListOfConnectedInfos pour trouver les voisins
```

## Algorithme propose : A* sur navmesh

### Etape 1 : Trouver le triangle sous une position

```
fonction FindTriangle(position, navmesh):
    // Utiliser meshGrid pour limiter la recherche
    gridX = (position.x - gridBoundsMin.x) / columnSectionLen
    gridY = (position.y - gridBoundsMin.y) / rowSectionLen
    candidats = gridData[gridY * gridSize + gridX]

    pour chaque triangleIndex dans candidats:
        tri = navmesh.triangles[triangleIndex]
        v0 = navmesh.vertices[tri.vertices[0]].location
        v1 = navmesh.vertices[tri.vertices[1]].location
        v2 = navmesh.vertices[tri.vertices[2]].location
        si PointDansTriangle(position, v0, v1, v2):
            retourner triangleIndex

    retourner -1  // pas trouve
```

### Etape 2 : A* de triangle en triangle

```
fonction AStar(startTri, goalTri, navmesh):
    openSet = file de priorite
    ajouter startTri avec cout 0
    cameFrom = map vide
    gScore = map, defaut infini
    gScore[startTri] = 0

    tant que openSet non vide:
        current = retirer le triangle avec le plus petit fScore

        si current == goalTri:
            reconstruire le chemin via cameFrom
            retourner chemin

        pour chaque edge i dans [0, 1, 2]:
            neighbor = navmesh.triangles[current].triangles[i]
            si neighbor == 0xFFFF: continuer  // pas de voisin

            // Cout = distance entre centres des triangles
            tentative_g = gScore[current] + distance(centre(current), centre(neighbor))

            si tentative_g < gScore[neighbor]:
                cameFrom[neighbor] = current
                gScore[neighbor] = tentative_g
                fScore = tentative_g + heuristique(centre(neighbor), centre(goalTri))
                ajouter neighbor a openSet avec fScore

    retourner echec  // pas de chemin
```

### Etape 3 : Convertir en waypoints

Le chemin A* donne une liste de triangles. Pour chaque paire de triangles consecutifs, le waypoint est le milieu de l'arete partagee entre les deux triangles. Ca donne un chemin lisse qui passe par le centre des passages.

### Etape 4 : Deplacer le joueur

Deux approches possibles :
1. **SetPosition frame par frame** — teleporter le joueur petit a petit vers chaque waypoint (risque de desync physique)
2. **Modifier la direction de mouvement** — simuler les inputs de deplacement pour que le joueur marche naturellement vers chaque waypoint (plus naturel, respecte la physique)

L'approche 2 est preferable car elle respecte les collisions et la gravite.

### Etape 5 : Gestion des portes

Quand le chemin passe par un portail de porte (`doorPortals`) :
1. Detecter qu'on approche d'une porte
2. Activer la porte (l'ouvrir)
3. Traverser
4. Recharger le navmesh de la nouvelle cellule
5. Continuer le pathfinding

## Gestion des changements de cellule

Quand le joueur passe une porte entre deux cellules :
1. La cellule change (evenement detectable via `RE::TESObjectCELL`)
2. Le navmesh change (nouveau tableau de triangles/vertices)
3. Il faut recalculer le chemin depuis la nouvelle position

Les `extraEdgeInfo` de type `kPortal` indiquent exactement quel triangle de l'autre navmesh est connecte, permettant une transition fluide.

## Classes de pathfinding internes du moteur (forward-declared)

Ces classes existent dans le moteur mais ne sont PAS completement definies dans CommonLibSSE. Elles pourraient servir de reference ou etre utilisees via reverse-engineering :

- **BSPathing** — gestionnaire principal de pathfinding (singleton)
- **BSPathingRequest** / **PathingRequest** — requete de pathfinding
- **BSPathingSolution** — solution (chemin trouve)
- **BSPathingLocation** — position pour pathfinding
- **NavMeshSearchClosePoint** — trouver le point navmesh le plus proche
- **NavMeshSearchMultipleGoals** — pathfinding multi-objectifs
- **PathingRequestSafeStraightLine** — ligne droite avec collision

## Mods existants qui utilisent le navmesh

- **Debug Menu** (Nexus 136456) : affiche les navmeshes en jeu, prouve que l'acces runtime fonctionne
- **Combat Pathing Revolution** (GitHub max-su-2019) : modifie l'IA de combat, code ouvert MIT
- **Navigator** (Nexus 52641) : corrige les navmeshes vanilla au niveau ESP (pas runtime)

Aucun mod existant ne fait de pathfinding custom complet — ce serait une premiere.

## Estimation de complexite

| Composant | Difficulte | Description |
|-----------|-----------|-------------|
| Lookup triangle sous position | Facile | Utiliser meshGrid + test point-dans-triangle |
| A* intra-cellule | Moyen | Algorithme classique sur graphe de triangles |
| Conversion en waypoints | Facile | Milieu des aretes partagees |
| Deplacement du joueur | Moyen | Simuler les inputs ou SetPosition progressif |
| Detection de blocage | Facile | Verifier si position change toutes les N secondes |
| Gestion des portes | Difficile | Detecter, ouvrir, traverser, recharger navmesh |
| Navigation inter-cellules | Difficile | Portails, changement de navmesh, recalcul |
| Pathfinding multi-cellules | Tres difficile | Planifier un chemin a travers plusieurs cellules |

## Approche recommandee

Phase 1 : Anti-blocage simple (rapide a coder)
- Detecter quand le joueur ne bouge plus
- Calculer la direction vers la destination
- Deplacer de quelques metres dans cette direction
- Laisser le Travel package reprendre

Phase 2 : Pathfinding intra-cellule
- Implementer le lookup triangle + A*
- Deplacer le joueur via waypoints dans une meme cellule
- Garder le Travel package pour l'exterieur

Phase 3 : Navigation complete
- Gerer les portes et transitions
- Pathfinding multi-cellules
- Remplacer completement le Travel package

## References

- CommonLibSSE-NG : https://github.com/CharmedBaryon/CommonLibSSE-NG
- Documentation : https://ng.commonlib.dev/
- Format NVNM (navmesh sur disque) : https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/NVNM_Field
- Format NAVI (info navmesh) : https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/NAVI
- Combat Pathing Revolution (code source) : https://github.com/max-su-2019/CombatPathingRevolution
