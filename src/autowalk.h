#pragma once

// AUTOWALK — Pont C++ vers Papyrus pour la marche automatique

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
                auto* targetRef = targetForm->AsReference();
                if (!targetRef) continue;
                auto diff = playerPos - targetRef->GetPosition();
                dist = diff.Length();
            }

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
        // Forcer l'initialisation du mouvement avant de lancer l'IA
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
        }

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
        // Sécurité C++ : remettre AI driven même si Papyrus échoue
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            player->SetAIDriven(false);
            player->EvaluatePackage();

            // Restaurer SpeedMult à sa valeur de base
            auto* avo = player->AsActorValueOwner();
            if (avo) {
                float base = avo->GetBaseActorValue(RE::ActorValue::kSpeedMult);
                avo->SetActorValue(RE::ActorValue::kSpeedMult, base);
                LOG("AutoWalk: restored SpeedMult to base={}", base);
            }

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
                if (hudMenu->uiMovie->GetVariable(&hudRoot, "_root.HUDMovieBaseInstance") && hudRoot.IsObject()) {
                    RE::GFxValue dataArr;
                    if (hudRoot.GetMember("CompassTargetDataA", &dataArr) && dataArr.IsArray()) {
                        uint32_t arrSize = dataArr.GetArraySize();
                        LOG("AutoWalk: CompassTargetDataA size={}", arrSize);

                        // Lire les types de marqueurs de la boussole
                        RE::GFxValue questTypeVal, questDoorTypeVal;
                        float questType = -1, questDoorType = -1;
                        if (hudRoot.GetMember("CompassMarkerQuest", &questTypeVal) && questTypeVal.IsNumber())
                            questType = static_cast<float>(questTypeVal.GetNumber());
                        if (hudRoot.GetMember("CompassMarkerQuestDoor", &questDoorTypeVal) && questDoorTypeVal.IsNumber())
                            questDoorType = static_cast<float>(questDoorTypeVal.GetNumber());

                        LOG("AutoWalk: quest marker types: quest={:.0f} questDoor={:.0f}", questType, questDoorType);

                        // Stride = 4 : heading, alpha, type, scale
                        for (uint32_t i = 0; i + 3 < arrSize; i += 4) {
                            RE::GFxValue headingVal, typeVal;
                            dataArr.GetElement(i, &headingVal);      // heading
                            dataArr.GetElement(i + 2, &typeVal);     // type

                            if (!typeVal.IsNumber()) continue;
                            float type = static_cast<float>(typeVal.GetNumber());

                            if (type == questType || type == questDoorType) {
                                if (headingVal.IsNumber()) {
                                    compassHeading = static_cast<float>(headingVal.GetNumber());
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
            StartAutoWalk(targetID, 100.0f);  // fallback
        }
    } else {
        StartAutoWalk(targetID, 100.0f);
    }
}

// AUTOWALK — FIN
