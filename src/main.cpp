#include "pch.h"
#include "AutoWalk.h"
#include "CrosshairAccessibility.h"
#include "MenuAccessibility.h"
#include "ObjectScanner.h"
#include "Settings.h"
#include "SpeechManager.h"
#include <SRAL.h>

void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg)
{
    if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
        logs::info("Game data loaded");

        // Load settings (key bindings, scanner parameters)
        Settings::GetSingleton()->Load();

        // Register world navigation systems
        ObjectScanner::GetSingleton()->Register();
        CrosshairAccessibility::GetSingleton()->Register();
        AutoWalk::GetSingleton()->Initialize();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);

    // Register for SKSE messages
    auto* messaging = SKSE::GetMessagingInterface();
    if (messaging) {
        messaging->RegisterListener(OnSKSEMessage);
    }

    // Initialize SRAL, excluding SAPI to prefer NVDA/JAWS
    logs::info("Initializing SRAL...");
    if (SRAL_Initialize(SRAL_ENGINE_SAPI)) {
        logs::info("SRAL initialized successfully");
    } else {
        logs::error("SRAL initialization failed");
    }

    // Initialize speech manager (creates dedicated speech log)
    SpeechManager::GetSingleton()->Initialize();

    // Speak test message via centralized manager
    if (SRAL_GetCurrentEngine() != 0) {
        SpeechManager::GetSingleton()->Speak("Skyrim accessibility plugin loaded");
    } else {
        logs::warn("No speech engine available");
    }

    // Register menu accessibility event handler
    MenuAccessibility::GetSingleton()->Register();

    return true;
}
