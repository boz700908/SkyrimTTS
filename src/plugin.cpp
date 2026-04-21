#include "common.h"
#include "menu_inventory.h"
#include "menu_container.h"
#include "menu_quickloot.h"
#include "menu_dialogue.h"
#include "menu_mcm.h"
#include "menu_journal.h"
#include "menu_levelup.h"
#include "menu_magic.h"
#include "menu_main.h"
#include "menu_msgbox.h"
#include "menu_hud.h"
#include "menu_stats.h"
#include "menu_racesex.h"
#include "menu_tween.h"
#include "menu_tutorial.h"
#include "menu_favorites.h"
#include "menu_uilistmenu.h"
#include "menu_map.h"
#include "menu_barter.h"
#include "menu_crafting.h"
#include "menu_loading.h"
#include "menu_sleepwait.h"
#include "menu_gift.h"
#include "menu_book.h"
#include "menu_training.h"
#include "menu_console.h"
#include "menu_character_sheet.h"
#include "loot_tracker.h"
#include "scanner.h"
#include "autowalk.h"

#include <Xinput.h>  // pour lire l'etat physique du gamepad dans le stuck watcher

// ---------------- Gamepad state (déclaré tôt pour accès depuis MenuListener) ----------------
static std::atomic_bool g_lbHeld{false};               // LB maintenu = mode scanner
static std::atomic_bool g_lbWasModifier{false};         // LB a été utilisé comme modificateur
static std::atomic_bool g_gamepadDetected{false};       // true dès qu'on reçoit un événement gamepad
static std::atomic_int  g_gamepadRemapCount{0};         // combien de fois on a appliqué le remap
static std::uint32_t g_savedControls = 0;               // état des contrôles sauvegardé avant LB
static bool g_controlsDisabled = false;                  // true si on a désactivé les contrôles

// Sauvegarde dynamique du mapping YButton vanilla dans kItemMenu quand LB est
// maintenu dans un menu item (inventaire/conteneur/marchand/gift). On désactive
// temporairement YButton pour que notre combo LB+Y déclenche nos stats sans que
// le moteur Skyrim mette aussi l'item en favori. Restauré au relâchement de LB.
static std::uint16_t g_savedItemMenuYButtonKey = 0xFFFF;  // 0xFFFF = pas sauvegardé
static bool          g_itemMenuYButtonDisabled = false;

// =============================================================================
// Détection "LB collé" — bug joueur : quand un dialogue forcé, fast travel, ou
// téléport vers activator s'ouvre pendant que LB est maintenu, l'event "LB up"
// n'arrive jamais à notre InputListener (filtré par le menu/état bloquant), et
// g_lbHeld reste à true indéfiniment. Résultat : tous les boutons suivants sont
// traités comme combos LB+X et les contrôles gameplay restent masqués.
//
// Solution : thread de polling qui lit l'état RÉEL du gamepad via XInput
// (XInputGetState + XINPUT_GAMEPAD_LEFT_SHOULDER).
// Si g_lbHeld=true mais LB pas physiquement pressé depuis > 2s, reset forcé.
// =============================================================================
static std::atomic<int64_t> g_lbLastSeenPressedMs{0};  // timestamp dernier moment où on a vu LB réellement enfoncé (physiquement ou event pressed)
static std::jthread         g_lbStuckWatcher;

static int64_t PluginNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Reset complet du state LB : restaure enabledControls, restaure YButton dans
// kItemMenu, clear des flags. Doit tourner sur le main thread (modifie
// ControlMap + mapping.inputKey). Appelé depuis le polling thread via AddTask,
// ou directement depuis MenuListener (qui tourne déjà main thread).
static void ResetLbStateMainThread(const char* reason) {
    auto* cm = RE::ControlMap::GetSingleton();

    // Restaurer les contrôles masqués
    if (g_controlsDisabled) {
        if (cm) {
            cm->enabledControls = static_cast<RE::UserEvents::USER_EVENT_FLAG>(g_savedControls);
        }
        g_controlsDisabled = false;
        LOG("GAMEPAD: ResetLbState ({}) — controls restored (0x{:08X})", reason, g_savedControls);
    }

    // Restaurer le mapping YButton dans kItemMenu si désactivé
    if (cm && g_itemMenuYButtonDisabled) {
        constexpr auto kItemMenuCtxId = static_cast<size_t>(
            RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu);
        auto* itemCtx = cm->controlMap[kItemMenuCtxId];
        if (itemCtx) {
            for (auto& mapping : itemCtx->deviceMappings[RE::INPUT_DEVICE::kGamepad]) {
                if (mapping.eventID == RE::BSFixedString("YButton") && mapping.inputKey == 0xFFFF) {
                    mapping.inputKey = g_savedItemMenuYButtonKey;
                    g_itemMenuYButtonDisabled = false;
                    LOG("GAMEPAD: ResetLbState ({}) — YButton mapping restored (0x{:04X})",
                        reason, g_savedItemMenuYButtonKey);
                    break;
                }
            }
        }
    }

    // Reset du flag principal
    g_lbHeld.store(false);
    g_lbWasModifier.store(false);
}

// Helper : reset depuis n'importe quel thread (dispatch sur main).
static void ResetLbStateAsync(const char* reason) {
    auto* task = SKSE::GetTaskInterface();
    if (!task) {
        // Pas de task interface (trop tôt dans le cycle de vie) — on tente direct.
        ResetLbStateMainThread(reason);
        return;
    }
    std::string reasonStr = reason;
    task->AddTask([reasonStr]() {
        ResetLbStateMainThread(reasonStr.c_str());
    });
}

// Thread de surveillance : polling toutes les 500ms de l'état physique de LB
// via XInput. Si on a g_lbHeld=true mais LB pas physiquement enfoncé depuis
// > 2s, on force le reset.
static void StartLbStuckWatcher() {
    if (g_lbStuckWatcher.joinable()) return;

    g_lbStuckWatcher = std::jthread([](std::stop_token stoken) {
        constexpr int64_t kStuckThresholdMs = 2000;  // 2s sans voir LB pressé -> stuck

        while (!stoken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (stoken.stop_requested()) break;

            // Si on ne pense pas que LB est maintenu, rien à vérifier.
            if (!g_lbHeld.load()) continue;

            // XInput est thread-safe et ne dépend pas du scenegraph Skyrim.
            // On peut l'appeler depuis ce worker sans passer par AddTask.
            XINPUT_STATE state{};
            DWORD result = XInputGetState(0, &state);  // manette 0

            int64_t now = PluginNowMs();

            if (result != ERROR_SUCCESS) {
                // Pas de manette branchée ou erreur — si on croit LB maintenu, stuck évident.
                int64_t lastSeen = g_lbLastSeenPressedMs.load();
                int64_t since = now - lastSeen;
                if (lastSeen != 0 && since > kStuckThresholdMs) {
                    LOG("GAMEPAD: LB stuck — no controller detected and {}ms since last event", since);
                    // Reset doit se faire sur main thread (modifie ControlMap).
                    auto* task = SKSE::GetTaskInterface();
                    if (task) task->AddTask([]() { ResetLbStateMainThread("no-controller"); });
                }
                continue;
            }

            const bool lbPhysicallyDown =
                (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;

            if (lbPhysicallyDown) {
                g_lbLastSeenPressedMs.store(now);
                continue;
            }

            // LB pas physiquement pressé. Depuis combien de temps ?
            int64_t lastSeen = g_lbLastSeenPressedMs.load();
            int64_t since = now - lastSeen;

            if (lastSeen != 0 && since > kStuckThresholdMs) {
                LOG("GAMEPAD: LB stuck detected (g_lbHeld=true but not pressed for {}ms) — forcing reset", since);
                auto* task = SKSE::GetTaskInterface();
                if (task) task->AddTask([]() { ResetLbStateMainThread("stuck-watcher"); });
                // On reset aussi g_lbHeld localement ici pour arrêter de log en boucle.
                // Le vrai cleanup (ControlMap etc) se fera sur main thread au-dessus.
                g_lbHeld.store(false);
            }
        }
        LOG("GAMEPAD: LB stuck watcher thread stopped");
    });
}

// ---------------- Menu open/close listener ----------------

class MenuListener : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* e,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
        if (!e) return RE::BSEventNotifyControl::kContinue;

        // Log only unhandled menus (light diagnostics)
        if (e->menuName != RE::RaceSexMenu::MENU_NAME &&
            e->menuName != RE::ContainerMenu::MENU_NAME &&
            e->menuName != RE::BarterMenu::MENU_NAME &&
            e->menuName != RE::InventoryMenu::MENU_NAME &&
            e->menuName != RE::MainMenu::MENU_NAME &&
            e->menuName != RE::TitleSequenceMenu::MENU_NAME &&
            e->menuName != RE::JournalMenu::MENU_NAME &&
            e->menuName != RE::MagicMenu::MENU_NAME &&
            e->menuName != RE::TweenMenu::MENU_NAME &&
            e->menuName != RE::LevelUpMenu::MENU_NAME &&
            e->menuName != RE::MessageBoxMenu::MENU_NAME &&
            e->menuName != RE::StatsMenu::MENU_NAME &&
            e->menuName != RE::TutorialMenu::MENU_NAME &&
            e->menuName != RE::DialogueMenu::MENU_NAME &&
            e->menuName != RE::FavoritesMenu::MENU_NAME &&
            e->menuName != RE::LoadingMenu::MENU_NAME &&
            e->menuName != RE::SleepWaitMenu::MENU_NAME &&
            e->menuName != RE::GiftMenu::MENU_NAME &&
            e->menuName != RE::BookMenu::MENU_NAME &&
            e->menuName != RE::TrainingMenu::MENU_NAME &&
            e->menuName != CHARSHEET_MENU_SHOWSTATS &&
            e->menuName != CHARSHEET_MENU_SHOWFACTIONS &&
            e->menuName != UILIST_MENU_NAME) {
            LOG("Menu {} : {}", e->opening ? "OPEN" : "CLOSE", e->menuName.c_str());
        }

        // Reset immédiat du state LB si un menu "piégeur" s'ouvre pendant LB
        // maintenu. Ces menus filtrent les events ButtonEvent, donc on ne verra
        // jamais le "LB released" : g_lbHeld resterait à true indéfiniment.
        // On force le reset dès l'ouverture pour couper court au bug.
        if (e->opening && g_lbHeld.load()) {
            if (e->menuName == RE::DialogueMenu::MENU_NAME ||
                e->menuName == RE::MessageBoxMenu::MENU_NAME ||
                e->menuName == RE::LoadingMenu::MENU_NAME ||
                e->menuName == RE::FaderMenu::MENU_NAME ||
                e->menuName == RE::Console::MENU_NAME ||
                e->menuName == RE::SleepWaitMenu::MENU_NAME ||
                e->menuName == RE::TutorialMenu::MENU_NAME ||
                e->menuName == RE::BookMenu::MENU_NAME) {
                LOG("GAMEPAD: '{}' opening with LB held — preemptive reset", e->menuName.c_str());
                ResetLbStateMainThread("menu-preempt");
            }
        }

        // Si un menu bloquant le mouvement ouvre pendant un autowalk, stopper
        // l'autowalk proprement pour éviter que AIDriven reste coincé à true.
        // Sinon le joueur / cheval se fige à la fermeture du menu.
        if (e->opening && g_autoWalking.load(std::memory_order_relaxed)) {
            if (e->menuName == RE::DialogueMenu::MENU_NAME ||
                e->menuName == RE::CraftingMenu::MENU_NAME ||
                e->menuName == RE::BarterMenu::MENU_NAME ||
                e->menuName == RE::ContainerMenu::MENU_NAME ||
                e->menuName == RE::GiftMenu::MENU_NAME ||
                e->menuName == RE::TrainingMenu::MENU_NAME ||
                e->menuName == RE::SleepWaitMenu::MENU_NAME ||
                e->menuName == RE::BookMenu::MENU_NAME) {
                LOG("AutoWalk: stopping due to blocking menu open: {}", e->menuName.c_str());
                StopAutoWalk();
            }
        }

        if (e->menuName == RE::DialogueMenu::MENU_NAME) {
            if (e->opening) {
                g_dialogueOpen.store(true);
                g_lastDialogueOption.clear();
                QueueDialogueRead();
                StartDialoguePolling();
            } else {
                g_dialogueOpen.store(false);
                StopDialoguePolling();
            }
        }

        if (e->menuName == RE::RaceSexMenu::MENU_NAME) {
            if (e->opening) {
                g_raceSexOpen.store(true);
                g_lastRaceSexCat.clear();
                g_lastRaceSexRace.clear();
                g_lastRaceSexSliderLabel.clear();
                g_lastRaceSexSliderValue = -1.0;
                g_lastRaceSexName.clear();
                g_lastRaceSexRaceDesc.clear();
                g_lastRaceSexSex = -1;
                g_lastRaceSexNameEntryActive = true;  // true au départ pour ne pas lire "Enter your name" à l'ouverture
                g_raceSexTickCount = 0;
                Speak(L"Character creation");
                QueueRaceSexRead();
                StartRaceSexPolling();
            } else {
                g_raceSexOpen.store(false);
                StopRaceSexPolling();
            }
        }

        if (e->menuName == RE::InventoryMenu::MENU_NAME) {
            if (e->opening) {
                g_tweenForeground.store(false);
                g_invOpen.store(true);
                g_lastInvCat.clear();
                g_lastInvItemAnnounce.clear();
                g_lastInvDesc.clear();
                Speak(L"Inventory open");
                QueueInventoryRead();
                StartInventoryPolling();
            } else {
                g_invOpen.store(false);
                StopInventoryPolling();
                if (g_tweenOpen.load()) g_tweenForeground.store(true);
            }
        }

        if (e->menuName == RE::ContainerMenu::MENU_NAME) {
            if (e->opening) {
                g_containerOpen.store(true);
                g_lastContainerCat.clear();
                g_lastContainerItemAnnounce.clear();
                g_lastContainerSide.clear();
                Speak(L"Container open");
                QueueContainerRead();
                StartContainerPolling();

                // Marque le conteneur/cadavre comme fouille des l'ouverture.
                // Meme si le joueur ne prend rien, il a deja "vu" le contenu,
                // donc le marquer "looted" est pertinent pour eviter de le
                // re-visiter. Le TESResetEvent nettoiera l'entree au respawn.
                if (auto* ref = GetActiveContainerMenuRef()) {
                    RE::FormID fid = ref->GetFormID();
                    LootTracker::GetSingleton()->MarkLooted(fid);
                    LOG("LootTracker: marked {:08X} as looted (ContainerMenu open)", fid);
                }
            } else {
                g_containerOpen.store(false);
                StopContainerPolling();
            }
        }

        // QuickLoot IE — menu custom (nom probable "LootMenu", à confirmer via logs)
        if (e->menuName == RE::BSFixedString(QUICKLOOT_MENU_NAME)) {
            LOG("QuickLoot: MenuOpenCloseEvent match, opening={}", e->opening);
            if (e->opening) {
                g_quickLootOpen.store(true);
                g_lastQuickLootItemAnnounce.clear();
                g_lastQuickLootItemName.clear();
                g_lastQuickLootItemCount = 0;
                g_lastQuickLootSelectedIndex = -1;
                // Pas de "Loot menu open" parlé car le menu apparaît dès que
                // le crosshair survole un conteneur, ce serait très verbeux.
                // On lit directement le premier item à la place.
                DiagnoseQuickLootNow();
                QueueQuickLootRead();
                StartQuickLootPolling();
            } else {
                g_quickLootOpen.store(false);
                StopQuickLootPolling();
            }
        }

        if (e->menuName == RE::BarterMenu::MENU_NAME) {
            LOG("BarterMenu event: opening={}", e->opening);
            if (e->opening) {
                g_barterOpen.store(true);
                g_lastBarterCat.clear();
                g_lastBarterItemAnnounce.clear();
                g_lastBarterSide.clear();
                g_lastBarterDesc.clear();
                Speak(L"Barter menu open");
                QueueBarterRead();
                StartBarterPolling();
            } else {
                g_barterOpen.store(false);
                StopBarterPolling();
            }
        }

        if (e->menuName == RE::CraftingMenu::MENU_NAME) {
            if (e->opening) {
                g_craftingOpen.store(true);
                g_lastCraftingCat.clear();
                g_lastCraftingItemAnnounce.clear();
                g_lastCraftingDesc.clear();
                g_craftingFirstReadDone = false;
                g_craftingModeDetected = false;
                g_craftingIsSimpleList = false;
                // Speak (pas SpeakQueue) : on veut couper toute annonce HUD en cours
                // (notification, tutoriel, furtivité) pour que l'ouverture du menu
                // soit clairement entendue. Le firstRead interne au menu utilise
                // ensuite SpeakQueue pour enchaîner après celle-ci.
                Speak(L"Crafting menu open");
                StartCraftingPolling();
            } else {
                g_craftingOpen.store(false);
                StopCraftingPolling();
            }
        }

        if (e->menuName == RE::Console::MENU_NAME) {
            if (e->opening) {
                g_consoleOpen.store(true);
                g_lastConsoleEntry.clear();
                g_lastConsoleMessage.clear();
                Speak(L"Console");
            } else {
                g_consoleOpen.store(false);
            }
        }

        if (e->menuName == RE::LoadingMenu::MENU_NAME) {
            if (e->opening) {
                g_loadingOpen.store(true);
                g_lastLoadingText.clear();
            } else {
                g_loadingOpen.store(false);
                // Après un fast travel / changement de cellule, le skeleton/shader
                // est encore instable pendant quelques secondes → même fenêtre de
                // crash que post-load. On bloque l'autowalk pendant 5s.
                AutoWalkArmSafetyCooldown(5000, "LoadingMenu close (cell change / fast travel)");
            }
        }

        if (e->menuName == RE::SleepWaitMenu::MENU_NAME) {
            if (e->opening) {
                g_sleepWaitOpen.store(true);
                g_lastSleepWaitHours.clear();
                g_lastSleepWaitTime.clear();
                auto* task = SKSE::GetTaskInterface();
                if (task) {
                    task->AddUITask([]() {
                        auto ui = RE::UI::GetSingleton();
                        if (!ui) return;
                        auto menu = ui->GetMenu(RE::SleepWaitMenu::MENU_NAME);
                        if (!menu) return;
                        RE::GFxMovieView* movie = menu->uiMovie.get();
                        if (!movie) return;
                        AnnounceSleepWaitOpen(movie);
                    });
                }
            } else {
                g_sleepWaitOpen.store(false);
            }
        }

        if (e->menuName == RE::GiftMenu::MENU_NAME) {
            if (e->opening) {
                g_giftOpen.store(true);
                g_lastGiftCat.clear();
                g_lastGiftItemAnnounce.clear();
                g_lastGiftItemName.clear();
                g_lastGiftItemCount = 0;
                Speak(L"Gift menu open");
                QueueGiftRead();
                StartGiftPolling();
            } else {
                g_giftOpen.store(false);
                StopGiftPolling();
            }
        }

        if (e->menuName == RE::TrainingMenu::MENU_NAME) {
            if (e->opening) {
                g_trainingOpen.store(true);
                g_lastTrainingCost.clear();
                auto* task = SKSE::GetTaskInterface();
                if (task) {
                    task->AddUITask([]() {
                        auto ui = RE::UI::GetSingleton();
                        if (!ui) return;
                        auto menu = ui->GetMenu(RE::TrainingMenu::MENU_NAME);
                        if (!menu) return;
                        RE::GFxMovieView* movie = menu->uiMovie.get();
                        if (!movie) return;
                        AnnounceTrainingOpen(movie);
                    });
                }
            } else {
                g_trainingOpen.store(false);
            }
        }

        if (e->menuName == RE::BookMenu::MENU_NAME) {
            if (e->opening) {
                g_bookOpen.store(true);
                auto* task = SKSE::GetTaskInterface();
                if (task) {
                    task->AddUITask([]() {
                        AnnounceBookContent();
                    });
                }
            } else {
                g_bookOpen.store(false);
            }
        }

        if (e->menuName == RE::JournalMenu::MENU_NAME) {
            if (e->opening) {
                g_journalOpen.store(true);
                g_lastJournalTab = -1;
                g_lastJournalTitle.clear();
                g_lastJournalDesc.clear();
                Speak(L"Journal open");
                QueueJournalRead();
                StartJournalPolling();
            } else {
                LOG("Journal CLOSE");
                // Lire les quêtes actives via UITask (le movie est encore vivant dans la UITask)
                auto* closeTask = SKSE::GetTaskInterface();
                if (closeTask) {
                    closeTask->AddUITask([]() {
                        ReadActiveQuestsFromJournal();
                    });
                }
                g_journalOpen.store(false);
                g_mcmOpen.store(false);
                StopJournalPolling();
            }
        }

        if (e->menuName == RE::MagicMenu::MENU_NAME) {
            if (e->opening) {
                g_tweenForeground.store(false);
                g_magicOpen.store(true);
                g_lastMagicCat.clear();
                g_lastMagicItem.clear();
                g_lastMagicEquipState = -1;
                g_lastMagicFavorite   = -1;
                g_lastMagicEffects.clear();
                Speak(L"Magic menu open");
                QueueMagicRead();
                StartMagicPolling();
            } else {
                g_magicOpen.store(false);
                StopMagicPolling();
                if (g_tweenOpen.load()) g_tweenForeground.store(true);
            }
        }

        if (e->menuName == RE::StatsMenu::MENU_NAME) {
            if (e->opening) {
                g_statsOpen.store(true);
                g_tweenForeground.store(false);
                StartStatsPoll();
            } else {
                g_statsOpen.store(false);
                StopStatsPoll();
                if (g_tweenOpen.load()) g_tweenForeground.store(true);
            }
        }

        if (e->menuName == RE::MapMenu::MENU_NAME) {
            if (e->opening) {
                g_tweenForeground.store(false);
                Speak(L"Map");
                OnMapOpen();
            } else {
                OnMapClose();
                if (g_tweenOpen.load()) g_tweenForeground.store(true);
                // CRITIQUE : si LB était maintenu sur la carte (LB+A → fast travel),
                // ne PAS restaurer g_savedControls (qui contient l'état minimal de la carte = 0x02).
                // Le jeu remet déjà les contrôles gameplay tout seul à la fermeture de la carte.
                // On efface juste notre flag pour que LB release ne tente pas de restaurer.
                if (g_controlsDisabled) {
                    g_controlsDisabled = false;
                    LOG("GAMEPAD: Map closed with LB held — clearing flag without restoring (game handles it)");
                }
                g_lbHeld.store(false);
            }
        }

        if (e->menuName == RE::TweenMenu::MENU_NAME) {
            if (e->opening) {
                g_tweenOpen.store(true);
                g_tweenForeground.store(true);
                g_lastTweenFrame.store(-1);
                g_tweenLevelAnnounced.store(false);
                Speak(L"Cross menu");
                StartTweenPolling();
            } else {
                g_tweenOpen.store(false);
                g_tweenForeground.store(false);
                StopTweenPolling();
            }
        }

        if (e->menuName == RE::LevelUpMenu::MENU_NAME) {
            if (e->opening) {
                g_levelUpOpen.store(true);
                g_levelUpSelection.store(0);
                Speak(L"Level gained! Choose your improvement.");
                AnnounceLevelUpSelection(true);
            } else {
                g_levelUpOpen.store(false);
                g_statsPrevKey.clear(); // re-announce current skill after level-up choice
            }
        }

        if (e->menuName == RE::TutorialMenu::MENU_NAME) {
            if (e->opening) {
                QueueTutorialRead();
            }
        }

        if (e->menuName == RE::FavoritesMenu::MENU_NAME) {
            if (e->opening) {
                g_favOpen.store(true);
                g_lastFavItemAnnounce.clear();
                g_lastFavCategory.clear();
                Speak(L"Favorites");
                QueueFavRead();
                StartFavPolling();
            } else {
                g_favOpen.store(false);
                StopFavPolling();
            }
        }

        if (e->menuName == UILIST_MENU_NAME) {
            if (e->opening) {
                g_uiListMenuOpen.store(true);
                g_lastUIListItem.clear();
                Speak(L"Activate menu");
                StartUIListPolling();
            } else {
                g_uiListMenuOpen.store(false);
                StopUIListPolling();
            }
        }

        // Menu custom du mod "Skyrim Character Sheet" (ouverture via touche U).
        // Affiche stats du joueur sur 6 onglets (Player/Attack/Defence/Magic/
        // Warrior/Thief) cycles par touche N.
        // Menus du mod "Skyrim Character Sheet" (ShowStats, ShowFactions).
        // Le mod cycle entre les deux via la touche N (fermeture d'un menu,
        // ouverture du suivant). On traite les deux avec le meme pipeline.
        if (e->menuName == CHARSHEET_MENU_SHOWSTATS ||
            e->menuName == CHARSHEET_MENU_SHOWFACTIONS) {
            if (e->opening) {
                bool wasOpen = g_charSheetOpen.load();
                g_charSheetOpen.store(true);
                ResetCharSheetState();
                QueueCharSheetOpen();
                if (!wasOpen) StartCharSheetPoll();  // un seul poll suffit entre ShowStats/ShowFactions
            } else {
                // Attendre un tick pour voir si un autre menu (ShowFactions
                // ou ShowStats) s'ouvre dans la foulee. Si oui, on garde
                // g_charSheetOpen=true. Sinon on ferme.
                auto* task = SKSE::GetTaskInterface();
                if (task) {
                    task->AddUITask([]() {
                        if (!GetActiveCharSheetDef()) {
                            g_charSheetOpen.store(false);
                            StopCharSheetPoll();
                        }
                    });
                }
            }
        }

        if (e->menuName == RE::MessageBoxMenu::MENU_NAME) {
            if (e->opening) {
                g_msgBoxOpen.store(true);
                QueueMsgBoxAnnounce();
            } else {
                g_msgBoxOpen.store(false);
            }
        }

        if (e->menuName == RE::MainMenu::MENU_NAME || e->menuName == RE::TitleSequenceMenu::MENU_NAME) {
            if (e->opening) {
                g_mainOpen.store(true);
                g_lastMainItem.clear();
                g_lastMainPathInfo.clear();
                g_mainLockedPath.clear();
                g_mainLockedMenuName.clear();
                g_lastSaveLoadItem.clear();
                g_lastConfirmText.clear();

                SpeakQueue(L"Main menu open");
                QueueMainMenuRead();
                StartMainMenuPolling();
            } else {
                g_mainOpen.store(false);
                StopMainMenuPolling();
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

static MenuListener g_menuListener;

static void RegisterMenuListener() {
    auto ui = RE::UI::GetSingleton();
    if (ui) {
        ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_menuListener);
    }
}

// g_gamepadNeedsRemap déclaré dans common.h
// g_lbHeld, g_lbWasModifier, g_gamepadDetected, g_gamepadRemapCount, g_controlsDisabled
// déclarés plus haut (avant MenuListener)
static ULONGLONG g_lastStickUpTime = 0;                 // throttle stick droit haut
static ULONGLONG g_lastStickDownTime = 0;               // throttle stick droit bas
static ULONGLONG g_lastStickLeftTime = 0;               // throttle stick droit gauche
static ULONGLONG g_lastStickRightTime = 0;              // throttle stick droit droite
static constexpr ULONGLONG STICK_REPEAT_MS = 300;       // délai entre chaque déclenchement stick
static constexpr float STICK_THRESHOLD = 0.5f;          // seuil de déclenchement du stick

// Helper : vérifie si un menu est ouvert (évite duplication)
static bool IsAnyMenuOpen() {
    return g_invOpen.load() || g_containerOpen.load() || g_barterOpen.load() || g_craftingOpen.load() ||
           g_journalOpen.load() || g_magicOpen.load() || g_mainOpen.load() || g_dialogueOpen.load() ||
           g_raceSexOpen.load() || g_tweenOpen.load() || g_statsOpen.load() || g_favOpen.load() ||
           g_msgBoxOpen.load() || g_uiListMenuOpen.load() || g_mapOpen.load();
}

// Remap sprint de LB vers LS et sneak de LS vers rien (géré par LB+LS dans notre code)
// Dans le contexte carte, désactive les boutons configurables qui ont une action native
// Retourne true si un remap a été effectué (false = déjà OK)
static bool RemapGamepadControls() {
    auto* controlMap = RE::ControlMap::GetSingleton();
    if (!controlMap) return false;

    bool changed = false;
    auto targetSprint = static_cast<std::uint16_t>(RE::BSWin32GamepadDevice::Key::kLeftThumb);
    constexpr auto kMapCtxId = static_cast<size_t>(RE::UserEvents::INPUT_CONTEXT_ID::kMap);

    // Boutons que l'utilisateur a assignés à des combos LB+X sur la carte
    // → doivent être désactivés dans le contexte carte pour éviter les conflits
    const std::uint16_t mapBlockedKeys[] = {
        static_cast<std::uint16_t>(GpIndexToCode(g_gpIdxScanNext.load())),
        static_cast<std::uint16_t>(GpIndexToCode(g_gpIdxScanPrev.load())),
        static_cast<std::uint16_t>(GpIndexToCode(g_gpIdxScanAnnounce.load())),
        static_cast<std::uint16_t>(GpIndexToCode(g_gpIdxMapSetRef.load())),
        static_cast<std::uint16_t>(GpIndexToCode(g_gpIdxPrimary.load())),
        static_cast<std::uint16_t>(GpIndexToCode(g_gpIdxVitals.load())),
    };

    for (size_t ctxIdx = 0; ctxIdx < RE::UserEvents::INPUT_CONTEXT_ID::kTotal; ctxIdx++) {
        auto* ctx = controlMap->controlMap[ctxIdx];
        if (!ctx) continue;
        for (auto& mapping : ctx->deviceMappings[RE::INPUT_DEVICE::kGamepad]) {
            // Sprint → LS click (tous contextes)
            if (mapping.eventID == RE::BSFixedString("Sprint") && mapping.inputKey != targetSprint) {
                LOG("GAMEPAD: Remapping Sprint 0x{:04X} → 0x{:04X} (LS) ctx={}", mapping.inputKey, targetSprint, ctxIdx);
                mapping.inputKey = targetSprint;
                changed = true;
            }
            // Sneak → désactivé (tous contextes)
            if (mapping.eventID == RE::BSFixedString("Sneak") && mapping.inputKey != 0xFFFF) {
                LOG("GAMEPAD: Remapping Sneak 0x{:04X} → disabled ctx={}", mapping.inputKey, ctxIdx);
                mapping.inputKey = 0xFFFF;
                changed = true;
            }
            // Dans les contextes d'inventaire / item menus : désactiver LB vanilla
            // pour éviter les conflits avec notre modificateur. LB est utilisé par
            // Skyrim vanilla pour changer de catégorie/page dans ces menus, ce qui
            // entre en conflit avec notre combo LB+X. On bloque juste LB, RB reste
            // actif pour la navigation vanilla si besoin.
            if (ctxIdx == static_cast<size_t>(RE::UserEvents::INPUT_CONTEXT_ID::kInventory) ||
                ctxIdx == static_cast<size_t>(RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu) ||
                ctxIdx == static_cast<size_t>(RE::UserEvents::INPUT_CONTEXT_ID::kFavorites)) {
                constexpr auto kLBCode = static_cast<std::uint16_t>(
                    RE::BSWin32GamepadDevice::Key::kLeftShoulder);
                if (mapping.inputKey == kLBCode) {
                    LOG("GAMEPAD ITEM: Disabling LB event '{}' in ctx={}",
                        mapping.eventID.c_str(), ctxIdx);
                    mapping.inputKey = 0xFFFF;
                    changed = true;
                }
            }

            // Dans le contexte CARTE uniquement : désactiver les boutons utilisés par nos combos
            if (ctxIdx == kMapCtxId) {
                for (std::uint16_t blockedKey : mapBlockedKeys) {
                    if (blockedKey != 0xFFFF && mapping.inputKey == blockedKey) {
                        LOG("GAMEPAD MAP: Remapping event '{}' from 0x{:04X} → disabled",
                            mapping.eventID.c_str(), blockedKey);
                        mapping.inputKey = 0xFFFF;
                        changed = true;
                        break;
                    }
                }

                // Désactiver la rotation/inclinaison de la caméra de carte via le
                // joystick droit. Par défaut, le RStick fait bouger la caméra sur
                // la carte, ce qui change le marqueur survolé et provoque des
                // lectures NVDA parasites des villes/POI que l'utilisateur veut
                // ignorer (il utilise notre scanner pour naviguer dans les
                // marqueurs). Nos propres combos RStick (cycle des filtres) sont
                // gérées en amont dans notre handler d'input, elles ne passent
                // pas par le ControlMap et restent donc fonctionnelles.
                if (mapping.eventID == RE::BSFixedString("MapLookMode") &&
                    mapping.inputKey != 0xFFFF) {
                    LOG("GAMEPAD MAP: Disabling MapLookMode (was 0x{:04X})", mapping.inputKey);
                    mapping.inputKey = 0xFFFF;
                    changed = true;
                }
                if (mapping.eventID == RE::BSFixedString("Rotate") &&
                    mapping.inputKey != 0xFFFF) {
                    LOG("GAMEPAD MAP: Disabling Rotate (was 0x{:04X})", mapping.inputKey);
                    mapping.inputKey = 0xFFFF;
                    changed = true;
                }
            }
        }

        // Dans le contexte CARTE uniquement : désactiver la touche clavier P vanilla
        // (PlacePlayerMarker) pour qu'elle ne rentre pas en conflit avec notre
        // MapPlaceCustomMarker. Le jeu n'essaiera plus de poser son propre marqueur
        // à la position du curseur — seul notre système répond à P.
        if (ctxIdx == kMapCtxId) {
            for (auto& mapping : ctx->deviceMappings[RE::INPUT_DEVICE::kKeyboard]) {
                if (mapping.eventID == RE::BSFixedString("PlacePlayerMarker") &&
                    mapping.inputKey != 0xFFFF) {
                    LOG("KEYBOARD MAP: Disabling PlacePlayerMarker (was 0x{:04X})", mapping.inputKey);
                    mapping.inputKey = 0xFFFF;
                    changed = true;
                }
            }
        }
    }
    return changed;
}

// Toggle sneak programmatiquement (LB + LS click)
static void ToggleSneakGamepad() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    bool wasSneaking = player->IsSneaking();
    // AsActorState() gère automatiquement l'offset SSE/AE
    auto* state = player->AsActorState();
    if (state) {
        state->actorState1.sneaking = wasSneaking ? 0 : 1;
    }
    Speak(wasSneaking ? L"Standing" : L"Sneaking");
    LOG("GAMEPAD: ToggleSneak → {}", wasSneaking ? "Standing" : "Sneaking");
}

// ---------------- Input listener ----------------

class InputListener : public RE::BSTEventSink<RE::InputEvent*> {
public:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) {
        if (!a_event || !*a_event) return RE::BSEventNotifyControl::kContinue;

        // Autowalk cancel check : si le joueur fait un input de mouvement
        // (WASD, Space, Escape, fleches, stick gauche, A/B/Start...) pendant
        // un autowalk actif, on l'annule. Style f4access : pas de polling
        // thread, on reagit aux events reels.
        AutoWalkInputUpdate(a_event);

        // Remap gamepad demandé par le HUD hook (premières secondes après chargement)
        if (g_gamepadNeedsRemap.exchange(false)) {
            RemapGamepadControls();
        }

        for (auto e = *a_event; e; e = e->next) {
            // --- Stick droit : catégories/filtres quand LB maintenu ---
            if (e->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
                auto* stick = static_cast<RE::ThumbstickEvent*>(e);
                if (stick && stick->IsRight() && g_lbHeld.load()) {
                    ULONGLONG now = GetTickCount64();
                    bool mapMode = g_mapOpen.load();
                    bool canAct = mapMode || !IsAnyMenuOpen();

                    if (canAct) {
                        // Scanner : Stick droit droite/gauche = catégorie
                        // Map     : Stick droit droite/gauche = SOUS-filtre (Cities, Towns...)
                        if (stick->xValue > STICK_THRESHOLD && (now - g_lastStickRightTime) > STICK_REPEAT_MS) {
                            g_lastStickRightTime = now;
                            g_lbWasModifier.store(true);
                            if (mapMode) {
                                LOG("GAMEPAD MAP: RStick RIGHT → MapCycleSubFilter (next)");
                                MapCycleSubFilter();
                            } else {
                                LOG("GAMEPAD: RStick RIGHT → ScannerNextCategory");
                                ScannerNextCategory();
                            }
                        } else if (stick->xValue < -STICK_THRESHOLD && (now - g_lastStickLeftTime) > STICK_REPEAT_MS) {
                            g_lastStickLeftTime = now;
                            g_lbWasModifier.store(true);
                            if (mapMode) {
                                LOG("GAMEPAD MAP: RStick LEFT → MapCyclePrevSubFilter");
                                MapCyclePrevSubFilter();
                            } else {
                                LOG("GAMEPAD: RStick LEFT → ScannerPrevCategory");
                                ScannerPrevCategory();
                            }
                        }
                        // Scanner : Stick droit haut/bas = sous-catégorie
                        // Map     : Stick droit haut/bas = filtre principal (Discovered, Undiscovered...)
                        //           Bas = next → Discovered en premier (après All qui est l'état initial)
                        if (stick->yValue > STICK_THRESHOLD && (now - g_lastStickUpTime) > STICK_REPEAT_MS) {
                            g_lastStickUpTime = now;
                            g_lbWasModifier.store(true);
                            if (mapMode) {
                                LOG("GAMEPAD MAP: RStick UP → MapCyclePrevFilter");
                                MapCyclePrevFilter();
                            } else {
                                LOG("GAMEPAD: RStick UP → ScannerCycleSubcategory");
                                ScannerCycleSubcategory();
                            }
                        } else if (stick->yValue < -STICK_THRESHOLD && (now - g_lastStickDownTime) > STICK_REPEAT_MS) {
                            g_lastStickDownTime = now;
                            g_lbWasModifier.store(true);
                            if (mapMode) {
                                LOG("GAMEPAD MAP: RStick DOWN → MapCycleFilter (next)");
                                MapCycleFilter();
                            } else {
                                LOG("GAMEPAD: RStick DOWN → ScannerCycleSubcategory");
                                ScannerCycleSubcategory();
                            }
                        }
                    }
                }
                // Stick gauche : annuler autowalk OU naviguer le menu en croix
                if (stick && stick->IsLeft()) {
                    float magnitude = std::sqrt(stick->xValue * stick->xValue + stick->yValue * stick->yValue);
                    if (magnitude > STICK_THRESHOLD) {
                        // Tween menu (menu en croix) ouvert → annoncer la direction
                        if (g_tweenForeground.load(std::memory_order_relaxed)) {
                            int targetFrame = 0;
                            // Déterminer la direction dominante
                            if (std::abs(stick->yValue) > std::abs(stick->xValue)) {
                                targetFrame = stick->yValue > 0 ? 2 : 5;  // haut=2, bas=5
                            } else {
                                targetFrame = stick->xValue < 0 ? 3 : 4;  // gauche=3, droite=4
                            }
                            int prev = g_lastTweenFrame.exchange(targetFrame);
                            if (prev != targetFrame) {
                                LOG("GAMEPAD: LStick tween direction frame={}", targetFrame);
                                AnnounceTweenNavKey(targetFrame);
                            }
                        }
                        // Sinon, annuler l'autowalk
                        else if (g_autoWalking.load()) {
                            LOG("GAMEPAD: Left stick movement during autowalk → stopping");
                            Speak(L"Stopping");
                            StopAutoWalk();
                        }
                    }
                }
                continue;
            }

            if (e->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) continue;

            auto* btn = e->AsButtonEvent();
            if (!btn) continue;

            // === DIAGNOSTIC SHIFT FREEZE ===
            // Loggue les inputs clavier qui nous interessent (Home, PageUp/Dn, X)
            // pour voir si Shift+key arrive bien dans le listener
            if (btn->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                const uint32_t diagCode = btn->GetIDCode();
                if (diagCode == 199 || diagCode == 201 || diagCode == 207 ||
                    diagCode == 209 || diagCode == 45) {
                    const bool diagShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                    const bool diagAlt   = (GetAsyncKeyState(VK_MENU)  & 0x8000) != 0;
                    LOG("InputDiag: kb dxCode={} isDown={} isUp={} shift={} alt={}",
                        diagCode, btn->IsDown(), btn->IsUp(), diagShift, diagAlt);
                }
            }

            // --- Gamepad : gestion LB comme modificateur ---
            if (btn->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                auto gpCode = btn->GetIDCode();

                // Premier événement gamepad
                if (!g_gamepadDetected.exchange(true)) {
                    LOG("GAMEPAD: First gamepad event received");
                }
                // Vérifier et ré-appliquer le remap si le jeu l'a écrasé (les 20 premiers events)
                if (g_gamepadRemapCount.load() < 20) {
                    if (RemapGamepadControls()) {
                        LOG("GAMEPAD: Remap re-applied (attempt {})", g_gamepadRemapCount.load());
                    }
                    g_gamepadRemapCount++;
                }

                // Tracker l'état de LB
                if (gpCode == RE::BSWin32GamepadDevice::Key::kLeftShoulder) {
                    // Sauvegarder les états AVANT la neutralisation car on va
                    // potentiellement modifier l'event ci-dessous.
                    const bool lbDown = btn->IsDown();
                    const bool lbUp   = btn->IsUp();

                    // Dans un menu item : "manger" l'event LB en mettant sa value
                    // et heldDownSecs à 0. Le moteur Scaleform voit alors un event
                    // neutre et ne déclenche pas son action vanilla (saut dans la
                    // liste, changement de page, etc.). Notre propre tracking de
                    // LB reste basé sur lbDown/lbUp sauvegardés juste au-dessus.
                    // Le const sur a_event est un artefact de l'API SKSE — les
                    // plugins peuvent modifier l'event en place pour "absorber"
                    // un input.
                    const bool inItemMenu =
                        g_invOpen.load(std::memory_order_relaxed) ||
                        g_containerOpen.load(std::memory_order_relaxed) ||
                        g_barterOpen.load(std::memory_order_relaxed) ||
                        g_favOpen.load(std::memory_order_relaxed) ||
                        g_giftOpen.load(std::memory_order_relaxed) ||
                        g_craftingOpen.load(std::memory_order_relaxed) ||
                        g_magicOpen.load(std::memory_order_relaxed);
                    if (inItemMenu) {
                        auto* mutableBtn = const_cast<RE::ButtonEvent*>(btn);
                        mutableBtn->value = 0.0f;
                        mutableBtn->heldDownSecs = 0.0f;
                    }

                    if (lbDown) {
                        g_lbHeld.store(true);
                        g_lbWasModifier.store(false);
                        g_lbLastSeenPressedMs.store(PluginNowMs());  // pour le stuck watcher
                        // Désactiver les contrôles pendant LB pour que les boutons
                        // de combo (A/B/X/Y/etc) ne déclenchent pas leur action
                        // vanilla en parallèle de notre combo :
                        // - Hors menus : masque gameplay (movement/fighting/...)
                        // - Dans un menu item (inventaire/conteneur/marchand/favoris) :
                        //   masque kMenu pour bloquer l'équipement, les favoris, etc.
                        // - Sur la carte : on ne touche à rien (la carte a son propre
                        //   remap ControlMap via RemapGamepadControls)
                        {
                            auto* cm = RE::ControlMap::GetSingleton();
                            if (cm && !g_controlsDisabled) {
                                g_savedControls = cm->enabledControls.underlying();
                                std::uint32_t mask = 0;
                                if (!IsAnyMenuOpen()) {
                                    mask = static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kMovement) |
                                           static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kActivate) |
                                           static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kFighting) |
                                           static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kSneaking) |
                                           static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kJumping) |
                                           static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kMainFour) |
                                           static_cast<std::uint32_t>(RE::UserEvents::USER_EVENT_FLAG::kPOVSwitch);
                                }
                                if (mask != 0) {
                                    cm->enabledControls = static_cast<RE::UserEvents::USER_EVENT_FLAG>(cm->enabledControls.underlying() & ~mask);
                                    g_controlsDisabled = true;
                                    LOG("GAMEPAD: LB pressed — controls masked (saved=0x{:08X}, mask=0x{:08X})", g_savedControls, mask);
                                } else {
                                    LOG("GAMEPAD: LB pressed (no mask applied)");
                                }
                            }

                            // Dans un menu item (inventaire/conteneur/marchand/gift/
                            // favoris/crafting/magie) : désactiver dynamiquement
                            // l'event YButton dans le ControlMap du contexte kItemMenu
                            // pour que la combo LB+Y n'active pas le favori vanilla
                            // en parallèle de nos stats. Sauvegardé ici, restauré
                            // au relâchement de LB.
                            // Note : le flag kMenu du enabledControls ne suffit PAS
                            // à bloquer YButton dans les menus — le moteur traite
                            // YButton via le ControlMap directement (SkyUI le déclare
                            // dans CONTEXT_ITEMMENU). Seule la désactivation du
                            // mapping lui-même marche.
                            if (cm && !g_itemMenuYButtonDisabled &&
                                (g_invOpen.load(std::memory_order_relaxed) ||
                                 g_containerOpen.load(std::memory_order_relaxed) ||
                                 g_barterOpen.load(std::memory_order_relaxed) ||
                                 g_favOpen.load(std::memory_order_relaxed) ||
                                 g_giftOpen.load(std::memory_order_relaxed) ||
                                 g_craftingOpen.load(std::memory_order_relaxed) ||
                                 g_magicOpen.load(std::memory_order_relaxed))) {
                                constexpr auto kItemMenuCtxId = static_cast<size_t>(
                                    RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu);
                                auto* itemCtx = cm->controlMap[kItemMenuCtxId];
                                if (itemCtx) {
                                    for (auto& mapping : itemCtx->deviceMappings[RE::INPUT_DEVICE::kGamepad]) {
                                        if (mapping.eventID == RE::BSFixedString("YButton")) {
                                            g_savedItemMenuYButtonKey = mapping.inputKey;
                                            mapping.inputKey = 0xFFFF;
                                            g_itemMenuYButtonDisabled = true;
                                            LOG("GAMEPAD: LB pressed — YButton disabled in kItemMenu (saved=0x{:04X})",
                                                g_savedItemMenuYButtonKey);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    } else if (lbUp) {
                        g_lbHeld.store(false);
                        auto* cm = RE::ControlMap::GetSingleton();
                        if (g_controlsDisabled) {
                            if (cm) {
                                cm->enabledControls = static_cast<RE::UserEvents::USER_EVENT_FLAG>(g_savedControls);
                            }
                            g_controlsDisabled = false;
                            LOG("GAMEPAD: LB released — controls restored (0x{:08X})", g_savedControls);
                        } else {
                            LOG("GAMEPAD: LB released");
                        }

                        // Restaurer le mapping YButton si on l'avait désactivé.
                        // On ne reset les variables QUE si la restauration a
                        // effectivement pu avoir lieu (cm + contexte + mapping
                        // trouvés). Sinon on garde l'état pour retenter au
                        // prochain relâchement de LB.
                        if (cm && g_itemMenuYButtonDisabled) {
                            constexpr auto kItemMenuCtxId = static_cast<size_t>(
                                RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu);
                            auto* itemCtx = cm->controlMap[kItemMenuCtxId];
                            if (itemCtx) {
                                bool restored = false;
                                for (auto& mapping : itemCtx->deviceMappings[RE::INPUT_DEVICE::kGamepad]) {
                                    if (mapping.eventID == RE::BSFixedString("YButton")) {
                                        mapping.inputKey = g_savedItemMenuYButtonKey;
                                        LOG("GAMEPAD: LB released — YButton restored to 0x{:04X}",
                                            g_savedItemMenuYButtonKey);
                                        restored = true;
                                        break;
                                    }
                                }
                                if (restored) {
                                    g_itemMenuYButtonDisabled = false;
                                    g_savedItemMenuYButtonKey = 0xFFFF;
                                }
                            }
                        }
                    }
                    continue;
                }

                // Tween menu (menu en croix) ouvert : D-pad navigue les directions
                if (g_tweenForeground.load(std::memory_order_relaxed) && btn->IsDown() && !g_lbHeld.load()) {
                    int targetFrame = 0;
                    if      (gpCode == RE::BSWin32GamepadDevice::Key::kUp)    targetFrame = 2;
                    else if (gpCode == RE::BSWin32GamepadDevice::Key::kLeft)  targetFrame = 3;
                    else if (gpCode == RE::BSWin32GamepadDevice::Key::kRight) targetFrame = 4;
                    else if (gpCode == RE::BSWin32GamepadDevice::Key::kDown)  targetFrame = 5;
                    if (targetFrame > 0) {
                        int prev = g_lastTweenFrame.exchange(targetFrame);
                        if (prev != targetFrame) {
                            LOG("GAMEPAD: D-pad tween direction frame={}", targetFrame);
                            AnnounceTweenNavKey(targetFrame);
                        }
                    }
                }

                // Level Up Menu ouvert : D-pad navigue entre Santé/Magicka/Vigueur
                // Comme on maintient notre propre sélection (g_levelUpSelection),
                // on traite le D-pad nous-mêmes et on vocalise à chaque changement.
                // A button = confirmer le choix actuel.
                if (g_levelUpOpen.load(std::memory_order_relaxed) && btn->IsDown() && !g_lbHeld.load()) {
                    const bool prev = (gpCode == RE::BSWin32GamepadDevice::Key::kLeft) ||
                                      (gpCode == RE::BSWin32GamepadDevice::Key::kUp);
                    const bool next = (gpCode == RE::BSWin32GamepadDevice::Key::kRight) ||
                                      (gpCode == RE::BSWin32GamepadDevice::Key::kDown);
                    if (prev) {
                        g_levelUpSelection.store((g_levelUpSelection.load() + 2) % 3);
                        LOG("GAMEPAD: LevelUp prev → selection={}", g_levelUpSelection.load());
                        AnnounceLevelUpSelection();
                        continue;
                    }
                    if (next) {
                        g_levelUpSelection.store((g_levelUpSelection.load() + 1) % 3);
                        LOG("GAMEPAD: LevelUp next → selection={}", g_levelUpSelection.load());
                        AnnounceLevelUpSelection();
                        continue;
                    }
                    if (gpCode == RE::BSWin32GamepadDevice::Key::kA) {
                        LOG("GAMEPAD: LevelUp A → confirm selection={}", g_levelUpSelection.load());
                        QueueConfirmLevelUp();
                        continue;
                    }
                }

                // MessageBox ouvert : D-pad navigue entre les choix (oui/non, etc.)
                // Comme on maintient notre propre sélection (g_msgBoxSelectedBtn),
                // on traite le D-pad nous-mêmes et on vocalise à chaque changement.
                // A = confirmer, B = annuler (équivalents Enter/Escape au clavier).
                if (g_msgBoxOpen.load(std::memory_order_relaxed) && btn->IsDown() && !g_lbHeld.load()) {
                    const bool mbUp   = (gpCode == RE::BSWin32GamepadDevice::Key::kUp) ||
                                        (gpCode == RE::BSWin32GamepadDevice::Key::kLeft);
                    const bool mbDown = (gpCode == RE::BSWin32GamepadDevice::Key::kDown) ||
                                        (gpCode == RE::BSWin32GamepadDevice::Key::kRight);
                    if (mbUp) {
                        int sel = g_msgBoxSelectedBtn.load();
                        if (sel > 0) g_msgBoxSelectedBtn.store(sel - 1);
                        LOG("GAMEPAD: MsgBox prev → selection={}", g_msgBoxSelectedBtn.load());
                        QueueAnnounceMsgBoxBtn();
                        continue;
                    }
                    if (mbDown) {
                        int sel   = g_msgBoxSelectedBtn.load();
                        int count = g_msgBoxBtnCount.load();
                        if (sel < count - 1) g_msgBoxSelectedBtn.store(sel + 1);
                        LOG("GAMEPAD: MsgBox next → selection={}", g_msgBoxSelectedBtn.load());
                        QueueAnnounceMsgBoxBtn();
                        continue;
                    }
                    if (gpCode == RE::BSWin32GamepadDevice::Key::kA) {
                        LOG("GAMEPAD: MsgBox A → confirm selection={}", g_msgBoxSelectedBtn.load());
                        QueueMsgBoxPress();
                        continue;
                    }
                    if (gpCode == RE::BSWin32GamepadDevice::Key::kB) {
                        LOG("GAMEPAD: MsgBox B → cancel");
                        QueueMsgBoxCancel();
                        continue;
                    }
                }

                // Charger les mappings configurables (une fois par frame suffit)
                const auto keyNext     = GpIndexToCode(g_gpIdxScanNext.load());
                const auto keyPrev     = GpIndexToCode(g_gpIdxScanPrev.load());
                const auto keyAnnounce = GpIndexToCode(g_gpIdxScanAnnounce.load());
                const auto keyMapSetRef= GpIndexToCode(g_gpIdxMapSetRef.load());
                const auto keyPrimary  = GpIndexToCode(g_gpIdxPrimary.load());
                const auto keyTeleport = GpIndexToCode(g_gpIdxTeleport.load());
                const auto keyVitals   = GpIndexToCode(g_gpIdxVitals.load());
                const auto keySneak    = GpIndexToCode(g_gpIdxSneak.load());
                const auto keyPOV      = GpIndexToCode(g_gpIdxPOV.load());
                const auto keyLockEnemy= GpIndexToCode(g_gpIdxLockEnemy.load());

                // Si LB est maintenu, intercepter les combos
                if (g_lbHeld.load() && btn->IsDown()) {
                    g_lbWasModifier.store(true);

                    // --- Carte ouverte : combos spécifiques map ---
                    if (g_mapOpen.load()) {
                        if (gpCode == keyNext) {
                            LOG("GAMEPAD MAP: LB+Next → MapNextMarker");
                            MapNextMarker();
                            continue;
                        }
                        if (gpCode == keyPrev) {
                            LOG("GAMEPAD MAP: LB+Prev → MapPrevMarker");
                            MapPrevMarker();
                            continue;
                        }
                        if (gpCode == keyAnnounce) {
                            LOG("GAMEPAD MAP: LB+Announce → MapAnnounceDetails");
                            MapAnnounceDetails();
                            continue;
                        }
                        if (gpCode == keyMapSetRef) {
                            LOG("GAMEPAD MAP: LB+SetRef → MapSetReference");
                            MapSetReference();
                            continue;
                        }
                        if (gpCode == keyPrimary) {
                            LOG("GAMEPAD MAP: LB+Primary → MapFastTravel");
                            MapFastTravel();
                            continue;
                        }
                        if (gpCode == keyVitals) {
                            LOG("GAMEPAD MAP: LB+Vitals → MapPlaceCustomMarker");
                            MapPlaceCustomMarker();
                            continue;
                        }
                        // Les autres boutons ne sont pas interceptés sur la carte
                        continue;
                    }

                    // --- Autres menus ouverts : whitelist LB+Y pour les stats contextuelles ---
                    // Dans les menus d'inventaire/conteneur/marchand, on autorise
                    // LB+Y (Vitals) pour lire or/poids (même comportement que la
                    // touche H clavier). Les autres combos sont ignorées.
                    if (IsAnyMenuOpen()) {
                        const bool inItemMenu = g_invOpen.load(std::memory_order_relaxed) ||
                                                g_containerOpen.load(std::memory_order_relaxed) ||
                                                g_barterOpen.load(std::memory_order_relaxed) ||
                                                g_giftOpen.load(std::memory_order_relaxed);
                        if (inItemMenu && gpCode == keyVitals) {
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
                            continue;
                        }
                        LOG("GAMEPAD: LB+0x{:04X} ignored (menu open)", gpCode);
                        continue;
                    }

                    // --- Hors menu : combos scanner ---
                    if (gpCode == keyNext) {
                        LOG("GAMEPAD: LB+Next → ScannerNextObject");
                        ScannerNextObject();
                        continue;
                    }
                    if (gpCode == keyPrev) {
                        LOG("GAMEPAD: LB+Prev → ScannerPrevObject");
                        ScannerPrevObject();
                        continue;
                    }
                    if (gpCode == keyAnnounce) {
                        LOG("GAMEPAD: LB+Announce → ScannerAnnounceCurrent");
                        ScannerAnnounceCurrent();
                        continue;
                    }
                    if (gpCode == keyPrimary) {
                        LOG("GAMEPAD: LB+Primary → ToggleAutoWalk");
                        ToggleAutoWalk();
                        continue;
                    }
                    if (gpCode == keyVitals) {
                        // Contextuel comme la touche H au clavier :
                        // - en inventaire/conteneur/marchand → or + poids
                        // - en jeu → vitals (HP/magicka/stamina)
                        if (g_invOpen.load(std::memory_order_relaxed)) {
                            LOG("GAMEPAD: LB+Vitals → AnnounceInventoryStats");
                            AnnounceInventoryStats();
                        } else if (g_containerOpen.load(std::memory_order_relaxed)) {
                            LOG("GAMEPAD: LB+Vitals → AnnounceContainerStats");
                            AnnounceContainerStats();
                        } else if (g_barterOpen.load(std::memory_order_relaxed)) {
                            LOG("GAMEPAD: LB+Vitals → AnnounceBarterStats");
                            AnnounceBarterStats();
                        } else {
                            LOG("GAMEPAD: LB+Vitals → AnnouncePlayerVitals");
                            AnnouncePlayerVitals();
                        }
                        continue;
                    }
                    if (gpCode == keyTeleport) {
                        LOG("GAMEPAD: LB+Teleport → ScannerTeleport");
                        ScannerTeleport();
                        continue;
                    }
                    if (gpCode == keySneak) {
                        LOG("GAMEPAD: LB+Sneak → ToggleSneak");
                        ToggleSneakGamepad();
                        continue;
                    }
                    if (gpCode == keyPOV) {
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
                        continue;
                    }

                    LOG("GAMEPAD: LB+0x{:04X} — combo non reconnu", gpCode);
                }

                // R3 (RightThumb) sans LB, hors menu : le moteur Skyrim a R3
                // mappe sur kPOVSwitch par defaut, ce qui change la vue mais sans
                // annonce -> joueur aveugle ne sait pas qu'il a change de POV
                // (bug signale sur v1.5).
                //
                // Solution style "F au clavier" : on fait le toggle POV nous-memes
                // avec annonce, et on consomme l'event pour que le moteur ne
                // re-toggle pas par-dessus.
                //
                // Si R3 est aussi mappe sur LockEnemy dans le MCM (cas par defaut),
                // on lance aussi le lock. Ainsi R3 fait : lock enemy + annonce POV.
                if (gpCode == RE::BSWin32GamepadDevice::Keys::kRightThumb &&
                    btn->IsDown() && !g_lbHeld.load() && !IsAnyMenuOpen()) {
                    // Lock enemy si R3 est mappe dessus dans le MCM
                    if (gpCode == keyLockEnemy) {
                        LOG("GAMEPAD: R3 solo -> LockNearestEnemy");
                        LockNearestEnemy();
                    }
                    // Toggle POV + annonce (remplace le comportement vanilla)
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
                    // Consommer l'event pour empecher le moteur de re-toggler.
                    auto* mutableBtn = const_cast<RE::ButtonEvent*>(btn);
                    mutableBtn->value = 0.0f;
                    mutableBtn->heldDownSecs = 0.0f;
                    LOG("GAMEPAD: R3 solo -> manual POV toggle + announce");
                    continue;
                }

                // Lock enemy (appui seul sans LB) — si mappe sur un autre bouton
                // que R3 (ex: D-pad comme dans le feedback joueur v1.5).
                if (gpCode == keyLockEnemy && btn->IsDown() && !g_lbHeld.load()) {
                    if (!IsAnyMenuOpen()) {
                        LOG("GAMEPAD: LockEnemy → LockNearestEnemy");
                        LockNearestEnemy();
                    }
                    continue;
                }

                // Gamepad sans LB : ne pas traiter ici, laisser le jeu gérer
                continue;
            }

            // --- Clavier : traitement existant (inchangé) ---
            if (!btn->IsDown()) continue;

            auto code = btn->GetIDCode();

            // MessageBox : navigation haut/bas, Entrée = bouton sélectionné, Escape = annuler
            if (g_msgBoxOpen.load(std::memory_order_relaxed)) {
                const bool mbUp   = (code == RE::BSKeyboardDevice::Keys::kUp)    || (code == RE::BSKeyboardDevice::Keys::kLeft)  ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)     || (code == RE::BSKeyboardDevice::Keys::kA);
                const bool mbDown = (code == RE::BSKeyboardDevice::Keys::kDown)  || (code == RE::BSKeyboardDevice::Keys::kRight) ||
                                    (code == RE::BSKeyboardDevice::Keys::kS)     || (code == RE::BSKeyboardDevice::Keys::kD);
                if (mbUp) {
                    int sel = g_msgBoxSelectedBtn.load();
                    if (sel > 0) g_msgBoxSelectedBtn.store(sel - 1);
                    QueueAnnounceMsgBoxBtn();
                    continue;
                }
                if (mbDown) {
                    int sel   = g_msgBoxSelectedBtn.load();
                    int count = g_msgBoxBtnCount.load();
                    if (sel < count - 1) g_msgBoxSelectedBtn.store(sel + 1);
                    QueueAnnounceMsgBoxBtn();
                    continue;
                }
                if (code == RE::BSKeyboardDevice::Keys::kEnter) {
                    QueueMsgBoxPress();
                    continue;
                }
                if (code == RE::BSKeyboardDevice::Keys::kEscape) {
                    QueueMsgBoxCancel();
                    continue;
                }
            }

            // Extended Hotkey System (EHS) : gestion des raccourcis dans le menu favoris.
            // EHS remplace favoritesmenu.swf et prend le controle de TOUTES les
            // assignations (numeros 1-8 ET Ctrl+F1-F12). Comme il stocke tout dans
            // son propre co-save invisible depuis GFx, on maintient notre propre
            // mapping pour pouvoir annoncer les raccourcis.
            //
            // DOIT etre traite AVANT le bloc g_favOpen plus bas (qui gere uniquement
            // la navigation haut/bas) ET avant tout autre handler global, pour que
            // nos annonces soient envoyees en priorite et que le flow favoritesmenu
            // ne soit jamais interrompu.
            //
            // Ne s'active que si EHS est reellement charge dans le processus SKSE.
            if (g_favOpen.load(std::memory_order_relaxed) &&
                g_ehsInstalled.load(std::memory_order_relaxed)) {
                // 1) Touches numeriques 1-8 (vanilla hotkeys interceptees par EHS)
                if (code >= RE::BSKeyboardDevice::Keys::kNum1 &&
                    code <= RE::BSKeyboardDevice::Keys::kNum8) {
                    int num = static_cast<int>(code) - static_cast<int>(RE::BSKeyboardDevice::Keys::kNum1) + 1;
                    QueueEHSHotkeyAnnounce(std::to_wstring(num));
                    continue;
                }
                // 2) Ctrl+F1..F12 (raccourcis etendus EHS)
                int fKey = 0;
                switch (code) {
                    case RE::BSKeyboardDevice::Keys::kF1:  fKey = 1;  break;
                    case RE::BSKeyboardDevice::Keys::kF2:  fKey = 2;  break;
                    case RE::BSKeyboardDevice::Keys::kF3:  fKey = 3;  break;
                    case RE::BSKeyboardDevice::Keys::kF4:  fKey = 4;  break;
                    case RE::BSKeyboardDevice::Keys::kF5:  fKey = 5;  break;
                    case RE::BSKeyboardDevice::Keys::kF6:  fKey = 6;  break;
                    case RE::BSKeyboardDevice::Keys::kF7:  fKey = 7;  break;
                    case RE::BSKeyboardDevice::Keys::kF8:  fKey = 8;  break;
                    case RE::BSKeyboardDevice::Keys::kF9:  fKey = 9;  break;
                    case RE::BSKeyboardDevice::Keys::kF10: fKey = 10; break;
                    case RE::BSKeyboardDevice::Keys::kF11: fKey = 11; break;
                    case RE::BSKeyboardDevice::Keys::kF12: fKey = 12; break;
                    default: break;
                }
                if (fKey > 0 && (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) {
                    QueueEHSHotkeyAnnounce(L"F" + std::to_wstring(fKey));
                    continue;
                }
            }

            // Main menu: any key press triggers a deferred read on the UI thread
            if (g_mainOpen.load(std::memory_order_relaxed)) {
                QueueMainMenuRead();
            }

            // Journal / MCM : navigation clavier
            if (g_journalOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kLeft)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kRight)  ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kA)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kD)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kEnter)  ||
                                    (code == RE::BSKeyboardDevice::Keys::kTab)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kEscape);
                if (navKey) QueueJournalRead();
            }

            // Menu en croix : tracking clavier direct (pas de GFx)
            if (g_tweenForeground.load(std::memory_order_relaxed)) {
                int targetFrame = 0;
                if      (code == RE::BSKeyboardDevice::Keys::kUp    || code == RE::BSKeyboardDevice::Keys::kW) targetFrame = 2;
                else if (code == RE::BSKeyboardDevice::Keys::kLeft  || code == RE::BSKeyboardDevice::Keys::kA) targetFrame = 3;
                else if (code == RE::BSKeyboardDevice::Keys::kRight || code == RE::BSKeyboardDevice::Keys::kD) targetFrame = 4;
                else if (code == RE::BSKeyboardDevice::Keys::kDown  || code == RE::BSKeyboardDevice::Keys::kS) targetFrame = 5;

                LOG("TweenMenu input: code={} targetFrame={}", code, targetFrame);

                if (targetFrame > 0) {
                    int prev = g_lastTweenFrame.exchange(targetFrame);
                    if (prev != targetFrame)
                        AnnounceTweenNavKey(targetFrame);
                }
            }

            // Magie : haut/bas changent l'item, gauche/droite changent la catégorie
            if (g_magicOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kLeft)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kRight)  ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kA)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kD);
                if (navKey) QueueMagicRead();
            }

            // Favoris : haut/bas naviguent dans la liste
            // Note: les touches 1-8 sont gerees nativement par le polling (80ms)
            // qui detecte le changement du champ .hotkey dans le dataProvider SkyUI.
            if (g_favOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS);
                if (navKey) QueueFavRead();
            }

            // Character Sheet (mods ShowStats/ShowFactions) : navigation
            // clavier custom. Le mod affiche toutes les colonnes simultanement
            // et ne gere pas le clavier. On maintient notre propre index de
            // stat (Haut/Bas) et de tab (Gauche/Droite).
            // N et P restent geres par le mod lui-meme pour cycler entre
            // ShowStats <-> ShowFactions.
            if (g_charSheetOpen.load(std::memory_order_relaxed)) {
                if (code == RE::BSKeyboardDevice::Keys::kDown ||
                    code == RE::BSKeyboardDevice::Keys::kS) {
                    int cur = g_charSheetOurIndex.load();
                    g_charSheetOurIndex.store(cur + 1);
                    QueueCharSheetRead();
                } else if (code == RE::BSKeyboardDevice::Keys::kUp ||
                           code == RE::BSKeyboardDevice::Keys::kW) {
                    int cur = g_charSheetOurIndex.load();
                    if (cur > 0) g_charSheetOurIndex.store(cur - 1);
                    QueueCharSheetRead();
                } else if (code == RE::BSKeyboardDevice::Keys::kRight ||
                           code == RE::BSKeyboardDevice::Keys::kD) {
                    // Onglet suivant (colonne de droite)
                    int cur = g_charSheetOurTabIndex.load();
                    g_charSheetOurTabIndex.store(cur + 1);  // clamp dans Read
                    g_charSheetOurIndex.store(0);  // reset stat index sur le nouveau tab
                    QueueCharSheetRead();
                } else if (code == RE::BSKeyboardDevice::Keys::kLeft ||
                           code == RE::BSKeyboardDevice::Keys::kA) {
                    // Onglet precedent (colonne de gauche)
                    int cur = g_charSheetOurTabIndex.load();
                    if (cur > 0) g_charSheetOurTabIndex.store(cur - 1);
                    g_charSheetOurIndex.store(0);
                    QueueCharSheetRead();
                } else if (code == RE::BSKeyboardDevice::Keys::kN ||
                           code == RE::BSKeyboardDevice::Keys::kP) {
                    // Le mod cycle entre ShowStats et ShowFactions. Reset nos
                    // index ; le polling detectera le changement de menu.
                    g_charSheetOurIndex.store(0);
                    g_charSheetOurTabIndex.store(0);
                    QueueCharSheetRead();
                }
            }

            // UIListMenu : haut/bas naviguent dans la liste
            if (g_uiListMenuOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)  ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)     ||
                                    (code == RE::BSKeyboardDevice::Keys::kS);
                if (navKey) QueueUIListRead();
            }

            // Dialogue : haut/bas naviguent dans les options de réponse
            if (g_dialogueOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS);
                if (navKey) QueueDialogueRead();
            }

            // QuickLoot IE : haut/bas changent l'item dans la liste de loot
            if (g_quickLootOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS);
                if (navKey) QueueQuickLootRead();
            }

            // Container : haut/bas changent l'item, gauche/droite changent de côté
            if (g_containerOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kLeft)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kRight)  ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kA)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kD);
                if (navKey) QueueContainerRead();
            }

            // Barter : haut/bas changent l'item, gauche/droite changent de catégorie, F switch de côté
            if (g_barterOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kLeft)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kRight)  ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kA)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kD);
                if (navKey) QueueBarterRead();
            }

            // Gift menu
            if (g_giftOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kLeft)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kRight)  ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kA)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kD);
                if (navKey) QueueGiftRead();
            }

            // Crafting
            if (g_craftingOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kLeft)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kRight)  ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kA)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kD);
                if (navKey) {
                    g_craftingFirstReadDone = true;  // débloquer la lecture des items
                    g_craftingForceAnnounce.store(true);  // forcer relecture (cas recettes dupliquées identiques)
                    QueueCraftingRead();
                }
            }

            // RaceSex : toute navigation déclenche une lecture immédiate
            if (g_raceSexOpen.load(std::memory_order_relaxed)) {
                bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                // Ctrl+Gauche/Droite = changer d'onglet (catégorie)
                if (code == RE::BSKeyboardDevice::Keys::kLeft || code == RE::BSKeyboardDevice::Keys::kRight) {
                    LOG("RaceSex: arrow key={} ctrl={}", code, ctrl);
                }
                if (ctrl && (code == RE::BSKeyboardDevice::Keys::kLeft || code == RE::BSKeyboardDevice::Keys::kRight)) {
                    bool next = (code == RE::BSKeyboardDevice::Keys::kRight);
                    auto* task = SKSE::GetTaskInterface();
                    if (task) {
                        task->AddUITask([next]() {
                            auto* ui2 = RE::UI::GetSingleton();
                            if (!ui2) return;
                            auto menu = ui2->GetMenu(RE::RaceSexMenu::MENU_NAME);
                            if (!menu || !menu->uiMovie) return;
                            RE::GFxValue panels;
                            if (menu->uiMovie->GetVariable(&panels, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance")) {
                                panels.Invoke(next ? "moveCategoriesUp" : "moveCategoriesDown", nullptr, nullptr, 0);
                                LOG("RaceSex: invoked {} on panels", next ? "moveCategoriesUp" : "moveCategoriesDown");
                            }
                        });
                    }
                    // Laisser le temps au GFx de changer puis relire
                    QueueRaceSexRead();
                    continue;
                }
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kLeft)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kRight)  ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kA)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kD);
                if (navKey) QueueRaceSexRead();
            }

            // Level Up Menu
            if (g_levelUpOpen.load(std::memory_order_relaxed)) {
                const bool prev = (code == RE::BSKeyboardDevice::Keys::kLeft)  || (code == RE::BSKeyboardDevice::Keys::kA) ||
                                  (code == RE::BSKeyboardDevice::Keys::kUp)    || (code == RE::BSKeyboardDevice::Keys::kW);
                const bool next = (code == RE::BSKeyboardDevice::Keys::kRight) || (code == RE::BSKeyboardDevice::Keys::kD) ||
                                  (code == RE::BSKeyboardDevice::Keys::kDown)  || (code == RE::BSKeyboardDevice::Keys::kS);
                if (prev) {
                    g_levelUpSelection.store((g_levelUpSelection.load() + 2) % 3);
                    AnnounceLevelUpSelection();
                    continue;
                }
                if (next) {
                    g_levelUpSelection.store((g_levelUpSelection.load() + 1) % 3);
                    AnnounceLevelUpSelection();
                    continue;
                }
                if (code == RE::BSKeyboardDevice::Keys::kEnter) {
                    QueueConfirmLevelUp();
                    continue;
                }
            }

            // Map menu (quand la carte est ouverte)
            if (g_mapOpen.load(std::memory_order_relaxed)) {
                if (code == RE::BSKeyboardDevice::Keys::kPageDown) {
                    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                    if (alt) MapCycleSubFilter();
                    else MapNextMarker();
                    continue;
                }
                if (code == RE::BSKeyboardDevice::Keys::kPageUp) {
                    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                    if (alt) MapCyclePrevSubFilter();
                    else MapPrevMarker();
                    continue;
                }
                if (code == RE::BSKeyboardDevice::Keys::kHome) {
                    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                    if (shift) {
                        MapSetReference();
                    } else {
                        MapAnnounceDetails();
                    }
                    continue;
                }
                if (code == RE::BSKeyboardDevice::Keys::kEnd) {
                    MapCycleFilter();
                    continue;
                }
                if (code == RE::BSKeyboardDevice::Keys::kEnter) {
                    MapFastTravel();
                    continue;
                }
                if (code == RE::BSKeyboardDevice::Keys::kP) {
                    MapPlaceCustomMarker();
                    continue;
                }
            }

            // Scanner + AutoWalk (seulement hors menus)
            {
                bool anyMenuOpen = g_invOpen.load() || g_containerOpen.load() || g_barterOpen.load() || g_craftingOpen.load() ||
                                   g_journalOpen.load() || g_magicOpen.load() ||
                                   g_mainOpen.load() || g_dialogueOpen.load() ||
                                   g_raceSexOpen.load() || g_tweenOpen.load() ||
                                   g_statsOpen.load() || g_favOpen.load() ||
                                   g_msgBoxOpen.load() || g_uiListMenuOpen.load() ||
                                   g_mapOpen.load();
                if (!anyMenuOpen) {
                    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

                    // Scanner : touches configurables via MCM
                    uint32_t dxCode = static_cast<uint32_t>(code);
                    if (dxCode == g_keyScan.load()) {
                        DoScan();
                        continue;
                    }
                    if (dxCode == g_keyNextObject.load()) {
                        if (shift) ScannerNextCategory();
                        else ScannerNextObject();
                        continue;
                    }
                    if (dxCode == g_keyPrevObject.load()) {
                        if (shift) ScannerPrevCategory();
                        else ScannerPrevObject();
                        continue;
                    }
                    if (dxCode == g_keyTeleport.load()) {
                        bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                        if (alt) {
                            ScannerTeleport();
                            continue;
                        }
                    }
                    if (dxCode == g_keyAnnounce.load()) {
                        if (shift) ToggleAutoWalk();
                        else ScannerAnnounceCurrent();
                        continue;
                    }
                    if (dxCode == g_keySubcategory.load()) {
                        ScannerCycleSubcategory();
                        continue;
                    }
                    // X = verrouiller l'ennemi le plus proche, Shift+X = toggle lock-on continu
                    if (code == RE::BSKeyboardDevice::Keys::kX) {
                        if (shift) ToggleLockOnEnemy();
                        else LockNearestEnemy();
                        continue;
                    }
                    // Stop autowalk si on marche et qu'on appuie sur une touche de mouvement
                    if (g_autoWalking.load()) {
                        if (code == RE::BSKeyboardDevice::Keys::kW ||
                            code == RE::BSKeyboardDevice::Keys::kA ||
                            code == RE::BSKeyboardDevice::Keys::kS ||
                            code == RE::BSKeyboardDevice::Keys::kD) {
                            Speak(L"Stopping");
                            StopAutoWalk();
                        }
                    }
                }
            }

            // F = toggle caméra première/troisième personne (uniquement en jeu).
            // Dans l'inventaire/conteneur/marchand/magie/favoris, F a une autre
            // signification (toggle favorite SkyUI, etc.) et ne change pas la camera,
            // donc on ne doit pas annoncer "First/Third person" dans ces contextes.
            //
            // Approche (même logique que le combo gamepad LB+POV) : on fait
            // NOUS-MÊMES le toggle via ForceFirst/ThirdPerson, on annonce le
            // nouvel état, et on consomme l'event pour éviter que le moteur
            // fasse un second toggle (double bascule = pas de changement visible).
            //
            // Pourquoi pas laisser le moteur faire : avant, on faisait AddTask +
            // IsInFirstPerson() pour lire l'état "après" le moteur. Mais selon le
            // timing des ticks, AddTask s'exécutait PARFOIS avant que le toggle
            // moteur soit appliqué -> annonce inversée. En faisant le toggle
            // nous-mêmes et en consommant l'event, on élimine la course.
            if (code == RE::BSKeyboardDevice::Keys::kF) {
                auto* ui = RE::UI::GetSingleton();
                const bool inGame = ui && !ui->GameIsPaused();
                if (inGame) {
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
                    // Consommer l'event pour empêcher le moteur de re-toggler.
                    // Le const sur a_event est un artefact de l'API SKSE — les
                    // plugins peuvent modifier l'event en place pour "absorber"
                    // un input (même logique que pour LB dans un menu item).
                    auto* mutableBtn = const_cast<RE::ButtonEvent*>(btn);
                    mutableBtn->value = 0.0f;
                    mutableBtn->heldDownSecs = 0.0f;
                }
                // En menu (Inventaire, etc.) on laisse F passer pour que le
                // favorite SkyUI fonctionne normalement.
            }

            // H = stats contextuelles (en jeu: vitals, en inventaire: or/poids)
            // Ctrl+H = effets actifs (poison, maladies, buffs)
            if (code == RE::BSKeyboardDevice::Keys::kH) {
                const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                if (ctrl) {
                    AnnounceActiveEffects();
                } else if (g_invOpen.load(std::memory_order_relaxed)) {
                    AnnounceInventoryStats();
                } else if (g_containerOpen.load(std::memory_order_relaxed)) {
                    AnnounceContainerStats();
                } else if (g_barterOpen.load(std::memory_order_relaxed)) {
                    AnnounceBarterStats();
                } else {
                    AnnouncePlayerVitals();
                }
                continue;
            }

            // Tri SkyUI (touches 1-4) dans inventaire/conteneur/marchand
            if (g_skyuiMode.load(std::memory_order_relaxed) &&
                (g_invOpen.load(std::memory_order_relaxed) || g_containerOpen.load(std::memory_order_relaxed) || g_barterOpen.load(std::memory_order_relaxed))) {
                // col 2 = itemNameColumn (state1=nom, state2=équipé, state3=volé, state4=enchanté)
                // col 4 = weightColumn, col 5 = valueColumn
                if (code == RE::BSKeyboardDevice::Keys::kNum1) { SkyUISortColumn(2, 2, L"Sort by equipped"); continue; }
                if (code == RE::BSKeyboardDevice::Keys::kNum2) { SkyUISortColumn(2, 1, L"Sort by name"); continue; }
                if (code == RE::BSKeyboardDevice::Keys::kNum3) { SkyUISortColumn(4, 1, L"Sort by weight"); continue; }
                if (code == RE::BSKeyboardDevice::Keys::kNum4) { SkyUISortColumn(5, 1, L"Sort by value"); continue; }
            }

            // Inventaire
            if (!g_invOpen.load(std::memory_order_relaxed)) continue;

            const bool up    = (code == RE::BSKeyboardDevice::Keys::kUp)    || (code == RE::BSKeyboardDevice::Keys::kW);
            const bool down  = (code == RE::BSKeyboardDevice::Keys::kDown)  || (code == RE::BSKeyboardDevice::Keys::kS);
            const bool left  = (code == RE::BSKeyboardDevice::Keys::kLeft)  || (code == RE::BSKeyboardDevice::Keys::kA) ||
                               (code == RE::BSKeyboardDevice::Keys::kQ);
            const bool right = (code == RE::BSKeyboardDevice::Keys::kRight) || (code == RE::BSKeyboardDevice::Keys::kD) ||
                               (code == RE::BSKeyboardDevice::Keys::kE);

            if (up || down || left || right) {
                QueueInventoryRead();
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

static InputListener g_inputListener;

static void RegisterInputListener() {
    auto* mgr = RE::BSInputDeviceManager::GetSingleton();
    if (mgr) {
        mgr->AddEventSink(&g_inputListener);
    }
}

// ---------------- Activate Listener (piliers puzzle) ----------------
class ActivateListener : public RE::BSTEventSink<RE::TESActivateEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* e,
        RE::BSTEventSource<RE::TESActivateEvent>*) override {
        if (!e || !e->objectActivated || !e->actionRef) return RE::BSEventNotifyControl::kContinue;

        // Seulement si le joueur active
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (e->actionRef.get() != player) return RE::BSEventNotifyControl::kContinue;

        auto* ref = e->objectActivated.get();
        if (!ref) return RE::BSEventNotifyControl::kContinue;

        // Vérifier si c'est un pilier ou anneau puzzle
        auto* vm = RE::SkyrimVM::GetSingleton();
        if (!vm || !vm->impl) return RE::BSEventNotifyControl::kContinue;
        auto* policy = vm->impl->GetObjectHandlePolicy();
        if (!policy) return RE::BSEventNotifyControl::kContinue;

        auto handle = policy->GetHandleForObject(
            static_cast<RE::VMTypeID>(RE::FormType::Reference), ref);

        RE::BSTSmartPointer<RE::BSScript::Object> scriptObj;
        std::string foundScript;
        const char* puzzleScripts[] = {"defaultPuzzlePillarScript", "HallofStoriesDiskScript"};
        for (auto* sName : puzzleScripts) {
            if (vm->impl->FindBoundObject(handle, sName, scriptObj) && scriptObj) {
                foundScript = sName;
                break;
            }
        }
        if (foundScript.empty()) return RE::BSEventNotifyControl::kContinue;

        LOG("ActivateListener: puzzle '{}' activated FormID={:08X}", foundScript, ref->GetFormID());

        // Le state actuel va changer vers le suivant (cycle 01→02→03→01)
        std::string currentState = scriptObj->currentState.c_str();
        int nextPos = 0;
        if (currentState == "position01") nextPos = 2;
        else if (currentState == "position02") nextPos = 3;
        else if (currentState == "position03") nextPos = 1;

        if (nextPos == 0) return RE::BSEventNotifyControl::kContinue;

        // Déterminer le symbole
        std::wstring symbol = L"Position " + std::to_wstring(nextPos);

        if (foundScript == "defaultPuzzlePillarScript") {
            // Piliers : toujours Eagle/Snake/Whale
            if (nextPos == 1) symbol = L"Eagle";
            else if (nextPos == 2) symbol = L"Snake";
            else if (nextPos == 3) symbol = L"Whale";
        } else if (foundScript == "HallofStoriesDiskScript") {
            // Anneaux : chercher via le linkedRef (serrure)
            auto* linkedRef = ref->GetLinkedRef(nullptr);
            if (linkedRef) {
                RE::FormID keyholeID = linkedRef->GetFormID();
                struct DS { RE::FormID id; const wchar_t* s1; const wchar_t* s2; const wchar_t* s3; };
                static const DS table[] = {
                    {0x0004E2B9, L"Bear", L"Moth", L"Owl"},
                    {0x000DB883, L"Moth", L"Owl", L"Wolf"},
                    {0x000F3986, L"Wolf", L"Hawk", L"Wolf"},
                    {0x000E4ECE, L"Hawk", L"Hawk", L"Dragon"},
                    {0x000B89F2, L"Bear", L"Whale", L"Snake"},
                    {0x000A46D6, L"Wolf", L"Moth", L"Dragon"},
                    {0x0007C536, L"Fox", L"Owl", L"Snake"},
                    {0x000B634C, L"Snake", L"Wolf", L"Moth"},
                    {0x000FC2DC, L"Fox", L"Moth", L"Dragon"},
                };
                for (auto& ds : table) {
                    if (ds.id == keyholeID) {
                        if (nextPos == 1) symbol = ds.s1;
                        else if (nextPos == 2) symbol = ds.s2;
                        else if (nextPos == 3) symbol = ds.s3;
                        break;
                    }
                }
            }
        }

        SpeakQueue(symbol);

        return RE::BSEventNotifyControl::kContinue;
    }
};

static void RegisterActivateListener() {
    auto* source = RE::ScriptEventSourceHolder::GetSingleton();
    if (source) {
        static ActivateListener listener;
        source->AddEventSink(&listener);
        LOG("ActivateListener registered");
    }
}

// ---------------- Death listener (son de kill) ----------------

class DeathListener : public RE::BSTEventSink<RE::TESDeathEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* e,
        RE::BSTEventSource<RE::TESDeathEvent>*) override {
        if (!e || !e->dead) return RE::BSEventNotifyControl::kContinue;

        auto* player = RE::PlayerCharacter::GetSingleton();

        // Si c'est le JOUEUR qui meurt pendant un autowalk, arrêter proprement.
        // Sans ce handler, le monitor autowalk continue de tourner sur un ragdoll
        // mort et finit par déclencher SetAIDriven(false)/(true) à 4s de blocage
        // → crash du pipeline de rendu (skeleton inconsistant).
        auto* dyingActor = e->actorDying ? e->actorDying->As<RE::Actor>() : nullptr;
        if (dyingActor && dyingActor == player && g_autoWalking.load()) {
            LOG("DeathListener: player died during autowalk — stopping");
            StopAutoWalk();
        }

        // Seulement si le joueur a tué
        if (!e->actorKiller || e->actorKiller.get() != player) return RE::BSEventNotifyControl::kContinue;

        auto* victim = e->actorDying ? e->actorDying->As<RE::Actor>() : nullptr;
        if (!victim) return RE::BSEventNotifyControl::kContinue;

        const char* name = victim->GetDisplayFullName();
        LOG("DeathListener: killed '{}'", name ? name : "?");

        // Son de kill custom
        PlaySoundOneShot(g_soundEnemyDeathID, g_volumeKill);

        return RE::BSEventNotifyControl::kContinue;
    }
};

static void RegisterDeathListener() {
    auto* source = RE::ScriptEventSourceHolder::GetSingleton();
    if (source) {
        static DeathListener listener;
        source->AddEventSink(&listener);
        LOG("DeathListener registered");
    }
}

// Hit listener — bip grave quand le joueur touche un dragon à l'arc
class HitListener : public RE::BSTEventSink<RE::TESHitEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* e,
        RE::BSTEventSource<RE::TESHitEvent>*) override {
        if (!e || !e->target || !e->cause) return RE::BSEventNotifyControl::kContinue;

        // Seulement si le joueur est l'attaquant
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (e->cause.get() != player) return RE::BSEventNotifyControl::kContinue;

        // Seulement si la cible est un dragon
        auto* victim = e->target->As<RE::Actor>();
        if (!victim || !IsDragon(victim)) return RE::BSEventNotifyControl::kContinue;

        LOG("HitListener: player hit dragon '{}'", victim->GetDisplayFullName() ? victim->GetDisplayFullName() : "?");

        // Son de touche dragon custom
        PlaySoundOneShot(g_soundDragonHitID, g_volumeDragonHit);

        return RE::BSEventNotifyControl::kContinue;
    }
};

static void RegisterHitListener() {
    auto* source = RE::ScriptEventSourceHolder::GetSingleton();
    if (source) {
        static HitListener listener;
        source->AddEventSink(&listener);
        LOG("HitListener registered");
    }
}

// Furniture listener — debug crafting station enter/exit
class FurnitureListener : public RE::BSTEventSink<RE::TESFurnitureEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* e, RE::BSTEventSource<RE::TESFurnitureEvent>*) override {
        if (!e || !e->actor || !e->targetFurniture) return RE::BSEventNotifyControl::kContinue;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (e->actor.get() != player) return RE::BSEventNotifyControl::kContinue;

        auto* furnRef = e->targetFurniture.get();
        std::string furnName = furnRef ? furnRef->GetDisplayFullName() : "unknown";
        RE::FormID furnID = furnRef ? furnRef->GetFormID() : 0;

        // Get base form type
        std::string baseType = "unknown";
        if (furnRef && furnRef->GetBaseObject()) {
            auto* base = furnRef->GetBaseObject();
            baseType = std::to_string(static_cast<int>(base->GetFormType()));
        }

        bool entering = (e->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter);
        LOG("FurnitureEvent: {} furniture '{}' FormID={:08X} baseType={}",
            entering ? "ENTER" : "EXIT", furnName, furnID, baseType);

        return RE::BSEventNotifyControl::kContinue;
    }
};

static void RegisterFurnitureListener() {
    auto* source = RE::ScriptEventSourceHolder::GetSingleton();
    if (source) {
        static FurnitureListener listener;
        source->AddEventSink(&listener);
        LOG("FurnitureListener registered");
    }
}

// =============================================================================
// LootTracker event sinks — nettoie le flag "looted" quand le moteur respawn
// une ref (TESResetEvent), et marque via take quand un item bouge depuis un
// conteneur vers le joueur (TESContainerChangedEvent) — utile pour les loots
// via QuickLoot IE / RE qui ne passent pas par le ContainerMenu standard.
// =============================================================================
class LootResetListener : public RE::BSTEventSink<RE::TESResetEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESResetEvent* e,
        RE::BSTEventSource<RE::TESResetEvent>*) override {
        if (!e || !e->object) return RE::BSEventNotifyControl::kContinue;
        RE::FormID fid = e->object->GetFormID();
        LootTracker::GetSingleton()->Clear(fid);
        LOG("LootTracker: reset event for {:08X}, cleared", fid);
        return RE::BSEventNotifyControl::kContinue;
    }
};

class LootContainerChangedListener : public RE::BSTEventSink<RE::TESContainerChangedEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* e,
        RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
        if (!e) return RE::BSEventNotifyControl::kContinue;

        // On s'interesse aux transferts VERS le joueur (take) depuis un conteneur
        // ou un cadavre. Le event payload contient des FormID, pas des refs, donc
        // on doit faire LookupByID pour savoir si la source est un conteneur.
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return RE::BSEventNotifyControl::kContinue;

        // Si la destination n'est pas le joueur, ignorer.
        if (e->newContainer != player->GetFormID()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // La source doit exister (pas 0) et etre un conteneur ou un acteur mort.
        if (e->oldContainer == 0) return RE::BSEventNotifyControl::kContinue;

        auto* srcForm = RE::TESForm::LookupByID(e->oldContainer);
        if (!srcForm) return RE::BSEventNotifyControl::kContinue;

        auto* srcRef = srcForm->AsReference();
        if (!srcRef) return RE::BSEventNotifyControl::kContinue;

        // Ignorer le joueur lui-meme (swap inventory/hotkey, equip, etc.).
        if (srcRef == player) return RE::BSEventNotifyControl::kContinue;

        // Filtrage : accepter UNIQUEMENT les conteneurs et les acteurs morts.
        // Sans ce filtre, recevoir une recompense de quete (PNJ vivant qui te
        // donne un item) marquerait le PNJ comme "looted", et quand il mourrait
        // plus tard, son cadavre s'annoncerait deja fouille sans avoir ete
        // ouvert. On veut eviter ce faux positif.
        auto* baseForm = srcRef->GetBaseObject();
        bool isContainer = baseForm && baseForm->Is(RE::FormType::Container);
        bool isDeadActor = false;
        if (auto* actor = srcRef->As<RE::Actor>()) {
            isDeadActor = actor->IsDead(false);
        }
        if (!isContainer && !isDeadActor) {
            return RE::BSEventNotifyControl::kContinue;
        }

        LootTracker::GetSingleton()->MarkLooted(e->oldContainer);
        LOG("LootTracker: marked {:08X} as looted (ContainerChanged take, {})",
            e->oldContainer, isContainer ? "container" : "dead actor");
        return RE::BSEventNotifyControl::kContinue;
    }
};

static void RegisterLootListeners() {
    auto* source = RE::ScriptEventSourceHolder::GetSingleton();
    if (source) {
        static LootResetListener resetListener;
        source->AddEventSink(&resetListener);
        LOG("LootResetListener registered");

        static LootContainerChangedListener changedListener;
        source->AddEventSink(&changedListener);
        LOG("LootContainerChangedListener registered");
    }
}

// =============================================================================
// LootTracker SKSE serialization — persiste l'etat entre sauvegardes.
// =============================================================================
static void LootTrackerOnSave(SKSE::SerializationInterface* intf) {
    LootTracker::GetSingleton()->Save(intf);
}

static void LootTrackerOnLoad(SKSE::SerializationInterface* intf) {
    std::uint32_t type = 0;
    std::uint32_t version = 0;
    std::uint32_t length = 0;
    while (intf->GetNextRecordInfo(type, version, length)) {
        if (type == LootTracker::kRecordType) {
            LootTracker::GetSingleton()->Load(intf, version);
        }
    }
}

static void LootTrackerOnRevert(SKSE::SerializationInterface*) {
    LootTracker::GetSingleton()->ClearAll();
    LOG("LootTracker: reverted (new game / exit to main menu)");
}

// ---------------- Plugin load ----------------

// ---------------- MCM Papyrus native functions ----------------

namespace MCMNative {
    static const char* SCRIPT_NAME = "SkyrimTTS_MCM_Native";

    void SetStealthAnnounce(RE::StaticFunctionTag*, bool enabled) {
        g_mcmStealthAnnounce.store(enabled);
        LOG("MCM: stealth announce = {}", enabled);
    }

    void SetTeleportEnabled(RE::StaticFunctionTag*, bool enabled) {
        g_mcmTeleportEnabled.store(enabled);
        LOG("MCM: teleport = {}", enabled);
    }

    void SetAimVolume(RE::StaticFunctionTag*, float vol) {
        g_volumeAim = std::clamp(vol, 0.0f, 2.0f);
        LOG("MCM: aim volume = {:.2f}", g_volumeAim);
    }

    void SetKillVolume(RE::StaticFunctionTag*, float vol) {
        g_volumeKill = std::clamp(vol, 0.0f, 2.0f);
        LOG("MCM: kill volume = {:.2f}", g_volumeKill);
    }

    void SetDragonHitVolume(RE::StaticFunctionTag*, float vol) {
        g_volumeDragonHit = std::clamp(vol, 0.0f, 2.0f);
        LOG("MCM: dragon hit volume = {:.2f}", g_volumeDragonHit);
    }

    void SetKeyScan(RE::StaticFunctionTag*, int keyCode) {
        g_keyScan.store(static_cast<uint32_t>(keyCode));
        LOG("MCM: key scan = {}", keyCode);
    }

    void SetKeyAnnounce(RE::StaticFunctionTag*, int keyCode) {
        g_keyAnnounce.store(static_cast<uint32_t>(keyCode));
        LOG("MCM: key announce = {}", keyCode);
    }

    void SetKeyNextObject(RE::StaticFunctionTag*, int keyCode) {
        g_keyNextObject.store(static_cast<uint32_t>(keyCode));
        LOG("MCM: key next = {}", keyCode);
    }

    void SetKeyPrevObject(RE::StaticFunctionTag*, int keyCode) {
        g_keyPrevObject.store(static_cast<uint32_t>(keyCode));
        LOG("MCM: key prev = {}", keyCode);
    }

    void SetKeySubcategory(RE::StaticFunctionTag*, int keyCode) {
        g_keySubcategory.store(static_cast<uint32_t>(keyCode));
        LOG("MCM: key subcategory = {}", keyCode);
    }

    void SetKeyTeleport(RE::StaticFunctionTag*, int keyCode) {
        g_keyTeleport.store(static_cast<uint32_t>(keyCode));
        LOG("MCM: key teleport = {}", keyCode);
    }

    void SetScanRange(RE::StaticFunctionTag*, float range) {
        g_mcmScanRange.store(range);
        LOG("MCM: scan range = {:.0f}", range);
    }

    void SetTeleportRange(RE::StaticFunctionTag*, float range) {
        g_mcmTeleportRange.store(std::clamp(range, 500.0f, 3000.0f));
        LOG("MCM: teleport range = {:.0f}", range);
    }

    void SetAutoAimEnabled(RE::StaticFunctionTag*, bool enabled) {
        g_mcmAutoAimEnabled.store(enabled);
        LOG("MCM: auto aim = {}", enabled);
        // Si on désactive la visée auto en cours d'utilisation, arrêter le tracking
        if (!enabled) {
            StopAutoAim();
        }
    }

    // Gamepad button configuration (valeur = index dans g_gamepadButtonCodes)
    void SetGpScanNext(RE::StaticFunctionTag*, int idx)    { g_gpIdxScanNext.store(idx);    LOG("MCM gp: ScanNext={}", idx); }
    void SetGpScanPrev(RE::StaticFunctionTag*, int idx)    { g_gpIdxScanPrev.store(idx);    LOG("MCM gp: ScanPrev={}", idx); }
    void SetGpScanAnnounce(RE::StaticFunctionTag*, int idx){ g_gpIdxScanAnnounce.store(idx); LOG("MCM gp: ScanAnnounce={}", idx); g_gamepadNeedsRemap.store(true); }
    void SetGpMapSetRef(RE::StaticFunctionTag*, int idx)   { g_gpIdxMapSetRef.store(idx);   LOG("MCM gp: MapSetRef={}", idx); }
    void SetGpPrimary(RE::StaticFunctionTag*, int idx)     { g_gpIdxPrimary.store(idx);     LOG("MCM gp: Primary={}", idx); g_gamepadNeedsRemap.store(true); }
    void SetGpTeleport(RE::StaticFunctionTag*, int idx)    { g_gpIdxTeleport.store(idx);    LOG("MCM gp: Teleport={}", idx); }
    void SetGpVitals(RE::StaticFunctionTag*, int idx)      { g_gpIdxVitals.store(idx);      LOG("MCM gp: Vitals={}", idx); g_gamepadNeedsRemap.store(true); }
    void SetGpSneak(RE::StaticFunctionTag*, int idx)       { g_gpIdxSneak.store(idx);       LOG("MCM gp: Sneak={}", idx); }
    void SetGpPOV(RE::StaticFunctionTag*, int idx)         { g_gpIdxPOV.store(idx);         LOG("MCM gp: POV={}", idx); }
    void SetGpLockEnemy(RE::StaticFunctionTag*, int idx)   { g_gpIdxLockEnemy.store(idx);   LOG("MCM gp: LockEnemy={}", idx); }

    bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("SetStealthAnnounce",  SCRIPT_NAME, SetStealthAnnounce);
        vm->RegisterFunction("SetTeleportEnabled",   SCRIPT_NAME, SetTeleportEnabled);
        vm->RegisterFunction("SetAimVolume",         SCRIPT_NAME, SetAimVolume);
        vm->RegisterFunction("SetKillVolume",        SCRIPT_NAME, SetKillVolume);
        vm->RegisterFunction("SetDragonHitVolume",   SCRIPT_NAME, SetDragonHitVolume);
        vm->RegisterFunction("SetKeyScan",           SCRIPT_NAME, SetKeyScan);
        vm->RegisterFunction("SetKeyAnnounce",       SCRIPT_NAME, SetKeyAnnounce);
        vm->RegisterFunction("SetKeyNextObject",     SCRIPT_NAME, SetKeyNextObject);
        vm->RegisterFunction("SetKeyPrevObject",     SCRIPT_NAME, SetKeyPrevObject);
        vm->RegisterFunction("SetKeySubcategory",    SCRIPT_NAME, SetKeySubcategory);
        vm->RegisterFunction("SetKeyTeleport",       SCRIPT_NAME, SetKeyTeleport);
        vm->RegisterFunction("SetScanRange",         SCRIPT_NAME, SetScanRange);
        vm->RegisterFunction("SetTeleportRange",     SCRIPT_NAME, SetTeleportRange);
        vm->RegisterFunction("SetAutoAimEnabled",    SCRIPT_NAME, SetAutoAimEnabled);
        vm->RegisterFunction("SetGpScanNext",        SCRIPT_NAME, SetGpScanNext);
        vm->RegisterFunction("SetGpScanPrev",        SCRIPT_NAME, SetGpScanPrev);
        vm->RegisterFunction("SetGpScanAnnounce",    SCRIPT_NAME, SetGpScanAnnounce);
        vm->RegisterFunction("SetGpMapSetRef",       SCRIPT_NAME, SetGpMapSetRef);
        vm->RegisterFunction("SetGpPrimary",         SCRIPT_NAME, SetGpPrimary);
        vm->RegisterFunction("SetGpTeleport",        SCRIPT_NAME, SetGpTeleport);
        vm->RegisterFunction("SetGpVitals",          SCRIPT_NAME, SetGpVitals);
        vm->RegisterFunction("SetGpSneak",           SCRIPT_NAME, SetGpSneak);
        vm->RegisterFunction("SetGpPOV",             SCRIPT_NAME, SetGpPOV);
        vm->RegisterFunction("SetGpLockEnemy",       SCRIPT_NAME, SetGpLockEnemy);
        LOG("MCM native functions registered on {}", SCRIPT_NAME);
        return true;
    }
}

// ---------------------------------------------------------------

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Data\\SKSE\\SkyrimNVDA.log", true);
        auto logger = std::make_shared<spdlog::logger>("SkyrimNVDA", std::move(sink));
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(logger));
    }

    LOG("SkyrimNVDA starting");
    LOG("CWD: {}", std::filesystem::current_path().string());
    LoadINISettings();

    SKSE::GetPapyrusInterface()->Register(MCMNative::BindPapyrusFunctions);

    // Enregistre le LootTracker pour la persistance cosave. Le UniqueID est
    // notre signature plugin (arbitraire mais doit rester stable entre versions
    // pour que les saves se relisent correctement).
    {
        constexpr std::uint32_t kSkyrimNVDAUniqueID = 'SNVD';
        auto* serial = SKSE::GetSerializationInterface();
        if (serial) {
            serial->SetUniqueID(kSkyrimNVDAUniqueID);
            serial->SetSaveCallback(LootTrackerOnSave);
            serial->SetLoadCallback(LootTrackerOnLoad);
            serial->SetRevertCallback(LootTrackerOnRevert);
            LOG("LootTracker: SKSE serialization registered (uniqueID=0x{:08X})",
                kSkyrimNVDAUniqueID);
        }
    }

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* msg) {
        if (!msg) return;

        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            if (nvdaController_testIfRunning() != 0) {
                LOG("NVDA not running or nvdaController not available");
            } else {
                LOG("nvdaController OK — NVDA is running");
            }
            LoadTranslationFile(); // fallback pour les clés absentes du BSScaleformTranslator

            // Traductions des mods custom : charge Interface\translations\<mod>_<LANG>.txt
            // (fallback sur english.txt si la langue n'est pas traduite)
            LoadExtraTranslationFile("ShowStats");       // Skyrim Character Sheet stats
            LoadExtraTranslationFile("ShowFactions");    // Skyrim Character Sheet factions

            DetectSkyUIFromPlugin();

            // Detection d'Extended Hotkey System (mod SKSE qui permet d'assigner
            // Ctrl+F1-F12 comme raccourcis favoris). On verifie que la DLL est
            // effectivement CHARGEE (pas juste presente sur disque) pour eviter
            // les faux positifs sur les installations cassees ou les mauvaises
            // versions (ex: v1.1 sur Skyrim AE 1.6.x). GetModuleHandleA retourne
            // nullptr si la DLL n'est pas mappee dans le processus.
            g_ehsInstalled.store(GetModuleHandleA("ExtendedHotkeySystem.dll") != nullptr);
            LOG("EHS detection: {}", g_ehsInstalled.load() ? "loaded" : "not loaded");
            RegisterMenuListener();
            RegisterCrosshairListener();
            RegisterActivateListener();
            RegisterDeathListener();
            RegisterHitListener();
            RegisterLootListeners();  // TESResetEvent + TESContainerChangedEvent
            RegisterAutoWalkModEventListener();  // SkyrimNVDA_AutoWalkArrived (envoyé par Papyrus)
            // RegisterFurnitureListener();  // DÉSACTIVÉ POUR TEST
            InstallHUDAdvanceMovieHook();
            InstallConsoleAdvanceMovieHook();
            InstallLoadingAdvanceMovieHook();
            InstallSleepWaitAdvanceMovieHook();
            InstallTrainingAdvanceMovieHook();
            StartBowAutoAimPolling();
            RemapGamepadControls();
            Speak(L"Plugin loaded");
            LOG("kDataLoaded: listeners registered");
        }

        if (msg->type == SKSE::MessagingInterface::kInputLoaded) {
            RegisterInputListener();
            StartLbStuckWatcher();  // polling LB stuck (bug: dialogue forcé pendant LB maintenu)
        }

        // Après chargement d'une sauvegarde : remettre SpeedMult à 100 + re-remap sprint
        if (msg->type == SKSE::MessagingInterface::kPostLoadGame) {
            AutoWalkSafetyReset();
            // Bloquer l'autowalk pendant 10s : le skeleton/shader du joueur est
            // en cours de reconstruction, un SetAIDriven pendant cette fenêtre
            // déclenche un null pointer dans le pipeline de rendu (crashs observés).
            AutoWalkArmSafetyCooldown(10000, "kPostLoadGame");
            RemapGamepadControls();  // re-appliquer au cas où le jeu recharge les contrôles
            // Restaurer les contrôles gamepad si bloqués
            if (g_controlsDisabled) {
                auto* cm = RE::ControlMap::GetSingleton();
                if (cm) cm->enabledControls = static_cast<RE::UserEvents::USER_EVENT_FLAG>(g_savedControls);
                g_controlsDisabled = false;
            }
            g_lbHeld.store(false);
            RegisterShoutListener();
            LOG("kPostLoadGame: autowalk safety reset, sprint remap reapplied, shout listener registered");
        }
    });

    return true;
}
