#include "common.h"
#include "menu_inventory.h"
#include "menu_container.h"
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
#include "scanner.h"
#include "autowalk.h"
// #include "pathfinding.h"  // Désactivé — en développement, Alt+Home pour activer

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
            e->menuName != UILIST_MENU_NAME) {
            LOG("Menu {} : {}", e->opening ? "OPEN" : "CLOSE", e->menuName.c_str());
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
            } else {
                g_containerOpen.store(false);
                StopContainerPolling();
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
                SpeakQueue(L"Crafting menu open");
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

// ---------------- Input listener ----------------

class InputListener : public RE::BSTEventSink<RE::InputEvent*> {
public:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) {
        if (!a_event || !*a_event) return RE::BSEventNotifyControl::kContinue;

        for (auto e = *a_event; e; e = e->next) {
            if (e->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) continue;

            auto* btn = e->AsButtonEvent();
            if (!btn || !btn->IsDown()) continue;

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

            // F6 = racesex diagnostic
            if (code == RE::BSKeyboardDevice::Keys::kF6) {
                DiagnoseRaceSexNow();
                continue;
            }

            // F7 = container diagnostic
            if (code == RE::BSKeyboardDevice::Keys::kF7) {
                DiagnoseContainerNow();
                continue;
            }

            // F8 = main menu diagnostic
            if (code == RE::BSKeyboardDevice::Keys::kF8) {
                DiagnoseMainMenuNow();
                continue;
            }

            // F9 = inventory diagnostic
            if (code == RE::BSKeyboardDevice::Keys::kF9) {
                DiagnoseInventoryNow();
                continue;
            }

            // F10 = journal diagnostic
            if (code == RE::BSKeyboardDevice::Keys::kF10) {
                DiagnoseJournalNow();
                continue;
            }

            // F11 = magic diagnostic
            if (code == RE::BSKeyboardDevice::Keys::kF11) {
                DiagnoseMagicNow();
                continue;
            }

            // F12 = levelup diagnostic
            if (code == RE::BSKeyboardDevice::Keys::kF12) {
                QueueDiagnoseLevelUp();
                continue;
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
            if (g_favOpen.load(std::memory_order_relaxed)) {
                const bool navKey = (code == RE::BSKeyboardDevice::Keys::kUp)    ||
                                    (code == RE::BSKeyboardDevice::Keys::kDown)   ||
                                    (code == RE::BSKeyboardDevice::Keys::kW)      ||
                                    (code == RE::BSKeyboardDevice::Keys::kS);
                if (navKey) QueueFavRead();
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
                    MapNextMarker();
                    continue;
                }
                if (code == RE::BSKeyboardDevice::Keys::kPageUp) {
                    MapPrevMarker();
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
                    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                    if (alt) MapCycleSubFilter();
                    else MapCycleFilter();
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

            // F = toggle caméra première/troisième personne
            if (code == RE::BSKeyboardDevice::Keys::kF) {
                // Délai court pour laisser le jeu changer la caméra avant de lire
                auto* task = SKSE::GetTaskInterface();
                if (task) {
                    task->AddTask([]() {
                        auto* camera = RE::PlayerCamera::GetSingleton();
                        if (camera) {
                            bool fp = camera->IsInFirstPerson();
                            Speak(fp ? L"First person" : L"Third person");
                        }
                    });
                }
                // Ne pas 'continue' — laisser le jeu traiter F normalement
            }

            // H = stats contextuelles (en jeu: vitals, en inventaire: or/poids)
            if (code == RE::BSKeyboardDevice::Keys::kH) {
                if (g_invOpen.load(std::memory_order_relaxed)) {
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

        // Seulement si le joueur a tué
        auto* player = RE::PlayerCharacter::GetSingleton();
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

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* msg) {
        if (!msg) return;

        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            if (nvdaController_testIfRunning() != 0) {
                LOG("NVDA not running or nvdaController not available");
            } else {
                LOG("nvdaController OK — NVDA is running");
            }
            LoadTranslationFile(); // fallback pour les clés absentes du BSScaleformTranslator
            DetectSkyUIFromPlugin();
            RegisterMenuListener();
            RegisterCrosshairListener();
            RegisterActivateListener();
            RegisterDeathListener();
            RegisterHitListener();
            // RegisterFurnitureListener();  // DÉSACTIVÉ POUR TEST
            InstallHUDAdvanceMovieHook();
            InstallConsoleAdvanceMovieHook();
            InstallLoadingAdvanceMovieHook();
            InstallSleepWaitAdvanceMovieHook();
            InstallTrainingAdvanceMovieHook();
            StartBowAutoAimPolling();
            Speak(L"Plugin loaded");
            LOG("kDataLoaded: listeners registered");
        }

        if (msg->type == SKSE::MessagingInterface::kInputLoaded) {
            RegisterInputListener();
        }

        // Après chargement d'une sauvegarde : remettre SpeedMult à 100
        if (msg->type == SKSE::MessagingInterface::kPostLoadGame) {
            AutoWalkSafetyReset();
            // PathfindingSafetyReset();  // Désactivé temporairement
            RegisterShoutListener();
            LOG("kPostLoadGame: autowalk/pathfinding safety reset, shout listener registered");
        }
    });

    return true;
}
