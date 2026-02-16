#include "AutoWalk.h"
#include "NavMeshPathfinder.h"
#include "SpeechManager.h"
#include <cmath>
#include <Xinput.h>

AutoWalk* AutoWalk::GetSingleton()
{
    static AutoWalk singleton;
    return &singleton;
}

void AutoWalk::Initialize()
{
    m_initialized = true;
    logs::info("AutoWalk: Initialized (navmesh A* pathfinding)");
}

void AutoWalk::WalkTo(RE::TESObjectREFR* a_target, float a_stopDistance,
                       std::function<void()> a_onArrival)
{
    if (!a_target || a_target->IsDisabled() || a_target->IsMarkedForDeletion()) {
        SpeechManager::GetSingleton()->Speak("Invalid target", true);
        return;
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    // Check if already close enough
    float dist = player->GetPosition().GetDistance(a_target->GetPosition());
    if (dist <= a_stopDistance) {
        if (a_onArrival) a_onArrival();
        return;
    }

    // Stop any existing walk
    if (m_targetRef) {
        Stop();
    }

    // Compute navmesh path
    auto pathResult = NavMeshPathfinder::GetSingleton()->FindPath(
        player->GetPosition(), a_target->GetPosition());

    if (!pathResult.complete || pathResult.waypoints.empty()) {
        SpeechManager::GetSingleton()->Speak("No path found", true);
        return;
    }

    m_targetRef = a_target;
    m_stopDistance = a_stopDistance;
    m_onArrival = std::move(a_onArrival);
    m_waypoints = std::move(pathResult.waypoints);
    m_currentWaypoint = 0;
    m_stuckTimer = 0.0f;
    m_hasRepathed = false;
    m_lastPosition = player->GetPosition();

    // Enable auto-move
    auto* playerControls = RE::PlayerControls::GetSingleton();
    if (playerControls) {
        m_wasAutoMoving = playerControls->data.autoMove;
        playerControls->data.autoMove = true;
    }

    // Face first waypoint
    FaceNextWaypoint();

    SpeechManager::GetSingleton()->Speak("Walking", true);
    logs::info("AutoWalk: Walking to {} ({} waypoints, {} units)",
               a_target->GetName(), m_waypoints.size(), dist);
}

void AutoWalk::Stop()
{
    if (!m_targetRef) return;

    // Restore auto-move
    auto* playerControls = RE::PlayerControls::GetSingleton();
    if (playerControls) {
        playerControls->data.autoMove = m_wasAutoMoving;
    }

    m_targetRef = nullptr;
    m_onArrival = nullptr;
    m_waypoints.clear();
    m_currentWaypoint = 0;
    m_stuckTimer = 0.0f;
    m_hasRepathed = false;
}

bool AutoWalk::IsWalking() const
{
    return m_targetRef != nullptr;
}

void AutoWalk::Update()
{
    if (!m_targetRef) return;

    // Cancel on movement input
    if (IsMovementKeyPressed()) {
        auto callback = std::move(m_onArrival);
        Stop();
        SpeechManager::GetSingleton()->Speak("Stopped", true);
        return;
    }

    // Check if target became invalid
    if (m_targetRef->IsDisabled() || m_targetRef->IsMarkedForDeletion()) {
        Stop();
        SpeechManager::GetSingleton()->Speak("Target lost", true);
        return;
    }

    // Advance waypoint if close enough
    AdvanceWaypoint();

    // Check final arrival
    CheckArrival();
    if (!m_targetRef) return;  // arrived

    // Face current waypoint
    FaceNextWaypoint();

    // Keep auto-move on
    auto* playerControls = RE::PlayerControls::GetSingleton();
    if (playerControls && !playerControls->data.autoMove) {
        playerControls->data.autoMove = true;
    }

    // Check if stuck
    CheckStuck();
}

void AutoWalk::FaceNextWaypoint()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || m_waypoints.empty() || m_currentWaypoint >= m_waypoints.size()) return;

    auto playerPos = player->GetPosition();
    auto& waypointPos = m_waypoints[m_currentWaypoint];

    float dx = waypointPos.x - playerPos.x;
    float dy = waypointPos.y - playerPos.y;

    float yaw = std::atan2(dx, dy);

    player->SetAngle(RE::NiPoint3(0.0f, 0.0f, yaw));
}

void AutoWalk::AdvanceWaypoint()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || m_waypoints.empty()) return;

    auto playerPos = player->GetPosition();

    while (m_currentWaypoint < m_waypoints.size()) {
        auto& wp = m_waypoints[m_currentWaypoint];
        float dx = wp.x - playerPos.x;
        float dy = wp.y - playerPos.y;
        float dist2D = std::sqrt(dx * dx + dy * dy);

        if (dist2D <= m_waypointReachDist) {
            m_currentWaypoint++;
            m_stuckTimer = 0.0f;  // Reset stuck timer on waypoint advance
        } else {
            break;
        }
    }
}

void AutoWalk::CheckArrival()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !m_targetRef) return;

    float dist = player->GetPosition().GetDistance(m_targetRef->GetPosition());
    constexpr float arrivalTolerance = 30.0f;

    if (dist <= m_stopDistance + arrivalTolerance) {
        auto callback = std::move(m_onArrival);
        Stop();
        if (callback) callback();
    }
}

void AutoWalk::CheckStuck()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    auto currentPos = player->GetPosition();
    float movedDist = currentPos.GetDistance(m_lastPosition);

    // Approximate frame time (~16ms at 60fps)
    constexpr float frameDelta = 0.016f;

    if (movedDist < 5.0f) {
        m_stuckTimer += frameDelta;
    } else {
        m_stuckTimer = 0.0f;
        m_lastPosition = currentPos;
    }

    if (m_stuckTimer > 2.0f) {
        if (!m_hasRepathed) {
            // Try re-pathing from current position
            m_hasRepathed = true;
            m_stuckTimer = 0.0f;

            logs::info("AutoWalk: Stuck, attempting re-path");

            auto pathResult = NavMeshPathfinder::GetSingleton()->FindPath(
                currentPos, m_targetRef->GetPosition());

            if (pathResult.complete && !pathResult.waypoints.empty()) {
                m_waypoints = std::move(pathResult.waypoints);
                m_currentWaypoint = 0;
                m_lastPosition = currentPos;
                SpeechManager::GetSingleton()->Speak("Re-routing", true);
                return;
            }
        }

        // Already tried re-pathing or re-path failed
        Stop();
        SpeechManager::GetSingleton()->Speak("Can't reach target", true);
    }
}

bool AutoWalk::IsMovementKeyPressed() const
{
    // Keyboard: WASD, Escape, Space
    if ((GetAsyncKeyState(0x57) & 0x8000) ||  // W
        (GetAsyncKeyState(0x41) & 0x8000) ||  // A
        (GetAsyncKeyState(0x53) & 0x8000) ||  // S
        (GetAsyncKeyState(0x44) & 0x8000) ||  // D
        (GetAsyncKeyState(0x1B) & 0x8000) ||  // Escape
        (GetAsyncKeyState(0x20) & 0x8000)) {  // Space
        return true;
    }

    // Gamepad: left stick or B/Start button
    XINPUT_STATE state{};
    if (XInputGetState(0, &state) == ERROR_SUCCESS) {
        constexpr SHORT deadzone = 8000;
        auto& gp = state.Gamepad;
        if (std::abs(gp.sThumbLX) > deadzone || std::abs(gp.sThumbLY) > deadzone) {
            return true;
        }
        if (gp.wButtons & (XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_START)) {
            return true;
        }
    }

    return false;
}
