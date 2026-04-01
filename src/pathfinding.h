#pragma once

// PATHFINDING — Navigation A* custom sur navmesh
// Système parallèle à autowalk.h, activable via Alt+Home
// Ne remplace PAS l'autowalk classique (Shift+Home)
//
// Pipeline : A* sur triangles → corridor → Funnel Algorithm → waypoints lisses
// Mouvement : via Papyrus Travel package (même système que autowalk)

#include <queue>
#include <unordered_map>
#include <unordered_set>

// ============================================================================
// CONFIGURATION
// ============================================================================

static constexpr float PF_WAYPOINT_REACH_DIST   = 80.0f;    // distance pour considérer un waypoint atteint
static constexpr float PF_ARRIVAL_DIST           = 120.0f;   // distance d'arrivée finale
static constexpr float PF_STUCK_THRESHOLD        = 5.0f;     // mouvement minimum (unités)
static constexpr float PF_STUCK_TIMEOUT          = 4.0f;     // secondes bloqué avant récupération
static constexpr float PF_POLL_INTERVAL_MS       = 100;      // intervalle du monitor (ms)
static constexpr float PF_JUMP_Z_THRESHOLD       = 50.0f;    // différence Z pour déclencher un saut
static constexpr float PF_DOOR_APPROACH_DIST     = 200.0f;   // distance pour activer une porte
static constexpr float PF_LEDGE_COST_MULT        = 3.0f;     // coût supplémentaire pour les rebords
static constexpr float PF_PREFERRED_BONUS        = 0.7f;     // multiplicateur de coût pour triangles préférés
static constexpr float PF_SLOPE_PENALTY          = 2.0f;     // pénalité pour les pentes raides (>27°)
static constexpr int   PF_MAX_ITERATIONS         = 50000;    // cap d'itérations A*
static constexpr float PF_FIND_TRI_RADIUS        = 200.0f;   // rayon de recherche si pas sur un triangle
static constexpr float PF_H_SCALE                = 0.999f;   // facteur heuristique (sous-estimation légère, comme Detour)
static constexpr float PF_PI                     = 3.14159265f;

// ============================================================================
// STRUCTURES
// ============================================================================

struct PathWaypoint {
    RE::NiPoint3 position;
    enum class Type : uint8_t {
        kNormal,
        kDoor,
        kJump,
        kLedgeDown,
        kCellTransition
    } type{Type::kNormal};
    RE::FormID doorFormID{0};
};

// Portail pour le Funnel Algorithm : arête partagée entre deux triangles
struct PF_Portal {
    RE::NiPoint3 left;
    RE::NiPoint3 right;
};

// Clé compacte pour le graphe A* : navmeshID (32 bits) + triangleIndex (16 bits)
static inline uint64_t PackNavKey(RE::FormID meshID, uint16_t triIdx) {
    return (static_cast<uint64_t>(meshID) << 32) | triIdx;
}
static inline std::pair<RE::FormID, uint16_t> UnpackNavKey(uint64_t key) {
    return {static_cast<RE::FormID>(key >> 32), static_cast<uint16_t>(key & 0xFFFF)};
}

// Nœud A* pour la file de priorité
struct PFNode {
    uint64_t key;
    float    gCost;
    float    fCost;
    bool operator>(const PFNode& other) const { return fCost > other.fCost; }
};

// ============================================================================
// ÉTAT GLOBAL
// ============================================================================

static std::atomic_bool          g_pathfinding{false};
static std::vector<PathWaypoint> g_pathWaypoints;
static int                       g_pathCurrentWP{0};
static std::wstring              g_pathTargetName;
static RE::FormID                g_pathTargetID{0};
static RE::NiPoint3              g_pathTargetPos{0, 0, 0};
static std::jthread              g_pathMonitor;
static RE::NiPoint3              g_pathLastPos{0, 0, 0};
static float                     g_pathStuckTimer{0.0f};
static int                       g_pathStuckRecoveries{0};
static RE::FormID                g_pathLastCellID{0};
static float                     g_pathLastAnnouncedAngle{-999.0f};
static std::atomic_bool          g_pathJumping{false};

// ============================================================================
// UTILITAIRES MATHÉMATIQUES
// ============================================================================

static float PF_Distance3D(const RE::NiPoint3& a, const RE::NiPoint3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static float PF_Distance2D(const RE::NiPoint3& a, const RE::NiPoint3& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Deux fois l'aire signée du triangle ABC en 2D (produit en croix)
// > 0 : C est à gauche de AB, < 0 : C est à droite, = 0 : colinéaire
static float PF_TriArea2D(const RE::NiPoint3& a, const RE::NiPoint3& b, const RE::NiPoint3& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool PF_VEqual2D(const RE::NiPoint3& a, const RE::NiPoint3& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return (dx * dx + dy * dy) < 0.01f;
}

// Heuristique A* avec sous-estimation légère (H_SCALE) et pénalité altitude
static float PF_Heuristic(const RE::NiPoint3& a, const RE::NiPoint3& b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    float dz = (a.z - b.z) * 1.5f;  // pénalité Z
    return std::sqrt(dx * dx + dy * dy + dz * dz) * PF_H_SCALE;
}

// ============================================================================
// REQUÊTES NAVMESH
// ============================================================================

static std::vector<RE::NavMesh*> PF_GetCellNavmeshes(RE::TESObjectCELL* cell) {
    std::vector<RE::NavMesh*> result;
    if (!cell) return result;
    auto& rd = cell->GetRuntimeData();
    if (!rd.navMeshes) return result;
    for (auto& nm : rd.navMeshes->navMeshes) {
        if (nm) result.push_back(nm.get());
    }
    return result;
}

static std::vector<RE::NavMesh*> PF_GetLoadedNavmeshes() {
    std::vector<RE::NavMesh*> result;
    std::unordered_set<RE::FormID> seen;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return result;
    auto* cell = player->GetParentCell();
    if (!cell) return result;

    auto addMeshes = [&](RE::TESObjectCELL* c) {
        if (!c) return;
        for (auto* nm : PF_GetCellNavmeshes(c)) {
            if (seen.insert(nm->GetFormID()).second)
                result.push_back(nm);
        }
    };

    addMeshes(cell);

    if (!cell->IsInteriorCell()) {
        auto* tes = RE::TES::GetSingleton();
        if (tes && tes->gridCells) {
            for (uint32_t gx = 0; gx < tes->gridCells->length; gx++)
                for (uint32_t gy = 0; gy < tes->gridCells->length; gy++) {
                    auto* adj = tes->gridCells->GetCell(gx, gy);
                    if (adj && adj->IsAttached()) addMeshes(adj);
                }
        }
        auto* ws = player->GetWorldspace();
        if (ws && ws->persistentCell) addMeshes(ws->persistentCell);
    }
    return result;
}

// Test point-dans-triangle 2D (coordonnées barycentriques)
static bool PF_PointInTriangle2D(const RE::NiPoint3& p,
                                  const RE::NiPoint3& a, const RE::NiPoint3& b, const RE::NiPoint3& c) {
    float v0x = c.x - a.x, v0y = c.y - a.y;
    float v1x = b.x - a.x, v1y = b.y - a.y;
    float v2x = p.x - a.x, v2y = p.y - a.y;

    float dot00 = v0x * v0x + v0y * v0y;
    float dot01 = v0x * v1x + v0y * v1y;
    float dot02 = v0x * v2x + v0y * v2y;
    float dot11 = v1x * v1x + v1y * v1y;
    float dot12 = v1x * v2x + v1y * v2y;

    float denom = dot00 * dot11 - dot01 * dot01;
    if (std::abs(denom) < 1e-10f) return false;
    float inv = 1.0f / denom;
    float u = (dot11 * dot02 - dot01 * dot12) * inv;
    float v = (dot00 * dot12 - dot01 * dot02) * inv;
    return u >= -0.01f && v >= -0.01f && (u + v) <= 1.02f;
}

// Centroïde avec vérification de bornes
static RE::NiPoint3 PF_TriangleCentroid(const RE::NavMesh* mesh, uint16_t triIdx) {
    if (triIdx >= mesh->triangles.size()) return {0, 0, 0};
    auto& tri = mesh->triangles[triIdx];
    if (tri.vertices[0] >= mesh->vertices.size() ||
        tri.vertices[1] >= mesh->vertices.size() ||
        tri.vertices[2] >= mesh->vertices.size()) return {0, 0, 0};
    auto& v0 = mesh->vertices[tri.vertices[0]].location;
    auto& v1 = mesh->vertices[tri.vertices[1]].location;
    auto& v2 = mesh->vertices[tri.vertices[2]].location;
    return {(v0.x + v1.x + v2.x) / 3.0f, (v0.y + v1.y + v2.y) / 3.0f, (v0.z + v1.z + v2.z) / 3.0f};
}

// Trouver les 2 sommets partagés entre deux triangles du même mesh
// Retourne le nombre de sommets trouvés (0, 1 ou 2)
static int PF_SharedVertices(const RE::NavMesh* mesh, uint16_t triA, uint16_t triB,
                              RE::NiPoint3& outV0, RE::NiPoint3& outV1) {
    if (triA >= mesh->triangles.size() || triB >= mesh->triangles.size()) return 0;
    auto& tA = mesh->triangles[triA];
    auto& tB = mesh->triangles[triB];
    int count = 0;
    RE::NiPoint3 shared[2];
    for (int i = 0; i < 3 && count < 2; i++) {
        for (int j = 0; j < 3; j++) {
            if (tA.vertices[i] == tB.vertices[j]) {
                if (tA.vertices[i] < mesh->vertices.size())
                    shared[count++] = mesh->vertices[tA.vertices[i]].location;
                break;
            }
        }
    }
    if (count >= 1) outV0 = shared[0];
    if (count >= 2) outV1 = shared[1];
    return count;
}

// Milieu de l'arête partagée (fallback si Funnel pas applicable)
static RE::NiPoint3 PF_SharedEdgeMidpoint(const RE::NavMesh* mesh, uint16_t triA, uint16_t triB) {
    RE::NiPoint3 v0, v1;
    if (PF_SharedVertices(mesh, triA, triB, v0, v1) == 2) {
        return {(v0.x + v1.x) / 2.0f, (v0.y + v1.y) / 2.0f, (v0.z + v1.z) / 2.0f};
    }
    return PF_TriangleCentroid(mesh, triB);
}

// Trouver le triangle contenant une position
struct PF_TriResult { RE::NavMesh* mesh{nullptr}; uint16_t triIdx{0}; bool found{false}; };

static PF_TriResult PF_FindTriangle(const RE::NiPoint3& pos, const std::vector<RE::NavMesh*>& meshes) {
    PF_TriResult best;
    float bestZDiff = 1e10f;

    for (auto* mesh : meshes) {
        if (!mesh || mesh->triangles.empty() || mesh->vertices.empty()) continue;

        auto& grid = mesh->meshGrid;
        bool gridSearched = false;

        if (grid.gridSize > 0 && grid.columnSectionLen > 0 && grid.rowSectionLen > 0) {
            int col = static_cast<int>((pos.x - grid.gridBoundsMin.x) / grid.columnSectionLen);
            int row = static_cast<int>((pos.y - grid.gridBoundsMin.y) / grid.rowSectionLen);
            if (col >= 0 && col < static_cast<int>(grid.gridSize) &&
                row >= 0 && row < static_cast<int>(grid.gridSize)) {
                uint32_t gridIdx = static_cast<uint32_t>(row) * grid.gridSize + static_cast<uint32_t>(col);
                if (gridIdx < grid.gridData.size()) {
                    gridSearched = true;
                    auto& candidates = grid.gridData[gridIdx];
                    for (uint32_t c = 0; c < candidates.size(); c++) {
                        uint16_t ti = candidates[c];
                        if (ti >= mesh->triangles.size()) continue;
                        auto& tri = mesh->triangles[ti];
                        if (static_cast<uint16_t>(tri.triangleFlags.get()) &
                            static_cast<uint16_t>(RE::BSNavmeshTriangle::TriangleFlag::kDeleted)) continue;
                        auto& va = mesh->vertices[tri.vertices[0]].location;
                        auto& vb = mesh->vertices[tri.vertices[1]].location;
                        auto& vc = mesh->vertices[tri.vertices[2]].location;
                        if (PF_PointInTriangle2D(pos, va, vb, vc)) {
                            float zDiff = std::abs(pos.z - (va.z + vb.z + vc.z) / 3.0f);
                            if (zDiff < bestZDiff) { bestZDiff = zDiff; best = {mesh, ti, true}; }
                        }
                    }
                }
            }
        }

        if (!best.found && !gridSearched) {
            for (uint32_t ti = 0; ti < mesh->triangles.size(); ti++) {
                auto& tri = mesh->triangles[ti];
                if (static_cast<uint16_t>(tri.triangleFlags.get()) &
                    static_cast<uint16_t>(RE::BSNavmeshTriangle::TriangleFlag::kDeleted)) continue;
                auto& va = mesh->vertices[tri.vertices[0]].location;
                auto& vb = mesh->vertices[tri.vertices[1]].location;
                auto& vc = mesh->vertices[tri.vertices[2]].location;
                if (PF_PointInTriangle2D(pos, va, vb, vc)) {
                    float zDiff = std::abs(pos.z - (va.z + vb.z + vc.z) / 3.0f);
                    if (zDiff < bestZDiff) { bestZDiff = zDiff; best = {mesh, static_cast<uint16_t>(ti), true}; }
                }
            }
        }
    }
    return best;
}

// Recherche en spirale si FindTriangle échoue
static PF_TriResult PF_FindTriangleNear(const RE::NiPoint3& pos, const std::vector<RE::NavMesh*>& meshes) {
    auto result = PF_FindTriangle(pos, meshes);
    if (result.found) return result;
    for (float r = 32.0f; r <= PF_FIND_TRI_RADIUS; r += 32.0f) {
        for (float angle = 0; angle < 2 * PF_PI; angle += PF_PI / 4.0f) {
            RE::NiPoint3 test = {pos.x + r * std::cos(angle), pos.y + r * std::sin(angle), pos.z};
            result = PF_FindTriangle(test, meshes);
            if (result.found) return result;
        }
    }
    return {};
}

// ============================================================================
// INDEX DES PORTAILS (extraEdgeInfo)
// ============================================================================

struct PF_PortalIndex {
    std::unordered_map<uint64_t, RE::BSNavmeshEdgeExtraInfo> portals;

    static uint64_t PortalKey(uint16_t triIdx, int edge) {
        return (static_cast<uint64_t>(triIdx) << 4) | static_cast<uint64_t>(edge);
    }

    void Build(const RE::NavMesh* mesh) {
        portals.clear();
        uint32_t linkIdx = 0;
        for (uint16_t t = 0; t < mesh->triangles.size(); t++) {
            auto flags = static_cast<uint16_t>(mesh->triangles[t].triangleFlags.get());
            for (int e = 0; e < 3; e++) {
                if (flags & static_cast<uint16_t>(1 << e)) {
                    if (linkIdx < mesh->extraEdgeInfo.size())
                        portals[PortalKey(t, e)] = mesh->extraEdgeInfo[linkIdx];
                    linkIdx++;
                }
            }
        }
    }

    const RE::BSNavmeshEdgeExtraInfo* Get(uint16_t triIdx, int edge) const {
        auto it = portals.find(PortalKey(triIdx, edge));
        return it != portals.end() ? &it->second : nullptr;
    }
};

// ============================================================================
// ALGORITHME A* AMÉLIORÉ
// ============================================================================

// Coût de traversée d'une arête : distance milieu-arête + pénalités pente/terrain
static float PF_EdgeCost(const RE::NiPoint3& from, const RE::NiPoint3& to,
                          const RE::NavMesh* mesh, uint16_t neighborIdx) {
    float dist = PF_Distance3D(from, to);

    // Pénalité de pente
    float dist2D = PF_Distance2D(from, to);
    if (dist2D > 1.0f) {
        float slope = std::abs(to.z - from.z) / dist2D;
        if (slope > 0.5f)  // >27° de pente
            dist *= PF_SLOPE_PENALTY;
    }

    // Bonus triangles préférés
    if (neighborIdx < mesh->triangles.size()) {
        auto nFlags = static_cast<uint16_t>(mesh->triangles[neighborIdx].triangleFlags.get());
        if (nFlags & static_cast<uint16_t>(RE::BSNavmeshTriangle::TriangleFlag::kPreferred))
            dist *= PF_PREFERRED_BONUS;
    }

    return dist;
}

static std::vector<PathWaypoint> PF_AStarPath(const RE::NiPoint3& startPos,
                                               const RE::NiPoint3& goalPos,
                                               const std::vector<RE::NavMesh*>& meshes) {
    std::vector<PathWaypoint> waypoints;

    // Index des meshes et portails
    std::unordered_map<RE::FormID, RE::NavMesh*> meshByID;
    std::unordered_map<RE::FormID, PF_PortalIndex> portalsByMesh;
    for (auto* m : meshes) {
        auto id = m->GetFormID();
        meshByID[id] = m;
        portalsByMesh[id].Build(m);
    }

    auto startResult = PF_FindTriangleNear(startPos, meshes);
    auto goalResult = PF_FindTriangleNear(goalPos, meshes);
    if (!startResult.found || !goalResult.found) {
        LOG("Pathfinding: start or goal not on navmesh");
        return waypoints;
    }

    uint64_t startKey = PackNavKey(startResult.mesh->GetFormID(), startResult.triIdx);
    uint64_t goalKey  = PackNavKey(goalResult.mesh->GetFormID(), goalResult.triIdx);

    LOG("Pathfinding: A* from mesh {:08X} tri {} to mesh {:08X} tri {}",
        startResult.mesh->GetFormID(), startResult.triIdx,
        goalResult.mesh->GetFormID(), goalResult.triIdx);

    if (startKey == goalKey) {
        waypoints.push_back({goalPos, PathWaypoint::Type::kNormal});
        return waypoints;
    }

    // A* avec pré-réservation
    std::priority_queue<PFNode, std::vector<PFNode>, std::greater<PFNode>> openSet;
    std::unordered_map<uint64_t, float>    gScore;    gScore.reserve(4096);
    std::unordered_map<uint64_t, uint64_t> cameFrom;  cameFrom.reserve(4096);
    std::unordered_map<uint64_t, RE::EDGE_EXTRA_INFO_TYPE> edgeTypes;

    gScore[startKey] = 0.0f;
    openSet.push({startKey, 0.0f, PF_Heuristic(PF_TriangleCentroid(startResult.mesh, startResult.triIdx), goalPos)});

    int iterations = 0;
    bool found = false;

    while (!openSet.empty() && iterations < PF_MAX_ITERATIONS) {
        iterations++;
        auto current = openSet.top(); openSet.pop();

        if (current.key == goalKey) { found = true; break; }

        auto gIt = gScore.find(current.key);
        if (gIt != gScore.end() && current.gCost > gIt->second + 0.1f) continue;

        auto [curMeshID, curTriIdx] = UnpackNavKey(current.key);
        auto meshIt = meshByID.find(curMeshID);
        if (meshIt == meshByID.end()) continue;
        auto* curMesh = meshIt->second;
        if (curTriIdx >= curMesh->triangles.size()) continue;

        auto& curTri = curMesh->triangles[curTriIdx];
        auto curCenter = PF_TriangleCentroid(curMesh, curTriIdx);
        auto curFlags = static_cast<uint16_t>(curTri.triangleFlags.get());

        for (int edge = 0; edge < 3; edge++) {
            uint16_t edgeFlag = static_cast<uint16_t>(1 << edge);
            bool isPortal = (curFlags & edgeFlag) != 0;

            if (isPortal) {
                auto portalIt = portalsByMesh.find(curMeshID);
                if (portalIt == portalsByMesh.end()) continue;
                auto* info = portalIt->second.Get(curTriIdx, edge);
                if (!info || info->type.get() == RE::EDGE_EXTRA_INFO_TYPE::kInvalid) continue;

                auto type = info->type.get();
                RE::FormID targetMeshID = info->portal.otherMeshID;
                uint16_t targetTriIdx = info->portal.triangle;
                auto targetMeshIt = meshByID.find(targetMeshID);
                if (targetMeshIt == meshByID.end()) continue;
                if (targetTriIdx >= targetMeshIt->second->triangles.size()) continue;

                uint64_t nKey = PackNavKey(targetMeshID, targetTriIdx);
                auto nCenter = PF_TriangleCentroid(targetMeshIt->second, targetTriIdx);
                float cost = PF_Distance3D(curCenter, nCenter);
                if (type == RE::EDGE_EXTRA_INFO_TYPE::kLedgeUp || type == RE::EDGE_EXTRA_INFO_TYPE::kLedgeDown)
                    cost *= PF_LEDGE_COST_MULT;

                float tentG = current.gCost + cost;
                if (gScore.find(nKey) == gScore.end() || tentG < gScore[nKey]) {
                    gScore[nKey] = tentG;
                    cameFrom[nKey] = current.key;
                    edgeTypes[nKey] = type;
                    openSet.push({nKey, tentG, tentG + PF_Heuristic(nCenter, goalPos)});
                }
            } else {
                uint16_t neighborIdx = curTri.triangles[edge];
                if (neighborIdx == 0xFFFF || neighborIdx >= curMesh->triangles.size()) continue;
                auto nFlags = static_cast<uint16_t>(curMesh->triangles[neighborIdx].triangleFlags.get());
                if (nFlags & static_cast<uint16_t>(RE::BSNavmeshTriangle::TriangleFlag::kDeleted)) continue;

                uint64_t nKey = PackNavKey(curMeshID, neighborIdx);
                // Coût = distance milieu d'arête + pénalités
                auto edgeMid = PF_SharedEdgeMidpoint(curMesh, curTriIdx, neighborIdx);
                float cost = PF_EdgeCost(curCenter, edgeMid, curMesh, neighborIdx);

                float tentG = current.gCost + cost;
                if (gScore.find(nKey) == gScore.end() || tentG < gScore[nKey]) {
                    gScore[nKey] = tentG;
                    cameFrom[nKey] = current.key;
                    openSet.push({nKey, tentG, tentG + PF_Heuristic(edgeMid, goalPos)});
                }
            }
        }
    }

    LOG("Pathfinding: A* completed in {} iterations, found={}", iterations, found);
    if (!found) return waypoints;

    // Reconstruction du corridor (séquence de triangles)
    std::vector<uint64_t> pathKeys;
    uint64_t key = goalKey;
    while (key != startKey) {
        pathKeys.push_back(key);
        auto it = cameFrom.find(key);
        if (it == cameFrom.end()) break;
        key = it->second;
    }
    pathKeys.push_back(startKey);
    std::reverse(pathKeys.begin(), pathKeys.end());

    // ================================================================
    // FUNNEL ALGORITHM — chemin lisse à travers le corridor
    // ================================================================

    // Vérifier si tout le corridor est dans le même navmesh (Funnel applicable)
    bool singleMesh = true;
    RE::FormID corridorMeshID = UnpackNavKey(pathKeys[0]).first;
    for (auto& pk : pathKeys) {
        if (UnpackNavKey(pk).first != corridorMeshID) { singleMesh = false; break; }
    }

    if (singleMesh && pathKeys.size() >= 2) {
        auto* mesh = meshByID[corridorMeshID];

        // Extraire les portails du corridor
        std::vector<PF_Portal> portals;
        portals.push_back({startPos, startPos});  // portail de départ

        for (size_t i = 0; i + 1 < pathKeys.size(); i++) {
            uint16_t triA = UnpackNavKey(pathKeys[i]).second;
            uint16_t triB = UnpackNavKey(pathKeys[i + 1]).second;

            RE::NiPoint3 v0, v1;
            if (PF_SharedVertices(mesh, triA, triB, v0, v1) == 2) {
                // Déterminer gauche/droite par rapport à la direction de marche
                auto centerA = PF_TriangleCentroid(mesh, triA);
                auto centerB = PF_TriangleCentroid(mesh, triB);
                float cross = PF_TriArea2D(centerA, centerB, v0);
                if (cross > 0)
                    portals.push_back({v0, v1});
                else
                    portals.push_back({v1, v0});
            }
        }

        portals.push_back({goalPos, goalPos});  // portail de destination

        // Funnel Algorithm (Simple Stupid Funnel Algorithm - SSFA)
        RE::NiPoint3 apex = portals[0].left;
        RE::NiPoint3 pLeft = apex, pRight = apex;
        int apexIdx = 0, leftIdx = 0, rightIdx = 0;
        std::vector<RE::NiPoint3> funnelPath;
        funnelPath.push_back(apex);

        int nPortals = static_cast<int>(portals.size());
        for (int i = 1; i < nPortals; i++) {
            auto& left = portals[i].left;
            auto& right = portals[i].right;

            // Mise à jour côté droit
            if (PF_TriArea2D(apex, pRight, right) <= 0.0f) {
                if (PF_VEqual2D(apex, pRight) || PF_TriArea2D(apex, pLeft, right) > 0.0f) {
                    pRight = right;
                    rightIdx = i;
                } else {
                    funnelPath.push_back(pLeft);
                    apex = pLeft;
                    apexIdx = leftIdx;
                    pLeft = apex; pRight = apex;
                    leftIdx = apexIdx; rightIdx = apexIdx;
                    i = apexIdx;
                    continue;
                }
            }

            // Mise à jour côté gauche
            if (PF_TriArea2D(apex, pLeft, left) >= 0.0f) {
                if (PF_VEqual2D(apex, pLeft) || PF_TriArea2D(apex, pRight, left) < 0.0f) {
                    pLeft = left;
                    leftIdx = i;
                } else {
                    funnelPath.push_back(pRight);
                    apex = pRight;
                    apexIdx = rightIdx;
                    pLeft = apex; pRight = apex;
                    leftIdx = apexIdx; rightIdx = apexIdx;
                    i = apexIdx;
                    continue;
                }
            }
        }

        // Ajouter la destination
        if (funnelPath.empty() || !PF_VEqual2D(funnelPath.back(), goalPos))
            funnelPath.push_back(goalPos);

        // Convertir en waypoints
        for (size_t i = 1; i < funnelPath.size(); i++) {
            PathWaypoint wp;
            wp.position = funnelPath[i];
            wp.type = PathWaypoint::Type::kNormal;
            waypoints.push_back(wp);
        }

        LOG("Pathfinding: Funnel produced {} waypoints from {} corridor triangles",
            waypoints.size(), pathKeys.size());
    } else {
        // Corridor multi-navmesh : fallback aux milieux d'arêtes + types de transition
        for (size_t i = 1; i < pathKeys.size(); i++) {
            auto [meshID_a, tri_a] = UnpackNavKey(pathKeys[i - 1]);
            auto [meshID_b, tri_b] = UnpackNavKey(pathKeys[i]);

            PathWaypoint wp;
            if (meshID_a == meshID_b) {
                wp.position = PF_SharedEdgeMidpoint(meshByID[meshID_a], tri_a, tri_b);
                wp.type = PathWaypoint::Type::kNormal;
            } else {
                wp.position = PF_TriangleCentroid(meshByID[meshID_b], tri_b);
                auto etIt = edgeTypes.find(pathKeys[i]);
                if (etIt != edgeTypes.end()) {
                    if (etIt->second == RE::EDGE_EXTRA_INFO_TYPE::kLedgeUp) wp.type = PathWaypoint::Type::kJump;
                    else if (etIt->second == RE::EDGE_EXTRA_INFO_TYPE::kLedgeDown) wp.type = PathWaypoint::Type::kLedgeDown;
                    else wp.type = PathWaypoint::Type::kCellTransition;
                }
            }
            waypoints.push_back(wp);
        }
        waypoints.push_back({goalPos, PathWaypoint::Type::kNormal});

        LOG("Pathfinding: multi-mesh fallback, {} waypoints from {} corridor triangles",
            waypoints.size(), pathKeys.size());
    }

    return waypoints;
}

// ============================================================================
// GESTION DES PORTES
// ============================================================================

static RE::TESObjectREFR* PF_FindDoorNear(const RE::NiPoint3& pos) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return nullptr;
    RE::TESObjectREFR* bestDoor = nullptr;
    float bestDist = 300.0f;

    auto searchCell = [&](RE::TESObjectCELL* cell) {
        if (!cell) return;
        for (auto& ref : cell->GetRuntimeData().references) {
            if (!ref) continue;
            auto* base = ref->GetBaseObject();
            if (!base || base->GetFormType() != RE::FormType::Door) continue;
            float d = PF_Distance3D(ref->GetPosition(), pos);
            if (d < bestDist) { bestDist = d; bestDoor = ref.get(); }
        }
    };

    auto* cell = player->GetParentCell();
    searchCell(cell);
    if (cell && !cell->IsInteriorCell()) {
        auto* tes = RE::TES::GetSingleton();
        if (tes && tes->gridCells) {
            for (uint32_t gx = 0; gx < tes->gridCells->length; gx++)
                for (uint32_t gy = 0; gy < tes->gridCells->length; gy++) {
                    auto* adj = tes->gridCells->GetCell(gx, gy);
                    if (adj && adj->IsAttached() && adj != cell) searchCell(adj);
                }
        }
    }
    return bestDoor;
}

static void PF_AnnotateDoors(std::vector<PathWaypoint>& waypoints, const std::vector<RE::NavMesh*>& meshes) {
    std::unordered_map<RE::FormID, std::vector<uint16_t>> doorTriangles;
    for (auto* mesh : meshes) {
        auto meshID = mesh->GetFormID();
        for (auto& dp : mesh->doorPortals) doorTriangles[meshID].push_back(dp.owningTriangleIndex);
        for (auto& cd : mesh->closedDoors) doorTriangles[meshID].push_back(cd.triangleIndex);
    }
    if (doorTriangles.empty()) return;

    for (auto& wp : waypoints) {
        if (wp.type != PathWaypoint::Type::kNormal) continue;
        for (auto* mesh : meshes) {
            auto it = doorTriangles.find(mesh->GetFormID());
            if (it == doorTriangles.end()) continue;
            for (uint16_t triIdx : it->second) {
                if (triIdx >= mesh->triangles.size()) continue;
                if (PF_Distance2D(wp.position, PF_TriangleCentroid(mesh, triIdx)) < PF_DOOR_APPROACH_DIST) {
                    auto* doorRef = PF_FindDoorNear(PF_TriangleCentroid(mesh, triIdx));
                    if (doorRef) {
                        wp.type = PathWaypoint::Type::kDoor;
                        wp.doorFormID = doorRef->GetFormID();
                    }
                    break;
                }
            }
        }
    }
}

// ============================================================================
// MOUVEMENT (via Papyrus, même système que autowalk)
// ============================================================================

static void StopPathfinding();
static void PF_RecalculatePath();
static void PF_WalkToWaypoint(const RE::NiPoint3& pos);
static void PF_StopPapyrusWalk();

static void PF_HandleDoor(const PathWaypoint& wp) {
    auto* form = RE::TESForm::LookupByID(wp.doorFormID);
    if (!form) return;
    auto* ref = form->AsReference();
    if (!ref) return;

    auto* extraLock = ref->extraList.GetByType<RE::ExtraLock>();
    if (extraLock && extraLock->lock && extraLock->lock->IsLocked()) {
        SpeakQueue(L"Door is locked");
        return;
    }

    SpeakQueue(L"Opening door");
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) ref->ActivateRef(player, 0, nullptr, 0, false);
}

static void PF_TriggerJump() {
    g_pathJumping.store(true);
    keybd_event(VK_SPACE, 0, 0, 0);
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        g_pathJumping.store(false);
    }).detach();
}

static void PF_AnnounceDirection(const RE::NiPoint3& from, const RE::NiPoint3& to) {
    float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
    float angle = std::atan2(dx, dy) * 180.0f / PF_PI;
    if (angle < 0) angle += 360.0f;

    if (g_pathLastAnnouncedAngle >= 0) {
        float diff = std::abs(angle - g_pathLastAnnouncedAngle);
        if (diff > 180.0f) diff = 360.0f - diff;
        if (diff < 30.0f) return;
    }
    g_pathLastAnnouncedAngle = angle;

    const wchar_t* dir = L"";
    if (angle >= 337.5f || angle < 22.5f)   dir = L"North";
    else if (angle < 67.5f)                  dir = L"Northeast";
    else if (angle < 112.5f)                 dir = L"East";
    else if (angle < 157.5f)                 dir = L"Southeast";
    else if (angle < 202.5f)                 dir = L"South";
    else if (angle < 247.5f)                 dir = L"Southwest";
    else if (angle < 292.5f)                 dir = L"West";
    else                                     dir = L"Northwest";

    SpeakQueue(std::wstring(L"Going ") + dir);
    if (dz > PF_JUMP_Z_THRESHOLD) SpeakQueue(L"Going upstairs");
    else if (dz < -PF_JUMP_Z_THRESHOLD) SpeakQueue(L"Going downstairs");
}

static void PF_HandleStuck() {
    g_pathStuckRecoveries++;
    g_pathStuckTimer = 0.0f;

    if (g_pathStuckRecoveries > 5) {
        Speak(L"Cannot reach target");
        StopPathfinding();
        return;
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    if (g_pathStuckRecoveries <= 2) {
        SpeakQueue(L"Obstacle, jumping");
        LOG("Pathfinding: stuck recovery #{} - jumping", g_pathStuckRecoveries);
        PF_StopPapyrusWalk();
        PF_TriggerJump();
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
            if (g_pathfinding.load() && g_pathCurrentWP < static_cast<int>(g_pathWaypoints.size()))
                PF_WalkToWaypoint(g_pathWaypoints[g_pathCurrentWP].position);
        }).detach();
    } else if (g_pathStuckRecoveries == 3) {
        SpeakQueue(L"Trying alternate route");
        LOG("Pathfinding: stuck recovery #3 - lateral offset");
        PF_StopPapyrusWalk();
        if (g_pathCurrentWP < static_cast<int>(g_pathWaypoints.size())) {
            auto& wp = g_pathWaypoints[g_pathCurrentWP];
            auto playerPos = player->GetPosition();
            float dx = wp.position.x - playerPos.x, dy = wp.position.y - playerPos.y;
            RE::NiPoint3 lateralPos = {playerPos.x - dy * 0.3f, playerPos.y + dx * 0.3f, playerPos.z};
            PathWaypoint lateralWP; lateralWP.position = lateralPos;
            g_pathWaypoints.insert(g_pathWaypoints.begin() + g_pathCurrentWP, lateralWP);
            PF_WalkToWaypoint(lateralPos);
        }
    } else {
        SpeakQueue(L"Recalculating route");
        LOG("Pathfinding: stuck recovery #{} - recalculating", g_pathStuckRecoveries);
        PF_StopPapyrusWalk();
        PF_RecalculatePath();
    }
}

// Envoyer un waypoint au script Papyrus
static void PF_WalkToWaypoint(const RE::NiPoint3& pos) {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddTask([pos]() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        auto* avo = player->AsActorValueOwner();
        if (avo) {
            float base = avo->GetBaseActorValue(RE::ActorValue::kSpeedMult);
            float current = avo->GetActorValue(RE::ActorValue::kSpeedMult);
            if (base > 0 && current != base) avo->SetActorValue(RE::ActorValue::kSpeedMult, base);
        }
        player->SetAIDriven(false);
        player->EvaluatePackage();

        auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SkyrimTTS_AutoWalkQuest");
        if (!quest) { LOG("Pathfinding: quest not found"); StopPathfinding(); return; }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return;
        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) return;
        auto handle = policy->GetHandleForObject(RE::FormType::Quest, quest);
        if (handle == policy->EmptyHandle()) return;

        auto* args = RE::MakeFunctionArguments(
            static_cast<std::int32_t>(0),
            static_cast<float>(PF_WAYPOINT_REACH_DIST),
            static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z));

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        vm->DispatchMethodCall(handle, RE::BSFixedString("SkyrimTTS_AutoWalk"),
                               RE::BSFixedString("OnWalkToTarget"), args, callback);
        LOG("Pathfinding: Papyrus walk to ({:.0f},{:.0f},{:.0f})", pos.x, pos.y, pos.z);
    });
}

static void PF_StopPapyrusWalk() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddTask([]() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            player->SetAIDriven(false);
            auto* avo = player->AsActorValueOwner();
            if (avo) {
                float base = avo->GetBaseActorValue(RE::ActorValue::kSpeedMult);
                avo->SetActorValue(RE::ActorValue::kSpeedMult, base);
            }
            player->EvaluatePackage();
        }
        auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SkyrimTTS_AutoWalkQuest");
        if (!quest) return;
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return;
        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) return;
        auto handle = policy->GetHandleForObject(RE::FormType::Quest, quest);
        if (handle == policy->EmptyHandle()) return;
        auto* args = RE::MakeFunctionArguments();
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        vm->DispatchMethodCall(handle, RE::BSFixedString("SkyrimTTS_AutoWalk"),
                               RE::BSFixedString("OnStopWalking"), args, callback);
    });
}

// Monitor thread
static void PF_StartMonitor() {
    if (g_pathMonitor.joinable()) { g_pathMonitor.request_stop(); g_pathMonitor.join(); }

    g_pathStuckTimer = 0.0f;
    g_pathStuckRecoveries = 0;
    g_pathLastAnnouncedAngle = -999.0f;

    if (!g_pathWaypoints.empty()) PF_WalkToWaypoint(g_pathWaypoints[0].position);

    g_pathMonitor = std::jthread([](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(PF_POLL_INTERVAL_MS)));
            if (!g_pathfinding.load()) break;

            if (!g_pathJumping.load() && IsMovementInputActive()) {
                Speak(L"Stopping");
                g_pathfinding.store(false);
                LOG("Pathfinding: cancelled by user input");
                PF_StopPapyrusWalk();
                break;
            }

            auto* taskIf = SKSE::GetTaskInterface();
            if (!taskIf) continue;

            taskIf->AddTask([]() {
                if (!g_pathfinding.load()) return;
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player) return;
                auto playerPos = player->GetPosition();

                // Détection changement de cellule
                auto* cell = player->GetParentCell();
                if (cell) {
                    RE::FormID cellID = cell->GetFormID();
                    if (g_pathLastCellID != 0 && cellID != g_pathLastCellID) {
                        g_pathLastCellID = cellID;
                        PF_StopPapyrusWalk();
                        SpeakQueue(L"New area, recalculating");
                        PF_RecalculatePath();
                        return;
                    }
                    g_pathLastCellID = cellID;
                }

                if (g_pathCurrentWP >= static_cast<int>(g_pathWaypoints.size())) {
                    Speak(L"Arrived at " + g_pathTargetName);
                    StopPathfinding();
                    return;
                }

                auto& wp = g_pathWaypoints[g_pathCurrentWP];
                float dist2D = PF_Distance2D(playerPos, wp.position);

                if (dist2D < PF_WAYPOINT_REACH_DIST) {
                    LOG("Pathfinding: reached WP#{} at ({:.0f},{:.0f},{:.0f})",
                        g_pathCurrentWP, playerPos.x, playerPos.y, playerPos.z);
                    g_pathCurrentWP++;
                    g_pathStuckTimer = 0.0f;
                    g_pathStuckRecoveries = 0;

                    if (g_pathCurrentWP >= static_cast<int>(g_pathWaypoints.size())) {
                        Speak(L"Arrived at " + g_pathTargetName);
                        StopPathfinding();
                        return;
                    }

                    auto& nextWP = g_pathWaypoints[g_pathCurrentWP];

                    switch (nextWP.type) {
                    case PathWaypoint::Type::kDoor:   PF_HandleDoor(nextWP); break;
                    case PathWaypoint::Type::kJump:   SpeakQueue(L"Jumping"); PF_TriggerJump(); break;
                    case PathWaypoint::Type::kLedgeDown: SpeakQueue(L"Dropping down"); break;
                    case PathWaypoint::Type::kCellTransition: SpeakQueue(L"Entering new area"); break;
                    default: break;
                    }

                    PF_AnnounceDirection(playerPos, nextWP.position);
                    PF_WalkToWaypoint(nextWP.position);
                    return;
                }

                // Détection de blocage
                float movedDist = PF_Distance3D(playerPos, g_pathLastPos);
                if (movedDist < PF_STUCK_THRESHOLD) {
                    g_pathStuckTimer += PF_POLL_INTERVAL_MS / 1000.0f;
                    if (g_pathStuckTimer >= 1.0f && static_cast<int>(g_pathStuckTimer * 10) % 10 == 0) {
                        LOG("Pathfinding: stuck {:.1f}s at ({:.0f},{:.0f},{:.0f}) -> WP#{} dist={:.0f}",
                            g_pathStuckTimer, playerPos.x, playerPos.y, playerPos.z, g_pathCurrentWP, dist2D);
                    }
                } else {
                    g_pathStuckTimer = 0.0f;
                    g_pathLastPos = playerPos;
                }

                if (g_pathStuckTimer > PF_STUCK_TIMEOUT) PF_HandleStuck();
            });
        }
    });
}

// ============================================================================
// RECALCUL ET API PUBLIQUE
// ============================================================================

static void PF_RecalculatePath() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    auto meshes = PF_GetLoadedNavmeshes();
    auto path = PF_AStarPath(player->GetPosition(), g_pathTargetPos, meshes);
    if (path.empty()) { Speak(L"No path found"); StopPathfinding(); return; }
    PF_AnnotateDoors(path, meshes);
    g_pathWaypoints = path;
    g_pathCurrentWP = 0;
    g_pathStuckTimer = 0.0f;
    g_pathLastPos = player->GetPosition();
    LOG("Pathfinding: recalculated, {} waypoints", path.size());
    if (!path.empty()) PF_WalkToWaypoint(path[0].position);
}

static void StopPathfinding() {
    g_pathfinding.store(false);
    if (g_pathMonitor.joinable()) g_pathMonitor.request_stop();
    PF_StopPapyrusWalk();
    g_pathWaypoints.clear();
    g_pathCurrentWP = 0;
    g_pathTargetName.clear();
    LOG("Pathfinding: stopped");
}

static RE::NiPoint3 PF_ResolveTargetPosition(RE::FormID targetID) {
    auto* form = RE::TESForm::LookupByID(targetID);
    if (form) {
        auto* ref = form->AsReference();
        if (ref && ref->Is3DLoaded()) return ref->GetPosition();
    }
    if (g_scanIndex >= 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
        auto& obj = *g_scannedFiltered[g_scanIndex];
        if (obj.lastKnownPos.x != 0 || obj.lastKnownPos.y != 0) return obj.lastKnownPos;
    }
    return {0, 0, 0};
}

static void TogglePathfinding() {
    if (g_pathfinding.load()) { Speak(L"Stopping navigation"); StopPathfinding(); return; }
    if (g_autoWalking.load()) StopAutoWalk();

    RE::FormID targetID = ScannerGetCurrentFormID();
    if (targetID == 0) { Speak(L"No target selected"); return; }

    g_pathTargetName = ScannerGetCurrentName();
    if (g_pathTargetName.empty()) g_pathTargetName = L"target";
    g_pathTargetID = targetID;

    auto targetPos = PF_ResolveTargetPosition(targetID);
    if (targetPos.x == 0 && targetPos.y == 0 && targetPos.z == 0) {
        Speak(L"Cannot determine target position"); return;
    }

    g_pathTargetPos = targetPos;
    Speak(L"Navigating to " + g_pathTargetName);

    auto* taskIf = SKSE::GetTaskInterface();
    if (!taskIf) return;
    taskIf->AddTask([]() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        auto meshes = PF_GetLoadedNavmeshes();
        if (meshes.empty()) { Speak(L"No navigation data"); return; }
        LOG("Pathfinding: {} navmeshes loaded", meshes.size());

        auto path = PF_AStarPath(player->GetPosition(), g_pathTargetPos, meshes);
        if (path.empty()) { Speak(L"No path found, try autowalk"); return; }
        PF_AnnotateDoors(path, meshes);

        g_pathWaypoints = path;
        g_pathCurrentWP = 0;
        g_pathStuckTimer = 0.0f;
        g_pathStuckRecoveries = 0;
        g_pathLastPos = player->GetPosition();
        auto* cell = player->GetParentCell();
        g_pathLastCellID = cell ? cell->GetFormID() : 0;
        g_pathfinding.store(true);

        LOG("Pathfinding: started with {} waypoints", path.size());
        PF_StartMonitor();
    });
}

static void PathfindingSafetyReset() {
    if (g_pathfinding.load()) { StopPathfinding(); LOG("Pathfinding: safety reset on load"); }
}

// PATHFINDING — FIN
