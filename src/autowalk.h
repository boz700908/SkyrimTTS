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

// =============================================================================
// DEBUG FLAGS - Tests de rollback anti-crash BSShaderAccumulator (2026-04-20)
// =============================================================================
// Pour identifier laquelle de ces 3 differences v1.3 -> v1.5 declenche le crash,
// on desactive chacune independamment. Remettre a true quand le coupable est
// identifie pour restaurer le comportement normal.
//
// Pour reactiver : changer chaque flag a 'true' et rebuild.
// Cote Papyrus (autowalk/SkyrimTTS_AutoWalk.psc) : des lignes 'ROLLBACK test'
// commentent StopCombat/StopCombatAlarm et le Disable/Delete du XMarker. Pour
// restaurer, decommenter ces blocs dans le .psc et recompiler.
// =============================================================================
static constexpr bool g_autoWalkEnableStuckRecovery = true;   // v1.5: true. rollback: pas de SetAIDriven toggle toutes les 4s
static constexpr bool g_autoWalkEnableXMarkerDelete = true;   // v1.5: true. rollback: pas de Disable+Delete du temp marker C++

static std::atomic_bool g_autoWalking{false};
static std::wstring     g_autoWalkTarget;
static RE::FormID       g_autoWalkTargetID{0};
static float            g_autoWalkStopDist{100.0f};
static RE::NiPoint3     g_autoWalkTargetPos{0, 0, 0};  // pour le mode coordonnées
static std::jthread     g_autoWalkMonitor;

// --- Routing de quete : nouvelle methode location-based ---
// true  = essayer d'abord le routing par hierarchie de locations (BGSLocation::specialRefs,
//         LocTypeDungeonEntrance, parentLoc, etc.). Plus fiable car aligne sur la
//         topologie logique du donjon (pas la geometrie 3D).
// false = sauter direct au routing compass historique (matching d'angle).
// En cas d'echec du routing location-based (location null, alias non rempli, mod
// custom mal tagge...), on retombe AUTOMATIQUEMENT sur le compass — donc on n'est
// jamais bloque, ce flag est juste un kill switch global au cas ou.
static constexpr bool g_useLocationRouting = true;

// Cooldown de sécurité : empêche de lancer l'autowalk pendant la fenêtre
// fragile après un load ou un changement de cellule. Pendant cette fenêtre,
// le skeleton/shader du joueur est en cours de reconstruction et un
// SetAIDriven déclenche un null pointer dans le pipeline de rendu (crashs
// observés : thread worker, instruction `and [rax+0xF4],...` avec rax=0,
// stack = BSFadeNode "Skeleton.nif" + BSShaderAccumulator + NiCamera).
// Stocke le timestamp (ms depuis epoch) jusqu'auquel l'autowalk est bloqué.
static std::atomic<int64_t> g_autoWalkUnsafeUntilMs{0};

static int64_t AutoWalkNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Arme le cooldown pour N millisecondes depuis maintenant. Appelée depuis
// plugin.cpp sur kPostLoadGame (10s) et TESCellFullyLoadedEvent (5s).
// IMPORTANT : on garde le MAX entre le cooldown existant et le nouveau. Sans ça,
// si kPostLoadGame arme 10s puis LoadingMenu arme 5s (appelés à 1ms d'intervalle),
// le 5s écrasait le 10s → cooldown effective trop courte → autowalk se lance
// pendant que le skeleton n'est pas stable → crash BSShaderAccumulator.
static void AutoWalkArmSafetyCooldown(int64_t durationMs, const char* reason) {
    int64_t newUntil = AutoWalkNowMs() + durationMs;
    int64_t currentUntil = g_autoWalkUnsafeUntilMs.load();
    int64_t finalUntil = (newUntil > currentUntil) ? newUntil : currentUntil;
    g_autoWalkUnsafeUntilMs.store(finalUntil);
    int64_t effectiveMs = finalUntil - AutoWalkNowMs();
    LOG("AutoWalk: safety cooldown armed for {}ms ({}), effective={}ms",
        durationMs, reason, effectiveMs);
}

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

// Temp marker C++ (mode boussole/fallback) — déclaré ici pour que StopAutoWalk puisse le cleanup
static RE::FormID g_autoWalkTempMarker{0};

// Sécurité : nettoyage COMPLET d'un autowalk potentiellement gravé dans la save
// (cas où le jeu a crashé pendant un autowalk → au reload la save contient
// AIDriven=true, DstMarker set, IsWalking=true, Travel package actif → si on toggle
// un nouvel autowalk, on superpose sur cet état bancal → crash immédiat).
//
// Stratégie en deux temps :
//   1. IMMÉDIAT (safe, pas d'engine-side AI) : reset C++ rapide (SetAIDriven false,
//      SpeedMult reset). Ces appels ne touchent pas au pathfinding ni au Travel
//      package, donc safe même pendant la fenêtre fragile post-load.
//   2. DIFFÉRÉ DE 3 SECONDES : dispatch OnLoadGameReset côté Papyrus. Le dispatch
//      appelle EvaluatePackage qui, lui, fait tourner le pathfinding interne. Si
//      on le fait à kPostLoadGame (T=0s), le skeleton/shader du joueur est encore
//      en reconstruction et EvaluatePackage peut crasher (BSShaderAccumulator).
//      En attendant 3s, on laisse la fenêtre fragile se refermer avant de toucher
//      à l'AI. Le cooldown de 10s armé à kPostLoadGame bloque de toute façon tout
//      nouvel autowalk pendant cette attente.
static void AutoWalkSafetyReset() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;

    // Étape 1 : reset C++ immédiat (safe — ne touche pas à l'AI/pathfinding)
    task->AddTask([]() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player && !g_autoWalking.load()) {
            player->SetAIDriven(false);
            auto* avo = player->AsActorValueOwner();
            if (avo) {
                float base = avo->GetBaseActorValue(RE::ActorValue::kSpeedMult);
                float current = avo->GetActorValue(RE::ActorValue::kSpeedMult);
                if (base > 0 && current != base) {
                    avo->SetActorValue(RE::ActorValue::kSpeedMult, base);
                    LOG("AutoWalk: safety reset SpeedMult {} -> {}", current, base);
                }
            }
            LOG("AutoWalk: safety reset on load — AIDriven=false (Papyrus cleanup delayed 3s)");
        }
    });

    // Étape 2 : dispatch OnLoadGameReset après 3 secondes, le temps que le
    // skeleton/shader du joueur finisse sa reconstruction. On utilise un
    // std::thread détaché qui sleep puis poste la dispatch sur le thread principal.
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        auto* task2 = SKSE::GetTaskInterface();
        if (!task2) return;
        task2->AddTask([]() {
            auto* quest = FindAutoWalkQuest();
            if (!quest) {
                LOG("AutoWalk: delayed safety reset — quest not found, skipping Papyrus cleanup");
                return;
            }
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
                RE::BSFixedString("OnLoadGameReset"),
                args,
                callback
            );
            LOG("AutoWalk: delayed (3s) safety reset — OnLoadGameReset dispatched to Papyrus");
        });
    }).detach();
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

                // Pendant un écran de chargement, un fondu, OU n'importe quel menu qui
                // met le jeu en pause (Inventaire, Journal, Carte, Stats, MessageBox,
                // Main Menu, etc.), on met le moniteur en PAUSE TOTALE.
                //
                // Deux raisons :
                //   1. LoadingMenu/FaderMenu : le skeleton/shader du joueur est en cours
                //      de reconstruction — toute intervention (stuck recovery,
                //      EvaluatePackage, SetAIDriven) peut déclencher le crash
                //      BSShaderAccumulator.
                //   2. Menus pausants (Inventaire/Journal/Carte/...) : le joueur ne bouge
                //      pas physiquement pendant qu'il lit son inventaire. Le stuck timer
                //      atteindrait 4s rapidement et déclencherait la recovery AIDriven
                //      toggle. Or le moteur est dans un état gelé pendant le pause →
                //      EvaluatePackage sur un monde gelé peut désynchroniser l'AI.
                //
                // On remet le stuck timer et la last pos à zéro pour ne pas déclencher
                // la recovery juste après la fermeture du menu.
                auto* ui = RE::UI::GetSingleton();
                if (ui && (ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
                           ui->IsMenuOpen("Fader Menu") ||
                           ui->GameIsPaused())) {
                    g_autoWalkStuckTimer = 0.0f;
                    g_autoWalkLastPos = player->GetPosition();
                    return;
                }

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
                    // Escalade graduelle pour minimiser les risques de crash :
                    //   - 4s : tentative douce (réappliquer les flags + un simple
                    //          EvaluatePackage). Suffit pour la majorité des blocages
                    //          (escaliers, petits obstacles, IA qui a besoin de
                    //          reconsidérer son chemin). Pas de toggle AIDriven donc
                    //          pas de risque de désynchroniser le moteur.
                    //   - 6s : si le doux n'a pas suffi, on passe au toggle complet
                    //          SetAIDriven(false)/(true) + EvaluatePackage. Plus
                    //          violent, peut provoquer des races avec la physique ou
                    //          les animations, mais nécessaire pour les blocages
                    //          coriaces.
                    //   - 10s : on abandonne, l'autowalk est vraiment coincé.
                    if (g_autoWalkEnableStuckRecovery && g_autoWalkStuckTimer > 4.0f && s_recoveryAttempt == 0) {
                        LOG("AutoWalk: stuck for 4s, gentle recovery (re-apply flags + EvaluatePackage)");
                        // Remettre les flags du char controller : au cas où ils auraient
                        // été effacés par un ragdoll, une transition de cellule ou une
                        // animation spéciale. Sans kTryStep, le joueur ne monte plus les
                        // escaliers automatiquement → reste bloqué indéfiniment.
                        auto* charCtrl = player->GetCharController();
                        if (charCtrl) {
                            charCtrl->flags.set(RE::CHARACTER_FLAGS::kTryStep);
                            charCtrl->flags.set(RE::CHARACTER_FLAGS::kCanJump);
                        }
                        player->EvaluatePackage();
                        s_recoveryAttempt = 1;
                    } else if (g_autoWalkEnableStuckRecovery && g_autoWalkStuckTimer > 6.0f && s_recoveryAttempt == 1) {
                        LOG("AutoWalk: still stuck at 6s, hard recovery (full AIDriven toggle)");
                        // Toggle complet : dernier recours avant d'abandonner. Plus
                        // risqué car peut perturber char controller / animations,
                        // mais débloque les cas vraiment coincés.
                        auto* charCtrl = player->GetCharController();
                        if (charCtrl) {
                            charCtrl->flags.set(RE::CHARACTER_FLAGS::kTryStep);
                            charCtrl->flags.set(RE::CHARACTER_FLAGS::kCanJump);
                        }
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

// =============================================================================
// DIAG-CRASH : dump de l'etat du joueur/scenegraph pour traquer le crash BSShaderAccumulator.
// Appele juste avant le dispatch (tag="PRE-DISPATCH") puis en continu apres le dispatch
// (tag="T+XXXms") par le CrashDiagPoll thread. Si un champ bascule entre deux appels,
// on le verra dans les logs juste avant le crash.
// IMPORTANT : doit tourner sur le main thread (AddTask), jamais sur un jthread.
// =============================================================================
static void AutoWalkRunCrashDiag(RE::FormID targetFormID, bool useCoords,
                                 float posX, float posY, float posZ, const char* tag) {
    auto* p = RE::PlayerCharacter::GetSingleton();
    if (!p) {
        LOG("AutoWalk DIAG [{}]: Player singleton is NULL !!!", tag);
        return;
    }

    auto* body3D = p->Get3D();
    auto* charCtrl = p->GetCharController();
    auto* cell = p->GetParentCell();
    auto pos = p->GetPosition();
    bool posValid = !std::isnan(pos.x) && !std::isnan(pos.y) && !std::isnan(pos.z);
    auto* process = p->GetActorRuntimeData().currentProcess;

    auto* ui = RE::UI::GetSingleton();
    bool loadingOpen = ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
    bool faderOpen = ui && ui->IsMenuOpen("Fader Menu");
    bool anyMenuOpen = ui && ui->GameIsPaused();

    LOG("=== AutoWalk DIAG [{}] ===", tag);
    LOG("  [Player core]");
    LOG("    Singleton:          OK");
    LOG("    3D model:           {}", body3D ? "OK" : "NULL !!!");
    LOG("    Char controller:    {}", charCtrl ? "OK" : "NULL !!!");
    LOG("    Position:           ({:.1f},{:.1f},{:.1f}) valid={}",
        pos.x, pos.y, pos.z, posValid);
    LOG("    currentProcess:     {}", process ? "OK" : "NULL !!!");
    LOG("    Race:               {}",
        p->GetRace() && p->GetRace()->GetName() ? p->GetRace()->GetName() : "?");

    LOG("  [Player state]");
    LOG("    IsDead:             {}", p->IsDead());
    LOG("    IsInCombat:         {}", p->IsInCombat());
    LOG("    IsOnMount:          {}", p->IsOnMount());
    LOG("    IsSneaking:         {}", p->IsSneaking());
    LOG("    IsInKillMove:       {}", p->IsInKillMove());
    LOG("    IsAIEnabled:        {}", p->IsAIEnabled());
    if (auto* avo = p->AsActorValueOwner()) {
        LOG("    Health:             {:.0f}/{:.0f}",
            avo->GetActorValue(RE::ActorValue::kHealth),
            avo->GetPermanentActorValue(RE::ActorValue::kHealth));
        LOG("    SpeedMult:          current={:.0f} base={:.0f}",
            avo->GetActorValue(RE::ActorValue::kSpeedMult),
            avo->GetBaseActorValue(RE::ActorValue::kSpeedMult));
    }

    LOG("  [Cell / world]");
    LOG("    Parent cell:        {} ({})",
        cell ? "OK" : "NULL !!!",
        cell && cell->GetName() ? cell->GetName() : "?");
    LOG("    Cell attached:      {}",
        cell ? (cell->IsAttached() ? "yes" : "no (loading?)") : "N/A");
    LOG("    Cell interior:      {}",
        cell ? (cell->IsInteriorCell() ? "yes" : "no (exterior)") : "N/A");
    if (auto* worldspace = p->GetWorldspace()) {
        LOG("    Worldspace:         {}",
            worldspace->GetName() ? worldspace->GetName() : "?");
    }

    LOG("  [UI / context]");
    LOG("    LoadingMenu open:   {}", loadingOpen);
    LOG("    Fader Menu open:    {}", faderOpen);
    LOG("    Game paused:        {}", anyMenuOpen);

    LOG("  [Char controller state]");
    if (charCtrl) {
        const char* stateStr = "?";
        switch (charCtrl->context.currentState) {
            case RE::hkpCharacterStateType::kOnGround: stateStr = "OnGround"; break;
            case RE::hkpCharacterStateType::kJumping:  stateStr = "Jumping"; break;
            case RE::hkpCharacterStateType::kInAir:    stateStr = "InAir"; break;
            case RE::hkpCharacterStateType::kClimbing: stateStr = "Climbing"; break;
            case RE::hkpCharacterStateType::kSwimming: stateStr = "Swimming"; break;
            default: break;
        }
        LOG("    State:              {} (raw={})",
            stateStr, static_cast<int>(charCtrl->context.currentState));
        LOG("    wantState:          {}", static_cast<int>(charCtrl->wantState));
    }

    LOG("  [AI / package]");
    if (process) {
        if (auto* pkg = process->GetRunningPackage()) {
            LOG("    Running package:    formID=0x{:08X} type={}",
                pkg->GetFormID(),
                static_cast<int>(pkg->packData.packType.underlying()));
        } else {
            LOG("    Running package:    NONE");
        }
    } else {
        LOG("    Running package:    N/A (no process)");
    }

    LOG("  [Target]");
    if (targetFormID != 0) {
        auto* tform = RE::TESForm::LookupByID(targetFormID);
        if (!tform) {
            LOG("    FormID:             0x{:08X} (NOT FOUND !!!)", targetFormID);
        } else {
            auto* tref = tform->AsReference();
            LOG("    FormID:             0x{:08X} type={}",
                targetFormID, static_cast<int>(tform->GetFormType()));
            LOG("    IsReference:        {}", tref ? "yes" : "no");
            if (tref) {
                auto tp = tref->GetPosition();
                auto* tcell = tref->GetParentCell();
                LOG("    Target position:    ({:.1f},{:.1f},{:.1f})", tp.x, tp.y, tp.z);
                LOG("    Target cell:        {} ({})",
                    tcell ? "OK" : "NULL",
                    tcell && tcell->GetName() ? tcell->GetName() : "?");
                LOG("    Target 3D:          {}",
                    tref->Get3D() ? "OK" : "NULL (not loaded)");
                auto diff = pos - tp;
                LOG("    Distance:           {:.1f}", diff.Length());
            }
        }
    } else if (useCoords) {
        LOG("    Coords mode:        pos=({:.1f},{:.1f},{:.1f})", posX, posY, posZ);
    } else {
        LOG("    No target specified");
    }

    // ============ DIAG ENRICHI POUR TRAQUER LE CRASH BSShaderAccumulator ============
    // Instruction qui crashe = and dword ptr [rax+0xF4] avec rax=0.
    // Offset 0xF4 = NiAVObject::flags. Donc le moteur tente de modifier
    // les flags d'un node 3D null pendant une passe de rendu/culling.
    // On logue tous les etats critiques liees au 3D/fade/render.
    LOG("  [DIAG-CRASH]");
    try {
        // --- Skeleton BSFadeNode du joueur : fade en cours ? ---
        if (body3D) {
            auto* fadeNode = body3D->AsFadeNode();
            if (fadeNode) {
                auto& rt = fadeNode->GetRuntimeData();
                LOG("    fadeNode.currentFade={:.3f} u128={:.3f} u140={:.3f}",
                    rt.currentFade, rt.unk128, rt.unk140);
                LOG("    fadeNode.flags152/153/154/155={:02X}/{:02X}/{:02X}/{:02X}",
                    (int)rt.unk152, (int)rt.unk153, (int)rt.unk154, (int)rt.unk155);
            }
            // --- Scan des enfants du skeleton : y a-t-il des nullptr ou flags aberrants ? ---
            auto* node = body3D->AsNode();
            if (node) {
                auto& children = node->GetChildren();
                int nullCount = 0;
                int totalCount = static_cast<int>(children.size());
                for (std::uint32_t i = 0; i < children.size(); ++i) {
                    if (!children[i]) nullCount++;
                }
                LOG("    skeleton.children total={} null={} selfFlags={:08X}",
                    totalCount, nullCount, body3D->GetFlags().underlying());

                // --- Dump recursif des enfants du skeleton : nom + type + flags + pointeur. ---
                // Si un enfant devient null d'un tick au suivant, on le verra. On se limite au
                // premier niveau (pas recursif profond) pour ne pas noyer le log.
                for (std::uint32_t i = 0; i < children.size() && i < 32; ++i) {
                    auto& ch = children[i];
                    if (!ch) {
                        LOG("      child[{}]=NULL", i);
                    } else {
                        const char* cname = ch->name.empty() ? "?" : ch->name.c_str();
                        LOG("      child[{}] name='{}' flags={:08X} ptr={:p}",
                            i, cname, ch->GetFlags().underlying(), static_cast<void*>(ch.get()));
                    }
                }
            }
        } else {
            LOG("    skeleton.3D=NULL (player has no 3D model)");
        }
    } catch (...) { LOG("    [diag skeleton] exception"); }

    try {
        // --- Actor flags internes (delayUpdateScenegraph, resetAI, etc.) ---
        auto& rt = p->GetActorRuntimeData();
        LOG("    actor.boolBits={:08X} boolFlags={:08X} criticalStage={}",
            rt.boolBits.underlying(), rt.boolFlags.underlying(),
            static_cast<int>(rt.criticalStage.underlying()));
    } catch (...) { LOG("    [diag actor flags] exception"); }

    try {
        // --- Char controller detail (state, want state, fall time, havok) ---
        if (charCtrl) {
            LOG("    cc.flags={:08X} fallTime={:.2f} speedPct={:.3f} scale={:.2f}",
                charCtrl->flags.underlying(), charCtrl->fallTime,
                charCtrl->speedPct, charCtrl->scale);
        }
    } catch (...) { LOG("    [diag cc detail] exception"); }

    try {
        // --- Camera state (transition, zoom) ---
        auto* cam = RE::PlayerCamera::GetSingleton();
        if (cam) {
            int camStateId = cam->currentState ? static_cast<int>(cam->currentState->id) : -1;
            LOG("    cam.state={} idleTimer={:.2f} yaw={:.3f} bowZoom={} weapSheath={}",
                camStateId, cam->idleTimer, cam->yaw,
                cam->bowZoomedIn, cam->isWeapSheathed);
        }
    } catch (...) { LOG("    [diag camera] exception"); }

    try {
        // --- Process + package detail ---
        if (process) {
            auto& pkg = process->currentPackage;
            LOG("    pkg.cur={:08X} target={:08X} procIdx={} startT={:.2f}",
                pkg.package ? pkg.package->GetFormID() : 0,
                pkg.target.native_handle(),
                pkg.currentProcedureIndex, pkg.packageStartTime);
            LOG("    pkg.modFlags={:08X} modIntFlag={:04X} actorPkgFlags={:02X}",
                pkg.modifiedPackageFlag, pkg.modifiedInterruptFlag,
                pkg.actorPackageFlags.underlying());

            // MiddleHigh (update 3D pending, killmove, furniture...)
            if (auto* mh = process->middleHigh) {
                LOG("    mh.update3D={:02X} alphaMult={:.2f} killMoveT={:.2f} forceNextUpdate={}",
                    mh->update3DModel.underlying(), mh->alphaMult,
                    mh->killMoveTimer, mh->forceNextUpdate);
                LOG("    mh.cc={} weapBone={} headNode={} furnID={} occupiedFurn={:08X}",
                    mh->charController.get() ? "ok" : "null",
                    mh->weaponBone ? "ok" : "null",
                    mh->headNode ? "ok" : "null",
                    mh->currentFurnitureMarkerID,
                    mh->occupiedFurniture.native_handle());
            }
            // High (fade, voice, door approach)
            if (auto* h = process->high) {
                LOG("    hi.fadeState={} fadeTrigger={:08X} maxAlpha={:.2f} approachDoor={}",
                    static_cast<int>(h->fadeState.underlying()),
                    h->fadeTrigger ? h->fadeTrigger->GetFormID() : 0,
                    h->maxAlpha, h->approachingAutoTeleportDoor);
                LOG("    hi.voiceState={} voiceT={:.2f} greetingPC={} talkingToPC={}",
                    static_cast<int>(h->voiceState.underlying()),
                    h->voiceTimer, h->greetingPlayer, h->talkingToPC);
            }
        }
    } catch (...) { LOG("    [diag process] exception"); }

    try {
        // --- Cell detail (state, detached, flags) ---
        if (cell) {
            LOG("    cell.state={} detached={} flags={:04X}",
                static_cast<int>(cell->cellState.underlying()),
                cell->cellDetached,
                cell->cellFlags.underlying());
        }
    } catch (...) { LOG("    [diag cell detail] exception"); }

    try {
        // --- Main state (freeze, onIdle, reload) ---
        auto* main = RE::Main::GetSingleton();
        if (main) {
            LOG("    main.gameActive={} onIdle={} freezeTime={} freezeNext={} reloadContent={} resetGame={} quitGame={}",
                main->gameActive, main->onIdle,
                main->freezeTime, main->freezeNextFrame,
                main->reloadContent, main->resetGame, main->quitGame);
        }
    } catch (...) { LOG("    [diag main] exception"); }

    try {
        // --- UI stack (menus en cours) ---
        auto* ui2 = RE::UI::GetSingleton();
        if (ui2) {
            LOG("    ui.stack={} pauses={} itemMenus={} modal={} closingAll={} visible={}",
                ui2->menuStack.size(),
                ui2->numPausesGame, ui2->numItemMenus,
                static_cast<int>(ui2->modal),
                static_cast<int>(ui2->closingAllMenus),
                static_cast<int>(ui2->menuSystemVisible));
        }
    } catch (...) { LOG("    [diag ui] exception"); }

    try {
        // --- ProcessLists (charge NPC, queues de tâches) ---
        auto* pl = RE::ProcessLists::GetSingleton();
        if (pl) {
            LOG("    pl.actors high={} mh={} ml={} low={}",
                pl->highActorHandles.size(),
                pl->middleHighActorHandles.size(),
                pl->middleLowActorHandles.size(),
                pl->lowActorHandles.size());
            LOG("    pl.runSched={} runMov={} runAnim={} magicEff={}",
                pl->runSchedules, pl->runMovement, pl->runAnimations,
                pl->magicEffects.size());
        }
    } catch (...) { LOG("    [diag processlists] exception"); }

    try {
        // --- Time / calendar (pour correler avec autres events) ---
        auto* cal = RE::Calendar::GetSingleton();
        if (cal) {
            LOG("    time.hour={:.3f} days={:.3f} scale={:.1f}",
                cal->GetHour(), cal->rawDaysPassed, cal->GetTimescale());
        }
    } catch (...) { LOG("    [diag time] exception"); }

    LOG("=== END AutoWalk DIAG [{}] ===", tag);
}

// =============================================================================
// CRASH DIAG POLL : thread dedie qui loggue l'etat du joueur en continu apres le
// dispatch pour capturer le moment exact ou un champ bascule et cause le crash.
//
// Cadence :
//   - Premieres 3s : toutes les 100ms (capture fine)
//   - Apres 3s     : toutes les 500ms (moins fin, ne sature pas le log)
//   - Arret        : quand g_autoWalking devient false OU apres 60s max
// =============================================================================
static std::jthread g_autoWalkCrashDiag;

static void StartAutoWalkCrashDiagPoll(RE::FormID targetFormID, bool useCoords,
                                       float posX, float posY, float posZ) {
    // Arreter un eventuel poll precedent (safety)
    if (g_autoWalkCrashDiag.joinable()) {
        g_autoWalkCrashDiag.request_stop();
        g_autoWalkCrashDiag.join();
    }

    g_autoWalkCrashDiag = std::jthread([targetFormID, useCoords, posX, posY, posZ](std::stop_token stoken) {
        auto start = std::chrono::steady_clock::now();
        int tickCount = 0;
        while (!stoken.stop_requested()) {
            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

            // Stop conditions
            if (!g_autoWalking.load()) break;
            if (elapsedMs > 60000) break;  // 60s max

            // Log en fonction de l'age
            int sleepMs = (elapsedMs < 3000) ? 100 : 500;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            if (stoken.stop_requested()) break;

            // Dispatcher le dump sur le main thread
            auto* taskIf = SKSE::GetTaskInterface();
            if (!taskIf) continue;
            long elapsedCapture = static_cast<long>(elapsedMs);
            taskIf->AddTask([targetFormID, useCoords, posX, posY, posZ, elapsedCapture]() {
                if (!g_autoWalking.load()) return;
                char tag[32];
                std::snprintf(tag, sizeof(tag), "T+%ldms", elapsedCapture);
                AutoWalkRunCrashDiag(targetFormID, useCoords, posX, posY, posZ, tag);
            });
            tickCount++;
        }
        LOG("AutoWalk CrashDiagPoll: stopped after {} ticks", tickCount);
    });
}

// =============================================================================
// ANTI-REBOND : empeche deux dispatches rapproches sur le meme FormID.
// Bien qu'un dispatch unique puisse aussi crasher, la rafale de dispatches rapides
// observee dans certains crash logs (20 dispatches en 20s) augmente nettement les
// chances de crash, probablement en empilant des EvaluatePackage mal finalises.
// Fenetre minimale : 500ms entre deux dispatches.
// =============================================================================
static std::atomic<int64_t> g_autoWalkLastDispatchMs{0};
static constexpr int64_t kAutoWalkMinDispatchGapMs = 500;

// Appelle OnWalkToTarget sur le script Papyrus de la quete AutoWalk
// Si posX/posY/posZ sont fournis (non-zero), le FormID est passé à 0 et le Papyrus
// utilise les coordonnées directement (mode FF* pour objets dynamiques).
static void StartAutoWalk(RE::FormID targetFormID, float stopDistance = 100.0f,
                          float posX = 0.f, float posY = 0.f, float posZ = 0.f) {
    // Anti-rebond : si moins de 500ms depuis le dernier dispatch, on refuse.
    {
        int64_t now = AutoWalkNowMs();
        int64_t last = g_autoWalkLastDispatchMs.load();
        int64_t gap = now - last;
        if (last > 0 && gap < kAutoWalkMinDispatchGapMs) {
            LOG("AutoWalk: REJECTED - too soon since last dispatch ({}ms ago, min={}ms)",
                gap, kAutoWalkMinDispatchGapMs);
            return;
        }
        g_autoWalkLastDispatchMs.store(now);
    }

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

        // === DIAGNOSTIC PRE-DISPATCH ===
        // Factorise dans AutoWalkRunCrashDiag() pour pouvoir etre re-appele par
        // le CrashDiagPoll thread apres le dispatch (toutes les 100ms les 3 premieres
        // secondes, puis 500ms). Si un champ bascule entre deux ticks, on le voit
        // juste avant le crash.
        AutoWalkRunCrashDiag(targetFormID, useCoords, posX, posY, posZ, "PRE-DISPATCH");
#if 0
        {
            auto* p = RE::PlayerCharacter::GetSingleton();
            if (!p) {
                LOG("AutoWalk PRE-DISPATCH DIAG: Player singleton is NULL !!!");
            } else {
                auto* body3D = p->Get3D();
                auto* charCtrl = p->GetCharController();
                auto* cell = p->GetParentCell();
                auto pos = p->GetPosition();
                bool posValid = !std::isnan(pos.x) && !std::isnan(pos.y) && !std::isnan(pos.z);
                auto* process = p->GetActorRuntimeData().currentProcess;

                auto* ui = RE::UI::GetSingleton();
                bool loadingOpen = ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
                bool faderOpen = ui && ui->IsMenuOpen("Fader Menu");
                bool anyMenuOpen = ui && ui->GameIsPaused();

                LOG("=== AutoWalk PRE-DISPATCH DIAG ===");
                LOG("  [Player core]");
                LOG("    Singleton:          OK");
                LOG("    3D model:           {}", body3D ? "OK" : "NULL !!!");
                LOG("    Char controller:    {}", charCtrl ? "OK" : "NULL !!!");
                LOG("    Position:           ({:.1f},{:.1f},{:.1f}) valid={}",
                    pos.x, pos.y, pos.z, posValid);
                LOG("    currentProcess:     {}", process ? "OK" : "NULL !!!");
                LOG("    Race:               {}",
                    p->GetRace() && p->GetRace()->GetName() ? p->GetRace()->GetName() : "?");

                LOG("  [Player state]");
                LOG("    IsDead:             {}", p->IsDead());
                LOG("    IsInCombat:         {}", p->IsInCombat());
                LOG("    IsOnMount:          {}", p->IsOnMount());
                LOG("    IsSneaking:         {}", p->IsSneaking());
                LOG("    IsInKillMove:       {}", p->IsInKillMove());
                LOG("    IsAIEnabled:        {}", p->IsAIEnabled());
                if (auto* avo = p->AsActorValueOwner()) {
                    LOG("    Health:             {:.0f}/{:.0f}",
                        avo->GetActorValue(RE::ActorValue::kHealth),
                        avo->GetPermanentActorValue(RE::ActorValue::kHealth));
                    LOG("    SpeedMult:          current={:.0f} base={:.0f}",
                        avo->GetActorValue(RE::ActorValue::kSpeedMult),
                        avo->GetBaseActorValue(RE::ActorValue::kSpeedMult));
                }

                LOG("  [Cell / world]");
                LOG("    Parent cell:        {} ({})",
                    cell ? "OK" : "NULL !!!",
                    cell && cell->GetName() ? cell->GetName() : "?");
                LOG("    Cell attached:      {}",
                    cell ? (cell->IsAttached() ? "yes" : "no (loading?)") : "N/A");
                LOG("    Cell interior:      {}",
                    cell ? (cell->IsInteriorCell() ? "yes" : "no (exterior)") : "N/A");
                if (auto* worldspace = p->GetWorldspace()) {
                    LOG("    Worldspace:         {}",
                        worldspace->GetName() ? worldspace->GetName() : "?");
                }

                LOG("  [UI / context]");
                LOG("    LoadingMenu open:   {}", loadingOpen);
                LOG("    Fader Menu open:    {}", faderOpen);
                LOG("    Game paused:        {}", anyMenuOpen);

                LOG("  [Char controller state]");
                if (charCtrl) {
                    const char* stateStr = "?";
                    switch (charCtrl->context.currentState) {
                        case RE::hkpCharacterStateType::kOnGround: stateStr = "OnGround"; break;
                        case RE::hkpCharacterStateType::kJumping:  stateStr = "Jumping"; break;
                        case RE::hkpCharacterStateType::kInAir:    stateStr = "InAir"; break;
                        case RE::hkpCharacterStateType::kClimbing: stateStr = "Climbing"; break;
                        case RE::hkpCharacterStateType::kSwimming: stateStr = "Swimming"; break;
                        default: break;
                    }
                    LOG("    State:              {} (raw={})",
                        stateStr, static_cast<int>(charCtrl->context.currentState));
                    LOG("    wantState:          {}", static_cast<int>(charCtrl->wantState));
                }

                LOG("  [AI / package]");
                if (process) {
                    if (auto* pkg = process->GetRunningPackage()) {
                        LOG("    Running package:    formID=0x{:08X} type={}",
                            pkg->GetFormID(),
                            static_cast<int>(pkg->packData.packType.underlying()));
                    } else {
                        LOG("    Running package:    NONE");
                    }
                } else {
                    LOG("    Running package:    N/A (no process)");
                }

                LOG("  [Target]");
                if (targetFormID != 0) {
                    auto* tform = RE::TESForm::LookupByID(targetFormID);
                    if (!tform) {
                        LOG("    FormID:             0x{:08X} (NOT FOUND !!!)", targetFormID);
                    } else {
                        auto* tref = tform->AsReference();
                        LOG("    FormID:             0x{:08X} type={}",
                            targetFormID, static_cast<int>(tform->GetFormType()));
                        LOG("    IsReference:        {}", tref ? "yes" : "no");
                        if (tref) {
                            auto tp = tref->GetPosition();
                            auto* tcell = tref->GetParentCell();
                            LOG("    Target position:    ({:.1f},{:.1f},{:.1f})", tp.x, tp.y, tp.z);
                            LOG("    Target cell:        {} ({})",
                                tcell ? "OK" : "NULL",
                                tcell && tcell->GetName() ? tcell->GetName() : "?");
                            LOG("    Target 3D:          {}",
                                tref->Get3D() ? "OK" : "NULL (not loaded)");
                            auto diff = pos - tp;
                            LOG("    Distance:           {:.1f}", diff.Length());
                        }
                    }
                } else if (useCoords) {
                    LOG("    Coords mode:        pos=({:.1f},{:.1f},{:.1f})", posX, posY, posZ);
                } else {
                    LOG("    No target specified");
                }

                // ============ DIAG ENRICHI POUR TRAQUER LE CRASH BSShaderAccumulator ============
                // Instruction qui crashe = and dword ptr [rax+0xF4] avec rax=0.
                // Offset 0xF4 = NiAVObject::flags. Donc le moteur tente de modifier
                // les flags d'un node 3D null pendant une passe de rendu/culling.
                // On logue tous les etats critiques liees au 3D/fade/render.
                LOG("  [DIAG-CRASH]");
                try {
                    // --- Skeleton BSFadeNode du joueur : fade en cours ? ---
                    if (body3D) {
                        auto* fadeNode = body3D->AsFadeNode();
                        if (fadeNode) {
                            auto& rt = fadeNode->GetRuntimeData();
                            LOG("    fadeNode.currentFade={:.3f} u128={:.3f} u140={:.3f}",
                                rt.currentFade, rt.unk128, rt.unk140);
                            LOG("    fadeNode.flags152/153/154/155={:02X}/{:02X}/{:02X}/{:02X}",
                                (int)rt.unk152, (int)rt.unk153, (int)rt.unk154, (int)rt.unk155);
                        }
                        // --- Scan des enfants du skeleton : y a-t-il des nullptr ou flags aberrants ? ---
                        auto* node = body3D->AsNode();
                        if (node) {
                            auto& children = node->GetChildren();
                            int nullCount = 0;
                            int totalCount = static_cast<int>(children.size());
                            for (std::uint32_t i = 0; i < children.size(); ++i) {
                                if (!children[i]) nullCount++;
                            }
                            LOG("    skeleton.children total={} null={} selfFlags={:08X}",
                                totalCount, nullCount, body3D->GetFlags().underlying());
                        }
                    } else {
                        LOG("    skeleton.3D=NULL (player has no 3D model)");
                    }
                } catch (...) { LOG("    [diag skeleton] exception"); }

                try {
                    // --- Actor flags internes (delayUpdateScenegraph, resetAI, etc.) ---
                    auto& rt = p->GetActorRuntimeData();
                    LOG("    actor.boolBits={:08X} boolFlags={:08X} criticalStage={}",
                        rt.boolBits.underlying(), rt.boolFlags.underlying(),
                        static_cast<int>(rt.criticalStage.underlying()));
                } catch (...) { LOG("    [diag actor flags] exception"); }

                try {
                    // --- Char controller detail (state, want state, fall time, havok) ---
                    if (charCtrl) {
                        LOG("    cc.flags={:08X} fallTime={:.2f} speedPct={:.3f} scale={:.2f}",
                            charCtrl->flags.underlying(), charCtrl->fallTime,
                            charCtrl->speedPct, charCtrl->scale);
                    }
                } catch (...) { LOG("    [diag cc detail] exception"); }

                try {
                    // --- Camera state (transition, zoom) ---
                    auto* cam = RE::PlayerCamera::GetSingleton();
                    if (cam) {
                        int camStateId = cam->currentState ? static_cast<int>(cam->currentState->id) : -1;
                        LOG("    cam.state={} idleTimer={:.2f} yaw={:.3f} bowZoom={} weapSheath={}",
                            camStateId, cam->idleTimer, cam->yaw,
                            cam->bowZoomedIn, cam->isWeapSheathed);
                    }
                } catch (...) { LOG("    [diag camera] exception"); }

                try {
                    // --- Process + package detail ---
                    if (process) {
                        auto& pkg = process->currentPackage;
                        LOG("    pkg.cur={:08X} target={:08X} procIdx={} startT={:.2f}",
                            pkg.package ? pkg.package->GetFormID() : 0,
                            pkg.target.native_handle(),
                            pkg.currentProcedureIndex, pkg.packageStartTime);
                        LOG("    pkg.modFlags={:08X} modIntFlag={:04X} actorPkgFlags={:02X}",
                            pkg.modifiedPackageFlag, pkg.modifiedInterruptFlag,
                            pkg.actorPackageFlags.underlying());

                        // MiddleHigh (update 3D pending, killmove, furniture...)
                        if (auto* mh = process->middleHigh) {
                            LOG("    mh.update3D={:02X} alphaMult={:.2f} killMoveT={:.2f} forceNextUpdate={}",
                                mh->update3DModel.underlying(), mh->alphaMult,
                                mh->killMoveTimer, mh->forceNextUpdate);
                            LOG("    mh.cc={} weapBone={} headNode={} furnID={} occupiedFurn={:08X}",
                                mh->charController.get() ? "ok" : "null",
                                mh->weaponBone ? "ok" : "null",
                                mh->headNode ? "ok" : "null",
                                mh->currentFurnitureMarkerID,
                                mh->occupiedFurniture.native_handle());
                        }
                        // High (fade, voice, door approach)
                        if (auto* h = process->high) {
                            LOG("    hi.fadeState={} fadeTrigger={:08X} maxAlpha={:.2f} approachDoor={}",
                                static_cast<int>(h->fadeState.underlying()),
                                h->fadeTrigger ? h->fadeTrigger->GetFormID() : 0,
                                h->maxAlpha, h->approachingAutoTeleportDoor);
                            LOG("    hi.voiceState={} voiceT={:.2f} greetingPC={} talkingToPC={}",
                                static_cast<int>(h->voiceState.underlying()),
                                h->voiceTimer, h->greetingPlayer, h->talkingToPC);
                        }
                    }
                } catch (...) { LOG("    [diag process] exception"); }

                try {
                    // --- Cell detail (state, detached, flags) ---
                    if (cell) {
                        LOG("    cell.state={} detached={} flags={:04X}",
                            static_cast<int>(cell->cellState.underlying()),
                            cell->cellDetached,
                            cell->cellFlags.underlying());
                    }
                } catch (...) { LOG("    [diag cell detail] exception"); }

                try {
                    // --- Main state (freeze, onIdle, reload) ---
                    auto* main = RE::Main::GetSingleton();
                    if (main) {
                        LOG("    main.gameActive={} onIdle={} freezeTime={} freezeNext={} reloadContent={} resetGame={} quitGame={}",
                            main->gameActive, main->onIdle,
                            main->freezeTime, main->freezeNextFrame,
                            main->reloadContent, main->resetGame, main->quitGame);
                    }
                } catch (...) { LOG("    [diag main] exception"); }

                try {
                    // --- UI stack (menus en cours) ---
                    auto* ui2 = RE::UI::GetSingleton();
                    if (ui2) {
                        LOG("    ui.stack={} pauses={} itemMenus={} modal={} closingAll={} visible={}",
                            ui2->menuStack.size(),
                            ui2->numPausesGame, ui2->numItemMenus,
                            static_cast<int>(ui2->modal),
                            static_cast<int>(ui2->closingAllMenus),
                            static_cast<int>(ui2->menuSystemVisible));
                    }
                } catch (...) { LOG("    [diag ui] exception"); }

                try {
                    // --- ProcessLists (charge NPC, queues de tâches) ---
                    auto* pl = RE::ProcessLists::GetSingleton();
                    if (pl) {
                        LOG("    pl.actors high={} mh={} ml={} low={}",
                            pl->highActorHandles.size(),
                            pl->middleHighActorHandles.size(),
                            pl->middleLowActorHandles.size(),
                            pl->lowActorHandles.size());
                        LOG("    pl.runSched={} runMov={} runAnim={} magicEff={}",
                            pl->runSchedules, pl->runMovement, pl->runAnimations,
                            pl->magicEffects.size());
                    }
                } catch (...) { LOG("    [diag processlists] exception"); }

                try {
                    // --- Time / calendar (pour correler avec autres events) ---
                    auto* cal = RE::Calendar::GetSingleton();
                    if (cal) {
                        LOG("    time.hour={:.3f} days={:.3f} scale={:.1f}",
                            cal->GetHour(), cal->rawDaysPassed, cal->GetTimescale());
                    }
                } catch (...) { LOG("    [diag time] exception"); }

                LOG("=== END PRE-DISPATCH DIAG ===");
            }
        }
#endif

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
            // Lance le poll de crash diag (se arrete auto quand g_autoWalking=false ou apres 60s)
            StartAutoWalkCrashDiagPoll(targetFormID, useCoords, posX, posY, posZ);
        } else {
            LOG("AutoWalk: DispatchMethodCall failed");
            Speak(L"AutoWalk error");
        }
    });
}

// Appelle OnStopWalking sur le script Papyrus
static void StopAutoWalk() {
    g_autoWalking.store(false);

    // Arrêter le monitor et attendre qu'il se termine
    if (g_autoWalkMonitor.joinable()) {
        g_autoWalkMonitor.request_stop();
        g_autoWalkMonitor.join();
    }

    // Arreter le poll de crash diag
    if (g_autoWalkCrashDiag.joinable()) {
        g_autoWalkCrashDiag.request_stop();
        g_autoWalkCrashDiag.join();
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

        // Supprimer le temp marker C++ s'il existe (mode boussole/fallback)
        // ROLLBACK test : g_autoWalkEnableXMarkerDelete=false garde le marker en vie.
        // Les markers s'accumulent mais c'est leger et on elimine un suspect du crash.
        if (g_autoWalkEnableXMarkerDelete && g_autoWalkTempMarker != 0) {
            auto* markerForm = RE::TESForm::LookupByID(g_autoWalkTempMarker);
            auto* markerRef = markerForm ? markerForm->As<RE::TESObjectREFR>() : nullptr;
            if (markerRef) {
                markerRef->Disable();
                markerRef->SetDelete(true);
                LOG("AutoWalk: deleted C++ temp marker {:08X}", g_autoWalkTempMarker);
            }
            g_autoWalkTempMarker = 0;
        } else if (g_autoWalkTempMarker != 0) {
            LOG("AutoWalk: kept C++ temp marker {:08X} alive (rollback test)", g_autoWalkTempMarker);
            g_autoWalkTempMarker = 0;
        }

        g_autoWalkTarget.clear();
        LOG("AutoWalk: stop requested");
    });
}

// Toggle autowalk vers l'objet sélectionné dans le scanner
// Créer un XMarker temporaire à une position donnée pour l'autowalk
// Retourne le FormID du marqueur créé, ou 0 en cas d'échec
// NOTE: g_autoWalkTempMarker est déclaré plus haut (avant StopAutoWalk) pour le cleanup

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

    // Placer dans la cellule du joueur via MoveTo (attache le ref a la cellule
    // et enregistre dans la grille), puis deplacer via SetPosition qui fait un
    // update complet (contrairement a l'ecriture directe de data.location qui
    // peut ne pas notifier le systeme de navmesh).
    auto* cell = player->GetParentCell();
    if (cell) {
        marker->MoveTo(player);
        marker->SetPosition(pos);
        // Forcer le chargement du 3D : sans ca le package Travel refuse de
        // pathfinder vers la ref et le joueur reste stuck (desired speed=0).
        marker->Load3D(false);
    }

    LOG("AutoWalk: created temp marker FormID={:08X} at ({:.0f}, {:.0f}, {:.0f}) 3D={}",
        marker->GetFormID(), pos.x, pos.y, pos.z,
        marker->Is3DLoaded() ? "loaded" : "not loaded");

    return marker->GetFormID();
}

// Forward declaration — le corps est plus bas, défini après le wrapper.
static void ToggleAutoWalkImpl();

// Wrapper public appelé depuis le thread clavier. On route tout le travail de
// lancement (lecture scanner, GFx HUD, recherche de porte dans les cellules
// voisines) vers le thread principal via AddTask pour éviter les races avec le
// cell streaming et le scan automatique qui peuvent modifier g_scannedAll et
// les données de cellules en parallèle.
//
// L'arrêt d'un autowalk en cours (g_autoWalking=true) reste fait directement
// depuis le thread clavier : StopAutoWalk() utilise déjà AddTask en interne
// =============================================================================
// Routing de quete : nouvelle methode "location-based"
// =============================================================================
//
// Idee : au lieu de deviner la porte par angle de boussole (instable en donjon
// multi-cellules), on lit la quete active du joueur, sa cible (alias resolu en
// REFR), la BGSLocation cible, et on parcourt les portes de la cellule courante
// pour trouver celle qui mene vers la bonne location.
//
// Cas geres :
//   1. Cible meme cellule       -> deja gere par le code en amont (3D loaded ou
//                                  same-cell quest fallback)
//   2. Cible meme donjon (autre cellule interieure) -> porte dont la destination
//                                  est == targetLoc OU enfant de targetLoc
//   3. Cible hors du donjon courant -> porte dont la destination sort du donjon
//                                  (worldspace exterieur OU location differente)
//   4. Cible interieure depuis exterieur -> porte d'entree principale du donjon
//                                  (taggee LocTypeDungeonEntrance dans l'ESM)
//
// Retourne nullptr si la methode n'est pas applicable -> appelant retombera sur
// l'algo compass historique automatiquement.

// Resolution de la quete active du joueur et de sa REFR cible.
// On passe par BGSStoryTeller::runningQuests (offsets stables, pas via PLAYER_RUNTIME_DATA
// dont le contenu varie selon la version SE/AE/VR du runtime). Pour chaque quete active
// (cochee dans le journal du joueur), on lit ses BGSQuestObjective et on prend le
// premier en etat "kDisplayed".
//
// IMPORTANT : `qt->alias` est un aliasID (identifiant unique), PAS un index dans
// quest->aliases. On utilise donc la fonction native du moteur
// `CreateRefHandleByAliasID` qui sait resoudre l'aliasID en ObjectRefHandle, peu
// importe la position dans la liste. C'est ce que fait deja le scanner principal.
static RE::TESObjectREFR* ResolveActiveQuestTargetRef() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        LOG("LocRouting: PlayerCharacter singleton null");
        return nullptr;
    }

    // Acces aux objectifs INSTANCIES du joueur (offset 0x580 SE / 0x588 AE).
    // Memes offsets que ceux qu'utilise le scanner principal (scanner.h:997),
    // qui lui trouve correctement les quetes actives. BGSStoryTeller::runningQuests
    // est vide sur cette version du runtime — passer par PlayerCharacter est fiable.
    int totalObj = 0;
    int totalDisplayed = 0;
    int totalTried = 0;
    try {
        auto& objectives = REL::RelocateMemberIfNewer<RE::BSTArray<RE::BGSInstancedQuestObjective>>(
            SKSE::RUNTIME_SSE_1_6_629, player, 0x580, 0x588);

        for (auto& instObj : objectives) {
            totalObj++;
            if (!instObj.Objective) continue;
            if (instObj.InstanceState != RE::QUEST_OBJECTIVE_STATE::kDisplayed) continue;
            totalDisplayed++;

            auto* obj = instObj.Objective;
            auto* quest = obj->ownerQuest;
            if (!quest) continue;

            // Filtrer : ne traiter QUE les quetes vraiment cochees dans le journal
            // du joueur (la quete "trackee" qui apparait sur la boussole HUD).
            // Sans ce filtre on prend la 1ere quete avec un objectif displayed,
            // ce qui peut etre Ciceron ou n'importe quelle quete secondaire active.
            if (!quest->IsActive()) continue;

            if (obj->numTargets == 0 || !obj->targets) continue;

            // Premier target -> resoudre via fonction native (gere l'aliasID
            // unique, pas un index dans aliases).
            auto* qt = obj->targets[0];
            if (!qt) continue;
            totalTried++;

            RE::ObjectRefHandle refHandle;
            quest->CreateRefHandleByAliasID(refHandle, qt->alias);
            auto sp = refHandle.get();

            LOG("LocRouting: try quest='{}' obj.idx={} alias.id={} handle={} ref={}",
                quest->GetFullName() ? quest->GetFullName() : "?",
                obj->index, qt->alias,
                refHandle.native_handle(),
                sp ? "ok" : "null");

            if (sp) {
                LOG("LocRouting: resolved -> ref={:08X}", sp->GetFormID());
                return sp.get();
            }
        }
    } catch (...) {
        LOG("LocRouting: exception while iterating player objectives");
        return nullptr;
    }
    LOG("LocRouting: nothing resolved. totalObj={} displayed={} tried={}",
        totalObj, totalDisplayed, totalTried);
    return nullptr;
}

// Remonte la hierarchie parentLoc jusqu'a trouver une location avec un keyword
// de type "donjon" (LocTypeDungeon). Si pas trouvee, retourne la location de depart.
static RE::BGSLocation* FindDungeonRootLocation(RE::BGSLocation* loc) {
    if (!loc) return nullptr;
    static RE::BGSKeyword* dungeonKw = nullptr;
    static bool dungeonKwTried = false;
    if (!dungeonKwTried) {
        dungeonKw = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("LocTypeDungeon");
        dungeonKwTried = true;
        LOG("LocRouting: LocTypeDungeon keyword lookup -> {}", dungeonKw ? "found" : "null");
    }

    auto* cur = loc;
    int depth = 0;
    while (cur && depth < 10) {
        LOG("LocRouting: walk parentLoc[{}]='{}' hasDungeonKw={}",
            depth,
            cur->GetName() ? cur->GetName() : "?",
            (dungeonKw && cur->HasKeyword(dungeonKw)) ? 1 : 0);
        if (dungeonKw && cur->HasKeyword(dungeonKw)) return cur;
        if (!cur->parentLoc) break;
        cur = cur->parentLoc;
        depth++;
    }
    return loc;  // pas de tag dungeon trouve -> renvoyer la location originale
}

// Cherche dans specialRefs de la location la ref de l'entree principale du donjon.
// Tag standard Skyrim : "OutsideEntranceMarker" (XMarker pose juste devant la porte
// d'entree, cote exterieur). Fallback : "MapMarkerRefType" (le map marker du donjon,
// generalement co-localise avec l'entree).
static RE::TESObjectREFR* FindDungeonEntranceRef(RE::BGSLocation* root) {
    if (!root) return nullptr;

    LOG("LocRouting: scanning specialRefs of '{}' (count={})",
        root->GetName() ? root->GetName() : "?",
        root->specialRefs.size());

    RE::TESObjectREFR* outsideMarker = nullptr;
    RE::TESObjectREFR* mapMarker = nullptr;

    for (auto& sr : root->specialRefs) {
        const char* typeName = sr.type && sr.type->GetFormEditorID() ? sr.type->GetFormEditorID() : "?";
        std::string n = typeName ? typeName : "";
        bool isOutside = (n == "OutsideEntranceMarker");
        bool isMapMarker = (n == "MapMarkerRefType");

        LOG("LocRouting:   specialRef type='{}' refID={:08X} outside={} map={}",
            typeName, sr.refData.refID, isOutside ? 1 : 0, isMapMarker ? 1 : 0);

        if (isOutside && !outsideMarker) {
            auto* form = RE::TESForm::LookupByID(sr.refData.refID);
            if (form) {
                auto* ref = form->AsReference();
                if (ref) outsideMarker = ref;
            }
        }
        if (isMapMarker && !mapMarker) {
            auto* form = RE::TESForm::LookupByID(sr.refData.refID);
            if (form) {
                auto* ref = form->AsReference();
                if (ref) mapMarker = ref;
            }
        }
    }

    // Priorite : OutsideEntranceMarker (devant la porte) > MapMarkerRefType (carte)
    if (outsideMarker) return outsideMarker;
    return mapMarker;
}

// Choix de porte par hierarchie de location. Retourne nullptr si aucune porte
// pertinente trouvee ou si l'info necessaire n'est pas disponible.
static RE::TESObjectREFR* TryFindQuestDoorByLocation() {
    if (!g_useLocationRouting) {
        LOG("LocRouting: disabled by g_useLocationRouting flag");
        return nullptr;
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return nullptr;

    // 1) Resoudre la cible de la quete active
    auto* targetRef = ResolveActiveQuestTargetRef();
    if (!targetRef) {
        LOG("LocRouting: no active quest target ref -> fallback compass");
        return nullptr;
    }

    // 2) Determiner la location cible
    auto* targetLoc = targetRef->GetCurrentLocation();
    if (!targetLoc) {
        // GetEditorLocation : version ESM-time, dispo meme si ref pas chargee
        targetLoc = targetRef->GetEditorLocation();
    }
    auto* targetCell = targetRef->GetParentCell();
    bool targetIsInterior = targetCell ? targetCell->IsInteriorCell() : false;

    // 3) Determiner ou est le joueur
    auto* playerCell = player->GetParentCell();
    if (!playerCell) {
        LOG("LocRouting: player has no parent cell -> fallback");
        return nullptr;
    }
    bool playerInInterior = playerCell->IsInteriorCell();
    auto* playerLoc = player->GetCurrentLocation();

    // Short-circuit : si la cible est DANS LA MEME CELLULE que le joueur, le routing
    // par porte n'a aucun sens — le moteur peut pathfinder directement vers elle
    // dans la piece courante. Cas typique : Yarle dans la salle du tr�ne de Fort-Dragon,
    // joueur aussi dans la salle du tr�ne. Notre code tentait sinon de router vers
    // une porte (Quartiers du jarl) parce que plusieurs sous-cellules de Fort-Dragon
    // partagent la meme location parente — mauvaise porte choisie.
    if (targetCell && targetCell == playerCell) {
        LOG("LocRouting: target in same cell as player ('{}'), skipping routing -> FormID direct",
            targetCell->GetName() ? targetCell->GetName() : "?");
        return nullptr;  // caller retombera sur Cas 1 (FormID direct vers le ref)
    }

    // Bug Skyrim : juste apres une transition de cellule, playerCell->IsInteriorCell()
    // peut retourner false alors qu'on est deja a l'interieur du donjon. Pour fiabiliser,
    // on regarde aussi si le joueur est dans la meme location que la cible : si oui, on
    // est forcement dans le donjon, peu importe ce que dit IsInteriorCell. Sans ce
    // check, on tomberait dans le mode "ext->dungeon" et l'autowalk pointerait vers
    // la sortie au lieu de continuer la progression.
    bool playerInsideTargetDungeon = false;
    if (targetLoc && playerLoc) {
        playerInsideTargetDungeon = (targetLoc == playerLoc) ||
                                    targetLoc->IsChild(playerLoc) ||
                                    playerLoc->IsChild(targetLoc);
    }
    bool effectivelyInInterior = playerInInterior || playerInsideTargetDungeon;

    LOG("LocRouting: targetRef={:08X} targetLoc='{}' targetInt={} playerLoc='{}' playerInt={} sameDungeon={} -> mode={}",
        targetRef->GetFormID(),
        targetLoc && targetLoc->GetName() ? targetLoc->GetName() : "?",
        targetIsInterior ? 1 : 0,
        playerLoc && playerLoc->GetName() ? playerLoc->GetName() : "?",
        playerInInterior ? 1 : 0,
        playerInsideTargetDungeon ? 1 : 0,
        effectivelyInInterior ? "interior" : "exterior");

    // === CAS 4 : Joueur exterieur, cible interieure -> chercher entree principale ===
    if (!effectivelyInInterior && targetIsInterior) {
        auto* root = FindDungeonRootLocation(targetLoc);
        if (root) {
            auto* entrance = FindDungeonEntranceRef(root);
            if (entrance) {
                LOG("LocRouting: ext->dungeon entrance found '{}' FormID={:08X} via location '{}'",
                    entrance->GetDisplayFullName() ? entrance->GetDisplayFullName() : "?",
                    entrance->GetFormID(),
                    root->GetName() ? root->GetName() : "?");
                return entrance;
            }
        }
        LOG("LocRouting: ext->dungeon no LocTypeDungeonEntrance found -> fallback compass");
        return nullptr;
    }

    // === CAS 2 et 3 : Joueur en interieur (ou dans la meme location que la cible) ===
    if (effectivelyInInterior) {
        // Determiner si on doit "sortir" du donjon ou "progresser" dedans
        bool needToExit = false;
        if (!targetIsInterior) {
            needToExit = true;
        } else if (targetLoc && playerLoc) {
            // Cible interieure mais autre donjon -> sortir d'abord
            bool sameDungeon = (targetLoc == playerLoc) ||
                               targetLoc->IsChild(playerLoc) ||
                               playerLoc->IsChild(targetLoc);
            if (!sameDungeon) needToExit = true;
        }
        LOG("LocRouting: interior mode, needToExit={}", needToExit ? 1 : 0);

        // Parcourir les portes de la cellule du joueur
        struct DoorCand {
            RE::TESObjectREFR* door;
            int score;
            float dist;
        };
        std::vector<DoorCand> cands;
        auto playerPos = player->GetPosition();

        for (auto& refHandle : playerCell->GetRuntimeData().references) {
            auto refPtr = refHandle.get();
            if (!refPtr) continue;
            auto* base = refPtr->GetBaseObject();
            if (!base || base->GetFormType() != RE::FormType::Door) continue;
            auto* et = refPtr->extraList.GetByType<RE::ExtraTeleport>();
            if (!et || !et->teleportData) {
                LOG("LocRouting: door '{}' FormID={:08X} REJECTED (no ExtraTeleport)",
                    refPtr->GetDisplayFullName() ? refPtr->GetDisplayFullName() : "?",
                    refPtr->GetFormID());
                continue;
            }
            auto destDoor = et->teleportData->linkedDoor.get();
            if (!destDoor) {
                LOG("LocRouting: door '{}' FormID={:08X} REJECTED (no linkedDoor)",
                    refPtr->GetDisplayFullName() ? refPtr->GetDisplayFullName() : "?",
                    refPtr->GetFormID());
                continue;
            }
            // destCell peut etre null si la cellule destination n'est pas chargee
            // en memoire (cas frequent pour les portes vers exterieur quand on est
            // en interieur). Dans ce cas on utilise GetWorldspace() qui retourne
            // le worldspace meme sans cellule chargee.
            auto* destCell = destDoor->GetParentCell();
            auto* destWorld = destDoor->GetWorldspace();
            bool destIsInterior = destCell ? destCell->IsInteriorCell() : (destWorld == nullptr);
            // Si la porte mene vers un worldspace exterieur ET la cellule du joueur
            // est interieure : c'est forcement une sortie vers exterieur, meme si
            // destCell est null parce que pas chargee.
            if (!destCell && destWorld) {
                destIsInterior = false;
            }
            auto* destLoc = destCell ? destCell->GetLocation() : nullptr;

            int score = 0;
            if (needToExit) {
                // Cas idéal : la porte mène vraiment vers l'exterieur (worldspace).
                // C'est le SEUL cas qui merite un score eleve, parce qu'on veut
                // sortir du complexe interieur pour aller a un objectif exterieur.
                if (!destIsInterior) {
                    score += 100;
                } else {
                    // Toutes les portes vers une autre cellule interieure (autre
                    // salle du donjon, prison, quartiers du jarl...) ne sortent
                    // PAS vraiment. On leur donne un petit score pour qu'elles
                    // restent candidates en dernier recours, mais elles seront
                    // rejetees si une vraie sortie exterieure existe ou si le
                    // seuil 50 est atteint. Si aucune ne sort, on tombera sur
                    // le fallback compass plus intelligent.
                    score += 10;
                }
            } else {
                // On veut PROGRESSER vers targetLoc (rester dans le donjon).
                // PIEGE : une porte qui sort vers l'exterieur (destInt=0) peut avoir
                // un destLoc qui matche playerLoc/targetLoc parce que le marker
                // exterieur est rattache a la meme location dans l'ESM. Mais c'est
                // une porte de SORTIE, pas de progression. On l'exclut donc du
                // scoring eleve : score 1 (en-dessous du seuil 50, donc rejet).
                if (!destIsInterior) {
                    score += 1;
                }
                else if (destLoc == targetLoc) score += 100;
                else if (targetLoc && destLoc && targetLoc->IsChild(destLoc)) score += 70;
                else if (targetLoc && destLoc && destLoc->IsChild(targetLoc)) score += 80;
                else score += 5;
            }

            float dx = refPtr->GetPositionX() - playerPos.x;
            float dy = refPtr->GetPositionY() - playerPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            const char* destCellName = destCell && destCell->GetName() ? destCell->GetName() :
                                       (destWorld && destWorld->GetName() ? destWorld->GetName() : "?");
            LOG("LocRouting: door '{}' FormID={:08X} -> '{}' destInt={} destLoc='{}' score={} dist={:.0f}",
                refPtr->GetDisplayFullName() ? refPtr->GetDisplayFullName() : "?",
                refPtr->GetFormID(),
                destCellName,
                destIsInterior ? 1 : 0,
                destLoc && destLoc->GetName() ? destLoc->GetName() : "?",
                score, dist);

            DoorCand c{};
            c.door = refPtr;
            c.score = score;
            c.dist = dist;
            cands.push_back(c);
        }

        if (cands.empty()) {
            LOG("LocRouting: no doors in player cell -> fallback compass");
            return nullptr;
        }

        // Trier par score decroissant, distance croissante en tie-breaker
        std::sort(cands.begin(), cands.end(), [](const DoorCand& a, const DoorCand& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.dist < b.dist;
        });

        auto& best = cands.front();
        // Refuser si meilleur score est trop faible (aucune porte vraiment pertinente)
        if (best.score < 50) {
            LOG("LocRouting: best door score={} too low (no clear winner) -> fallback compass", best.score);
            return nullptr;
        }

        LOG("LocRouting: chose door '{}' FormID={:08X} score={} dist={:.0f}",
            best.door->GetDisplayFullName() ? best.door->GetDisplayFullName() : "?",
            best.door->GetFormID(), best.score, best.dist);
        return best.door;
    }

    // Cas non gere (ex: ext->ext, devrait etre traite ailleurs)
    LOG("LocRouting: no rule matches -> fallback compass");
    return nullptr;
}

// =============================================================================

// pour son cleanup, donc safe.
static void ToggleAutoWalk() {
    LOG("InputDiag: ToggleAutoWalk ENTRY, g_autoWalking={}", g_autoWalking.load());
    if (g_autoWalking.load()) {
        Speak(L"Stopping");
        StopAutoWalk();
        return;
    }

    auto* task = SKSE::GetTaskInterface();
    if (!task) { ToggleAutoWalkImpl(); return; }
    task->AddTask([]() { ToggleAutoWalkImpl(); });
}

static void ToggleAutoWalkImpl() {
    LOG("AutoWalk: ToggleAutoWalk called");

    // Garde anti-crash : refuser si on est dans la fenêtre d'instabilité
    // post-load / post-cell-change (skeleton encore en train de s'initialiser).
    {
        int64_t now = AutoWalkNowMs();
        int64_t until = g_autoWalkUnsafeUntilMs.load();
        if (now < until) {
            int64_t waitMs = until - now;
            LOG("AutoWalk: blocked by safety cooldown ({}ms remaining)", waitMs);
            Speak(L"Please wait, game still loading");
            return;
        }
    }

    // Double-check : aussi refuser si un LoadingMenu ou Fader Menu est actif.
    // Ces menus sont présents pendant les transitions (fast travel, cell change,
    // load). Comme le cooldown, ça protège contre le crash skeleton/shader.
    {
        auto* ui = RE::UI::GetSingleton();
        if (ui) {
            if (ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
                ui->IsMenuOpen("Fader Menu")) {
                LOG("AutoWalk: blocked because LoadingMenu or Fader Menu is open");
                Speak(L"Please wait, game still loading");
                return;
            }
        }
    }

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
            // Utiliser la position VISUELLE (mesh rendu) plutôt que la position
            // physique (hitbox Havok). Pour un objet jeté qui rebondit / glisse,
            // la hitbox physique continue de bouger pendant plusieurs secondes
            // après l'impact au sol, ce qui faisait dériver la cible d'autowalk
            // de jusqu'à 2-3 mètres. La position du mesh rendu correspond à ce
            // que le joueur "voit" visuellement (ou audible via mods 3D audio).
            // Fallback sur GetPosition() si le mesh n'est pas dispo (cas rare).
            RE::NiPoint3 pos;
            auto* node = ref->Get3D();
            if (node) {
                pos = node->world.translate;
            } else {
                pos = ref->GetPosition();
            }
            LOG("AutoWalk: dynamic FormID {:08X} is loaded in 3D at ({:.0f},{:.0f},{:.0f}), using coords mode (source={})",
                targetID, pos.x, pos.y, pos.z, node ? "mesh" : "physics");
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

        // === NOUVEAU : routing par hierarchie de location (plus fiable que compass) ===
        // Avant de tomber sur l'algo compass historique, essayer de resoudre la cible
        // de quete via BGSStoryTeller -> objectifs -> alias -> location -> hierarchie.
        // Si ca trouve la bonne porte, on l'utilise direct. Sinon fallback compass.
        {
            auto* locDoor = TryFindQuestDoorByLocation();
            if (locDoor) {
                targetID = locDoor->GetFormID();
                LOG("AutoWalk: routed via location, target door FormID={:08X} '{}'",
                    targetID,
                    locDoor->GetDisplayFullName() ? locDoor->GetDisplayFullName() : "?");
                Speak(L"Walking to " + targetName);
                StartAutoWalk(targetID, 150.0f, 0.0f, 0.0f, 0.0f);
                return;
            }
            LOG("AutoWalk: location-based routing returned nullptr, falling back to compass");
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

                        // PASSE 1 : chercher EN PRIORITE un marker questDoor.
                        // Skyrim pose ce type de marker directement sur la porte qu'il
                        // faut traverser pour suivre la prochaine etape de la quete
                        // (calcul interne via les waypoints invisibles du moteur).
                        // C'est notre "GPS" le plus fiable : il pointe sur une porte
                        // precise, pas vers la cible finale a travers les murs.
                        // Stride = 4 : heading, alpha, type, scale
                        float questHeadingFallback = -1.0f;
                        for (uint32_t i = 0; i + 3 < arrSize; i += 4) {
                            RE::GFxValue headingVal, typeVal;
                            dataArr.GetElement(i, &headingVal);
                            dataArr.GetElement(i + 2, &typeVal);

                            if (!SafeIsNumber(typeVal)) continue;
                            float type = static_cast<float>(SafeGetNumber(typeVal));

                            if (type == questDoorType && SafeIsNumber(headingVal)) {
                                compassHeading = static_cast<float>(SafeGetNumber(headingVal));
                                LOG("AutoWalk: found compass questDoor marker heading={:.1f} (priority)", compassHeading);
                                break;
                            }
                            if (type == questType && SafeIsNumber(headingVal) && questHeadingFallback < 0) {
                                questHeadingFallback = static_cast<float>(SafeGetNumber(headingVal));
                            }
                        }

                        // PASSE 2 : aucun questDoor trouve (cible deja dans la meme
                        // cellule, ou quete sans etape intermediaire) -> fallback
                        // sur le marker quest classique (cible finale a travers murs).
                        if (compassHeading < 0 && questHeadingFallback >= 0) {
                            compassHeading = questHeadingFallback;
                            LOG("AutoWalk: no questDoor marker, falling back to quest marker heading={:.1f}", compassHeading);
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
            // Position visuelle (mesh) en priorité, fallback physique. Même
            // justification que plus haut : évite la dérive Havok des objets
            // jetés qui rebondissent encore.
            RE::NiPoint3 pos;
            auto* node = ref->Get3D();
            if (node) {
                pos = node->world.translate;
            } else {
                pos = ref->GetPosition();
            }
            StartAutoWalk(targetID, 100.0f, pos.x, pos.y, pos.z);
        } else if (g_scanIndex >= 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
            auto& obj = *g_scannedFiltered[g_scanIndex];
            StartAutoWalk(targetID, 100.0f, obj.lastKnownPos.x, obj.lastKnownPos.y, obj.lastKnownPos.z);
        } else {
            StartAutoWalk(targetID, 100.0f, 0.f, 0.f, 0.f);  // fallback
        }
    } else {
        // FormID normal (non-FF). Stratégie :
        //
        // Cas 1: cell target CONNUE (ref->GetParentCell() != null)
        //   → FormID brut. Le moteur Skyrim sait naviguer vers une cible interior
        //   via Travel package (pathfind vers porte d'entrée du bâtiment).
        //   Ex: Jarl à Fort-Dragon depuis Whiterun → engine pathfind vers la porte.
        //
        // Cas 2: cell target NULL, joueur en INTERIOR
        //   → FormID brut. Le joueur est dans un donjon et la cible est dans une
        //   autre worldspace. Coord mode ne marche pas (XMarker à coords exterior
        //   dans cellule interior = absurde). Le moteur doit naviguer via exits.
        //   Ex: "Échappez-vous d'Helgen" → target en Tamriel exterior, joueur en
        //   interior → engine pathfind vers la sortie du donjon via Travel package.
        //
        // Cas 3: cell target NULL, joueur en EXTERIOR
        //   → coord mode avec ref->GetPosition(). Le dispatch FormID brut sur un ref
        //   exterior non chargé cause le crash BSShaderAccumulator (cas Alvor).
        //   Ici le XMarker aux coords exterior est valide (même worldspace que joueur).
        auto* finalForm = RE::TESForm::LookupByID(targetID);
        auto* finalRef = finalForm ? finalForm->AsReference() : nullptr;
        auto* finalCell = finalRef ? finalRef->GetParentCell() : nullptr;
        auto* playerForCell = RE::PlayerCharacter::GetSingleton();
        auto* playerCell = playerForCell ? playerForCell->GetParentCell() : nullptr;
        bool playerInInterior = playerCell && playerCell->IsInteriorCell();

        // === NOUVEAU : routing par hierarchie de location pour les cibles de quete
        // Si la cible courante est un objectif de quete (Irileth, Cicero, etc.) et
        // qu'on a une porte de routing identifiee, on l'utilise comme cible
        // intermediaire au lieu de tomber dans le coord mode du cas 3 qui bug
        // sur les PNJ dont la cellule n'est pas chargee.
        // Coherent avec le systeme location-based deja utilise pour les FF*.
        bool currentIsQuest = false;
        if (g_scanIndex >= 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
            currentIsQuest = (g_scannedFiltered[g_scanIndex]->category == kCatQuests);
        }
        if (currentIsQuest) {
            auto* locDoor = TryFindQuestDoorByLocation();
            if (locDoor) {
                targetID = locDoor->GetFormID();
                LOG("AutoWalk: non-FF quest target -> routed via location, target door FormID={:08X} '{}'",
                    targetID,
                    locDoor->GetDisplayFullName() ? locDoor->GetDisplayFullName() : "?");
                StartAutoWalk(targetID, 100.0f, 0.f, 0.f, 0.f);
                return;
            }
            LOG("AutoWalk: non-FF quest, location routing returned nullptr, falling back to regular non-FF logic");
        }

        if (finalRef && finalCell) {
            // Cas 1 : cell connue → FormID brut, le moteur gère la nav interior→exterior
            LOG("AutoWalk: non-FF ref, cell='{}' known → FormID mode (engine handles nav)",
                finalCell->GetName() ? finalCell->GetName() : "?");
            StartAutoWalk(targetID, 100.0f, 0.f, 0.f, 0.f);
        } else if (finalRef && playerInInterior) {
            // Cas 2 : cell cible null + joueur en interior → FormID brut (engine via exits)
            //
            // GARDE ANTI-CRASH : si la cible est extremement lointaine (> 30000u)
            // ET n'a pas son 3D charge, le dispatch FormID mode peut causer un
            // crash BSShaderAccumulator (cas vu sur "Visit the College of
            // Winterhold" depuis Arcadia's Cauldron a 153000u — crash dump :
            // acces a rax+0xF4 avec rax=0, skeleton.nif + BSShaderAccumulator).
            // On bascule alors en coord mode sur la position cible, qui est sur
            // (meme principe que le Cas 3 qui marche deja en exterieur).
            auto* playerForDist = RE::PlayerCharacter::GetSingleton();
            auto tpos = finalRef->GetPosition();
            float tdist = 0;
            if (playerForDist) {
                auto pp = playerForDist->GetPosition();
                float dx = tpos.x - pp.x, dy = tpos.y - pp.y;
                tdist = std::sqrt(dx * dx + dy * dy);
            }
            bool crashRisk = !finalRef->Is3DLoaded() && tdist > 30000.0f;
            if (crashRisk) {
                LOG("AutoWalk: non-FF ref cell=NULL, player in interior BUT target too far ({:.0f}u) and 3D not loaded -> coord mode (anti-crash)",
                    tdist);
                StartAutoWalk(targetID, 100.0f, tpos.x, tpos.y, tpos.z);
                return;
            }
            LOG("AutoWalk: non-FF ref cell=NULL, player in interior → FormID mode (engine navigates via exits)");
            StartAutoWalk(targetID, 100.0f, 0.f, 0.f, 0.f);
        } else if (finalRef) {
            // Cas 3 : cell cible null + joueur exterior.
            //
            // Probleme : coord mode vers finalRef->GetPosition() ne marche pas
            // pour les PNJ distants (Irileth en voyage de quete, Cicero avec sa
            // caravane) -- le moteur ne peut pas pathfinder sur 14000+ unites en
            // coord mode sans navmesh intermediaire charge.
            //
            // Solution hierarchique (par ordre de preference) :
            //   3a. PNJ proche (< 8000u) et meme worldspace racine -> coord direct.
            //   3b. Lire le heading du marker compass quete/questDoor du HUD
            //       (tjrs a jour, c'est ce que Skyrim utilise pour guider le
            //       joueur voyant) et placer un XMarker temporaire a 6000u dans
            //       cette direction. Navigation "par bonds" : joueur arrive,
            //       re-toggle, nouveau bond. Garanti dans le navmesh charge.
            //   3c. Fallback NAM0 (horseLocMarker, plus fiable que MNAM) de la
            //       location cible, avec check IsDisabled + distance max.
            //   3d. Dernier recours : coord direct (comportement historique).
            //
            // ATTENTION : worldLocMarker (MNAM) est BUGUE dans beaucoup de LCTN
            // vanilla (ex: WhiterunWatchtowerLocation pointe sur FortGreymoor
            // a 20000u de la). On ne l'utilise QU'apres validation stricte.

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                StartAutoWalk(targetID, 100.0f, 0.f, 0.f, 0.f);
                return;
            }
            auto playerPos = player->GetPosition();
            auto* playerWS = player->GetWorldspace();
            auto targetPos = finalRef->GetPosition();

            auto dist2D = [](const RE::NiPoint3& a, const RE::NiPoint3& b) {
                float dx = a.x - b.x, dy = a.y - b.y;
                return std::sqrt(dx * dx + dy * dy);
            };

            auto worldspacesCompatible = [](RE::TESWorldSpace* a, RE::TESWorldSpace* b) -> bool {
                if (!a || !b) return true;
                if (a == b) return true;
                auto* ra = a;
                while (ra->parentWorld) ra = ra->parentWorld;
                auto* rb = b;
                while (rb->parentWorld) rb = rb->parentWorld;
                return ra == rb;
            };

            constexpr float kNearCoordRadius = 8000.0f;
            constexpr float kHopDistance = 6000.0f;
            constexpr float kMaxMarkerDist = 30000.0f;

            // ============ 3.0 Joueur dans un sous-worldspace walled (ville) ============
            // Blancherive, Solitude, Vendeaume, etc. sont des worldspaces separes
            // du Tamriel racine. Leur navmesh s'arrete aux murs. Si la cible est
            // hors de ce sous-worldspace, il faut d'abord sortir par la porte
            // principale. On cherche une porte dans TOUTES les cellules attachees
            // de la grille + persistentCell + cellule du joueur, dont la destination
            // est dans un worldspace different du notre.
            auto* playerCellNow = player->GetParentCell();
            auto* targetWS = finalRef->GetWorldspace();
            if (playerWS && playerWS->parentWorld != nullptr &&
                (!targetWS || !worldspacesCompatible(playerWS, targetWS) || targetWS != playerWS)) {
                RE::TESObjectREFR* exitDoor = nullptr;
                float bestExitDist = 999999.0f;

                auto scanCellForExit = [&](RE::TESObjectCELL* c) {
                    if (!c) return;
                    for (auto& refHandle : c->GetRuntimeData().references) {
                        auto refPtr = refHandle.get();
                        if (!refPtr) continue;
                        auto* base = refPtr->GetBaseObject();
                        if (!base || base->GetFormType() != RE::FormType::Door) continue;
                        auto* et = refPtr->extraList.GetByType<RE::ExtraTeleport>();
                        if (!et || !et->teleportData) continue;
                        auto destDoor = et->teleportData->linkedDoor.get();
                        if (!destDoor) continue;
                        auto* destWS = destDoor->GetWorldspace();
                        if (!destWS || destWS == playerWS) continue;  // on veut sortir
                        float dx = refPtr->GetPositionX() - playerPos.x;
                        float dy = refPtr->GetPositionY() - playerPos.y;
                        float d = std::sqrt(dx * dx + dy * dy);
                        if (d < bestExitDist) {
                            bestExitDist = d;
                            exitDoor = refPtr;
                        }
                    }
                };

                // 1) Cellule courante du joueur
                scanCellForExit(playerCellNow);
                // 2) Persistent cell du worldspace (markers, portes globales)
                scanCellForExit(playerWS->persistentCell);
                // 3) Toutes les cellules attachees de la grille active
                auto* tes = RE::TES::GetSingleton();
                if (tes && tes->gridCells) {
                    for (uint32_t gx = 0; gx < tes->gridCells->length; gx++) {
                        for (uint32_t gy = 0; gy < tes->gridCells->length; gy++) {
                            auto* c = tes->gridCells->GetCell(gx, gy);
                            if (c && c->IsAttached() && c != playerCellNow) scanCellForExit(c);
                        }
                    }
                }

                if (exitDoor) {
                    LOG("AutoWalk case 3.0: walled city exit found, door FormID={:08X} '{}' dist={:.0f}",
                        exitDoor->GetFormID(),
                        exitDoor->GetDisplayFullName() ? exitDoor->GetDisplayFullName() : "?",
                        bestExitDist);
                    StartAutoWalk(exitDoor->GetFormID(), 100.0f, 0.f, 0.f, 0.f);
                    return;
                }
                LOG("AutoWalk case 3.0: player in walled worldspace '{}' but no exit door found after scanning grid+persistent",
                    playerWS->GetName() ? playerWS->GetName() : "?");
            }

            // ============ 3a. Cible proche -> coord direct ============
            bool targetPosPlausible =
                (targetPos.x != 0 || targetPos.y != 0) &&
                worldspacesCompatible(playerWS, finalRef->GetWorldspace());
            if (targetPosPlausible && dist2D(playerPos, targetPos) <= kNearCoordRadius) {
                LOG("AutoWalk case 3a: target close ({:.0f}u) -> coord mode pos=({:.0f},{:.0f},{:.0f})",
                    dist2D(playerPos, targetPos), targetPos.x, targetPos.y, targetPos.z);
                StartAutoWalk(targetID, 100.0f, targetPos.x, targetPos.y, targetPos.z);
                return;
            }

            // ============ 3b. Compass heading + XMarker par bonds ============
            float compassHeading = -1.0f;
            auto* ui = RE::UI::GetSingleton();
            if (ui) {
                auto hudMenu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
                if (hudMenu && hudMenu->uiMovie) {
                    RE::GFxValue hudRoot;
                    if (hudMenu->uiMovie->GetVariable(&hudRoot, "_root.HUDMovieBaseInstance") && SafeIsObject(hudRoot)) {
                        RE::GFxValue dataArr;
                        if (hudRoot.GetMember("CompassTargetDataA", &dataArr) && SafeIsArray(dataArr)) {
                            RE::GFxValue qtVal, qdVal;
                            float qt = -1, qd = -1;
                            if (hudRoot.GetMember("CompassMarkerQuest", &qtVal) && SafeIsNumber(qtVal))
                                qt = static_cast<float>(SafeGetNumber(qtVal));
                            if (hudRoot.GetMember("CompassMarkerQuestDoor", &qdVal) && SafeIsNumber(qdVal))
                                qd = static_cast<float>(SafeGetNumber(qdVal));

                            uint32_t arrSize = SafeGetArraySize(dataArr);
                            float questFallback = -1.0f;
                            for (uint32_t i = 0; i + 3 < arrSize; i += 4) {
                                RE::GFxValue hVal, tVal;
                                dataArr.GetElement(i, &hVal);
                                dataArr.GetElement(i + 2, &tVal);
                                if (!SafeIsNumber(tVal) || !SafeIsNumber(hVal)) continue;
                                float tp = static_cast<float>(SafeGetNumber(tVal));
                                float h  = static_cast<float>(SafeGetNumber(hVal));
                                if (tp == qd) { compassHeading = h; break; }
                                if (tp == qt && questFallback < 0) questFallback = h;
                            }
                            if (compassHeading < 0 && questFallback >= 0) {
                                compassHeading = questFallback;
                            }
                        }
                    }
                }
            }

            if (compassHeading >= 0) {
                float rad = compassHeading * 3.14159265f / 180.0f;
                RE::NiPoint3 hopPos;
                hopPos.x = playerPos.x + std::sin(rad) * kHopDistance;
                hopPos.y = playerPos.y + std::cos(rad) * kHopDistance;
                hopPos.z = playerPos.z;

                RE::FormID markerID = CreateTempMarkerAt(hopPos);
                if (markerID != 0) {
                    g_autoWalkTempMarker = markerID;
                    LOG("AutoWalk case 3b: compass hop marker FormID={:08X} heading={:.1f} deg at ({:.0f},{:.0f},{:.0f})",
                        markerID, compassHeading, hopPos.x, hopPos.y, hopPos.z);
                    // Dispatch en coord mode (pos x/y/z != 0) pour que le package
                    // Papyrus utilise SetPosition du marker plutot que ForceRefTo,
                    // ce qui contourne le probleme du Travel package qui echoue
                    // silencieusement sur un XMarker runtime FF*.
                    StartAutoWalk(markerID, 150.0f, hopPos.x, hopPos.y, hopPos.z);
                    return;
                }
                LOG("AutoWalk case 3b: compass heading={:.1f} deg but CreateTempMarkerAt failed", compassHeading);
            } else {
                LOG("AutoWalk case 3b: no compass quest marker available");
            }

            // ============ 3c. Fallback NAM0 / MNAM validated ============
            auto validateMarker = [&](RE::TESObjectREFR* m) -> bool {
                if (!m) return false;
                if (m->IsDisabled() || m->IsDeleted() || m->IsMarkedForDeletion()) return false;
                if (!worldspacesCompatible(playerWS, m->GetWorldspace())) return false;
                return dist2D(playerPos, m->GetPosition()) <= kMaxMarkerDist;
            };

            RE::TESObjectREFR* routeMarker = nullptr;
            const char* routeReason = "?";
            auto* loc = finalRef->GetCurrentLocation();
            if (!loc) loc = finalRef->GetEditorLocation();
            for (int depth = 0; loc && depth < 5 && !routeMarker; ++depth) {
                auto hlm = loc->horseLocMarker.get();
                auto wlm = loc->worldLocMarker.get();
                auto* hRaw = hlm.get();
                auto* wRaw = wlm.get();
                LOG("AutoWalk routing: loc[{}]='{}' NAM0={} MNAM={} parent={}",
                    depth,
                    loc->GetName() ? loc->GetName() : "?",
                    hRaw ? (hRaw->IsDisabled() ? "disabled" : hRaw->IsDeleted() ? "deleted" : "ok") : "null",
                    wRaw ? (wRaw->IsDisabled() ? "disabled" : wRaw->IsDeleted() ? "deleted" : "ok") : "null",
                    loc->parentLoc && loc->parentLoc->GetName() ? loc->parentLoc->GetName() : "null");

                if (validateMarker(hRaw)) { routeMarker = hRaw; routeReason = "NAM0"; break; }
                if (validateMarker(wRaw)) { routeMarker = wRaw; routeReason = "MNAM"; break; }
                loc = loc->parentLoc;
            }

            if (routeMarker) {
                auto mp = routeMarker->GetPosition();
                LOG("AutoWalk case 3c: fallback {} FormID={:08X} pos=({:.0f},{:.0f},{:.0f}) dist={:.0f}",
                    routeReason, routeMarker->GetFormID(), mp.x, mp.y, mp.z, dist2D(playerPos, mp));
                StartAutoWalk(routeMarker->GetFormID(), 100.0f, 0.f, 0.f, 0.f);
                return;
            }

            // ============ 3d. Dernier recours : coord direct ============
            LOG("AutoWalk case 3d: no compass/NAM0/MNAM usable -> last-resort coord mode pos=({:.0f},{:.0f},{:.0f})",
                targetPos.x, targetPos.y, targetPos.z);
            StartAutoWalk(targetID, 100.0f, targetPos.x, targetPos.y, targetPos.z);
        } else {
            // Pas de ref du tout : dispatch brut (peut crasher mais on n'a rien d'autre)
            StartAutoWalk(targetID, 100.0f, 0.f, 0.f, 0.f);
        }
    }
}

// AUTOWALK — FIN
