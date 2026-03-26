#pragma once

// VOCALISATION MENU CRAFTING (forge, meule, établi, fonderie, tannage) - DEBUT

static std::atomic_bool g_craftingOpen{false};
static std::atomic_bool g_craftingPendingUIRead{false};
static std::jthread     g_craftingPollThread;
static std::wstring     g_lastCraftingCat;
static std::wstring     g_lastCraftingItemAnnounce;
static std::wstring     g_lastCraftingDesc;
static bool             g_craftingFirstReadDone{false};

struct CraftingSnapshot {
    std::wstring itemText;
    std::wstring catText;
    std::wstring descText;
    std::wstring ingredientsText;  // matériaux requis
    std::wstring valueText;
    std::wstring weightText;
    std::wstring weaponDamageText;
    std::wstring apparelArmorText;
    bool         inCategoryMode{true};  // true = dans les catégories, false = dans les items
};

static bool g_craftingIsSimpleList{false};  // true = tannerie/meule/établi (pas de catégories)
static bool g_craftingModeDetected{false};

// Lecture sûre pour mode simple — ne lit QUE les textfields de ItemInfo (pas de getters AS2)
static bool ReadCraftingSnapshotSimple(CraftingSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::CraftingMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    std::string tmp;

    // Lire le nom de l'item — essayer la liste d'abord, puis ItemInfo
    if (GetGFxString(movie, "_root.Menu.ItemListTweener.List_mc.selectedEntry.text", tmp) && !tmp.empty())
        snap.itemText = StripMarkupForSpeech(Utf8ToWString(tmp));
    else if (GetGFxString(movie, "_root.Menu.ItemInfoHolder.ItemInfo.ItemText.ItemTextField.text", tmp) && !tmp.empty())
        snap.itemText = StripMarkupForSpeech(Utf8ToWString(tmp));

    // Stats
    auto readField = [&](const char* path, std::wstring& out) {
        std::string s;
        if (GetGFxString(movie, path, s) && !s.empty())
            out = Utf8ToWString(s);
    };
    readField("_root.Menu.ItemInfoHolder.ItemInfo.ItemValueText.text", snap.valueText);
    readField("_root.Menu.ItemInfoHolder.ItemInfo.WeaponDamageValue.text", snap.weaponDamageText);
    readField("_root.Menu.ItemInfoHolder.ItemInfo.ApparelArmorValue.text", snap.apparelArmorText);

    snap.valueText = SanitizeNumericText(snap.valueText);
    snap.weaponDamageText = StripMarkupForSpeech(snap.weaponDamageText);
    snap.apparelArmorText = StripMarkupForSpeech(snap.apparelArmorText);

    // Matériaux requis
    std::string ingredients;
    if (GetGFxString(movie, "_root.Menu.ItemInfoHolder.AdditionalDescriptionHolder.AdditionalDescription.text", ingredients) && !ingredients.empty())
        snap.ingredientsText = StripMarkupForSpeech(Utf8ToWString(ingredients));

    snap.inCategoryMode = false;
    return !snap.itemText.empty();
}

// Lecture pour mode forge (catégories) — utilise CategoryList et selectedEntry
static bool ReadCraftingSnapshotForge(CraftingSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::CraftingMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    std::string tmp;

    // Item sélectionné — préférer l'ItemCard, ajouter la quantité si > 1
    std::string itemCardText, listText;
    GetGFxString(movie, "_root.Menu.ItemInfoHolder.ItemInfo.ItemText.ItemTextField.text", itemCardText);
    GetGFxString(movie, "_root.Menu.CategoryList.ItemsList.selectedEntry.text", listText);
    double countNum = 0;
    GetGFxNumber(movie, "_root.Menu.CategoryList.ItemsList.selectedEntry.count", countNum);
    if (!itemCardText.empty())
        snap.itemText = StripMarkupForSpeech(Utf8ToWString(itemCardText));
    else if (!listText.empty())
        snap.itemText = ResolveUIString(movie, listText);
    // Ajouter la quantité produite si > 1
    if (static_cast<int>(countNum) > 1)
        snap.itemText += L" (" + std::to_wstring(static_cast<int>(countNum)) + L")";

    // Catégorie
    if (GetGFxString(movie, "_root.Menu.CategoryList.CategoriesList.centeredEntry.text", tmp) && !tmp.empty())
        snap.catText = ResolveUIString(movie, tmp);

    // Mode catégorie ou items
    double panelState = 0;
    GetGFxNumber(movie, "_root.Menu.CategoryList.currentState", panelState);
    snap.inCategoryMode = (static_cast<int>(panelState) != 2);

    // Stats
    auto readField = [&](const char* path, std::wstring& out) {
        std::string s;
        if (GetGFxString(movie, path, s) && !s.empty())
            out = Utf8ToWString(s);
    };
    readField("_root.Menu.ItemInfoHolder.ItemInfo.ItemValueText.text", snap.valueText);
    readField("_root.Menu.ItemInfoHolder.ItemInfo.WeaponDamageValue.text", snap.weaponDamageText);
    readField("_root.Menu.ItemInfoHolder.ItemInfo.ApparelArmorValue.text", snap.apparelArmorText);

    snap.valueText = SanitizeNumericText(snap.valueText);
    snap.weaponDamageText = StripMarkupForSpeech(snap.weaponDamageText);
    snap.apparelArmorText = StripMarkupForSpeech(snap.apparelArmorText);

    // Matériaux requis
    std::string ingredients;
    if (GetGFxString(movie, "_root.Menu.AdditionalDescriptionHolder.AdditionalDescription.text", ingredients) && !ingredients.empty())
        snap.ingredientsText = StripMarkupForSpeech(Utf8ToWString(ingredients));
    else if (GetGFxString(movie, "_root.Menu.ItemInfoHolder.AdditionalDescriptionHolder.AdditionalDescription.text", ingredients) && !ingredients.empty())
        snap.ingredientsText = StripMarkupForSpeech(Utf8ToWString(ingredients));

    return !snap.itemText.empty() || !snap.catText.empty();
}

// Détecte le mode (forge vs simple) via MenuType (simple double read, pas de getter)
static void DetectCraftingMode() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::CraftingMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    // Détection : si ItemListTweener existe ET currentState n'est pas un panel mode → simple
    // Si CategoryList.currentState == 1 (ONE_PANEL) ou 2 (TWO_PANELS) → forge/tannerie (avec catégories)
    double panelState = 0;
    bool hasPanelState = GetGFxNumber(movie, "_root.Menu.CategoryList.currentState", panelState);
    if (hasPanelState && panelState >= 1.0) {
        g_craftingIsSimpleList = false;  // mode forge/catégories (forge, tannerie)
    } else {
        g_craftingIsSimpleList = true;   // mode simple (meule, établi)
    }
    g_craftingModeDetected = true;
    LOG("Crafting mode detected: {} (panelState={})", g_craftingIsSimpleList ? "simple" : "forge", panelState);
}

static void AnnounceCraftingChangeImpl() {
    if (!g_craftingOpen.load()) return;

    // Détecter le mode si pas encore fait
    if (!g_craftingModeDetected) DetectCraftingMode();

    CraftingSnapshot snap;
    bool ok = g_craftingIsSimpleList ? ReadCraftingSnapshotSimple(snap) : ReadCraftingSnapshotForge(snap);
    if (!ok) return;

    const bool catChanged = !snap.catText.empty() && snap.catText != g_lastCraftingCat;
    const bool itemChanged = !snap.itemText.empty() && snap.itemText != g_lastCraftingItemAnnounce;

    const bool firstRead = !g_craftingFirstReadDone;

    // Mode simple (tannerie, meule, etc.) — pas de catégories, lire l'item directement
    if (g_craftingIsSimpleList) {
        if (itemChanged || firstRead) {
            std::wstring announce = snap.itemText;
            auto isZero = [](const std::wstring& s) {
                try { return std::stof(s) == 0.0f; } catch (...) { return s.empty(); }
            };
            if (!snap.weaponDamageText.empty() && !isZero(snap.weaponDamageText))
                announce += L", damage " + snap.weaponDamageText;
            if (!snap.apparelArmorText.empty() && !isZero(snap.apparelArmorText))
                announce += L", armor " + snap.apparelArmorText;
            if (firstRead) SpeakQueue(announce); else Speak(announce);
            g_lastCraftingItemAnnounce = snap.itemText;
            g_lastCraftingDesc.clear();
            g_craftingFirstReadDone = true;
        }
        if (!snap.ingredientsText.empty() && snap.ingredientsText != g_lastCraftingDesc) {
            SpeakQueue(snap.ingredientsText);
            g_lastCraftingDesc = snap.ingredientsText;
        }
        return;
    }

    // En mode catégorie : lire seulement la catégorie
    if (snap.inCategoryMode) {
        bool returnedFromItems = !g_lastCraftingItemAnnounce.empty();
        if (catChanged || firstRead || returnedFromItems) {
            if (!snap.catText.empty()) {
                if (firstRead) SpeakQueue(snap.catText); else Speak(snap.catText);
            }
            g_lastCraftingCat = snap.catText;
            g_craftingFirstReadDone = true;
        }
        g_lastCraftingItemAnnounce.clear();
        g_lastCraftingDesc.clear();
        return;
    }

    // En mode items : lire les items
    if (catChanged) {
        Speak(snap.catText);
        g_lastCraftingCat = snap.catText;
    }
    const bool ingredientsChanged = !snap.ingredientsText.empty() && snap.ingredientsText != g_lastCraftingDesc;
    if (itemChanged || ingredientsChanged) {
        // Construire l'annonce avec stats
        std::wstring announce = snap.itemText;
        auto isZero = [](const std::wstring& s) {
            try { return std::stof(s) == 0.0f; } catch (...) { return s.empty(); }
        };
        if (!snap.weaponDamageText.empty() && !isZero(snap.weaponDamageText))
            announce += L", damage " + snap.weaponDamageText;
        if (!snap.apparelArmorText.empty() && !isZero(snap.apparelArmorText))
            announce += L", armor " + snap.apparelArmorText;

        if (firstRead) SpeakQueue(announce); else Speak(announce);
        g_lastCraftingItemAnnounce = snap.itemText;
        g_lastCraftingDesc.clear();
    }
    // Matériaux requis
    if (!snap.ingredientsText.empty() && snap.ingredientsText != g_lastCraftingDesc) {
        SpeakQueue(snap.ingredientsText);
        g_lastCraftingDesc = snap.ingredientsText;
    }
}

static void QueueCraftingRead() {
    if (!g_craftingOpen.load(std::memory_order_relaxed)) return;
    if (g_craftingPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_craftingPendingUIRead.store(false); return; }
    task->AddUITask([]() {
        g_craftingPendingUIRead.store(false);
        if (g_craftingOpen.load()) AnnounceCraftingChangeImpl();
    });
}

static void StartCraftingPolling() {
    if (g_craftingPollThread.joinable()) { g_craftingPollThread.request_stop(); g_craftingPollThread.join(); }
    g_craftingPollThread = std::jthread([](std::stop_token st) {
        // Attendre 500ms pour laisser le menu s'initialiser
        for (int i = 0; i < 5 && !st.stop_requested(); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Détecter le mode via AddUITask et attendre le résultat
        if (!st.stop_requested() && g_craftingOpen.load(std::memory_order_relaxed)) {
            std::atomic_bool detected{false};
            auto* task = SKSE::GetTaskInterface();
            if (task) {
                task->AddUITask([&detected]() {
                    DetectCraftingMode();
                    detected.store(true);
                });
            }
            // Attendre que la détection soit faite (max 2 secondes)
            for (int i = 0; i < 40 && !detected.load() && !st.stop_requested(); i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        LOG("Crafting: {} mode detected, starting polling", g_craftingIsSimpleList ? "simple" : "forge");
        // Mode forge : polling normal
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_craftingOpen.load(std::memory_order_relaxed)) QueueCraftingRead();
        }
    });
}

static void StopCraftingPolling() {
    if (g_craftingPollThread.joinable()) { g_craftingPollThread.request_stop(); g_craftingPollThread.join(); }
}

// VOCALISATION MENU CRAFTING - FIN
