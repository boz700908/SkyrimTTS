#pragma once

#include "pch.h"
#include <functional>
#include <vector>

class AutoWalk
{
public:
    static AutoWalk* GetSingleton();

    void Initialize();
    void WalkTo(RE::TESObjectREFR* a_target, float a_stopDistance = 100.0f,
                std::function<void()> a_onArrival = nullptr);
    void Stop();
    bool IsWalking() const;
    void Update();

private:
    AutoWalk() = default;
    ~AutoWalk() = default;
    AutoWalk(const AutoWalk&) = delete;
    AutoWalk(AutoWalk&&) = delete;
    AutoWalk& operator=(const AutoWalk&) = delete;
    AutoWalk& operator=(AutoWalk&&) = delete;

    void FaceNextWaypoint();
    void AdvanceWaypoint();
    void CheckArrival();
    void CheckStuck();
    bool IsMovementKeyPressed() const;

    RE::TESObjectREFR* m_targetRef = nullptr;
    float m_stopDistance = 100.0f;
    std::function<void()> m_onArrival;
    bool m_wasAutoMoving = false;
    bool m_initialized = false;

    // Navmesh waypoint path
    std::vector<RE::NiPoint3> m_waypoints;
    std::size_t m_currentWaypoint = 0;
    float m_waypointReachDist = 64.0f;

    // Stuck detection
    RE::NiPoint3 m_lastPosition;
    float m_stuckTimer = 0.0f;
    bool m_hasRepathed = false;
};
