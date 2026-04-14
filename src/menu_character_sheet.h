#pragma once

// VOCALISATION CHARACTER SHEET (mod "Skyrim Character Sheet") - DEBUT
//
// Le mod ajoute deux menus custom :
//
// 1) ShowStats : stats du joueur sur un ou plusieurs onglets parmi 6
//    (Player, Attack, Defence, Magic, Warrior, Thief) + bandeau d'infos
//    (nom, niveau, race, perks). Les onglets sont cycles par la touche N.
//    Ouvert par U. Une seule ScrollingList visible a la fois.
//
// 2) ShowFactions : stats des factions sur 3 onglets (Faction, Thane,
//    Champion). Cycle par N/P. Ouvert automatiquement quand on cycle
//    depuis ShowStats.
//
// Le mod ne gere pas la navigation clavier dans ses ScrollingList
// (selectedIndex reste a -1). On maintient donc notre propre index pour
// permettre au joueur de defiler via fleche Haut/Bas. Reset a chaque
// changement de tab ou de menu.
//
// Structure GFx commune (verifiee via ffdec -dumpSWF) :
//   _root.rootObj.title                = titre du menu
//   _root.rootObj.menuClose            = bouton fermer
//   _root.rootObj.<ItemList>           = ScrollingList par onglet
//   _root.rootObj.<Header>             = TextField header par onglet
// Chaque entree ScrollingList = { displayName, displayValue, iconKey, iconScale }

static constexpr const char* CHARSHEET_MENU_SHOWSTATS    = "ShowStats";
static constexpr const char* CHARSHEET_MENU_SHOWFACTIONS = "ShowFactions";

// Table : menu -> (listNames, headerNames)
struct CharSheetMenuDef {
    const char* menuName;
    std::vector<const char*> listNames;
    std::vector<const char*> headerNames;
};

static const std::vector<CharSheetMenuDef>& GetCharSheetMenuDefs() {
    static const std::vector<CharSheetMenuDef> defs = {
        {
            CHARSHEET_MENU_SHOWSTATS,
            {
                "playerItemList",
                "attackItemList",
                "defenceItemList",
                "perksMagicItemList",
                "perksWarriorItemList",
                "perksThiefItemList"
            },
            {
                "playerValuesHeader",
                "playerAttackHeader",
                "playerDefenceHeader",
                "playerPerksMagicHeader",
                "playerPerksWarriorHeader",
                "playerPerksThiefHeader"
            }
        },
        {
            CHARSHEET_MENU_SHOWFACTIONS,
            {
                "factionItemList",
                "thaneItemList",
                "championItemList"
            },
            {
                "factionValuesHeader",
                "factionThanesHeader",
                "factionChampionHeader"
            }
        }
    };
    return defs;
}

// Retourne la def pour le menu actuellement ouvert, ou nullptr
static const CharSheetMenuDef* GetActiveCharSheetDef() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return nullptr;
    for (const auto& def : GetCharSheetMenuDefs()) {
        if (ui->IsMenuOpen(def.menuName)) return &def;
    }
    return nullptr;
}

static std::atomic_bool g_charSheetOpen{false};
static std::atomic_bool g_charSheetPendingRead{false};
static std::jthread     g_charSheetPollThread;
static std::wstring     g_lastCharSheetKey;
static std::wstring     g_lastCharSheetTab;
static std::string      g_lastCharSheetListName;
static std::string      g_lastCharSheetMenuName;  // pour detecter changement de menu (ShowStats -> ShowFactions)
static std::atomic_int  g_charSheetOurIndex{0};
// Le mod affiche toutes les colonnes/onglets simultanement a l'ecran (sans
// les cycler). On maintient notre propre index de colonne active pour
// permettre au joueur de changer d'onglet via fleche Gauche/Droite.
// Index dans CharSheetMenuDef.listNames du menu courant.
static std::atomic_int  g_charSheetOurTabIndex{0};

struct CharSheetEntry {
    std::wstring name;
    std::wstring value;
};

struct CharSheetSnapshot {
    const char*  menuName{nullptr};   // ShowStats ou ShowFactions
    std::wstring tabLabel;            // header du tab actif
    std::string  activeListName;      // nom GFx de la liste active
    int          selectedIndex{-1};
    CharSheetEntry entry;
    size_t       totalEntries{0};
};

// Lit la snapshot courante du menu ouvert (ShowStats ou ShowFactions).
static bool ReadCharSheetSnapshot(CharSheetSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;

    const CharSheetMenuDef* def = GetActiveCharSheetDef();
    if (!def) return false;
    snap.menuName = def->menuName;

    auto menu = ui->GetMenu(def->menuName);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    // Le mod affiche toutes les colonnes simultanement. On utilise notre
    // propre index de tab (g_charSheetOurTabIndex) pour choisir quelle
    // colonne lire, clampe a [0, listNames.size()-1].
    int tabIdx = g_charSheetOurTabIndex.load();
    int maxTab = static_cast<int>(def->listNames.size());
    if (tabIdx >= maxTab) { tabIdx = maxTab - 1; g_charSheetOurTabIndex.store(tabIdx); }
    if (tabIdx < 0)       { tabIdx = 0;          g_charSheetOurTabIndex.store(tabIdx); }

    // 1. Header du tab courant
    if (static_cast<size_t>(tabIdx) < def->headerNames.size()) {
        const char* headerName = def->headerNames[tabIdx];
        std::string textPath = std::string("_root.rootObj.") + headerName + ".text";
        std::string text;
        if (GetGFxString(movie, textPath.c_str(), text) && !text.empty()) {
            snap.tabLabel = ResolveUIString(movie, text);
        }
    }

    // 2. Liste du tab courant
    {
        const char* listName = def->listNames[tabIdx];
        const std::string listRoot = std::string("_root.rootObj.") + listName;
        snap.activeListName = listName;

        // Le mod maintient selectedIndex a -1 (pas de navigation clavier).
        // On utilise notre propre index (g_charSheetOurIndex), clampe a [0, length-1].
        int ourIdx = g_charSheetOurIndex.load();
        double lenD = 0.0;
        GetGFxNumber(movie, (listRoot + "._dataProvider.length").c_str(), lenD);
        int totalLen = static_cast<int>(lenD);
        if (totalLen > 0) {
            if (ourIdx >= totalLen) { ourIdx = totalLen - 1; g_charSheetOurIndex.store(ourIdx); }
            if (ourIdx < 0)         { ourIdx = 0;             g_charSheetOurIndex.store(ourIdx); }
        }
        snap.selectedIndex = ourIdx;
        snap.totalEntries = static_cast<size_t>(totalLen);

        // DataProvider : acces via GetElement (Array) ou fallback chemin direct
        RE::GFxValue dpVal;
        bool dpOk = SafeGetVariable(movie, dpVal, (listRoot + "._dataProvider").c_str());
        bool dpIsArray = dpOk && SafeIsArray(dpVal);

        if (dpIsArray && snap.selectedIndex >= 0 &&
            static_cast<size_t>(snap.selectedIndex) < snap.totalEntries) {
            RE::GFxValue entryVal;
            if (dpVal.GetElement(static_cast<std::uint32_t>(snap.selectedIndex), &entryVal) &&
                SafeIsObject(entryVal)) {
                RE::GFxValue nameVal, valueVal;
                if (entryVal.GetMember("displayName", &nameVal) && SafeIsString(nameVal)) {
                    std::string s = SafeGetString(nameVal);
                    if (!s.empty()) snap.entry.name = ResolveUIString(movie, s);
                }
                if (entryVal.GetMember("displayValue", &valueVal) && SafeIsString(valueVal)) {
                    std::string s = SafeGetString(valueVal);
                    if (!s.empty()) snap.entry.value = ResolveUIString(movie, s);
                }
            }
        } else if (snap.selectedIndex >= 0 && totalLen > 0) {
            std::string entryPath = listRoot + "._dataProvider." + std::to_string(snap.selectedIndex);
            std::string nameStr, valueStr;
            if (GetGFxString(movie, (entryPath + ".displayName").c_str(), nameStr) && !nameStr.empty())
                snap.entry.name = ResolveUIString(movie, nameStr);
            if (GetGFxString(movie, (entryPath + ".displayValue").c_str(), valueStr) && !valueStr.empty())
                snap.entry.value = ResolveUIString(movie, valueStr);
        }
    }

    return !snap.activeListName.empty();
}

// Annonce a l'ouverture du menu : titre + infos joueur + tab + premiere stat.
// Ne lit les infos joueur (name/level/race) que pour ShowStats (ShowFactions
// n'a que factionCount/thaneCount comme infos d'entete).
static void AnnounceCharSheetOpenImpl() {
    const CharSheetMenuDef* def = GetActiveCharSheetDef();
    if (!def) return;
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(def->menuName);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    // Titre du menu
    std::string title;
    if (GetGFxString(movie, "_root.rootObj.title.text", title) && !title.empty()) {
        SpeakQueue(ResolveUIString(movie, title));
    }

    auto readPair = [movie](const char* labelPath, const char* valuePath) -> std::wstring {
        std::string label, value;
        bool hasLabel = GetGFxString(movie, labelPath, label) && !label.empty();
        bool hasValue = GetGFxString(movie, valuePath, value) && !value.empty();
        if (!hasLabel && !hasValue) return L"";
        std::wstring result;
        if (hasLabel) result = ResolveUIString(movie, label);
        if (hasValue) {
            if (!result.empty()) result += L": ";
            result += ResolveUIString(movie, value);
        }
        return result;
    };

    if (def->menuName == std::string(CHARSHEET_MENU_SHOWSTATS)) {
        // Infos joueur (nom/niveau/race)
        std::wstring line;
        line = readPair("_root.rootObj.name.text", "_root.rootObj.nameValue.text");
        if (!line.empty()) SpeakQueue(line);
        line = readPair("_root.rootObj.level.text", "_root.rootObj.levelValue.text");
        if (!line.empty()) SpeakQueue(line);
        line = readPair("_root.rootObj.race.text", "_root.rootObj.raceValue.text");
        if (!line.empty()) SpeakQueue(line);
    } else if (def->menuName == std::string(CHARSHEET_MENU_SHOWFACTIONS)) {
        // Infos factions : compte des factions/thanes
        std::wstring line;
        line = readPair("_root.rootObj.factionCount.text", "_root.rootObj.factionCountValue.text");
        if (!line.empty()) SpeakQueue(line);
        line = readPair("_root.rootObj.thaneCount.text", "_root.rootObj.thaneCountValue.text");
        if (!line.empty()) SpeakQueue(line);
    }

    // Annoncer le tab actif + la premiere stat
    CharSheetSnapshot snap;
    if (ReadCharSheetSnapshot(snap)) {
        if (!snap.tabLabel.empty()) SpeakQueue(snap.tabLabel);
        if (!snap.entry.name.empty()) {
            std::wstring msg = snap.entry.name;
            if (!snap.entry.value.empty()) msg += L": " + snap.entry.value;
            SpeakQueue(msg);
            g_lastCharSheetKey = std::to_wstring(snap.selectedIndex) + L"|" +
                                 Utf8ToWString(snap.activeListName) + L"|" +
                                 snap.entry.name + L"|" + snap.entry.value;
            g_lastCharSheetTab = snap.tabLabel;
            g_lastCharSheetListName = snap.activeListName;
            g_lastCharSheetMenuName = def->menuName;
        }
    }
}

// Annonce une stat lors d'une navigation (fleches, N/P, changement auto via
// polling). Detecte 3 types de changements :
//   - menu (ShowStats <-> ShowFactions)
//   - tab/colonne (fleche Gauche/Droite)
//   - stat (fleche Haut/Bas)
// Dans tous les cas ou le tab change, on annonce le label de colonne
// (ex: "Defense", "Mage") AVANT la stat. Si la colonne est vide, on annonce
// juste le label et "empty" pour que le joueur sache ou il est.
static void AnnounceCharSheetChangeImpl() {
    if (!g_charSheetOpen.load()) return;

    CharSheetSnapshot snap;
    if (!ReadCharSheetSnapshot(snap)) return;
    if (snap.activeListName.empty()) return;

    bool menuChanged = !g_lastCharSheetMenuName.empty() &&
                       snap.menuName && g_lastCharSheetMenuName != snap.menuName;
    bool listChanged = !g_lastCharSheetListName.empty() &&
                       snap.activeListName != g_lastCharSheetListName;
    bool tabChanged  = !g_lastCharSheetTab.empty() &&
                       snap.tabLabel != g_lastCharSheetTab;

    g_lastCharSheetListName = snap.activeListName;
    g_lastCharSheetMenuName = snap.menuName ? snap.menuName : "";

    // Annonce prioritaire : changement de colonne (tab) — toujours annoncer
    // le label de la nouvelle colonne, meme si elle est vide.
    if (menuChanged || listChanged || tabChanged) {
        if (!snap.tabLabel.empty()) {
            Speak(snap.tabLabel);
            g_lastCharSheetTab = snap.tabLabel;
        }

        if (!snap.entry.name.empty()) {
            std::wstring msg = snap.entry.name;
            if (!snap.entry.value.empty()) msg += L": " + snap.entry.value;
            SpeakQueue(msg);
        } else if (snap.totalEntries == 0) {
            SpeakQueue(L"empty");
        }

        g_lastCharSheetKey = std::to_wstring(snap.selectedIndex) + L"|" +
                             Utf8ToWString(snap.activeListName) + L"|" +
                             snap.entry.name + L"|" + snap.entry.value;
        LOG("CharSheet: tab change menu='{}' list='{}' idx={}/{} name='{}' value='{}'",
            snap.menuName ? snap.menuName : "?",
            snap.activeListName, snap.selectedIndex, snap.totalEntries,
            WStringToUtf8(snap.entry.name), WStringToUtf8(snap.entry.value));
        return;
    }

    // Meme colonne : annoncer seulement si stat a change (dedup par cle)
    if (snap.entry.name.empty()) return;

    std::wstring key = std::to_wstring(snap.selectedIndex) + L"|" +
                       Utf8ToWString(snap.activeListName) + L"|" +
                       snap.entry.name + L"|" + snap.entry.value;
    if (key == g_lastCharSheetKey) return;
    g_lastCharSheetKey = key;

    std::wstring msg = snap.entry.name;
    if (!snap.entry.value.empty()) msg += L": " + snap.entry.value;
    Speak(msg);

    LOG("CharSheet: menu='{}' list='{}' idx={}/{} name='{}' value='{}'",
        snap.menuName ? snap.menuName : "?",
        snap.activeListName, snap.selectedIndex, snap.totalEntries,
        WStringToUtf8(snap.entry.name), WStringToUtf8(snap.entry.value));
}

static void QueueCharSheetRead() {
    if (g_charSheetPendingRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        g_charSheetPendingRead.store(false);
        if (g_charSheetOpen.load()) AnnounceCharSheetChangeImpl();
    });
}

static void QueueCharSheetOpen() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        if (g_charSheetOpen.load()) AnnounceCharSheetOpenImpl();
    });
}

static void StopCharSheetPoll() {
    if (g_charSheetPollThread.joinable()) {
        g_charSheetPollThread.request_stop();
        g_charSheetPollThread.join();
    }
}

static void StartCharSheetPoll() {
    StopCharSheetPoll();
    g_charSheetPollThread = std::jthread([](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!g_charSheetOpen.load()) break;
            QueueCharSheetRead();
        }
    });
}

// Reset complet de l'etat (appele a l'ouverture/fermeture de chaque menu)
static void ResetCharSheetState() {
    g_lastCharSheetKey.clear();
    g_lastCharSheetTab.clear();
    g_lastCharSheetListName.clear();
    g_lastCharSheetMenuName.clear();
    g_charSheetOurIndex.store(0);
    g_charSheetOurTabIndex.store(0);
}

// VOCALISATION CHARACTER SHEET - FIN
