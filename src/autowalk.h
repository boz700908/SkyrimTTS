#pragma once

// AUTOWALK — Pont C++ vers Papyrus pour la marche automatique

// Helper pour trouver la quête AutoWalk — cherche par EditorID d'abord (AE natif + po3_Tweaks),
// puis fallback par plugin name + local FormID (compatible SE 1.5.97 sans po3_Tweaks).
static RE::TESQuest* FindAutoWalkQuest() {
    // Tentative 1 : EditorID (fonctionne sur AE natif ou SE avec po3_Tweaks Load EditorIDs)
    auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SkyrimTTS_AutoWalkQuest");
    if (quest) {
        LOG("AutoWalk: quest found via EditorID");
        return quest;
    }
    LOG("AutoWalk: EditorID lookup failed, trying LookupForm fallback");

    // Tentative 2 : lookup par plugin + local FormID (0x800 dans SkyrimTTS_AutoWalk.esp)
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        LOG("AutoWalk: TESDataHandler is null!");
        return nullptr;
    }

    // Tentative 2 : lookup par plugin name
    const char* espName = "SkyrimTTS_AutoWalk.esp";
    quest = dataHandler->LookupForm<RE::TESQuest>(0x800, espName);
    if (quest) {
        LOG("AutoWalk: quest found via LookupForm (FormID 0x800)");
        return quest;
    }

    // Tentative 3 : chercher le plugin et construire le FormID résolu manuellement
    auto* modFile = dataHandler->LookupModByName(espName);
    if (modFile) {
        LOG("AutoWalk: plugin found, compileIndex={}, smallFileCompileIndex={}",
            modFile->compileIndex, modFile->smallFileCompileIndex);
        RE::FormID resolvedID = (static_cast<RE::FormID>(modFile->compileIndex) << 24) | 0x800;
        LOG("AutoWalk: trying resolved FormID 0x{:08X}", resolvedID);
        auto* form = RE::TESForm::LookupByID(resolvedID);
        if (form) {
            quest = form->As<RE::TESQuest>();
            if (quest) {
                LOG("AutoWalk: quest found via resolved FormID!");
                return quest;
            }
        }
        // Tentative 4 : si c'est un light plugin (FE slot), essayer avec smallFileCompileIndex
        if (modFile->compileIndex == 0xFE || modFile->compileIndex == 0xFF) {
            resolvedID = 0xFE000000 | (static_cast<RE::FormID>(modFile->smallFileCompileIndex) << 12) | 0x800;
            LOG("AutoWalk: trying light FormID 0x{:08X}", resolvedID);
            form = RE::TESForm::LookupByID(resolvedID);
            if (form) {
                quest = form->As<RE::TESQuest>();
                if (quest) {
                    LOG("AutoWalk: quest found via light FormID!");
                    return quest;
                }
            }
        }
    } else {
        LOG("AutoWalk: plugin '{}' NOT found by LookupModByName", espName);
    }

    LOG("AutoWalk: all lookup methods failed");

    return nullptr;
}

static std::atomic_bool g_autoWalking{false};
static std::wstring     g_autoWalkTarget;
static RE::FormID       g_autoWalkTargetID{0};
static float            g_autoWalkStopDist{100.0f};
static RE::NiPoint3     g_autoWalkTargetPos{0, 0, 0};  // pour le mode coordonnées
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

    // État du monitoring (statics pour persister entre les ticks AddTask)
    static int s_recoveryAttempt = 0;
    static bool s_diagLogged = false;
    static float s_lastDistMounted = -1.0f;
    s_recoveryAttempt = 0;
    s_diagLogged = false;
    s_lastDistMounted = -1.0f;

    // Le jthread ne fait que le timing + input check.
    // TOUTES les lectures de données du jeu (position, refs, cellules) sont
    // faites sur le thread principal via AddTask pour éviter les race conditions
    // qui causaient des crashes (null pointer sur refs déchargées).
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
                        if (!p) return;
                        p->SetAIDriven(false);
                        auto* avo = p->AsActorValueOwner();
                        if (avo) {
                            float base = avo->GetBaseActorValue(RE::ActorValue::kSpeedMult);
                            avo->SetActorValue(RE::ActorValue::kSpeedMult, base);
                        }
                        p->EvaluatePackage();
                    });
                }
                break;
            }

            // Dispatcher le tick de monitoring sur le thread principal du jeu
            auto* taskIf = SKSE::GetTaskInterface();
            if (!taskIf) continue;
            taskIf->AddTask([]() {
                if (!g_autoWalking.load()) return;

                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player) return;

                const bool isMounted = player->IsOnMount();
                auto playerPos = player->GetPosition();
                float dist = 0.0f;

                // Mode coordonnées (marqueur personnalisé, objets dynamiques)
                if (g_autoWalkTargetPos.x != 0 || g_autoWalkTargetPos.y != 0) {
                    float dx = playerPos.x - g_autoWalkTargetPos.x;
                    float dy = playerPos.y - g_autoWalkTargetPos.y;
                    dist = std::sqrt(dx * dx + dy * dy);
                } else {
                    // Mode FormID classique
                    auto* targetForm = RE::TESForm::LookupByID(g_autoWalkTargetID);
                    if (!targetForm) {
                        Speak(L"Target lost");
                        g_autoWalking.store(false);
                        auto* p = RE::PlayerCharacter::GetSingleton();
                        if (p) { p->SetAIDriven(false); p->EvaluatePackage(); }
                        return;
                    }
                    auto* targetRef = targetForm->AsReference();
                    if (!targetRef) return;
                    auto diff = playerPos - targetRef->GetPosition();
                    dist = diff.Length();
                }

                // Arrivée
                if (dist <= g_autoWalkStopDist + 50.0f) {
                    Speak(L"Arrived at " + g_autoWalkTarget);
                    g_autoWalking.store(false);
                    LOG("AutoWalk: arrived, distance {}", dist);

                    player->SetAIDriven(false);
                    auto* avo = player->AsActorValueOwner();
                    if (avo) {
                        float base = avo->GetBaseActorValue(RE::ActorValue::kSpeedMult);
                        avo->SetActorValue(RE::ActorValue::kSpeedMult, base);
                    }
                    player->EvaluatePackage();

                    RE::FormID targetID = g_autoWalkTargetID;
                    if (targetID != 0) {
                        auto* targetForm = RE::TESForm::LookupByID(targetID);
                        if (targetForm) {
                            auto* targetRef = targetForm->AsReference();
                            if (targetRef) {
                                auto targetPos = targetRef->GetPosition();
                                float dx = targetPos.x - playerPos.x;
                                float dy = targetPos.y - playerPos.y;
                                float dz = targetPos.z - playerPos.z;
                                float yaw = std::atan2(dx, dy);
                                if (yaw < 0) yaw += 2.0f * 3.14159265f;
                                player->data.angle.z = yaw;
                                float hDist = std::sqrt(dx * dx + dy * dy);
                                if (hDist > 1.0f) {
                                    float pitch = -std::atan2(dz, hDist);
                                    player->data.angle.x = pitch;
                                }
                                LOG("AutoWalk: oriented toward target yaw={:.2f} pitch={:.2f}", yaw, player->data.angle.x);
                            }
                        }
                    }
                    return;
                }

                // Détection de blocage — deux stratégies selon le mode
                if (isMounted) {
                    if (s_lastDistMounted < 0.0f) {
                        s_lastDistMounted = dist;
                    } else if (dist < s_lastDistMounted - 5.0f) {
                        g_autoWalkStuckTimer = 0.0f;
                        s_lastDistMounted = dist;
                        s_diagLogged = false;
                    } else {
                        g_autoWalkStuckTimer += 0.25f;
                    }
                } else {
                    auto movedDiff = playerPos - g_autoWalkLastPos;
                    float movedDist = movedDiff.Length();
                    if (movedDist < 5.0f) {
                        g_autoWalkStuckTimer += 0.25f;
                    } else {
                        g_autoWalkStuckTimer = 0.0f;
                        g_autoWalkLastPos = playerPos;
                        s_recoveryAttempt = 0;
                        s_diagLogged = false;
                    }
                }

                // Diagnostic de blocage (se déclenche une seule fois à 4s)
                if (g_autoWalkStuckTimer > 4.0f && !s_diagLogged) {
                    s_diagLogged = true;
                    LOG("=== AutoWalk STUCK DIAGNOSTIC ===");
                    LOG("  Player pos: ({:.0f}, {:.0f}, {:.0f})", playerPos.x, playerPos.y, playerPos.z);

                    if (g_autoWalkTargetPos.x != 0 || g_autoWalkTargetPos.y != 0) {
                        LOG("  Target (coords): ({:.0f}, {:.0f}, {:.0f}) dist={:.0f}",
                            g_autoWalkTargetPos.x, g_autoWalkTargetPos.y, g_autoWalkTargetPos.z, dist);
                    } else {
                        auto* tf = RE::TESForm::LookupByID(g_autoWalkTargetID);
                        if (tf) {
                            auto* tr = tf->AsReference();
                            if (tr) {
                                auto tp = tr->GetPosition();
                                auto* tc = tr->GetParentCell();
                                LOG("  Target (formID=0x{:08X}): ({:.0f}, {:.0f}, {:.0f}) cell='{}' dist={:.0f}",
                                    g_autoWalkTargetID, tp.x, tp.y, tp.z,
                                    tc && tc->GetName() ? tc->GetName() : "?", dist);
                            }
                        }
                    }

                    // Diagnostic IA/physique (déjà sur le thread principal)
                    auto* process = player->GetActorRuntimeData().currentProcess;
                    if (process) {
                        auto* pkg = process->GetRunningPackage();
                        if (pkg) {
                            LOG("  Running package: formID=0x{:08X} type={}",
                                pkg->GetFormID(),
                                static_cast<int>(pkg->packData.packType.underlying()));
                        } else {
                            LOG("  Running package: NONE (IA may have abandoned)");
                        }
                        auto* hpd = process->high;
                        if (hpd) {
                            auto& desiredSpeed = hpd->pathingDesiredMovementSpeed;
                            auto& currentSpeed = hpd->pathingCurrentMovementSpeed;
                            float dMag = std::sqrt(desiredSpeed.x * desiredSpeed.x +
                                                   desiredSpeed.y * desiredSpeed.y +
                                                   desiredSpeed.z * desiredSpeed.z);
                            float cMag = std::sqrt(currentSpeed.x * currentSpeed.x +
                                                   currentSpeed.y * currentSpeed.y +
                                                   currentSpeed.z * currentSpeed.z);
                            LOG("  Path speed: desired={:.1f} current={:.1f}", dMag, cMag);
                        }
                    }
                    auto ctrlPtr = player->GetCharController();
                    if (ctrlPtr) {
                        const char* stateStr = "?";
                        switch (ctrlPtr->context.currentState) {
                            case RE::hkpCharacterStateType::kOnGround: stateStr = "OnGround"; break;
                            case RE::hkpCharacterStateType::kJumping:  stateStr = "Jumping"; break;
                            case RE::hkpCharacterStateType::kInAir:    stateStr = "InAir"; break;
                            case RE::hkpCharacterStateType::kClimbing: stateStr = "Climbing"; break;
                            case RE::hkpCharacterStateType::kSwimming: stateStr = "Swimming"; break;
                            default: break;
                        }
                        LOG("  Character state: {}", stateStr);
                    }
                    auto* cell = player->GetParentCell();
                    if (cell) {
                        LOG("  Player cell: '{}' (interior={})",
                            cell->GetName() ? cell->GetName() : "?", cell->IsInteriorCell());
                    }
                    LOG("=== END STUCK DIAGNOSTIC ===");
                }

                // Récupération progressive de blocage
                if (isMounted) {
                    if (g_autoWalkStuckTimer > 30.0f) {
                        Speak(L"Horse stuck");
                        g_autoWalking.store(false);
                        LOG("AutoWalk: horse stuck for 30s, giving up");
                        StopAutoWalk();
                    }
                } else {
                    if (g_autoWalkStuckTimer > 3.0f && s_recoveryAttempt == 0) {
                        LOG("AutoWalk: stuck for 3s, simulating Space press for directional jump");
                        keybd_event(VK_SPACE, 0, 0, 0);
                        std::thread([]() {
                            std::this_thread::sleep_for(std::chrono::milliseconds(150));
                            keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0);
                        }).detach();
                        s_recoveryAttempt = 1;
                    } else if (g_autoWalkStuckTimer > 6.0f && s_recoveryAttempt < 2) {
                        LOG("AutoWalk: stuck for 6s, re-evaluating package");
                        player->SetAIDriven(false);
                        player->EvaluatePackage();
                        player->SetAIDriven(true);
                        player->EvaluatePackage();
                        s_recoveryAttempt = 2;
                    } else if (g_autoWalkStuckTimer > 10.0f) {
                        Speak(L"Can't reach target");
                        g_autoWalking.store(false);
                        LOG("AutoWalk: stuck for 10s, giving up");
                        player->SetAIDriven(false);
                        auto* avo = player->AsActorValueOwner();
                        if (avo) {
                            float base = avo->GetBaseActorValue(RE::ActorValue::kSpeedMult);
                            avo->SetActorValue(RE::ActorValue::kSpeedMult, base);
                        }
                        player->EvaluatePackage();
                    }
                }
            });  // fin AddTask
        }
    });
}

// Appelle OnWalkToTarget sur le script Papyrus de la quete AutoWalk
// Si posX/posY/posZ sont fournis (non-zero), le FormID est passé à 0 et le Papyrus
// utilise les coordonnées directement (mode FF* pour objets dynamiques).
static void StartAutoWalk(RE::FormID targetFormID, float stopDistance = 100.0f,
                          float posX = 0.f, float posY = 0.f, float posZ = 0.f) {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;

    g_autoWalkTargetID = targetFormID;
    g_autoWalkStopDist = stopDistance;
    g_autoWalkTargetPos = {posX, posY, posZ};  // stocker pour le moniteur

    // Si FormID dynamique (FF*), passer en mode coordonnées
    bool useCoords = (posX != 0.f || posY != 0.f || posZ != 0.f);

    task->AddTask([targetFormID, stopDistance, posX, posY, posZ, useCoords]() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            // Forcer l'initialisation du mouvement avant de lancer l'IA
            // (corrige le bug de vitesse lente si autowalk lancé sans marcher après un chargement)
            auto* avo = player->AsActorValueOwner();
            if (avo) {
                float base = avo->GetBaseActorValue(RE::ActorValue::kSpeedMult);
                float current = avo->GetActorValue(RE::ActorValue::kSpeedMult);
                if (base > 0 && current != base) {
                    avo->SetActorValue(RE::ActorValue::kSpeedMult, base);
                    LOG("AutoWalk: pre-start SpeedMult fix {} -> {}", current, base);
                }
            }
            // S'assurer que l'IA est bien désactivée avant de la réactiver
            player->SetAIDriven(false);
            player->EvaluatePackage();

            // Activer kTryStep pour monter les escaliers automatiquement
            auto* charCtrl = player->GetCharController();
            if (charCtrl) {
                charCtrl->flags.set(RE::CHARACTER_FLAGS::kTryStep);
                charCtrl->flags.set(RE::CHARACTER_FLAGS::kCanJump);
                LOG("AutoWalk: kTryStep + kCanJump enabled");
            }
        }

        auto* quest = FindAutoWalkQuest();
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

        // 5 paramètres : aiFormID, afStopDistance, afX, afY, afZ
        // Mode coordonnées : formID=0 + position x,y,z
        // Mode normal : formID valide + 0,0,0
        auto* args = RE::MakeFunctionArguments(
            static_cast<std::int32_t>(useCoords ? 0 : static_cast<std::int32_t>(targetFormID)),
            static_cast<float>(stopDistance),
            static_cast<float>(posX),
            static_cast<float>(posY),
            static_cast<float>(posZ)
        );

        if (useCoords) {
            LOG("AutoWalk: coords mode FormID={:08X} pos=({:.0f},{:.0f},{:.0f})", targetFormID, posX, posY, posZ);
        }

        LOG("AutoWalk: quest running={}, formID={:08X}, handle={:X}",
            quest->IsRunning(), quest->GetFormID(), handle);

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        bool ok = vm->DispatchMethodCall(
            handle,
            RE::BSFixedString("SkyrimTTS_AutoWalk"),
            RE::BSFixedString("OnWalkToTarget"),
            args,
            callback
        );

        LOG("AutoWalk: DispatchMethodCall returned {}", ok);

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
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            player->SetAIDriven(false);
            auto* avo = player->AsActorValueOwner();
            if (avo) {
                float base = avo->GetBaseActorValue(RE::ActorValue::kSpeedMult);
                avo->SetActorValue(RE::ActorValue::kSpeedMult, base);
                LOG("AutoWalk: restored SpeedMult to base={}", base);
            }
            LOG("AutoWalk: C++ safety reset AIDriven=false");
            player->EvaluatePackage();
        }

        auto* quest = FindAutoWalkQuest();
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
// Créer un XMarker temporaire à une position donnée pour l'autowalk
// Retourne le FormID du marqueur créé, ou 0 en cas d'échec
static RE::FormID g_autoWalkTempMarker{0};

static RE::FormID CreateTempMarkerAt(const RE::NiPoint3& pos) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return 0;

    // XMarkerHeading FormID = 0x10
    auto* xmarkerBase = RE::TESForm::LookupByID(0x10);
    if (!xmarkerBase) {
        LOG("AutoWalk: XMarkerHeading (0x10) not found");
        return 0;
    }

    auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESObjectREFR>();
    if (!factory) {
        LOG("AutoWalk: no form factory for TESObjectREFR");
        return 0;
    }

    auto* marker = factory->Create();
    if (!marker) {
        LOG("AutoWalk: failed to create marker");
        return 0;
    }

    marker->SetObjectReference(static_cast<RE::TESBoundObject*>(xmarkerBase));
    marker->data.location = pos;

    // Placer dans la cellule du joueur
    auto* cell = player->GetParentCell();
    if (cell) {
        // Utiliser MoveTo pour placer correctement
        marker->MoveTo(player);
        marker->data.location = pos;
    }

    LOG("AutoWalk: created temp marker FormID={:08X} at ({:.0f}, {:.0f}, {:.0f})",
        marker->GetFormID(), pos.x, pos.y, pos.z);

    return marker->GetFormID();
}

static void ToggleAutoWalk() {
    LOG("InputDiag: ToggleAutoWalk ENTRY, g_autoWalking={}", g_autoWalking.load());
    if (g_autoWalking.load()) {
        Speak(L"Stopping");
        StopAutoWalk();
        return;
    }

    LOG("AutoWalk: ToggleAutoWalk called");

    // Désactiver l'aimlock et le toggle lock-on avant de marcher
    // (sinon la caméra tremble et le joueur ne bouge pas)
    if (g_autoAimTracking.load()) StopAutoAim();
    if (g_toggleLockOn.load()) {
        StopToggleLockOn();
        LOG("AutoWalk: disabled toggle lock-on before walking");
    }

    RE::FormID targetID = ScannerGetCurrentFormID();
    if (targetID == 0) {
        Speak(L"No target selected. Scan first.");
        return;
    }

    std::wstring targetName = ScannerGetCurrentName();
    if (targetName.empty()) targetName = L"target";
    g_autoWalkTarget = targetName;

    // Si FormID dynamique (FF*), vérifier si l'objet est chargé en 3D (ex: objet jeté au sol).
    // Si oui → marcher directement vers lui. Si non → chercher la porte via boussole.
    if ((targetID >> 24) == 0xFF) {
        // Vérifier si l'objet existe en 3D dans le monde (objet jeté au sol, PNJ invoqué, etc.)
        auto* refForm = RE::TESForm::LookupByID(targetID);
        auto* ref = refForm ? refForm->AsReference() : nullptr;
        if (ref && ref->Is3DLoaded() && !ref->IsDisabled() && !ref->IsDeleted()) {
            auto pos = ref->GetPosition();
            LOG("AutoWalk: dynamic FormID {:08X} is loaded in 3D at ({:.0f},{:.0f},{:.0f}), using coords mode",
                targetID, pos.x, pos.y, pos.z);
            Speak(L"Walking to " + targetName);
            StartAutoWalk(targetID, 150.0f, pos.x, pos.y, pos.z);
            return;
        }

        // Cas spécifique : cible de quête dans la MÊME cellule que le joueur, résolue
        // par le scanner via le PASS 1 (ignore CTDA).
        // La ref n'est pas 3D-loaded (alias de quête qui n'est pas encore vraiment spawn
        // dans le monde, ex: Pierre de dragon à Bleak Falls Barrow alors qu'on est déjà
        // dans le donjon), mais on a quand même une position valide dans obj.lastKnownPos
        // (celle qu'on utilise pour afficher la distance dans le scanner).
        // → On marche directement vers ces coordonnées.
        //
        // IMPORTANT : on ne fait ça que si la cible est dans la même cellule. Pour les
        // cibles cross-cell (ex: joueur dehors, cible dans un donjon), on garde la
        // logique boussole historique (recherche de porte d'entrée) — c'est ce qui
        // fonctionnait avant pour ces cas-là.
        if (g_scanIndex >= 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
            auto& currentObj = *g_scannedFiltered[g_scanIndex];
            if (currentObj.category == kCatQuests &&
                (currentObj.lastKnownPos.x != 0 || currentObj.lastKnownPos.y != 0)) {
                // Vérifier que la ref target est dans la même cellule que le joueur.
                // Si ref est null (LookupByID a échoué), on ne fait rien → fallback boussole.
                auto* playerForSameCellCheck = RE::PlayerCharacter::GetSingleton();
                auto* playerCellCheck = playerForSameCellCheck ? playerForSameCellCheck->GetParentCell() : nullptr;
                auto* targetCellCheck = ref ? ref->GetParentCell() : nullptr;
                if (ref && playerCellCheck && targetCellCheck == playerCellCheck) {
                    auto pos = currentObj.lastKnownPos;
                    LOG("AutoWalk: dynamic FormID {:08X} quest target in same cell, using scanner lastKnownPos=({:.0f},{:.0f},{:.0f})",
                        targetID, pos.x, pos.y, pos.z);
                    Speak(L"Walking to " + targetName);
                    StartAutoWalk(targetID, 150.0f, pos.x, pos.y, pos.z);
                    return;
                } else {
                    LOG("AutoWalk: dynamic FormID {:08X} quest target not in same cell (ref={}, playerCell={}, targetCell={}), falling back to compass",
                        targetID, ref ? "ok" : "null",
                        playerCellCheck ? "ok" : "null",
                        targetCellCheck ? "ok" : "null");
                }
            }
        }

        LOG("AutoWalk: dynamic FormID {:08X}, searching for entrance door via compass", targetID);

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) { Speak(L"Cannot walk to this target"); return; }
        auto playerPos = player->GetPosition();

        // 1) Lire le heading de la boussole pour les marqueurs de quête
        float compassHeading = -1.0f;
        auto* ui = RE::UI::GetSingleton();
        if (ui) {
            auto hudMenu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
            if (hudMenu && hudMenu->uiMovie) {
                // Lire CompassTargetDataA depuis le HUD
                RE::GFxValue hudRoot;
                if (hudMenu->uiMovie->GetVariable(&hudRoot, "_root.HUDMovieBaseInstance") && SafeIsObject(hudRoot)) {
                    RE::GFxValue dataArr;
                    if (hudRoot.GetMember("CompassTargetDataA", &dataArr) && SafeIsArray(dataArr)) {
                        uint32_t arrSize = SafeGetArraySize(dataArr);
                        LOG("AutoWalk: CompassTargetDataA size={}", arrSize);

                        // Lire les types de marqueurs de la boussole
                        RE::GFxValue questTypeVal, questDoorTypeVal;
                        float questType = -1, questDoorType = -1;
                        if (hudRoot.GetMember("CompassMarkerQuest", &questTypeVal) && SafeIsNumber(questTypeVal))
                            questType = static_cast<float>(SafeGetNumber(questTypeVal));
                        if (hudRoot.GetMember("CompassMarkerQuestDoor", &questDoorTypeVal) && SafeIsNumber(questDoorTypeVal))
                            questDoorType = static_cast<float>(SafeGetNumber(questDoorTypeVal));

                        LOG("AutoWalk: quest marker types: quest={:.0f} questDoor={:.0f}", questType, questDoorType);

                        // Stride = 4 : heading, alpha, type, scale
                        for (uint32_t i = 0; i + 3 < arrSize; i += 4) {
                            RE::GFxValue headingVal, typeVal;
                            dataArr.GetElement(i, &headingVal);      // heading
                            dataArr.GetElement(i + 2, &typeVal);     // type

                            if (!SafeIsNumber(typeVal)) continue;
                            float type = static_cast<float>(SafeGetNumber(typeVal));

                            if (type == questType || type == questDoorType) {
                                if (SafeIsNumber(headingVal)) {
                                    compassHeading = static_cast<float>(SafeGetNumber(headingVal));
                                    LOG("AutoWalk: found compass quest marker heading={:.1f} type={:.0f}", compassHeading, type);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (compassHeading < 0) {
            LOG("AutoWalk: no compass quest marker found, cannot determine entrance");
            Speak(L"No quest marker on compass");
            return;
        }

        // 2) Convertir le compass heading (degrés, 0=nord) en radians pour comparer
        float compassRad = compassHeading * 3.14159265f / 180.0f;

        // 3) Chercher la porte d'entrée qui correspond au heading de la boussole
        // La cible est dans une cellule intérieure — chercher les portes qui y mènent
        RE::TESObjectREFR* bestDoor = nullptr;
        float bestHeadingDiff = 999.0f;

        // Obtenir la cellule cible via le scanner
        RE::TESObjectCELL* targetCell = nullptr;
        if (g_scanIndex >= 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
            auto& obj = *g_scannedFiltered[g_scanIndex];
            // Chercher la ref via CreateRefHandleByAliasID pour obtenir sa cellule
            auto* form = RE::TESForm::LookupByID(obj.formID);
            if (form) {
                auto* ref = form->AsReference();
                if (ref) targetCell = ref->GetParentCell();
            }
            // Si LookupByID échoue (FF*), utiliser la position en cache
            // et chercher toutes les portes dont le heading correspond
        }

        auto searchDoors = [&](RE::TESObjectCELL* cell) {
            if (!cell) return;
            for (auto& refHandle : cell->GetRuntimeData().references) {
                auto refPtr = refHandle.get();
                if (!refPtr) continue;
                auto* base = refPtr->GetBaseObject();
                if (!base || base->GetFormType() != RE::FormType::Door) continue;
                auto* extraTele = refPtr->extraList.GetByType<RE::ExtraTeleport>();
                if (!extraTele || !extraTele->teleportData) continue;
                auto linkedDoor = extraTele->teleportData->linkedDoor.get();
                if (!linkedDoor) continue;
                auto* destCell = linkedDoor->GetParentCell();
                if (!destCell) continue;

                // Calculer le heading de cette porte depuis le joueur
                auto doorPos = refPtr->GetPosition();
                float dx = doorPos.x - playerPos.x;
                float dy = doorPos.y - playerPos.y;
                float doorAngle = std::atan2(dx, dy);  // radians, 0=nord
                if (doorAngle < 0) doorAngle += 2.0f * 3.14159265f;

                // Comparer avec le heading de la boussole
                float diff = std::abs(doorAngle - compassRad);
                if (diff > 3.14159265f) diff = 2.0f * 3.14159265f - diff;

                float doorDist = std::sqrt(dx * dx + dy * dy);

                LOG("AutoWalk: door '{}' FormID={:08X} -> '{}' heading={:.1f}° diff={:.1f}° dist={:.0f}",
                    refPtr->GetDisplayFullName() ? refPtr->GetDisplayFullName() : "?",
                    refPtr->GetFormID(),
                    destCell->GetName() ? destCell->GetName() : "?",
                    doorAngle * 180.0f / 3.14159265f,
                    diff * 180.0f / 3.14159265f,
                    doorDist);

                if (diff < bestHeadingDiff) {
                    bestHeadingDiff = diff;
                    bestDoor = refPtr;
                }
            }
        };

        // Chercher dans la cellule du joueur (intérieur ou extérieur)
        auto* playerCell = player->GetParentCell();
        if (playerCell) {
            searchDoors(playerCell);
        }

        // En extérieur, chercher aussi dans les cellules voisines et persistante
        if (playerCell && !playerCell->IsInteriorCell()) {
            auto* tes = RE::TES::GetSingleton();
            if (tes && tes->gridCells) {
                for (uint32_t gx = 0; gx < tes->gridCells->length; gx++) {
                    for (uint32_t gy = 0; gy < tes->gridCells->length; gy++) {
                        auto* cell = tes->gridCells->GetCell(gx, gy);
                        if (cell && cell->IsAttached() && cell != playerCell) searchDoors(cell);
                    }
                }
            }
            auto* ws = player->GetWorldspace();
            if (ws && ws->persistentCell) {
                searchDoors(ws->persistentCell);
            }
        }

        if (bestDoor) {
            targetID = bestDoor->GetFormID();
            LOG("AutoWalk: best door match '{}' FormID={:08X} (heading diff={:.1f}°)",
                bestDoor->GetDisplayFullName() ? bestDoor->GetDisplayFullName() : "?",
                targetID, bestHeadingDiff * 180.0f / 3.14159265f);
        } else {
            LOG("AutoWalk: no matching door found, using XMarker fallback");
            // Fallback : créer un XMarker dans la direction de la boussole
            if (g_scanIndex >= 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
                auto& obj = *g_scannedFiltered[g_scanIndex];
                if (obj.lastKnownPos.x != 0 || obj.lastKnownPos.y != 0) {
                    RE::NiPoint3 targetPos = obj.lastKnownPos;
                    targetPos.z = playerPos.z;
                    RE::FormID markerID = CreateTempMarkerAt(targetPos);
                    if (markerID != 0) {
                        g_autoWalkTempMarker = markerID;
                        targetID = markerID;
                    } else {
                        Speak(L"Cannot walk to this target");
                        return;
                    }
                } else {
                    Speak(L"Cannot walk to this target");
                    return;
                }
            } else {
                Speak(L"Cannot walk to this target");
                return;
            }
        }
    }

    Speak(L"Walking to " + targetName);
    // Si le FormID est dynamique (FF*), passer en mode coordonnées
    if ((targetID >> 24) == 0xFF) {
        auto* refForm = RE::TESForm::LookupByID(targetID);
        auto* ref = refForm ? refForm->AsReference() : nullptr;
        if (ref) {
            auto pos = ref->GetPosition();
            StartAutoWalk(targetID, 100.0f, pos.x, pos.y, pos.z);
        } else if (g_scanIndex >= 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
            auto& obj = *g_scannedFiltered[g_scanIndex];
            StartAutoWalk(targetID, 100.0f, obj.lastKnownPos.x, obj.lastKnownPos.y, obj.lastKnownPos.z);
        } else {
            StartAutoWalk(targetID, 100.0f, 0.f, 0.f, 0.f);  // fallback
        }
    } else {
        StartAutoWalk(targetID, 100.0f, 0.f, 0.f, 0.f);
    }
}

// AUTOWALK — FIN
