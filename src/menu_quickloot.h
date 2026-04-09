#pragma once

// VOCALISATION MENU QUICKLOOT IE - DEBUT
//
// QuickLoot IE est un menu Scaleform custom qui remplace le comportement
// "survol d'un conteneur" de Skyrim. Au lieu d'ouvrir le ContainerMenu vanilla
// quand tu pointes un conteneur ou un cadavre, le mod affiche une petite
// overlay (LootMenu) avec la liste des items. Tu navigues dedans avec les
// flèches haut/bas et tu prends avec E (ou R selon config MCM).
//
// Architecture interne (décompilée du SWF LootMenuIE.swf) :
// - Le menu est enregistré comme "LootMenu" dans le moteur (nom à confirmer
//   au premier test via les logs)
// - L'objet racine est exposé à _root.lootMenu
// - La liste des items : _root.lootMenu.itemList
// - Index sélectionné : _root.lootMenu.itemList.selectedIndex
// - Tableau de données : _root.lootMenu.itemList._dataProvider
// - Chaque entrée a : displayName, value, weight, count, stolen, etc.

// ---------------- État global ----------------

static std::atomic_bool g_quickLootOpen{false};
static std::atomic_bool g_quickLootPendingRead{false};
static std::jthread     g_quickLootPollThread;
static std::wstring     g_lastQuickLootItemAnnounce;
static std::wstring     g_lastQuickLootItemName;
static int              g_lastQuickLootItemCount{0};
static int              g_lastQuickLootSelectedIndex{-1};

// Nom du menu QuickLoot IE dans le moteur.
// Probablement "LootMenu" d'après l'analyse du SWF et des strings du DLL.
// À confirmer au premier test — si le log montre un nom différent, remplacer ici.
static const char* QUICKLOOT_MENU_NAME = "LootMenu";

// ---------------- Snapshot ----------------

struct QuickLootSnapshot {
    std::wstring itemText;
    int          count{0};
    std::wstring valueText;
    std::wstring weightText;
    bool         stolen{false};
    int          selectedIndex{-1};
};

static bool ReadQuickLootSnapshot(QuickLootSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(QUICKLOOT_MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    // Index sélectionné
    double num = 0.0;
    if (!GetGFxNumber(movie, "_root.lootMenu.itemList.selectedIndex", num)) {
        return false;
    }
    snap.selectedIndex = static_cast<int>(num);
    if (snap.selectedIndex < 0) return false;

    // Construire le path dynamique vers l'item sélectionné dans _dataProvider
    // Format : _root.lootMenu.itemList._dataProvider[N]
    // Puisque GFx ne supporte pas toujours l'indexation via GetVariable,
    // on utilise d'abord _dataProvider.N comme alternative (notation point)
    char pathBuf[256];

    // Nom de l'item
    std::string tmp;
    snprintf(pathBuf, sizeof(pathBuf), "_root.lootMenu.itemList._dataProvider.%d.displayName", snap.selectedIndex);
    if (GetGFxString(movie, pathBuf, tmp) && !tmp.empty()) {
        snap.itemText = ResolveUIString(movie, tmp);
    }

    // Quantité
    snprintf(pathBuf, sizeof(pathBuf), "_root.lootMenu.itemList._dataProvider.%d.count", snap.selectedIndex);
    if (GetGFxNumber(movie, pathBuf, num)) {
        snap.count = static_cast<int>(num);
    }

    // Valeur
    snprintf(pathBuf, sizeof(pathBuf), "_root.lootMenu.itemList._dataProvider.%d.value", snap.selectedIndex);
    if (GetGFxNumber(movie, pathBuf, num)) {
        snap.valueText = std::to_wstring(static_cast<int>(num));
    }

    // Poids
    snprintf(pathBuf, sizeof(pathBuf), "_root.lootMenu.itemList._dataProvider.%d.weight", snap.selectedIndex);
    if (GetGFxNumber(movie, pathBuf, num)) {
        // Formater avec 1 décimale si pas entier
        wchar_t wbuf[32];
        if (num == static_cast<int>(num)) {
            swprintf(wbuf, 32, L"%d", static_cast<int>(num));
        } else {
            swprintf(wbuf, 32, L"%.1f", num);
        }
        snap.weightText = wbuf;
    }

    // Volé ?
    snprintf(pathBuf, sizeof(pathBuf), "_root.lootMenu.itemList._dataProvider.%d.stolen", snap.selectedIndex);
    double stolenNum = 0.0;
    if (GetGFxNumber(movie, pathBuf, stolenNum)) {
        snap.stolen = (stolenNum > 0.5);
    }

    return !snap.itemText.empty();
}

// ---------------- Construction annonce ----------------

static std::wstring BuildQuickLootItemAnnouncement(const QuickLootSnapshot& snap) {
    if (snap.itemText.empty()) return L"";
    std::wstring msg = snap.itemText;
    if (snap.count > 1) {
        msg += L", " + std::to_wstring(snap.count);
    }
    if (!snap.valueText.empty() && snap.valueText != L"0") {
        msg += L", value " + snap.valueText;
    }
    if (!snap.weightText.empty() && snap.weightText != L"0") {
        msg += L", weight " + snap.weightText;
    }
    if (snap.stolen) {
        msg += L", stolen";
    }
    return msg;
}

// ---------------- Logique d'annonce ----------------

static void AnnounceQuickLootChangeImpl() {
    if (!g_quickLootOpen.load()) return;

    QuickLootSnapshot snap;
    if (!ReadQuickLootSnapshot(snap)) {
        // Liste vide ou pas encore prête — silencieux
        return;
    }

    const std::wstring announce = BuildQuickLootItemAnnouncement(snap);
    if (announce.empty()) return;

    const bool indexChanged = (snap.selectedIndex != g_lastQuickLootSelectedIndex);
    const bool nameChanged  = (snap.itemText != g_lastQuickLootItemName);
    const bool countChanged = (snap.count != g_lastQuickLootItemCount);

    if (!indexChanged && !nameChanged && !countChanged) return;

    const bool firstRead = g_lastQuickLootItemAnnounce.empty();

    // Si même item mais count qui change (après prise partielle) → dire juste le compte
    if (!firstRead && !indexChanged && nameChanged == false && countChanged) {
        if (snap.count > 1)
            Speak(std::to_wstring(snap.count));
        else if (snap.count == 1)
            Speak(L"1");
        g_lastQuickLootItemCount = snap.count;
        return;
    }

    if (firstRead) {
        SpeakQueue(announce);
    } else {
        Speak(announce);
    }

    g_lastQuickLootItemAnnounce = announce;
    g_lastQuickLootItemName = snap.itemText;
    g_lastQuickLootItemCount = snap.count;
    g_lastQuickLootSelectedIndex = snap.selectedIndex;
}

// ---------------- Polling et déclencheur ----------------

static void QueueQuickLootRead() {
    if (!g_quickLootOpen.load(std::memory_order_relaxed)) return;
    if (g_quickLootPendingRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_quickLootPendingRead.store(false); return; }
    task->AddUITask([]() {
        g_quickLootPendingRead.store(false);
        if (g_quickLootOpen.load()) AnnounceQuickLootChangeImpl();
    });
}

static void StartQuickLootPolling() {
    if (g_quickLootPollThread.joinable()) {
        g_quickLootPollThread.request_stop();
        g_quickLootPollThread.join();
    }
    g_quickLootPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_quickLootOpen.load(std::memory_order_relaxed)) QueueQuickLootRead();
        }
    });
}

static void StopQuickLootPolling() {
    if (g_quickLootPollThread.joinable()) {
        g_quickLootPollThread.request_stop();
        g_quickLootPollThread.join();
    }
}

// ---------------- Diagnostic ----------------
// À appeler au premier test pour logger les paths GFx disponibles et confirmer
// que nos hypothèses sur la structure du menu sont correctes.

static void DiagnoseQuickLootNow() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        auto ui = RE::UI::GetSingleton();
        if (!ui) { LOG("QuickLoot diag: no UI"); return; }
        auto menu = ui->GetMenu(QUICKLOOT_MENU_NAME);
        if (!menu) { LOG("QuickLoot diag: menu '{}' not open", QUICKLOOT_MENU_NAME); return; }
        RE::GFxMovieView* movie = menu->uiMovie.get();
        if (!movie) { LOG("QuickLoot diag: no movie"); return; }

        double idx = -1, length = -1;
        std::string firstName;
        bool hasLootMenu = GetGFxNumber(movie, "_root.lootMenu.itemList.selectedIndex", idx);
        GetGFxNumber(movie, "_root.lootMenu.itemList._dataProvider.length", length);
        GetGFxString(movie, "_root.lootMenu.itemList._dataProvider.0.displayName", firstName);

        LOG("QuickLoot diag: hasLootMenu={} selectedIndex={} listLength={} firstItem='{}'",
            hasLootMenu, (int)idx, (int)length, firstName);
    });
}

// VOCALISATION MENU QUICKLOOT IE - FIN
