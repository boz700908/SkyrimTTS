#pragma once

// AUTOWALK — Pont C++ vers Papyrus pour la marche automatique

static std::atomic_bool g_autoWalking{false};
static std::wstring     g_autoWalkTarget;
static RE::FormID       g_autoWalkTargetID{0};
static float            g_autoWalkStopDist{100.0f};
static std::jthread     g_autoWalkMonitor;

// Détection de blocage
static RE::NiPoint3     g_autoWalkLastPos{0, 0, 0};
static float            g_autoWalkStuckTimer{0.0f};
static bool             g_autoWalkHasRepathed{false};

// Vérifier si le joueur appuie sur une touche de mouvement
static bool IsMovementInputActive() {
    // Clavier : WASD, Escape, Space
    if ((GetAsyncKeyState(0x57) & 0x8000) ||  // W
        (GetAsyncKeyState(0x41) & 0x8000) ||  // A
        (GetAsyncKeyState(0x53) & 0x8000) ||  // S
        (GetAsyncKeyState(0x44) & 0x8000) ||  // D
        (GetAsyncKeyState(0x1B) & 0x8000) ||  // Escape
        (GetAsyncKeyState(0x20) & 0x8000)) {  // Space
        return true;
    }
    return false;
}

// Forward declaration
static void StopAutoWalk();

// Sécurité : remettre AIDriven à false et SpeedMult à sa base au chargement
// (corrige le cas où le jeu a été quitté/sauvegardé pendant un autowalk)
static void AutoWalkSafetyReset() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddTask([]() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player && !g_autoWalking.load()) {
            player->SetAIDriven(false);
            // Restaurer SpeedMult à sa valeur de base (corrige les sauvegardes polluées)
            auto* avo = player->AsActorValueOwner();
            if (avo) {
                float base = avo->GetBaseActorValue(RE::ActorValue::kSpeedMult);
                float current = avo->GetActorValue(RE::ActorValue::kSpeedMult);
                if (base > 0 && current != base) {
                    avo->SetActorValue(RE::ActorValue::kSpeedMult, base);
                    LOG("AutoWalk: safety reset SpeedMult {} -> {}", current, base);
                }
            }
            player->EvaluatePackage();
            LOG("AutoWalk: safety reset on load — AIDriven=false");
        }
    });
}

// Polling C++ : vérifie la distance et annonce l'arrivée
static void StartAutoWalkMonitor() {
    // Arrêter le précédent si existant
    if (g_autoWalkMonitor.joinable()) {
        g_autoWalkMonitor.request_stop();
        g_autoWalkMonitor.join();
    }

    g_autoWalkStuckTimer = 0.0f;
    g_autoWalkHasRepathed = false;

    g_autoWalkMonitor = std::jthread([](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            if (!g_autoWalking.load()) break;

            // Annuler si le joueur appuie sur une touche de mouvement ou manette
            if (IsMovementInputActive()) {
                Speak(L"Stopping");
                g_autoWalking.store(false);
                LOG("AutoWalk: cancelled by user input");
                auto* taskIf = SKSE::GetTaskInterface();
                if (taskIf) {
                    taskIf->AddTask([]() {
                        auto* p = RE::PlayerCharacter::GetSingleton();
                        if (p) {
                            p->SetAIDriven(false);
                            p->EvaluatePackage();
                        }
                    });
                }
                break;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) continue;

            // Annuler si le joueur est en combat
            if (player->IsInCombat()) {
                Speak(L"Combat, stopping");
                g_autoWalking.store(false);
                LOG("AutoWalk: cancelled by combat");
                auto* taskIf = SKSE::GetTaskInterface();
                if (taskIf) {
                    taskIf->AddTask([]() {
                        auto* p = RE::PlayerCharacter::GetSingleton();
                        if (p) {
                            p->SetAIDriven(false);
                            p->EvaluatePackage();
                        }
                    });
                }
                break;
            }

            auto* targetForm = RE::TESForm::LookupByID(g_autoWalkTargetID);
            if (!targetForm) {
                Speak(L"Target lost");
                g_autoWalking.store(false);
                break;
            }
            auto* targetRef = targetForm->AsReference();
            if (!targetRef) continue;

            auto playerPos = player->GetPosition();
            auto diff = playerPos - targetRef->GetPosition();
            float dist = diff.Length();

            // Arrivée
            if (dist <= g_autoWalkStopDist + 50.0f) {
                Speak(L"Arrived at " + g_autoWalkTarget);
                g_autoWalking.store(false);
                LOG("AutoWalk: arrived, distance {}", dist);
                auto* taskIf = SKSE::GetTaskInterface();
                if (taskIf) {
                    taskIf->AddTask([]() {
                        auto* p = RE::PlayerCharacter::GetSingleton();
                        if (p) {
                            p->SetAIDriven(false);
                            p->EvaluatePackage();
                        }
                    });
                }
                break;
            }

            // Détection de blocage : si le joueur n'a pas bougé de >5 unités en 250ms
            auto movedDiff = playerPos - g_autoWalkLastPos;
            float movedDist = movedDiff.Length();
            if (movedDist < 5.0f) {
                g_autoWalkStuckTimer += 0.25f;
            } else {
                g_autoWalkStuckTimer = 0.0f;
                g_autoWalkLastPos = playerPos;
            }

            if (g_autoWalkStuckTimer > 20.0f) {
                Speak(L"Can't reach target");
                g_autoWalking.store(false);
                LOG("AutoWalk: stuck for 3s, giving up");
                auto* taskIf = SKSE::GetTaskInterface();
                if (taskIf) {
                    taskIf->AddTask([]() {
                        auto* p = RE::PlayerCharacter::GetSingleton();
                        if (p) {
                            p->SetAIDriven(false);
                            p->EvaluatePackage();
                        }
                    });
                }
                break;
            }
        }
    });
}

// Appelle OnWalkToTarget sur le script Papyrus de la quete AutoWalk
static void StartAutoWalk(RE::FormID targetFormID, float stopDistance = 100.0f) {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;

    g_autoWalkTargetID = targetFormID;
    g_autoWalkStopDist = stopDistance;

    task->AddTask([targetFormID, stopDistance]() {
        auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SkyrimTTS_AutoWalkQuest");
        if (!quest) {
            LOG("AutoWalk: quest SkyrimTTS_AutoWalkQuest not found");
            Speak(L"AutoWalk quest not found");
            return;
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            LOG("AutoWalk: VM not available");
            return;
        }

        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            LOG("AutoWalk: no handle policy");
            return;
        }

        auto handle = policy->GetHandleForObject(RE::FormType::Quest, quest);
        if (handle == policy->EmptyHandle()) {
            LOG("AutoWalk: could not get quest handle");
            Speak(L"AutoWalk error: no quest handle");
            return;
        }

        auto* args = RE::MakeFunctionArguments(
            static_cast<std::int32_t>(targetFormID),
            static_cast<float>(stopDistance)
        );

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        bool ok = vm->DispatchMethodCall(
            handle,
            RE::BSFixedString("SkyrimTTS_AutoWalk"),
            RE::BSFixedString("OnWalkToTarget"),
            args,
            callback
        );

        if (ok) {
            g_autoWalking.store(true);
            auto* p = RE::PlayerCharacter::GetSingleton();
            if (p) g_autoWalkLastPos = p->GetPosition();
            LOG("AutoWalk: started toward FormID {:08X}, distance {}", targetFormID, stopDistance);
            StartAutoWalkMonitor();
        } else {
            LOG("AutoWalk: DispatchMethodCall failed");
            Speak(L"AutoWalk error");
        }
    });
}

// Appelle OnStopWalking sur le script Papyrus
static void StopAutoWalk() {
    g_autoWalking.store(false);

    // Arrêter le monitor
    if (g_autoWalkMonitor.joinable()) {
        g_autoWalkMonitor.request_stop();
    }

    auto* task = SKSE::GetTaskInterface();
    if (!task) return;

    task->AddTask([]() {
        // Sécurité C++ : remettre AI driven même si Papyrus échoue
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            player->SetAIDriven(false);
            player->EvaluatePackage();
            LOG("AutoWalk: C++ safety reset AIDriven=false");
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
        vm->DispatchMethodCall(
            handle,
            RE::BSFixedString("SkyrimTTS_AutoWalk"),
            RE::BSFixedString("OnStopWalking"),
            args,
            callback
        );

        g_autoWalkTarget.clear();
        LOG("AutoWalk: stop requested");
    });
}

// Toggle autowalk vers l'objet sélectionné dans le scanner
static void ToggleAutoWalk() {
    if (g_autoWalking.load()) {
        Speak(L"Stopping");
        StopAutoWalk();
        return;
    }

    RE::FormID targetID = ScannerGetCurrentFormID();
    if (targetID == 0) {
        Speak(L"No target selected. Scan first.");
        return;
    }

    std::wstring targetName = ScannerGetCurrentName();
    if (targetName.empty()) targetName = L"target";
    g_autoWalkTarget = targetName;
    Speak(L"Walking to " + targetName);

    StartAutoWalk(targetID, 100.0f);
}

// AUTOWALK — FIN
