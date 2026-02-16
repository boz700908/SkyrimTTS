#include "CrosshairAccessibility.h"
#include "Settings.h"
#include "SpeechManager.h"
#include <SKSE/API.h>

CrosshairAccessibility* CrosshairAccessibility::GetSingleton()
{
    static CrosshairAccessibility singleton;
    return &singleton;
}

void CrosshairAccessibility::Register()
{
    if (!Settings::GetSingleton()->IsCrosshairEnabled()) {
        logs::info("CrosshairAccessibility: Disabled in settings");
        return;
    }

    auto* source = SKSE::GetCrosshairRefEventSource();
    if (source) {
        source->AddEventSink(this);
        logs::info("CrosshairAccessibility: Registered for crosshair events");
    } else {
        logs::error("CrosshairAccessibility: Failed to get crosshair event source");
    }
}

RE::BSEventNotifyControl CrosshairAccessibility::ProcessEvent(
    const SKSE::CrosshairRefEvent* a_event,
    [[maybe_unused]] RE::BSTEventSource<SKSE::CrosshairRefEvent>* a_source)
{
    if (!a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Skip when game is paused (menu open)
    auto* ui = RE::UI::GetSingleton();
    if (ui && ui->GameIsPaused()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto ref = a_event->crosshairRef.get();
    auto now = GetCurrentTimeMs();

    if (!ref) {
        // No target - reset tracking after cooldown
        auto lastTime = m_lastTargetTime.load(std::memory_order_relaxed);
        auto cooldown = Settings::GetSingleton()->GetCrosshairCooldownMs();
        if (lastTime > 0 && (now - lastTime) > cooldown) {
            m_lastAnnouncedFormID.store(0, std::memory_order_relaxed);
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    m_lastTargetTime.store(now, std::memory_order_relaxed);

    auto formID = ref->GetFormID();
    auto lastFormID = m_lastAnnouncedFormID.load(std::memory_order_relaxed);

    if (formID == lastFormID) {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Claim this announcement (atomic swap)
    if (m_lastAnnouncedFormID.compare_exchange_strong(lastFormID, formID, std::memory_order_relaxed)) {
        AnnounceTarget(ref);
    }

    return RE::BSEventNotifyControl::kContinue;
}

void CrosshairAccessibility::AnnounceTarget(RE::TESObjectREFR* a_ref)
{
    if (!a_ref) return;

    std::string announcement;

    // Try to get the display name
    auto* name = a_ref->GetName();
    if (name && name[0] != '\0') {
        announcement = name;
    } else {
        // Try base object name
        auto* baseObj = a_ref->GetBaseObject();
        if (baseObj) {
            auto* baseName = baseObj->GetName();
            if (baseName && baseName[0] != '\0') {
                announcement = baseName;
            }
        }
    }

    if (announcement.empty()) {
        return;
    }

    // Add context for specific types
    auto* baseObj = a_ref->GetBaseObject();
    if (baseObj) {
        auto formType = baseObj->GetFormType();
        if (formType == RE::FormType::Door) {
            auto* lock = a_ref->GetLock();
            if (lock && lock->IsLocked()) {
                announcement += ", locked";
            }
        } else if (formType == RE::FormType::Container) {
            auto* lock = a_ref->GetLock();
            if (lock && lock->IsLocked()) {
                announcement += ", locked";
            }
        }
    }

    // Add NPC context
    auto* actor = a_ref->As<RE::Actor>();
    if (actor) {
        if (actor->IsInCombat()) {
            announcement += ", hostile";
        }
    }

    SpeechManager::GetSingleton()->Speak(announcement, true);
}

std::uint64_t CrosshairAccessibility::GetCurrentTimeMs() const
{
    auto now = std::chrono::steady_clock::now();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}
