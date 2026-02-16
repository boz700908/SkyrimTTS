#include "AutoWalk.h"
#include "SpeechManager.h"

AutoWalk* AutoWalk::GetSingleton()
{
    static AutoWalk singleton;
    return &singleton;
}

void AutoWalk::Initialize()
{
    // Look for the ESP quest - for now this is a stub
    // Future: look up quest by EditorID "SA_AutoWalkQuest" via TESForm::LookupByEditorID
    m_espAvailable = false;
    m_initialized = true;
    logs::info("AutoWalk: Initialized (ESP not available - auto-walk disabled until skyrim-access.esp is installed)");
}

void AutoWalk::WalkTo(RE::TESObjectREFR* a_target, float a_stopDistance,
                       std::function<void()> a_onArrival)
{
    if (!m_initialized) {
        return;
    }

    if (!m_espAvailable) {
        SpeechManager::GetSingleton()->Speak("Auto-walk requires skyrim-access.esp", true);
        return;
    }

    if (!a_target || a_target->IsDisabled() || a_target->IsMarkedForDeletion()) {
        SpeechManager::GetSingleton()->Speak("Invalid target", true);
        return;
    }

    // Check if already close enough
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        float dist = player->GetPosition().GetDistance(a_target->GetPosition());
        if (dist <= a_stopDistance) {
            if (a_onArrival) a_onArrival();
            return;
        }
    }

    m_targetRef = a_target;
    m_stopDistance = a_stopDistance;
    m_onArrival = std::move(a_onArrival);

    // Future: dispatch Papyrus call here
    // vm->DispatchMethodCall(quest, "SkyrimAccess_AutoWalk", "OnWalkToTarget", args, callback);
}

void AutoWalk::Stop()
{
    if (!m_targetRef) return;

    m_targetRef = nullptr;
    m_onArrival = nullptr;

    // Future: dispatch Papyrus OnStopWalking call
}

bool AutoWalk::IsWalking() const
{
    return m_targetRef != nullptr;
}

void AutoWalk::Update()
{
    if (!m_targetRef) return;

    // Cancel on movement keys
    if (IsMovementKeyPressed()) {
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

    CheckArrival();
}

void AutoWalk::CheckArrival()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !m_targetRef) return;

    float dist = player->GetPosition().GetDistance(m_targetRef->GetPosition());
    constexpr float arrivalTolerance = 30.0f;

    if (dist <= m_stopDistance + arrivalTolerance) {
        auto callback = std::move(m_onArrival);
        m_targetRef = nullptr;
        m_onArrival = nullptr;
        if (callback) callback();
    }
}

bool AutoWalk::IsMovementKeyPressed() const
{
    // Check WASD + Escape + Space via Windows API
    // VK codes: W=0x57, A=0x41, S=0x53, D=0x44, Escape=0x1B, Space=0x20
    return (GetAsyncKeyState(0x57) & 0x8000) ||
           (GetAsyncKeyState(0x41) & 0x8000) ||
           (GetAsyncKeyState(0x53) & 0x8000) ||
           (GetAsyncKeyState(0x44) & 0x8000) ||
           (GetAsyncKeyState(0x1B) & 0x8000) ||
           (GetAsyncKeyState(0x20) & 0x8000);
}
