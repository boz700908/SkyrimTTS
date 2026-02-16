#pragma once

#include "pch.h"
#include <atomic>
#include <SKSE/Events.h>

class CrosshairAccessibility : public RE::BSTEventSink<SKSE::CrosshairRefEvent>
{
public:
    static CrosshairAccessibility* GetSingleton();

    void Register();

    RE::BSEventNotifyControl ProcessEvent(
        const SKSE::CrosshairRefEvent* a_event,
        RE::BSTEventSource<SKSE::CrosshairRefEvent>* a_source) override;

private:
    CrosshairAccessibility() = default;
    ~CrosshairAccessibility() override = default;
    CrosshairAccessibility(const CrosshairAccessibility&) = delete;
    CrosshairAccessibility(CrosshairAccessibility&&) = delete;
    CrosshairAccessibility& operator=(const CrosshairAccessibility&) = delete;
    CrosshairAccessibility& operator=(CrosshairAccessibility&&) = delete;

    void AnnounceTarget(RE::TESObjectREFR* a_ref);
    std::uint64_t GetCurrentTimeMs() const;

    std::atomic<RE::FormID> m_lastAnnouncedFormID{ 0 };
    std::atomic<std::uint64_t> m_lastTargetTime{ 0 };
};
