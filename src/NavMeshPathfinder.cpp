#include "NavMeshPathfinder.h"
#include <cmath>
#include <algorithm>

NavMeshPathfinder* NavMeshPathfinder::GetSingleton()
{
    static NavMeshPathfinder singleton;
    return &singleton;
}

std::vector<RE::NavMesh*> NavMeshPathfinder::GetNavMeshesForPosition(const RE::NiPoint3& a_pos)
{
    std::vector<RE::NavMesh*> result;

    auto* tes = RE::TES::GetSingleton();
    if (!tes) return result;

    auto* cell = tes->GetCell(a_pos);
    if (!cell) return result;

    auto& runtimeData = cell->GetRuntimeData();
    if (!runtimeData.navMeshes) return result;

    for (auto& meshPtr : runtimeData.navMeshes->navMeshes) {
        if (meshPtr) {
            result.push_back(meshPtr.get());
        }
    }

    return result;
}

bool NavMeshPathfinder::PointInTriangle2D(const RE::NiPoint3& p, const RE::NiPoint3& a,
                                           const RE::NiPoint3& b, const RE::NiPoint3& c)
{
    // Barycentric coordinate method (2D, ignoring Z)
    float dX = p.x - c.x;
    float dY = p.y - c.y;
    float dX21 = c.x - b.x;
    float dY12 = b.y - c.y;
    float d = dY12 * (a.x - c.x) + dX21 * (a.y - c.y);

    if (std::abs(d) < 1e-6f) return false;

    float s = dY12 * dX + dX21 * dY;
    float t = (c.y - a.y) * dX + (a.x - c.x) * dY;

    if (d < 0.0f) {
        s = -s;
        t = -t;
        d = -d;
    }

    return s >= 0.0f && t >= 0.0f && (s + t) <= d;
}

bool NavMeshPathfinder::FindTriangleInMeshGrid(RE::BSNavmesh* a_mesh, const RE::NiPoint3& a_pos,
                                                std::uint16_t& a_outTriIndex)
{
    auto& grid = a_mesh->meshGrid;
    if (grid.gridSize == 0 || grid.columnSectionLen <= 0.0f || grid.rowSectionLen <= 0.0f) {
        return false;
    }

    float relX = a_pos.x - grid.gridBoundsMin.x;
    float relY = a_pos.y - grid.gridBoundsMin.y;

    if (relX < 0.0f || relY < 0.0f) return false;

    auto gridX = static_cast<std::uint32_t>(relX / grid.columnSectionLen);
    auto gridY = static_cast<std::uint32_t>(relY / grid.rowSectionLen);

    if (gridX >= grid.gridSize || gridY >= grid.gridSize) return false;

    std::uint32_t cellIndex = gridY * grid.gridSize + gridX;
    if (cellIndex >= grid.gridData.size()) return false;

    auto& candidates = grid.gridData[cellIndex];
    for (std::uint32_t i = 0; i < candidates.size(); ++i) {
        std::uint16_t triIdx = candidates[i];
        if (triIdx >= a_mesh->triangles.size()) continue;

        auto& tri = a_mesh->triangles[triIdx];

        // Skip deleted triangles
        if (tri.triangleFlags.any(RE::BSNavmeshTriangle::TriangleFlag::kDeleted)) continue;

        auto& v0 = a_mesh->vertices[tri.vertices[0]].location;
        auto& v1 = a_mesh->vertices[tri.vertices[1]].location;
        auto& v2 = a_mesh->vertices[tri.vertices[2]].location;

        if (PointInTriangle2D(a_pos, v0, v1, v2)) {
            a_outTriIndex = triIdx;
            return true;
        }
    }

    return false;
}

bool NavMeshPathfinder::FindTriangleAt(const RE::NiPoint3& a_pos, RE::NavMesh*& a_outMesh,
                                        std::uint16_t& a_outTriIndex)
{
    auto meshes = GetNavMeshesForPosition(a_pos);

    // First try grid-accelerated lookup
    for (auto* mesh : meshes) {
        if (FindTriangleInMeshGrid(mesh, a_pos, a_outTriIndex)) {
            a_outMesh = mesh;
            return true;
        }
    }

    // Fallback: brute-force check all triangles (for positions near grid cell edges)
    float bestDistSq = (std::numeric_limits<float>::max)();
    RE::NavMesh* bestMesh = nullptr;
    std::uint16_t bestTri = 0;

    for (auto* mesh : meshes) {
        for (std::uint32_t i = 0; i < mesh->triangles.size(); ++i) {
            auto& tri = mesh->triangles[i];
            if (tri.triangleFlags.any(RE::BSNavmeshTriangle::TriangleFlag::kDeleted)) continue;

            auto center = GetTriangleCenter(mesh, static_cast<std::uint16_t>(i));
            float dx = center.x - a_pos.x;
            float dy = center.y - a_pos.y;
            float distSq = dx * dx + dy * dy;

            if (distSq < bestDistSq) {
                // Verify with point-in-triangle test
                auto& v0 = mesh->vertices[tri.vertices[0]].location;
                auto& v1 = mesh->vertices[tri.vertices[1]].location;
                auto& v2 = mesh->vertices[tri.vertices[2]].location;

                if (PointInTriangle2D(a_pos, v0, v1, v2)) {
                    bestDistSq = distSq;
                    bestMesh = mesh;
                    bestTri = static_cast<std::uint16_t>(i);
                }
            }
        }
    }

    if (bestMesh) {
        a_outMesh = bestMesh;
        a_outTriIndex = bestTri;
        return true;
    }

    // Last resort: find nearest triangle center even if point isn't inside
    for (auto* mesh : meshes) {
        for (std::uint32_t i = 0; i < mesh->triangles.size(); ++i) {
            auto& tri = mesh->triangles[i];
            if (tri.triangleFlags.any(RE::BSNavmeshTriangle::TriangleFlag::kDeleted)) continue;

            auto center = GetTriangleCenter(mesh, static_cast<std::uint16_t>(i));
            float dx = center.x - a_pos.x;
            float dy = center.y - a_pos.y;
            float dz = center.z - a_pos.z;
            float distSq = dx * dx + dy * dy + dz * dz;

            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestMesh = mesh;
                bestTri = static_cast<std::uint16_t>(i);
            }
        }
    }

    if (bestMesh) {
        a_outMesh = bestMesh;
        a_outTriIndex = bestTri;
        return true;
    }

    return false;
}

RE::NiPoint3 NavMeshPathfinder::GetTriangleCenter(RE::BSNavmesh* a_mesh, std::uint16_t a_triIndex)
{
    auto& tri = a_mesh->triangles[a_triIndex];
    auto& v0 = a_mesh->vertices[tri.vertices[0]].location;
    auto& v1 = a_mesh->vertices[tri.vertices[1]].location;
    auto& v2 = a_mesh->vertices[tri.vertices[2]].location;

    return RE::NiPoint3(
        (v0.x + v1.x + v2.x) / 3.0f,
        (v0.y + v1.y + v2.y) / 3.0f,
        (v0.z + v1.z + v2.z) / 3.0f
    );
}

void NavMeshPathfinder::SimplifyWaypoints(std::vector<RE::NiPoint3>& a_waypoints)
{
    if (a_waypoints.size() <= 2) return;

    std::vector<RE::NiPoint3> simplified;
    simplified.push_back(a_waypoints.front());

    for (std::size_t i = 1; i < a_waypoints.size() - 1; ++i) {
        auto& prev = simplified.back();
        auto& curr = a_waypoints[i];
        auto& next = a_waypoints[i + 1];

        // Direction from prev to curr
        float dx1 = curr.x - prev.x;
        float dy1 = curr.y - prev.y;
        float len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);

        // Direction from curr to next
        float dx2 = next.x - curr.x;
        float dy2 = next.y - curr.y;
        float len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

        if (len1 < 1.0f || len2 < 1.0f) continue;

        // Dot product for angle check
        float dot = (dx1 * dx2 + dy1 * dy2) / (len1 * len2);
        // cos(10°) ≈ 0.985 — if directions are nearly the same, skip this waypoint
        if (dot < 0.985f) {
            simplified.push_back(curr);
        }
    }

    simplified.push_back(a_waypoints.back());
    a_waypoints = std::move(simplified);
}

NavMeshPathfinder::PathResult NavMeshPathfinder::FindPath(const RE::NiPoint3& a_start,
                                                           const RE::NiPoint3& a_goal)
{
    PathResult result;

    // Distance cap to avoid extremely long pathfinding
    float totalDist = a_start.GetDistance(a_goal);
    if (totalDist > 8192.0f) {
        logs::info("NavMeshPathfinder: Target too far ({} units), max 8192", totalDist);
        return result;
    }

    // Find start and goal triangles
    RE::NavMesh* startMesh = nullptr;
    RE::NavMesh* goalMesh = nullptr;
    std::uint16_t startTri = 0;
    std::uint16_t goalTri = 0;

    if (!FindTriangleAt(a_start, startMesh, startTri)) {
        logs::info("NavMeshPathfinder: Could not find navmesh triangle at start position");
        return result;
    }

    if (!FindTriangleAt(a_goal, goalMesh, goalTri)) {
        logs::info("NavMeshPathfinder: Could not find navmesh triangle at goal position");
        return result;
    }

    TriangleID startID{ startMesh->GetFormID(), startTri };
    TriangleID goalID{ goalMesh->GetFormID(), goalTri };

    // Same triangle — just walk straight
    if (startID == goalID) {
        result.waypoints.push_back(a_goal);
        result.complete = true;
        return result;
    }

    RE::NiPoint3 goalCenter = GetTriangleCenter(goalMesh, goalTri);

    // A* open set (min-heap by fCost)
    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;

    // Closed set and cost tracking
    std::unordered_map<TriangleID, float, TriangleIDHash> gCosts;
    std::unordered_map<TriangleID, TriangleID, TriangleIDHash> cameFrom;

    // Cache of mesh pointers by FormID
    std::unordered_map<RE::FormID, RE::NavMesh*> meshCache;
    meshCache[startMesh->GetFormID()] = startMesh;
    meshCache[goalMesh->GetFormID()] = goalMesh;

    AStarNode startNode;
    startNode.id = startID;
    startNode.gCost = 0.0f;
    RE::NiPoint3 startCenter = GetTriangleCenter(startMesh, startTri);
    startNode.fCost = startCenter.GetDistance(goalCenter);

    openSet.push(startNode);
    gCosts[startID] = 0.0f;

    constexpr std::uint32_t maxIterations = 5000;
    std::uint32_t iterations = 0;
    bool found = false;

    while (!openSet.empty() && iterations < maxIterations) {
        ++iterations;

        AStarNode current = openSet.top();
        openSet.pop();

        if (current.id == goalID) {
            found = true;
            break;
        }

        // Skip if we already found a better path to this node
        auto it = gCosts.find(current.id);
        if (it != gCosts.end() && current.gCost > it->second) {
            continue;
        }

        // Get the mesh for this node
        RE::NavMesh* currentMesh = nullptr;
        auto cacheIt = meshCache.find(current.id.meshID);
        if (cacheIt != meshCache.end()) {
            currentMesh = cacheIt->second;
        } else {
            currentMesh = RE::TESForm::LookupByID<RE::NavMesh>(current.id.meshID);
            if (currentMesh) {
                meshCache[current.id.meshID] = currentMesh;
            }
        }

        if (!currentMesh || current.id.triIndex >= currentMesh->triangles.size()) continue;

        auto& currentTri = currentMesh->triangles[current.id.triIndex];
        RE::NiPoint3 currentCenter = GetTriangleCenter(currentMesh, current.id.triIndex);

        // Check all 3 edges
        for (int edge = 0; edge < 3; ++edge) {
            TriangleID neighborID;
            RE::NavMesh* neighborMesh = nullptr;

            // Check if this edge links to another navmesh
            bool isPortal = false;
            using TF = RE::BSNavmeshTriangle::TriangleFlag;
            if ((edge == 0 && currentTri.triangleFlags.any(TF::kEdge0_Link)) ||
                (edge == 1 && currentTri.triangleFlags.any(TF::kEdge1_Link)) ||
                (edge == 2 && currentTri.triangleFlags.any(TF::kEdge2_Link))) {
                isPortal = true;
            }

            if (isPortal) {
                // The triangles[edge] value is an index into extraEdgeInfo
                std::uint16_t extraIdx = currentTri.triangles[edge];
                if (extraIdx >= currentMesh->extraEdgeInfo.size()) continue;

                auto& extraInfo = currentMesh->extraEdgeInfo[extraIdx];
                if (extraInfo.type.get() != RE::EDGE_EXTRA_INFO_TYPE::kPortal) continue;

                // Look up the connected mesh
                RE::FormID otherMeshID = extraInfo.portal.otherMeshID;
                auto portalCacheIt = meshCache.find(otherMeshID);
                if (portalCacheIt != meshCache.end()) {
                    neighborMesh = portalCacheIt->second;
                } else {
                    neighborMesh = RE::TESForm::LookupByID<RE::NavMesh>(otherMeshID);
                    if (neighborMesh) {
                        meshCache[otherMeshID] = neighborMesh;
                    }
                }

                if (!neighborMesh) continue;

                neighborID.meshID = otherMeshID;
                neighborID.triIndex = extraInfo.portal.triangle;
            } else {
                // Same-mesh neighbor
                std::uint16_t adjTri = currentTri.triangles[edge];
                if (adjTri == 0xFFFF) continue;

                neighborID.meshID = current.id.meshID;
                neighborID.triIndex = adjTri;
                neighborMesh = currentMesh;
            }

            if (neighborID.triIndex >= neighborMesh->triangles.size()) continue;

            auto& neighborTriData = neighborMesh->triangles[neighborID.triIndex];
            if (neighborTriData.triangleFlags.any(TF::kDeleted)) continue;

            RE::NiPoint3 neighborCenter = GetTriangleCenter(neighborMesh, neighborID.triIndex);
            float tentativeG = current.gCost + currentCenter.GetDistance(neighborCenter);

            auto gIt = gCosts.find(neighborID);
            if (gIt != gCosts.end() && tentativeG >= gIt->second) continue;

            gCosts[neighborID] = tentativeG;
            cameFrom[neighborID] = current.id;

            AStarNode neighborNode;
            neighborNode.id = neighborID;
            neighborNode.gCost = tentativeG;
            neighborNode.fCost = tentativeG + neighborCenter.GetDistance(goalCenter);
            openSet.push(neighborNode);
        }
    }

    if (!found) {
        logs::info("NavMeshPathfinder: No path found after {} iterations", iterations);
        return result;
    }

    // Reconstruct path
    std::vector<TriangleID> triPath;
    TriangleID current = goalID;
    while (!(current == startID)) {
        triPath.push_back(current);
        auto cfIt = cameFrom.find(current);
        if (cfIt == cameFrom.end()) break;
        current = cfIt->second;
    }
    std::reverse(triPath.begin(), triPath.end());

    // Convert triangle path to world waypoints
    for (auto& triID : triPath) {
        RE::NavMesh* mesh = meshCache[triID.meshID];
        if (mesh) {
            result.waypoints.push_back(GetTriangleCenter(mesh, triID.triIndex));
        }
    }

    // Replace last waypoint with actual goal position
    if (!result.waypoints.empty()) {
        result.waypoints.back() = a_goal;
    }

    SimplifyWaypoints(result.waypoints);

    result.complete = true;
    logs::info("NavMeshPathfinder: Path found with {} waypoints ({} iterations)",
               result.waypoints.size(), iterations);

    return result;
}
