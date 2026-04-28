#pragma once

// AUTOWALK — Pont C++ vers Papyrus pour la marche automatique

// Helper pour trouver la quête AutoWalk — cherche par EditorID d'abord (AE natif + po3_Tweaks),
// puis fallback par plugin name + local FormID (compatible SE 1.5.97 sans po3_Tweaks).
static RE::TESQuest* FindAutoWalkQuest() {
    // Log "found via X" une seule fois par session pour ne pas flooder a chaque
    // autowalk/stop. En cas d'echec, on re-tente et on re-log (utile si le mod
    // a ete charge tardivement).
    static bool s_loggedSuccess = false;

    // Tentative 1 : EditorID (fonctionne sur AE natif ou SE avec po3_Tweaks Load EditorIDs)
    auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SkyrimTTS_AutoWalkQuest");
    if (quest) {
        if (!s_loggedSuccess) { LOG("AutoWalk: quest found via EditorID"); s_loggedSuccess = true; }
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
        if (!s_loggedSuccess) { LOG("AutoWalk: quest found via LookupForm (FormID 0x800)"); s_loggedSuccess = true; }
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
                if (!s_loggedSuccess) { LOG("AutoWalk: quest found via resolved FormID!"); s_loggedSuccess = true; }
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
                    if (!s_loggedSuccess) { LOG("AutoWalk: quest found via light FormID!"); s_loggedSuccess = true; }
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

// Timestamp du dernier dispatch (ms). Utilise par l'anti-rebond 500ms dans
// StartAutoWalk pour eviter les rafales de dispatches rapproches.
static std::atomic<int64_t> g_autoWalkLastDispatchMs{0};

// Cooldown de securite : empeche de lancer l'autowalk pendant la fenetre
// fragile apres un load ou un changement de cellule. On garde ce cooldown
// meme apres le refactor f4access-style (zero mutation C++) car le dispatch
// Papyrus fait quand meme EvaluatePackage cote script, qui peut encore
// poser probleme si le skeleton/shader est en cours de reconstruction.
// Stocke le timestamp (ms depuis epoch) jusqu'auquel l'autowalk est bloque.
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

// Forward declaration : definie plus bas apres StopAutoWalk.
// Permet a AutoWalkInputUpdate d'etre defini avant DispatchPapyrusStop.
static void DispatchPapyrusStop();

// =============================================================================
// Annule l'autowalk si le joueur fait un input de mouvement reel.
// Appele depuis InputListener::ProcessEvent (plugin.cpp) — style f4access :
// pas de polling thread separe, on reagit a chaque event d'input reel.
//
// Regle importante : si le joueur est en train d'utiliser un combo modifieur
// (LB maintenu au gamepad, Ctrl ou Shift maintenu au clavier), AUCUN cancel
// n'est declenche. Raison : ces combos servent a naviguer dans le scanner
// pendant l'autowalk (LB+fleches pour cycler, Ctrl+X pour lock, etc.) et
// il ne faut pas que ca arrete la marche.
//
// Cancel declenche par :
//   Clavier (sans Ctrl/Shift) : WASD (mouvement reel), Space (saut), Escape (menu)
//   Gamepad (sans LB) : stick gauche bouge (seuil 0.3) — vrai mouvement joueur
//
// Le bouton A qui lance l'autowalk (LB+A) n'annule PAS parce que LB est
// maintenu a ce moment-la. Pareil pour les D-pad et fleches en combos.
//
// Retourne true si on a annule l'autowalk, false sinon.
// =============================================================================
static bool AutoWalkInputUpdate(RE::InputEvent* const* a_event) {
    if (!g_autoWalking.load()) return false;
    if (!a_event || !*a_event) return false;

    // Si un modifieur est maintenu, le joueur utilise un combo (scanner,
    // lock enemy, annonce distance, etc.) — ne rien cancel.
    if (g_lbHeld.load()) return false;
    const bool ctrlHeld  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shiftHeld = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
    if (ctrlHeld || shiftHeld) return false;

    bool shouldCancel = false;

    for (auto e = *a_event; e && !shouldCancel; e = e->next) {
        auto type = e->GetEventType();

        if (type == RE::INPUT_EVENT_TYPE::kButton) {
            auto* btn = e->AsButtonEvent();
            if (!btn || !btn->IsPressed()) continue;
            const auto code = btn->GetIDCode();

            if (btn->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                using K = RE::BSKeyboardDevice::Keys;
                // Seulement les vraies touches de mouvement / sortie de jeu.
                // Pas les fleches (utilisees dans des combos Ctrl+fleche) ni les
                // touches de fonction du scanner (Home, End, PageUp/Down).
                if (code == K::kW || code == K::kA || code == K::kS || code == K::kD ||
                    code == K::kSpacebar || code == K::kEscape) {
                    shouldCancel = true;
                }
            }
            // Gamepad : plus aucun bouton ne cancel. Le A qui a declenche
            // l'autowalk est protege par le check g_lbHeld en haut. Le stick
            // gauche (mouvement reel) est gere en kThumbstick plus bas. Les
            // autres boutons (B, Start, Back, D-pad) sont utilises dans des
            // combos LB+X ou ne represente pas un mouvement joueur.
        } else if (type == RE::INPUT_EVENT_TYPE::kThumbstick) {
            auto* stick = static_cast<RE::ThumbstickEvent*>(e);
            // Seuil 0.3 : evite les faux positifs du stick au repos (drift).
            // On ne reagit qu'au stick gauche (mouvement), pas au droit (camera).
            if (stick && stick->IsLeft()) {
                const float mag = std::sqrt(stick->xValue * stick->xValue +
                                            stick->yValue * stick->yValue);
                if (mag > 0.3f) {
                    shouldCancel = true;
                }
            }
        }
    }

    if (shouldCancel) {
        Speak(L"Stopping");
        g_autoWalking.store(false);
        LOG("AutoWalk: cancelled by user input (event-driven)");
        DispatchPapyrusStop();
        return true;
    }
    return false;
}

// Forward declaration
static void StopAutoWalk();

// Sécurité : nettoyage d'un autowalk potentiellement gravé dans la save
// (cas où le jeu a crashé pendant un autowalk → au reload la save contient
// AIDriven=true, DstMarker set, IsWalking=true, Travel package actif → si on
// lance un nouvel autowalk, on superpose sur cet état bancal → crash immédiat).
//
// Style f4access : aucune mutation C++. On dispatche simplement OnLoadGameReset
// au Papyrus, qui fera tout le cleanup (DstMarker.Clear, Traveler.ForceRefTo,
// Game.SetPlayerAIDriven(false), EvaluatePackage). Le dispatch est DIFFÉRÉ DE
// 3 SECONDES pour laisser le skeleton/shader finir sa reconstruction post-load.
// Le cooldown de 10s armé à kPostLoadGame bloque tout nouvel autowalk pendant
// cette attente.
static void AutoWalkSafetyReset() {
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        auto* task = SKSE::GetTaskInterface();
        if (!task) return;
        task->AddTask([]() {
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

// =============================================================================
// Helper : dispatche OnStopWalking au Papyrus. Utilise par le listener ModEvent
// d'arrivee et par AutoWalkInputUpdate (cancel input).
// Contrairement a StopAutoWalk(), ce helper ne clear pas g_autoWalkTarget
// (c'est StopAutoWalk qui fait le cleanup complet quand c'est demande
// explicitement depuis scanner.h).
//
// Style f4access : aucune mutation d'acteur cote C++, uniquement dispatch Papyrus.
// =============================================================================
static void DispatchPapyrusStop() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddTask([]() {
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
    });
}

// =============================================================================
// ModEvent listener : écoute "SkyrimNVDA_AutoWalkArrived" envoyé par le script
// Papyrus à l'arrivée du joueur. Annonce "Arrived at X" et stoppe le monitor.
// Le cleanup (AIDriven/SpeedMult/EvaluatePackage) est fait côté Papyrus dans
// StopWalkingInternal avant l'envoi de l'event.
//
// Papyrus est responsable de l'orientation du joueur via Actor.SetLookAt avant
// d'envoyer l'event, comme dans f4access.
// =============================================================================
class AutoWalkModEventListener : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    static AutoWalkModEventListener* GetSingleton() {
        static AutoWalkModEventListener s;
        return &s;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const SKSE::ModCallbackEvent* a_event,
        RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        if (a_event->eventName == "SkyrimNVDA_AutoWalkArrived") {
            if (!g_autoWalking.load()) {
                LOG("AutoWalk: arrived event received but g_autoWalking=false, ignoring");
                return RE::BSEventNotifyControl::kContinue;
            }
            LOG("AutoWalk: arrived event received from Papyrus");
            Speak(L"Arrived at " + g_autoWalkTarget);
            g_autoWalking.store(false);
            // Pas de cleanup C++ ici : Papyrus StopWalkingInternal s'en charge.
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

static void RegisterAutoWalkModEventListener() {
    auto* src = SKSE::GetModCallbackEventSource();
    if (!src) {
        LOG("AutoWalk: GetModCallbackEventSource returned null, listener NOT registered");
        return;
    }
    src->AddEventSink(AutoWalkModEventListener::GetSingleton());
    LOG("AutoWalk: ModEvent listener registered (SkyrimNVDA_AutoWalkArrived)");
}

// =============================================================================
// ANTI-REBOND : empeche deux dispatches rapproches sur le meme FormID.
// Bien qu'un dispatch unique puisse aussi crasher, la rafale de dispatches rapides
// observee dans certains crash logs (20 dispatches en 20s) augmente nettement les
// chances de crash, probablement en empilant des EvaluatePackage mal finalises.
// Fenetre minimale : 500ms entre deux dispatches.
// g_autoWalkLastDispatchMs est declare plus haut (utilise aussi pour la grace
// period dans AutoWalkInputUpdate).
// =============================================================================
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

    // Si FormID dynamique (FF*), passer en mode coordonnées
    bool useCoords = (posX != 0.f || posY != 0.f || posZ != 0.f);

    task->AddTask([targetFormID, stopDistance, posX, posY, posZ, useCoords]() {
        auto* player = RE::PlayerCharacter::GetSingleton();

        // Detection mounted : si le joueur est sur un cheval, on dispatche vers
        // OnWalkToTargetMounted (6 args : ajoute le mount FormID) au lieu de
        // OnWalkToTarget. Le Papyrus fait alors SetPlayerAIDriven(true) +
        // EvaluatePackage cote cheval pour que l'IA prenne le couple en charge.
        // Reproduit le comportement v1.4 qui fonctionnait, sans mutation C++.
        RE::FormID mountFormID = 0;
        bool mounted = false;
        if (player && player->IsOnMount()) {
            RE::NiPointer<RE::Actor> mount;
            if (player->GetMount(mount) && mount) {
                mountFormID = mount->GetFormID();
                mounted = true;
                LOG("AutoWalk: mounted detected, mount FormID={:08X}", mountFormID);
            } else {
                LOG("AutoWalk: IsOnMount=true but GetMount failed -> falling back to foot mode");
            }
        }

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
            // Pas de SetAIDriven/EvaluatePackage C++ : le Papyrus fait son clean slate
            // lui-même en appelant StopWalkingInternal au début de StartWalkToRef
            // si IsWalking était déjà true. Style f4access : zéro mutation acteur C++.

            // Activer kTryStep pour monter les escaliers automatiquement.
            // Ce sont des flags physiques du char controller, pas de l'AI — safe.
            auto* charCtrl = player->GetCharController();
            if (charCtrl) {
                charCtrl->flags.set(RE::CHARACTER_FLAGS::kTryStep);
                charCtrl->flags.set(RE::CHARACTER_FLAGS::kCanJump);
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

        // Mode a pied : 5 args (aiFormID, afStopDistance, afX, afY, afZ).
        // Mode mounted : 6 args (les memes + aiMountFormID).
        // Mode coordonnees : formID=0 + position x,y,z.
        // Mode normal : formID valide + 0,0,0.
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        bool ok = false;
        if (mounted) {
            LOG("AutoWalk: dispatching OnWalkToTargetMounted (target={:08X} mount={:08X} stopDist={} useCoords={})",
                targetFormID, mountFormID, stopDistance, useCoords);
            auto* args = RE::MakeFunctionArguments(
                static_cast<std::int32_t>(useCoords ? 0 : static_cast<std::int32_t>(targetFormID)),
                static_cast<float>(stopDistance),
                static_cast<float>(posX),
                static_cast<float>(posY),
                static_cast<float>(posZ),
                static_cast<std::int32_t>(mountFormID)
            );
            ok = vm->DispatchMethodCall(
                handle,
                RE::BSFixedString("SkyrimTTS_AutoWalk"),
                RE::BSFixedString("OnWalkToTargetMounted"),
                args,
                callback
            );
        } else {
            auto* args = RE::MakeFunctionArguments(
                static_cast<std::int32_t>(useCoords ? 0 : static_cast<std::int32_t>(targetFormID)),
                static_cast<float>(stopDistance),
                static_cast<float>(posX),
                static_cast<float>(posY),
                static_cast<float>(posZ)
            );
            ok = vm->DispatchMethodCall(
                handle,
                RE::BSFixedString("SkyrimTTS_AutoWalk"),
                RE::BSFixedString("OnWalkToTarget"),
                args,
                callback
            );
        }

        if (ok) {
            g_autoWalking.store(true);
            if (mounted) {
                LOG("AutoWalk: started MOUNTED mode FormID={:08X} mountFormID={:08X} stopDist={}",
                    targetFormID, mountFormID, stopDistance);
            } else if (useCoords) {
                LOG("AutoWalk: started coords mode FormID={:08X} pos=({:.0f},{:.0f},{:.0f}) stopDist={}",
                    targetFormID, posX, posY, posZ, stopDistance);
            } else {
                LOG("AutoWalk: started FormID mode {:08X} stopDist={}", targetFormID, stopDistance);
            }
            // Pas de monitor C++ : le cancel input est detecte par AutoWalkInputUpdate()
            // appele depuis InputListener::ProcessEvent (style f4access).
            // L'arrivee est detectee cote Papyrus (OnUpdate + ModEvent).
        } else {
            LOG("AutoWalk: DispatchMethodCall failed (mounted={})", mounted);
            Speak(L"AutoWalk error");
        }
    });
}

// Appelle OnStopWalking sur le script Papyrus
static void StopAutoWalk() {
    g_autoWalking.store(false);

    auto* task = SKSE::GetTaskInterface();
    if (!task) return;

    task->AddTask([]() {
        // Pas de SetAIDriven/SpeedMult/EvaluatePackage C++ ici : c'est Papyrus
        // StopWalkingInternal qui s'en charge via Game.SetPlayerAIDriven(false).
        // Style f4access : le C++ ne mute JAMAIS l'acteur, il dispatche au script.
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

        // Pas de cleanup XMarker C++ ici : Papyrus StopWalkingInternal s'en charge
        // (detecte les XMarkers 0x10 crees par PlaceAtMe et fait Disable + Delete).
        // Style f4access : zero manipulation de ref cote C++.

        g_autoWalkTarget.clear();
        LOG("AutoWalk: stop requested");
    });
}

// =============================================================================
// REMOTE ACTIVATE — Activer/ramasser l'objet courant du scanner a distance.
// Touche G : appelle Activate(player) sur la ref ciblee comme si on avait
// presse E a cote. Marche pour items (ramassage), conteneurs (ouverture),
// portes de cellule (teleportation a travers la porte), activateurs scriptes
// (leviers, piedestaux, etc.). Limite de 2000 unites enforced cote C++ avant
// dispatch Papyrus pour eviter d'activer un truc trop loin par accident.
// =============================================================================
static constexpr float kRemoteActivateMaxDistance = 2000.0f;

// Dispatch Papyrus OnRemoteActivate(formID). Appel avec FormID deja valide
// (lookup + distance check faits par l'appelant).
static void StartRemoteActivate(RE::FormID targetFormID) {
    auto* task = SKSE::GetTaskInterface();
    if (!task) {
        LOG("RemoteActivate: SKSE TaskInterface null, cannot dispatch");
        return;
    }

    task->AddTask([targetFormID]() {
        auto* quest = FindAutoWalkQuest();
        if (!quest) {
            LOG("RemoteActivate: AutoWalk quest not found");
            Speak(L"Activate failed: quest not found");
            return;
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            LOG("RemoteActivate: VM not available");
            return;
        }

        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            LOG("RemoteActivate: no handle policy");
            return;
        }

        auto handle = policy->GetHandleForObject(RE::FormType::Quest, quest);
        if (handle == policy->EmptyHandle()) {
            LOG("RemoteActivate: could not get quest handle");
            return;
        }

        auto* args = RE::MakeFunctionArguments(static_cast<std::int32_t>(targetFormID));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        bool ok = vm->DispatchMethodCall(
            handle,
            RE::BSFixedString("SkyrimTTS_AutoWalk"),
            RE::BSFixedString("OnRemoteActivate"),
            args,
            callback
        );

        if (ok) {
            LOG("RemoteActivate: dispatched OnRemoteActivate FormID={:08X}", targetFormID);
        } else {
            LOG("RemoteActivate: DispatchMethodCall failed for FormID={:08X}", targetFormID);
            Speak(L"Activate failed");
        }
    });
}

// Touche G hors menus : prend l'objet courant du scanner et l'active a distance.
// Verifications : objet selectionne, lookup ref valide, ref pas supprimee,
// distance <= 2000 unites. Logs verbeux pour diagnostiquer les echecs.
//
// Invalidation post-pickup : pour les items ramassables (kCatItems), on schedule
// un check differe 800ms apres dispatch qui re-verifie l'etat de la ref. Si elle
// est deleted/disabled/!Is3DLoaded (le moteur a fini le pickup), on met
// formID=0 pour la sortir definitivement de la liste — sinon le scanner garde
// l'objet visible meme apres ramassage car RefreshFilteredList pouvait passer
// pile entre Activate() et le cleanup engine. Pour les autres categories
// (conteneurs, portes, activateurs), on ne touche a rien : un coffre activable
// a distance reste dans la cellule, une porte aussi, et c'est correct qu'ils
// restent dans la liste.
static void ScannerActivateCurrent() {
    LOG("InputDiag: ScannerActivateCurrent ENTRY");

    if (g_scannedFiltered.empty() || g_scanIndex < 0 ||
        g_scanIndex >= static_cast<int>(g_scannedFiltered.size())) {
        LOG("RemoteActivate: no current target (empty={}, idx={})",
            g_scannedFiltered.empty(), g_scanIndex);
        Speak(L"No target selected. Scan first.");
        return;
    }

    auto& obj = *g_scannedFiltered[g_scanIndex];
    RE::FormID targetID = obj.formID;
    std::wstring targetName = obj.name;
    ScanCategory targetCategory = obj.category;

    if (targetID == 0) {
        LOG("RemoteActivate: target FormID is 0");
        Speak(L"No valid target");
        return;
    }

    auto* task = SKSE::GetTaskInterface();
    if (!task) {
        LOG("RemoteActivate: SKSE TaskInterface null");
        return;
    }

    task->AddTask([targetID, targetName, targetCategory]() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            LOG("RemoteActivate: player singleton null");
            return;
        }

        auto* form = RE::TESForm::LookupByID(targetID);
        auto* targetRef = form ? form->AsReference() : nullptr;
        if (!targetRef) {
            LOG("RemoteActivate: FormID {:08X} not found (despawned?)", targetID);
            Speak(L"Target not found");
            return;
        }

        if (targetRef->IsDeleted()) {
            LOG("RemoteActivate: FormID {:08X} is deleted", targetID);
            Speak(L"Target no longer exists");
            return;
        }

        if (targetRef->IsDisabled()) {
            LOG("RemoteActivate: FormID {:08X} is disabled", targetID);
            Speak(L"Target not available");
            return;
        }

        // Distance check : on lit la position 3D si chargee, sinon GetPosition.
        auto playerPos = player->GetPosition();
        auto targetPos = targetRef->GetPosition();
        float dist = (playerPos - targetPos).Length();
        if (dist > kRemoteActivateMaxDistance) {
            LOG("RemoteActivate: blocked - distance {:.0f} > {:.0f} for FormID={:08X}",
                dist, kRemoteActivateMaxDistance, targetID);
            Speak(L"Target is too far");
            return;
        }

        LOG("RemoteActivate: activating '{}' FormID={:08X} dist={:.0f} cat={}",
            WStringToUtf8(targetName), targetID, dist, static_cast<int>(targetCategory));
        StartRemoteActivate(targetID);

        // Invalidation differee pour les items ramassables uniquement.
        // 800ms : laisse le temps au moteur de faire Disable() + Delete() apres
        // le Activate() Papyrus. Marche aussi pour les ingredients ramasses
        // depuis une plante (la ref est consumed apres pickup).
        if (targetCategory == kCatItems) {
            std::thread([targetID]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
                auto* taskLater = SKSE::GetTaskInterface();
                if (!taskLater) return;
                taskLater->AddTask([targetID]() {
                    auto* form2 = RE::TESForm::LookupByID(targetID);
                    auto* ref2 = form2 ? form2->AsReference() : nullptr;
                    bool gone = !ref2 ||
                                ref2->IsDeleted() ||
                                ref2->IsDisabled() ||
                                !ref2->Is3DLoaded() ||
                                !ref2->GetParentCell();
                    if (gone) {
                        // Capturer le formID du voisin (objet PRECEDENT dans la liste
                        // filtree) AVANT invalidation : permet de restaurer la position
                        // du scanner apres rebuild. Sinon ApplyCategoryFilter perd la
                        // position courante (le formID courant devient 0 -> il retombe
                        // a l'index 0 = retour en haut de la liste, tres frustrant
                        // quand on ramasse en serie).
                        // On vise PRECEDENT pour que le prochain Right donne l'objet
                        // juste apres l'objet ramasse (continuite de navigation).
                        RE::FormID neighborFormID = 0;
                        if (g_scanIndex > 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
                            // L'item a l'index courant est celui qu'on vient de ramasser
                            // (formID == targetID). On prend l'index-1 comme ancre.
                            if (g_scannedFiltered[g_scanIndex]->formID == targetID) {
                                neighborFormID = g_scannedFiltered[g_scanIndex - 1]->formID;
                            }
                        }

                        // Invalider dans g_scannedAll : la prochaine ApplyCategoryFilter
                        // sautera l'entree (formID == 0 -> continue).
                        bool invalidated = false;
                        for (auto& o : g_scannedAll) {
                            if (o.formID == targetID) {
                                LOG("RemoteActivate: post-pickup invalidate FormID={:08X}", targetID);
                                o.formID = 0;
                                o.category = kCatAll;
                                invalidated = true;
                            }
                        }

                        // Rebuild la liste filtree + restaurer la position sur le voisin.
                        if (invalidated) {
                            ApplyCategoryFilter();
                            if (neighborFormID != 0) {
                                for (int i = 0; i < static_cast<int>(g_scannedFiltered.size()); i++) {
                                    if (g_scannedFiltered[i]->formID == neighborFormID) {
                                        g_scanIndex = i;
                                        LOG("RemoteActivate: scanner cursor restored to neighbor idx={} formID={:08X}",
                                            i, neighborFormID);
                                        break;
                                    }
                                }
                            }
                        }
                    } else {
                        LOG("RemoteActivate: post-pickup check FormID={:08X} still present (activate refused?)", targetID);
                    }
                });
            }).detach();
        }
    });
}

// Forward declaration — le corps est plus bas, défini après le wrapper.
static void ToggleAutoWalkImpl();

// Wrapper public appele depuis le thread clavier. On route le travail de
// lancement (lecture scanner, dispatch Papyrus) vers le thread principal via
// AddTask pour eviter les races avec le cell streaming et le scan automatique
// qui peuvent modifier g_scannedAll en parallele.
//
// L'arret d'un autowalk en cours (g_autoWalking=true) reste fait directement
// depuis le thread clavier : StopAutoWalk() utilise deja AddTask en interne
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

    // Style f4access pur : on fait confiance au Travel Package natif du moteur
    // Skyrim pour gerer TOUT le pathfinding, meme cross-worldspace interior vers
    // interior. Chaque load door a un ExtraNavMeshPortal qui relie les navmeshes
    // des deux cotes, et le NavMeshInfoMap global permet au moteur de calculer
    // un chemin piéton a travers plusieurs cellules non chargees (exactement ce
    // que font les NPCs vanilla quand ils voyagent entre villes).
    //
    // On ne fait plus de routing manuel vers une porte intermediaire. Si le
    // chemin est long (ex: Fort-Dragon vers Haut Hrothgar), le joueur peut
    // stopper l'autowalk, fast-travel vers une ville plus proche, puis relancer
    // l'autowalk : c'est au joueur de decider.
    //
    // Pour les refs dynamiques (FF*) non chargees en 3D, on fallback sur les
    // coordonnees de lastKnownPos (le scanner les a stockees). Le Papyrus crée
    // alors un XMarker temporaire aux coords et pathfind vers lui — le moteur
    // marche dans la bonne direction meme si le navmesh global n'atteint pas la
    // cible finale.

    Speak(L"Walking to " + targetName);

    // Cas FF* : refs dynamiques (aliases de quete spawn, objets jetes, PNJ
    // invoques). Si chargees en 3D, coord mode vers la position visuelle
    // (stable, pas la hitbox physique qui derive apres rebond). Sinon coord
    // mode vers lastKnownPos stocke par le scanner.
    if ((targetID >> 24) == 0xFF) {
        auto* refForm = RE::TESForm::LookupByID(targetID);
        auto* ref = refForm ? refForm->AsReference() : nullptr;
        if (ref && ref->Is3DLoaded() && !ref->IsDisabled() && !ref->IsDeleted()) {
            RE::NiPoint3 pos;
            auto* node = ref->Get3D();
            if (node) {
                pos = node->world.translate;
            } else {
                pos = ref->GetPosition();
            }
            LOG("AutoWalk: FF {:08X} 3D-loaded, coords mode pos=({:.0f},{:.0f},{:.0f}) source={}",
                targetID, pos.x, pos.y, pos.z, node ? "mesh" : "physics");
            StartAutoWalk(targetID, 150.0f, pos.x, pos.y, pos.z);
            return;
        }

        // Ref FF non chargee en 3D : utiliser lastKnownPos du scanner en coord mode.
        // Le Papyrus creera un XMarker aux coords et marchera vers lui via Travel
        // package. Le moteur pathfinde localement jusqu'a ce que le marker soit
        // inaccessible (navmesh coupe) et s'arretera la — le joueur peut alors
        // fast-travel ou relancer.
        if (g_scanIndex >= 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
            auto& obj = *g_scannedFiltered[g_scanIndex];
            if (obj.lastKnownPos.x != 0 || obj.lastKnownPos.y != 0) {
                LOG("AutoWalk: FF {:08X} not 3D-loaded, fallback coord mode via scanner lastKnownPos=({:.0f},{:.0f},{:.0f})",
                    targetID, obj.lastKnownPos.x, obj.lastKnownPos.y, obj.lastKnownPos.z);
                StartAutoWalk(0, 100.0f, obj.lastKnownPos.x, obj.lastKnownPos.y, obj.lastKnownPos.z);
                return;
            }
        }
        LOG("AutoWalk: FF {:08X} not 3D-loaded and no lastKnownPos, cannot walk", targetID);
        Speak(L"Cannot walk to this target");
        return;
    }

    // Cas non-FF : ref persistente (PNJ, porte, objet statique). Dispatch FormID
    // brut au Papyrus -> ForceRefTo(ref) -> Travel package natif. Le moteur fait
    // tout le pathfinding, y compris cross-cell cross-worldspace.
    LOG("AutoWalk: non-FF {:08X} dispatch direct (engine Travel package handles nav)", targetID);
    StartAutoWalk(targetID, 100.0f, 0.f, 0.f, 0.f);
}

// AUTOWALK — FIN
