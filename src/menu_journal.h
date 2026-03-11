#pragma once

// VOCALISATION JOURNAL - DEBUT

// --- Constantes d'onglet (questjournal.as : PAGE_QUEST=0, PAGE_STATS=1, PAGE_SYSTEM=2) ---
static constexpr int JOURNAL_TAB_QUESTS = 0;
static constexpr int JOURNAL_TAB_STATS  = 1;
static constexpr int JOURNAL_TAB_SYSTEM = 2;

// --- Chemins GFx (questjournal.as + QuestsPage.as + QuestCenteredList.as) ---
static constexpr const char* JOURNAL_TAB_PATH         = "_root.QuestJournalFader.Menu_mc.iCurrentTab";
static constexpr const char* JOURNAL_QUEST_TITLE_PATH = "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.questTitleText.text";
static constexpr const char* JOURNAL_QUEST_LIST_PATH  = "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.TitleList_mc.List_mc.centeredEntry.textField.text";
static constexpr const char* JOURNAL_QUEST_DESC_PATH  = "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.questDescriptionText.text";

// --- État ---
static std::atomic_bool g_journalOpen{false};
static std::atomic_bool g_journalPendingUIRead{false};
static std::jthread     g_journalPollThread;
static int              g_lastJournalTab{-1};
static std::wstring     g_lastJournalTitle;
static std::wstring     g_lastJournalDesc;
static int              g_lastSystemState{-1};
static std::wstring     g_lastSystemItem;
static std::wstring     g_lastStatsCategory;

// --- Snapshot ---
struct StatEntry {
    std::wstring name;
    std::wstring value;
};

struct QuestObjective {
    std::wstring text;
    bool active{false};
    bool completed{false};
    bool failed{false};
};

struct JournalSnapshot {
    int          tab{-1};
    std::wstring questTitle;
    std::wstring questDesc;
    bool         questActive{false};
    std::vector<QuestObjective> objectives;
    // Onglet Stats
    std::wstring statsCategory;
    std::vector<StatEntry> statsEntries;
    // Onglet System
    int          systemState{-1};
    std::wstring systemItem;
};

static bool ReadJournalSnapshot(JournalSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::JournalMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    double tab = 0.0;
    if (GetGFxNumber(movie, JOURNAL_TAB_PATH, tab))
        snap.tab = static_cast<int>(tab);

    if (snap.tab == JOURNAL_TAB_QUESTS) {
        std::string tmp;
        if (!GetGFxString(movie, JOURNAL_QUEST_TITLE_PATH, tmp) || tmp.empty())
            GetGFxString(movie, JOURNAL_QUEST_LIST_PATH, tmp);
        if (!tmp.empty())
            snap.questTitle = ResolveUIString(movie, tmp);

        if (GetGFxString(movie, JOURNAL_QUEST_DESC_PATH, tmp) && !tmp.empty())
            snap.questDesc = StripMarkupForSpeech(ResolveUIString(movie, tmp));

        // active : booléen sur l'objet centeredEntry (QuestCenteredList.as)
        RE::GFxValue activeVal;
        if (movie->GetVariable(&activeVal, "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.TitleList_mc.List_mc.centeredEntry.active"))
            snap.questActive = activeVal.IsBool() ? activeVal.GetBool() : (activeVal.IsNumber() && activeVal.GetNumber() != 0.0);

        // objectifs (ObjectiveScrollingList.as : entryList[i].text / .active / .completed / .failed)
        RE::GFxValue entryList;
        if (movie->GetVariable(&entryList, "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.objectiveList.entryList") && entryList.IsArray()) {
            const auto len = entryList.GetArraySize();
            for (std::uint32_t i = 0; i < len; ++i) {
                RE::GFxValue item;
                entryList.GetElement(i, &item);
                if (!item.IsObject()) continue;

                RE::GFxValue textVal;
                if (!item.GetMember("text", &textVal) || !textVal.IsString()) continue;
                std::string txt = textVal.GetString();
                if (txt.empty()) continue;

                QuestObjective obj;
                obj.text = ResolveUIString(movie, txt);

                auto readBool = [&](const char* field) -> bool {
                    RE::GFxValue v;
                    if (!item.GetMember(field, &v)) return false;
                    return v.IsBool() ? v.GetBool() : (v.IsNumber() && v.GetNumber() != 0.0);
                };
                obj.active    = readBool("active");
                obj.completed = readBool("completed");
                obj.failed    = readBool("failed");
                snap.objectives.push_back(std::move(obj));
            }
        }
    } else if (snap.tab == JOURNAL_TAB_STATS) {
        static constexpr const char* STATS_PREFIX = "_root.QuestJournalFader.Menu_mc.StatsFader.Page_mc.";

        std::string tmp;
        std::string catPath = std::string(STATS_PREFIX) + "CategoryList_mc.List_mc.selectedEntry.text";
        if (GetGFxString(movie, catPath.c_str(), tmp) && !tmp.empty())
            snap.statsCategory = ResolveUIString(movie, tmp);

        // Stats de la catégorie (StatsList_mc.entryList[i].text + .value)
        RE::GFxValue entryList;
        std::string listPath = std::string(STATS_PREFIX) + "StatsList_mc.entryList";
        if (movie->GetVariable(&entryList, listPath.c_str()) && entryList.IsArray()) {
            const auto len = entryList.GetArraySize();
            for (std::uint32_t i = 0; i < len; ++i) {
                RE::GFxValue item;
                entryList.GetElement(i, &item);
                if (!item.IsObject()) continue;

                RE::GFxValue textVal;
                if (!item.GetMember("text", &textVal) || !textVal.IsString()) continue;
                std::string txt = textVal.GetString();
                if (txt.empty()) continue;

                StatEntry entry;
                entry.name = ResolveUIString(movie, txt);

                RE::GFxValue valueVal;
                if (item.GetMember("value", &valueVal)) {
                    if (valueVal.IsNumber()) {
                        double v = valueVal.GetNumber();
                        long long iv = static_cast<long long>(v);
                        entry.value = std::to_wstring(iv);
                    } else if (valueVal.IsString()) {
                        entry.value = Utf8ToWString(valueVal.GetString());
                    }
                }
                snap.statsEntries.push_back(std::move(entry));
            }
        }
    } else if (snap.tab == JOURNAL_TAB_SYSTEM) {
        static constexpr const char* SYS_PREFIX = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc.";

        double stateVal = 0.0;
        std::string statePath = std::string(SYS_PREFIX) + "iCurrentState";
        if (GetGFxNumber(movie, statePath.c_str(), stateVal))
            snap.systemState = static_cast<int>(stateVal);

        // État → chemin de l'élément sélectionné (SystemPage.as)
        const char* itemSuffix = nullptr;
        switch (snap.systemState) {
            case 0:  itemSuffix = "CategoryList_mc.List_mc.selectedEntry.text"; break;
            case 3:  itemSuffix = "SettingsPanel.List_mc.selectedEntry.text"; break;
            case 4: {
                // OPTIONS_LISTS_STATE: announce label + current value
                // SettingsOptionItem.movieType: 0=ScrollBar(slider), 1=OptionStepper, 2=CheckBox
                // SettingsOptionItem.value: 0.0-1.0 for slider, selectedIndex for stepper, 0/1 for checkbox
                const std::string eb = std::string(SYS_PREFIX) + "OptionsListsPanel.OptionsLists.List_mc.selectedEntry";
                std::string labelStr;
                if (GetGFxString(movie, (eb + ".text").c_str(), labelStr) && !labelStr.empty()) {
                    std::wstring label = ResolveUIString(movie, labelStr);
                    double movieTypeD = 0.0;
                    bool hasType = GetGFxNumber(movie, (eb + ".movieType").c_str(), movieTypeD);
                    int mtype = hasType ? static_cast<int>(movieTypeD) : -1;
                    std::wstring valStr;
                    if (mtype == 0) {
                        // Slider: value in [0.0, 1.0] → percentage
                        double v = 0.0;
                        if (GetGFxNumber(movie, (eb + ".value").c_str(), v))
                            valStr = std::to_wstring(static_cast<int>(v * 100.0 + 0.5)) + L"%";
                    } else if (mtype == 1) {
                        // Stepper: read current label from OptionStepper_mc.textField.text
                        std::string s;
                        if (GetGFxString(movie, (eb + ".OptionStepper_mc.textField.text").c_str(), s) && !s.empty())
                            valStr = ResolveUIString(movie, s);
                    } else if (mtype == 2) {
                        // Checkbox: value 0=off, 1=on
                        double v = 0.0;
                        if (GetGFxNumber(movie, (eb + ".value").c_str(), v))
                            valStr = (v != 0.0) ? L"on" : L"off";
                    }
                    snap.systemItem = valStr.empty() ? label : label + L": " + valStr;
                }
                break;
            }
            case 1: {
                // SAVE_LOAD_STATE : même logique que menu_main.h TryReadSaveLoadEntry
                // On attend que name soit chargé (async ~500ms) avant d'annoncer
                const std::string eb = std::string(SYS_PREFIX) + "SaveLoadListHolder.List_mc.selectedEntry";

                double fileNumD = 0.0;
                bool hasFileNum = GetGFxNumber(movie, (eb + ".fileNum").c_str(), fileNumD);
                std::string textStr;
                GetGFxString(movie, (eb + ".text").c_str(), textStr);
                std::wstring textW = textStr.empty() ? L"" : ResolveUIString(movie, textStr);

                if (!hasFileNum && textW.empty()) break;

                // Préfixe "Save 042: "
                std::wstring prefix;
                if (hasFileNum) {
                    std::wostringstream ss;
                    ss << L"Save " << std::setw(3) << std::setfill(L'0') << static_cast<int>(fileNumD);
                    prefix = ss.str() + L": ";
                }

                // name pas encore chargé → skip, le polling réessaiera
                std::string nameStr;
                GetGFxString(movie, (eb + ".name").c_str(), nameStr);
                if (nameStr.empty()) break;

                std::wstring charName = Utf8ToWString(nameStr);
                std::wstring msg = prefix + charName;

                // Location depuis text (sans préfixe [M]/[Q]/[A]/[R])
                if (!textW.empty() && textW != charName) {
                    std::wstring location = textW;
                    if (location.size() >= 3 && location[0] == L'[' && location[2] == L']')
                        location = location.substr(3);
                    if (!location.empty())
                        msg += L", " + location;
                }

                std::string raceStr;
                if (GetGFxString(movie, (eb + ".raceName").c_str(), raceStr) && !raceStr.empty())
                    msg += L", " + Utf8ToWString(raceStr);
                double levelD = 0.0;
                if (GetGFxNumber(movie, (eb + ".level").c_str(), levelD) && levelD > 0.0)
                    msg += L", niveau " + std::to_wstring(static_cast<int>(levelD));
                std::string playTimeStr;
                if (GetGFxString(movie, (eb + ".playTime").c_str(), playTimeStr) && !playTimeStr.empty())
                    msg += L", " + Utf8ToWString(playTimeStr);
                std::string dateStr;
                if (GetGFxString(movie, (eb + ".dateString").c_str(), dateStr) && !dateStr.empty())
                    msg += L", " + Utf8ToWString(dateStr);

                snap.systemItem = std::move(msg);
                break;
            }
            case 6:  itemSuffix = "InputMappingPanel.List_mc.selectedEntry.text"; break;
            case 8:  itemSuffix = "PCQuitPanel.List_mc.selectedEntry.text"; break;
            case 13: itemSuffix = "HelpListPanel.List_mc.selectedEntry.text"; break;
            case 2: case 5: case 7: case 9: case 10:
                     itemSuffix = "ConfirmPanel.ConfirmText.textField.text"; break;
            default: break;
        }

        if (itemSuffix) {
            std::string fullPath = std::string(SYS_PREFIX) + itemSuffix;
            std::string tmp;
            if (GetGFxString(movie, fullPath.c_str(), tmp) && !tmp.empty())
                snap.systemItem = ResolveUIString(movie, tmp);
        }
    }

    return snap.tab >= 0;
}

static const wchar_t* JournalTabName(int tab) {
    switch (tab) {
        case JOURNAL_TAB_QUESTS: return L"Quests";
        case JOURNAL_TAB_STATS:  return L"Stats";
        case JOURNAL_TAB_SYSTEM: return L"System";
        default:                 return L"";
    }
}

static void AnnounceJournalChangeImpl() {
    if (!g_journalOpen.load()) return;
    JournalSnapshot snap;
    if (!ReadJournalSnapshot(snap)) return;

    const bool tabChanged = snap.tab != g_lastJournalTab;
    const bool firstRead = g_lastJournalTab < 0;
    if (tabChanged) {
        const wchar_t* name = JournalTabName(snap.tab);
        if (*name) { if (firstRead) SpeakQueue(name); else Speak(name); }
        g_lastJournalTab = snap.tab;
        g_lastJournalTitle.clear();
        g_lastJournalDesc.clear();
        g_lastSystemState = -1;
        g_lastSystemItem.clear();
        g_lastStatsCategory.clear();
    }

    if (snap.tab == JOURNAL_TAB_STATS && !snap.statsCategory.empty()) {
        if (snap.statsCategory != g_lastStatsCategory) {
            if (firstRead) SpeakQueue(snap.statsCategory); else Speak(snap.statsCategory);
            g_lastStatsCategory = snap.statsCategory;
            for (const auto& stat : snap.statsEntries) {
                if (stat.name.empty()) continue;
                std::wstring line = stat.name;
                if (!stat.value.empty()) line += L": " + stat.value;
                SpeakQueue(line); // s'enchaîne après la catégorie
            }
        }
    } else if (snap.tab == JOURNAL_TAB_SYSTEM && snap.systemState >= 0) {
        if (snap.systemState != g_lastSystemState) {
            g_lastSystemItem.clear();
            g_lastSystemState = snap.systemState;
        }
        if (!snap.systemItem.empty() && snap.systemItem != g_lastSystemItem) {
            if (firstRead) SpeakQueue(snap.systemItem); else Speak(snap.systemItem);
            g_lastSystemItem = snap.systemItem;
        }
    } else if (snap.tab == JOURNAL_TAB_QUESTS) {
        const bool titleChanged = !snap.questTitle.empty() && snap.questTitle != g_lastJournalTitle;
        if (titleChanged) {
            std::wstring announce = snap.questTitle;
            if (snap.questActive) announce += L", active";
            if (firstRead) SpeakQueue(announce); else Speak(announce);
            g_lastJournalTitle = snap.questTitle;
            g_lastJournalDesc.clear();

            // Objectifs : s'enchaînent après le titre
            for (const auto& obj : snap.objectives) {
                if (obj.text.empty()) continue;
                std::wstring objLine = obj.text;
                if (obj.completed)     objLine += L", completed";
                else if (obj.failed)   objLine += L", failed";
                SpeakQueue(objLine);
            }
        }
        if (!snap.questDesc.empty() && snap.questDesc != g_lastJournalDesc) {
            SpeakQueue(snap.questDesc); // s'enchaîne après titre + objectifs
            g_lastJournalDesc = snap.questDesc;
        }
    }
}

static void QueueJournalRead() {
    if (!g_journalOpen.load(std::memory_order_relaxed)) return;
    if (g_journalPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_journalPendingUIRead.store(false); return; }
    task->AddUITask([]() {
        g_journalPendingUIRead.store(false);
        if (g_journalOpen.load()) AnnounceJournalChangeImpl();
    });
}

static void StartJournalPolling() {
    if (g_journalPollThread.joinable()) { g_journalPollThread.request_stop(); g_journalPollThread.join(); }
    g_journalPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_journalOpen.load(std::memory_order_relaxed)) QueueJournalRead();
        }
    });
}

static void StopJournalPolling() {
    if (g_journalPollThread.joinable()) { g_journalPollThread.request_stop(); g_journalPollThread.join(); }
}

static void DiagnoseJournalNow() {
    if (!g_journalOpen.load(std::memory_order_relaxed)) {
        Speak(L"Journal closed");
        return;
    }
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        JournalSnapshot snap;
        if (!ReadJournalSnapshot(snap)) { Speak(L"Journal: no data"); return; }
        Speak(JournalTabName(snap.tab));
        if (snap.tab == JOURNAL_TAB_QUESTS && !snap.questTitle.empty())
            Speak(snap.questTitle);
    });
}

// VOCALISATION JOURNAL - FIN
