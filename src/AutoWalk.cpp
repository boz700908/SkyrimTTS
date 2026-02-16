#include "AutoWalk.h"
#include "SpeechManager.h"

AutoWalk* AutoWalk::GetSingleton()
{
    static AutoWalk singleton;
    return &singleton;
}

void AutoWalk::Initialize()
{
    // Look up the quest from our ESP by EditorID
    m_quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SA_AutoWalkQuest");
    if (!m_quest) {
        m_espAvailable = false;
        m_initialized = true;
        logs::info("AutoWalk: SA_AutoWalkQuest not found - auto-walk disabled (install skyrim-access.esp)");
        return;
    }

    // Get a VM handle for the quest so we can dispatch Papyrus calls
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        m_espAvailable = false;
        m_initialized = true;
        logs::error("AutoWalk: Failed to get VirtualMachine");
        return;
    }

    auto* handlePolicy = vm->GetObjectHandlePolicy();
    if (!handlePolicy) {
        m_espAvailable = false;
        m_initialized = true;
        logs::error("AutoWalk: Failed to get handle policy");
        return;
    }

    m_questHandle = handlePolicy->GetHandleForObject(
        static_cast<RE::VMTypeID>(RE::FormType::Quest), m_quest);

    if (m_questHandle == handlePolicy->EmptyHandle()) {
        m_espAvailable = false;
        m_initialized = true;
        logs::error("AutoWalk: Failed to get VM handle for quest");
        return;
    }

    m_espAvailable = true;
    m_initialized = true;
    logs::info("AutoWalk: Initialized with quest handle {:X}", m_questHandle);
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

    // Stop any existing walk first
    if (m_targetRef) {
        DispatchPapyrusStop();
    }

    m_targetRef = a_target;
    m_stopDistance = a_stopDistance;
    m_onArrival = std::move(a_onArrival);

    // Dispatch Papyrus call: OnWalkToTarget(formID, stopDistance)
    auto* args = RE::MakeFunctionArguments(
        static_cast<std::int32_t>(a_target->GetFormID()),
        static_cast<float>(a_stopDistance));

    if (DispatchPapyrusCall("OnWalkToTarget", args)) {
        SpeechManager::GetSingleton()->Speak("Walking to target", true);
        logs::info("AutoWalk: Walking to {} (FormID {:08X}, distance {})",
            a_target->GetName(), a_target->GetFormID(), a_stopDistance);
    } else {
        m_targetRef = nullptr;
        m_onArrival = nullptr;
        SpeechManager::GetSingleton()->Speak("Failed to start walking", true);
        logs::error("AutoWalk: DispatchMethodCall failed");
    }
}

void AutoWalk::Stop()
{
    if (!m_targetRef) return;

    DispatchPapyrusStop();

    m_targetRef = nullptr;
    m_onArrival = nullptr;
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
        DispatchPapyrusStop();
        m_targetRef = nullptr;
        m_onArrival = nullptr;
        if (callback) callback();
    }
}

bool AutoWalk::IsMovementKeyPressed() const
{
    return (GetAsyncKeyState(0x57) & 0x8000) ||  // W
           (GetAsyncKeyState(0x41) & 0x8000) ||  // A
           (GetAsyncKeyState(0x53) & 0x8000) ||  // S
           (GetAsyncKeyState(0x44) & 0x8000) ||  // D
           (GetAsyncKeyState(0x1B) & 0x8000) ||  // Escape
           (GetAsyncKeyState(0x20) & 0x8000);    // Space
}

bool AutoWalk::DispatchPapyrusCall(const char* a_functionName, RE::BSScript::IFunctionArguments* a_args)
{
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) return false;

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    return vm->DispatchMethodCall(
        m_questHandle,
        RE::BSFixedString("SA_AutoWalkScript"),
        RE::BSFixedString(a_functionName),
        a_args,
        callback);
}

bool AutoWalk::DispatchPapyrusStop()
{
    auto* args = RE::MakeFunctionArguments();
    return DispatchPapyrusCall("OnStopWalking", args);
}
