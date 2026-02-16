#pragma once

#include "pch.h"
#include <vector>
#include <unordered_map>
#include <queue>

class NavMeshPathfinder
{
public:
    static NavMeshPathfinder* GetSingleton();

    struct PathResult {
        std::vector<RE::NiPoint3> waypoints;
        bool complete = false;
    };

    PathResult FindPath(const RE::NiPoint3& a_start, const RE::NiPoint3& a_goal);

private:
    NavMeshPathfinder() = default;
    ~NavMeshPathfinder() = default;
    NavMeshPathfinder(const NavMeshPathfinder&) = delete;
    NavMeshPathfinder& operator=(const NavMeshPathfinder&) = delete;

    struct TriangleID {
        RE::FormID meshID = 0;
        std::uint16_t triIndex = 0;

        bool operator==(const TriangleID& other) const {
            return meshID == other.meshID && triIndex == other.triIndex;
        }
    };

    struct TriangleIDHash {
        std::size_t operator()(const TriangleID& id) const {
            return std::hash<std::uint64_t>{}(
                (static_cast<std::uint64_t>(id.meshID) << 16) | id.triIndex);
        }
    };

    struct AStarNode {
        TriangleID id;
        float gCost = 0.0f;
        float fCost = 0.0f;
        TriangleID parent;
        bool hasParent = false;

        bool operator>(const AStarNode& other) const {
            return fCost > other.fCost;
        }
    };

    // Find which navmesh triangle contains a world position
    bool FindTriangleAt(const RE::NiPoint3& a_pos, RE::NavMesh*& a_outMesh,
                        std::uint16_t& a_outTriIndex);

    // Get triangle center point
    RE::NiPoint3 GetTriangleCenter(RE::BSNavmesh* a_mesh, std::uint16_t a_triIndex);

    // Point-in-triangle test (2D XY plane)
    bool PointInTriangle2D(const RE::NiPoint3& p, const RE::NiPoint3& a,
                           const RE::NiPoint3& b, const RE::NiPoint3& c);

    // Get navmeshes from cell at position
    std::vector<RE::NavMesh*> GetNavMeshesForPosition(const RE::NiPoint3& a_pos);

    // Try to find triangle using the navmesh grid acceleration structure
    bool FindTriangleInMeshGrid(RE::BSNavmesh* a_mesh, const RE::NiPoint3& a_pos,
                                std::uint16_t& a_outTriIndex);

    // Simplify waypoints by removing collinear intermediate points
    void SimplifyWaypoints(std::vector<RE::NiPoint3>& a_waypoints);
};
