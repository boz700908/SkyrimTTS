#pragma once

// =============================================================================
// Gamepad vtable hooks — architecture style SkyrimSoulsRE
// =============================================================================
//
// Probleme : un simple BSTEventSink<InputEvent*> enregistre sur
// BSInputDeviceManager est appele APRES les sinks internes du moteur
// (PlayerControls, MenuControls), donc muter un event dans notre sink ne
// bloque plus rien. Et l'approche historique (muter ControlMap::enabledControls
// et les mapping.inputKey) laisse une corruption persistante en cas de crash,
// et peut casser les manettes detectees en kOrbis (PS native).
//
// Solution : hook vtable sur les deux classes qui dispatchent les events vers
// les handlers du jeu :
//   - PlayerControls::ProcessEvent : gameplay (mouvement, combat, POV, jump...)
//   - MenuControls::ProcessEvent   : menus (Journal, Inventaire, Favoris...)
//
// Nos thunks sont appeles par le dispatcher du moteur a la place de la fonction
// originale. Ils ont l'event intact. Ils peuvent :
//   1. Lire l'event (detecter combo LB+X, R3 solo...) et declencher nos actions
//      de mod (ScannerNext, ToggleAutoWalk, LockNearestEnemy, annonces...).
//   2. Muter l'event (userEvent="" + idCode=0) pour que le moteur l'ignore et
//      ne declenche pas l'action vanilla en parallele.
//   3. Appeler la fonction originale, qui voit les events mutes et les skip.
//
// Avantages :
//   - Aucune mutation persistante de ControlMap -> zero risque de coincer le
//     gamepad en cas de crash ou d'etat incoherent.
//   - Reversible a chaque event : a l'event suivant, le moteur recoit un nouvel
//     InputEvent* avec ses valeurs normales.
//   - Compatible manettes Xbox, PS via Steam Input XInput, et meme PS native
//     (on ne touche pas aux mappings dependant du type de pad).
//
// L'ancien InputListener (plugin.cpp) garde la main sur :
//   - Events clavier (pas geres par les hooks).
//   - Events ThumbstickEvent (stick droit, geres separement).
//   - Autres events non-button.
// =============================================================================

#include "common.h"
#include "RE/M/MenuControls.h"
#include "RE/P/PlayerControls.h"
// Handlers dont on hook CanProcess pour forcer un return false quand LB est
// maintenu (voir InstallHooks). Inclure ces headers donne acces a leur vtable.
#include "RE/F/FavoritesHandler.h"
#include "RE/S/ShoutHandler.h"
#include "RE/J/JumpHandler.h"
#include "RE/S/SneakHandler.h"
#include "RE/S/SprintHandler.h"
#include "RE/A/AttackBlockHandler.h"
#include "RE/R/ReadyWeaponHandler.h"
#include "RE/A/ActivateHandler.h"
#include "RE/T/TogglePOVHandler.h"

// Toutes les fonctions d'action (ScannerNextObject, LockNearestEnemy, MapNext...,
// ToggleAutoWalk, AnnouncePlayerVitals, ToggleSneakGamepad, IsAnyMenuOpen,
// RemapGamepadControls, PluginNowMs, etc.) sont deja definies par les autres
// headers/sources inclus avant ce header dans plugin.cpp (common.h, menu_*.h,
// scanner.h, autowalk.h, puis les definitions static dans plugin.cpp).
// On les appelle directement depuis les fonctions inline ci-dessous — le
// compilateur les resout dans le meme TU.

namespace GamepadHook
{
    // Signature du ProcessEvent hooke : taille identique sur PlayerControls et
    // MenuControls (les deux sont des BSTEventSink<InputEvent*>).
    using ProcessEventFn = RE::BSEventNotifyControl (*)(
        void*, RE::InputEvent* const*, RE::BSTEventSource<RE::InputEvent*>*);

    inline ProcessEventFn _PlayerControlsProcessEvent = nullptr;
    inline ProcessEventFn _MenuControlsProcessEvent   = nullptr;

    // Neutralisation d'un event (pattern SkyrimSoulsRE).
    // userEvent="" + idCode=0 -> le moteur ne matche plus aucun mapping.
    inline void NeutralizeEvent(RE::IDEvent* idEvent) {
        idEvent->userEvent = "";
        idEvent->idCode = 0;
    }

    // Traite un event gamepad button : declenche l'action de mod correspondante
    // si applicable, et retourne true si l'event doit etre neutralise (pour
    // empecher le moteur de dispatcher l'action vanilla en parallele).
    //
    // Appele depuis PlayerControls_ProcessEvent_Hook et MenuControls_ProcessEvent_Hook.
    // Centralise toute la logique de boutons gamepad en un seul endroit.
    inline bool ProcessGamepadButton(RE::ButtonEvent* btn) {
        if (!btn) return false;

        using K = RE::BSWin32GamepadDevice::Keys;
        const auto idCode = btn->idCode;

        // Event deja neutralise par un precedent appel du hook (cas du double
        // dispatch PlayerControls + MenuControls en menu) : skip pour eviter les
        // logs dupliques "combo non reconnu" et les double traitements.
        if (idCode == 0) return false;

        const bool isDown = btn->IsDown();
        const bool isUp   = btn->IsUp();

        // Re-appliquer le remap si le jeu l'a ecrase (les 20 premiers events,
        // comme avant le refactor). Log le type de pad au premier event vu.
        static std::atomic_int s_remapCount{0};
        static std::atomic_bool s_firstEventSeen{false};
        if (!s_firstEventSeen.exchange(true)) {
            LOG("GAMEPAD: First gamepad event received (via hook)");
            // Logger gamePadMapType ici plutot qu'au kDataLoaded : au demarrage
            // le moteur n'a pas encore initialise le type correctement.
            if (auto* cm2 = RE::ControlMap::GetSingleton()) {
                auto padType = cm2->gamePadMapType.get();
                const char* padName = (padType == RE::PC_GAMEPAD_TYPE::kOrbis)   ? "Orbis (PS native)" :
                                      (padType == RE::PC_GAMEPAD_TYPE::kDirectX) ? "DirectX (XInput/Xbox/Steam Input)" :
                                      "unknown";
                LOG("GAMEPAD: ControlMap::gamePadMapType = {} ({})",
                    static_cast<int>(padType), padName);
            }
        }
        if (s_remapCount.load() < 20) {
            if (RemapGamepadControls()) {
                LOG("GAMEPAD: Remap re-applied (attempt {})", s_remapCount.load());
            }
            s_remapCount++;
        }

        // ===== LB : track state, pas de neutralisation ici =====
        // LB lui-meme n'est pas neutralise (on a besoin que le moteur gere le
        // relachement correctement). Notre InputListener-sink tracking est
        // complementaire mais ici on est plus sur : le hook voit tous les events.
        if (idCode == K::kLeftShoulder) {
            if (isDown) {
                g_lbHeld.store(true);
                g_lbWasModifier.store(false);
                // Note : on ne peut pas appeler PluginNowMs ici sans forward
                // declaration ; c'est fait plus haut. g_lbLastSeenPressedMs
                // est mis a jour pour le stuck watcher.
                LOG("GAMEPAD: LB pressed (hook)");
            } else if (isUp) {
                g_lbHeld.store(false);
                LOG("GAMEPAD: LB released (hook)");
            }
            // Neutraliser LB dans menu item (empecher SkyUI de changer de
            // categorie en parallele).
            const bool inItemMenu =
                g_invOpen.load(std::memory_order_relaxed) ||
                g_containerOpen.load(std::memory_order_relaxed) ||
                g_barterOpen.load(std::memory_order_relaxed) ||
                g_favOpen.load(std::memory_order_relaxed) ||
                g_giftOpen.load(std::memory_order_relaxed) ||
                g_craftingOpen.load(std::memory_order_relaxed) ||
                g_magicOpen.load(std::memory_order_relaxed);
            return inItemMenu;
        }

        // ===== Tween menu (menu en croix) : D-pad directions =====
        if (g_tweenForeground.load(std::memory_order_relaxed) && isDown && !g_lbHeld.load()) {
            int targetFrame = 0;
            if      (idCode == K::kUp)    targetFrame = 2;
            else if (idCode == K::kLeft)  targetFrame = 3;
            else if (idCode == K::kRight) targetFrame = 4;
            else if (idCode == K::kDown)  targetFrame = 5;
            if (targetFrame > 0) {
                int prev = g_lastTweenFrame.exchange(targetFrame);
                if (prev != targetFrame) {
                    LOG("GAMEPAD: D-pad tween direction frame={}", targetFrame);
                    AnnounceTweenNavKey(targetFrame);
                }
                return false;  // on laisse le jeu voir le D-pad (il gere la navigation)
            }
        }

        // ===== Level Up Menu : D-pad + A =====
        if (g_levelUpOpen.load(std::memory_order_relaxed) && isDown && !g_lbHeld.load()) {
            const bool prev = (idCode == K::kLeft) || (idCode == K::kUp);
            const bool next = (idCode == K::kRight) || (idCode == K::kDown);
            if (prev) {
                g_levelUpSelection.store((g_levelUpSelection.load() + 2) % 3);
                LOG("GAMEPAD: LevelUp prev → selection={}", g_levelUpSelection.load());
                AnnounceLevelUpSelection();
                return true;
            }
            if (next) {
                g_levelUpSelection.store((g_levelUpSelection.load() + 1) % 3);
                LOG("GAMEPAD: LevelUp next → selection={}", g_levelUpSelection.load());
                AnnounceLevelUpSelection();
                return true;
            }
            if (idCode == K::kA) {
                LOG("GAMEPAD: LevelUp A → confirm selection={}", g_levelUpSelection.load());
                QueueConfirmLevelUp();
                return true;
            }
        }

        // ===== MessageBox : D-pad + A/B =====
        if (g_msgBoxOpen.load(std::memory_order_relaxed) && isDown && !g_lbHeld.load()) {
            const bool mbUp   = (idCode == K::kUp)   || (idCode == K::kLeft);
            const bool mbDown = (idCode == K::kDown) || (idCode == K::kRight);
            if (mbUp) {
                int sel = g_msgBoxSelectedBtn.load();
                if (sel > 0) g_msgBoxSelectedBtn.store(sel - 1);
                LOG("GAMEPAD: MsgBox prev → selection={}", g_msgBoxSelectedBtn.load());
                QueueAnnounceMsgBoxBtn();
                return true;
            }
            if (mbDown) {
                int sel   = g_msgBoxSelectedBtn.load();
                int count = g_msgBoxBtnCount.load();
                if (sel < count - 1) g_msgBoxSelectedBtn.store(sel + 1);
                LOG("GAMEPAD: MsgBox next → selection={}", g_msgBoxSelectedBtn.load());
                QueueAnnounceMsgBoxBtn();
                return true;
            }
            if (idCode == K::kA) {
                LOG("GAMEPAD: MsgBox A → confirm selection={}", g_msgBoxSelectedBtn.load());
                QueueMsgBoxPress();
                return true;
            }
            if (idCode == K::kB) {
                LOG("GAMEPAD: MsgBox B → cancel");
                QueueMsgBoxCancel();
                return true;
            }
        }

        // Charger les mappings configurables (MCM user-defined).
        const auto keyNext     = GpIndexToCode(g_gpIdxScanNext.load());
        const auto keyPrev     = GpIndexToCode(g_gpIdxScanPrev.load());
        const auto keyAnnounce = GpIndexToCode(g_gpIdxScanAnnounce.load());
        const auto keyMapSetRef= GpIndexToCode(g_gpIdxMapSetRef.load());
        const auto keyPrimary       = GpIndexToCode(g_gpIdxPrimary.load());
        const auto keyRemoteActivate= GpIndexToCode(g_gpIdxRemoteActivate.load());
        const auto keyTeleport      = GpIndexToCode(g_gpIdxTeleport.load());
        const auto keyVitals        = GpIndexToCode(g_gpIdxVitals.load());
        const auto keySneak    = GpIndexToCode(g_gpIdxSneak.load());
        const auto keyPOV      = GpIndexToCode(g_gpIdxPOV.load());
        const auto keyLockEnemy= GpIndexToCode(g_gpIdxLockEnemy.load());

        // ===== Combos LB+X =====
        if (g_lbHeld.load() && isDown) {
            g_lbWasModifier.store(true);

            // --- Carte ouverte : combos specifiques map ---
            if (g_mapOpen.load()) {
                if (idCode == keyNext)      { LOG("GAMEPAD MAP: LB+Next → MapNextMarker"); MapNextMarker(); return true; }
                if (idCode == keyPrev)      { LOG("GAMEPAD MAP: LB+Prev → MapPrevMarker"); MapPrevMarker(); return true; }
                if (idCode == keyAnnounce)  { LOG("GAMEPAD MAP: LB+Announce → MapAnnounceDetails"); MapAnnounceDetails(); return true; }
                if (idCode == keyMapSetRef) { LOG("GAMEPAD MAP: LB+SetRef → MapSetReference"); MapSetReference(); return true; }
                if (idCode == keyPrimary)   { LOG("GAMEPAD MAP: LB+Primary → MapFastTravel"); MapFastTravel(); return true; }
                if (idCode == keyVitals)    { LOG("GAMEPAD MAP: LB+Vitals → MapPlaceCustomMarker"); MapPlaceCustomMarker(); return true; }
                // Autres boutons sur la carte : neutralise quand meme (LB maintenu
                // = pas de pan de carte / zoom parallele).
                return true;
            }

            // --- Menus ouverts : whitelist LB+Y pour stats ---
            if (IsAnyMenuOpen()) {
                const bool inItemMenu = g_invOpen.load(std::memory_order_relaxed) ||
                                        g_containerOpen.load(std::memory_order_relaxed) ||
                                        g_barterOpen.load(std::memory_order_relaxed) ||
                                        g_giftOpen.load(std::memory_order_relaxed);
                if (inItemMenu && idCode == keyVitals) {
                    if (g_invOpen.load(std::memory_order_relaxed)) {
                        LOG("GAMEPAD: LB+Vitals (menu) → AnnounceInventoryStats");
                        AnnounceInventoryStats();
                    } else if (g_containerOpen.load(std::memory_order_relaxed)) {
                        LOG("GAMEPAD: LB+Vitals (menu) → AnnounceContainerStats");
                        AnnounceContainerStats();
                    } else if (g_barterOpen.load(std::memory_order_relaxed)) {
                        LOG("GAMEPAD: LB+Vitals (menu) → AnnounceBarterStats");
                        AnnounceBarterStats();
                    }
                    return true;
                }
                LOG("GAMEPAD: LB+0x{:04X} ignored (menu open)", idCode);
                return true;
            }

            // --- Hors menu : combos scanner ---
            if (idCode == keyNext)     { LOG("GAMEPAD: LB+Next → ScannerNextObject"); ScannerNextObject(); return true; }
            if (idCode == keyPrev)     { LOG("GAMEPAD: LB+Prev → ScannerPrevObject"); ScannerPrevObject(); return true; }
            if (idCode == keyAnnounce) { LOG("GAMEPAD: LB+Announce → ScannerAnnounceCurrent"); ScannerAnnounceCurrent(); return true; }
            if (idCode == keyPrimary)  { LOG("GAMEPAD: LB+Primary → ToggleAutoWalk"); ToggleAutoWalk(); return true; }
            if (idCode == keyRemoteActivate) {
                LOG("GAMEPAD: LB+RemoteActivate → ScannerActivateCurrent");
                ScannerActivateCurrent();
                return true;
            }
            if (idCode == keyVitals) {
                if (g_invOpen.load(std::memory_order_relaxed)) {
                    LOG("GAMEPAD: LB+Vitals → AnnounceInventoryStats"); AnnounceInventoryStats();
                } else if (g_containerOpen.load(std::memory_order_relaxed)) {
                    LOG("GAMEPAD: LB+Vitals → AnnounceContainerStats"); AnnounceContainerStats();
                } else if (g_barterOpen.load(std::memory_order_relaxed)) {
                    LOG("GAMEPAD: LB+Vitals → AnnounceBarterStats"); AnnounceBarterStats();
                } else {
                    LOG("GAMEPAD: LB+Vitals → AnnouncePlayerVitals"); AnnouncePlayerVitals();
                }
                return true;
            }
            if (idCode == keyTeleport) { LOG("GAMEPAD: LB+Teleport → ScannerTeleport"); ScannerTeleport(); return true; }
            if (idCode == keySneak)    { LOG("GAMEPAD: LB+Sneak → ToggleSneak"); ToggleSneakGamepad(); return true; }
            if (idCode == keyPOV) {
                LOG("GAMEPAD: LB+POV → TogglePOV");
                auto* camera = RE::PlayerCamera::GetSingleton();
                if (camera) {
                    if (camera->IsInFirstPerson()) {
                        camera->ForceThirdPerson();
                        Speak(L"Third person");
                    } else {
                        camera->ForceFirstPerson();
                        Speak(L"First person");
                    }
                }
                return true;
            }

            LOG("GAMEPAD: LB+0x{:04X} — combo non reconnu", idCode);
            // Combo non reconnu mais LB maintenu : neutraliser quand meme pour
            // empecher l'action vanilla (comportement "LB = mode scanner").
            return true;
        }

        // ===== Lock enemy (R3 ou autre bouton assigne, sans LB) =====
        if (idCode == keyLockEnemy && isDown && !g_lbHeld.load()) {
            if (!IsAnyMenuOpen()) {
                LOG("GAMEPAD: LockEnemy → LockNearestEnemy");
                LockNearestEnemy();
            }
            // Neutraliser : si keyLockEnemy == R3 (defaut), empecher le POV
            // silencieux vanilla. Si c'est un autre bouton, l'action vanilla
            // serait redondante ou genante.
            return true;
        }

        // ===== R3 solo sans LB (si pas mappe sur keyLockEnemy) =====
        // Meme cas que ci-dessus si keyLockEnemy a ete remappe par le joueur :
        // on veut toujours empecher le POV vanilla silencieux.
        if (idCode == K::kRightThumb && isDown && !g_lbHeld.load() && !IsAnyMenuOpen()) {
            return true;
        }

        // Pas de traitement : l'event passe au moteur normalement.
        return false;
    }

    // Parcourt la chaine d'events, traite chaque event gamepad button via
    // ProcessGamepadButton, et neutralise ceux qui doivent l'etre.
    inline void ProcessChain(RE::InputEvent* const* a_event) {
        if (!a_event || !*a_event) return;
        for (RE::InputEvent* evn = *a_event; evn; evn = evn->next) {
            if (!evn || !evn->HasIDCode()) continue;
            if (evn->GetDevice() != RE::INPUT_DEVICE::kGamepad) continue;
            auto* btn = static_cast<RE::ButtonEvent*>(evn);
            if (ProcessGamepadButton(btn)) {
                NeutralizeEvent(static_cast<RE::IDEvent*>(evn));
            }
        }
    }

    // Thunk : PlayerControls::ProcessEvent hook.
    // Appele par le dispatcher du moteur lorsqu'il distribue aux sinks
    // internes. On a l'event intact.
    static RE::BSEventNotifyControl PlayerControls_ProcessEvent_Hook(
        void* a_this,
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>* a_source)
    {
        ProcessChain(a_event);
        return _PlayerControlsProcessEvent(a_this, a_event, a_source);
    }

    // Thunk : MenuControls::ProcessEvent hook.
    static RE::BSEventNotifyControl MenuControls_ProcessEvent_Hook(
        void* a_this,
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>* a_source)
    {
        ProcessChain(a_event);
        return _MenuControlsProcessEvent(a_this, a_event, a_source);
    }

    // =========================================================================
    // Hook sur CanProcess des handlers individuels (style SkyrimSoulsRE).
    //
    // Pourquoi : notre mutation d'event dans MenuControls/PlayerControls ne
    // suffit pas toujours a bloquer l'action vanilla. Certains handlers
    // (ShoutHandler, FavoritesHandler, AttackBlockHandler...) font leur propre
    // lecture d'event (souvent via le ControlMap pour retrouver le userEvent
    // meme si on a mis idCode=0). Plutot que de deviner quel handler lit quoi,
    // on hook directement leur CanProcess pour forcer un return false quand
    // LB est maintenu — ainsi aucun handler ne traite l'event pendant un combo.
    //
    // Slot 0x1 pour MenuEventHandler ET PlayerInputHandler (meme signature :
    // virtual bool CanProcess(InputEvent*)).
    // =========================================================================
    using CanProcessFn = bool (*)(void*, RE::InputEvent*);

    // Un pointeur vers l'original par VTable. Utilise un template pour stocker
    // un original par handler-type (meme si tous les handlers ont la meme
    // signature, chacun a sa propre fonction originale a appeler).
    template<typename T>
    struct HandlerCanProcessHook {
        inline static CanProcessFn _original = nullptr;

        static bool Hook(void* a_this, RE::InputEvent* a_event) {
            // Si LB est maintenu, empecher le handler vanilla de s'executer.
            // L'action est deja declenchee par notre ProcessGamepadButton.
            if (g_lbHeld.load()) return false;
            return _original(a_this, a_event);
        }

        static void Install(REL::Relocation<std::uintptr_t> vtable, const char* name) {
            _original = reinterpret_cast<CanProcessFn>(vtable.write_vfunc(0x1, Hook));
            LOG("GamepadHook: {}::CanProcess hooked", name);
        }
    };

    // Installe les hooks. A appeler une fois au demarrage (kDataLoaded).
    inline void InstallHooks() {
        // Hook principal : mutation d'events dans les deux dispatchers.
        REL::Relocation<std::uintptr_t> playerVTable(RE::VTABLE_PlayerControls[0]);
        _PlayerControlsProcessEvent = reinterpret_cast<ProcessEventFn>(
            playerVTable.write_vfunc(0x1, PlayerControls_ProcessEvent_Hook));
        LOG("GamepadHook: PlayerControls::ProcessEvent hooked (original={:#x})",
            reinterpret_cast<std::uintptr_t>(_PlayerControlsProcessEvent));

        REL::Relocation<std::uintptr_t> menuVTable(RE::VTABLE_MenuControls[0]);
        _MenuControlsProcessEvent = reinterpret_cast<ProcessEventFn>(
            menuVTable.write_vfunc(0x1, MenuControls_ProcessEvent_Hook));
        LOG("GamepadHook: MenuControls::ProcessEvent hooked (original={:#x})",
            reinterpret_cast<std::uintptr_t>(_MenuControlsProcessEvent));

        // Hooks individuels : force CanProcess a retourner false quand LB est
        // maintenu. Evite que les handlers vanilla (ShoutHandler sur D-pad,
        // FavoritesHandler sur menu open, AttackBlockHandler sur RB/LT...)
        // s'executent en parallele de nos combos.
        // On hook les handlers susceptibles de recevoir un bouton configure
        // dans nos combos. Les autres (MovementHandler, LookHandler) ne sont
        // pas dangereux : ils reagissent sur stick gauche / axes souris.
        HandlerCanProcessHook<RE::FavoritesHandler>::Install(
            REL::Relocation<std::uintptr_t>(RE::VTABLE_FavoritesHandler[0]), "FavoritesHandler");
        HandlerCanProcessHook<RE::ShoutHandler>::Install(
            REL::Relocation<std::uintptr_t>(RE::VTABLE_ShoutHandler[0]), "ShoutHandler");
        HandlerCanProcessHook<RE::JumpHandler>::Install(
            REL::Relocation<std::uintptr_t>(RE::VTABLE_JumpHandler[0]), "JumpHandler");
        HandlerCanProcessHook<RE::SneakHandler>::Install(
            REL::Relocation<std::uintptr_t>(RE::VTABLE_SneakHandler[0]), "SneakHandler");
        HandlerCanProcessHook<RE::SprintHandler>::Install(
            REL::Relocation<std::uintptr_t>(RE::VTABLE_SprintHandler[0]), "SprintHandler");
        HandlerCanProcessHook<RE::AttackBlockHandler>::Install(
            REL::Relocation<std::uintptr_t>(RE::VTABLE_AttackBlockHandler[0]), "AttackBlockHandler");
        HandlerCanProcessHook<RE::ReadyWeaponHandler>::Install(
            REL::Relocation<std::uintptr_t>(RE::VTABLE_ReadyWeaponHandler[0]), "ReadyWeaponHandler");
        HandlerCanProcessHook<RE::ActivateHandler>::Install(
            REL::Relocation<std::uintptr_t>(RE::VTABLE_ActivateHandler[0]), "ActivateHandler");
        HandlerCanProcessHook<RE::TogglePOVHandler>::Install(
            REL::Relocation<std::uintptr_t>(RE::VTABLE_TogglePOVHandler[0]), "TogglePOVHandler");
    }
}
