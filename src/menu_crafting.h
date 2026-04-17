#pragma once

// VOCALISATION MENU CRAFTING (forge, meule, établi, fonderie, tannage) - DEBUT

static std::atomic_bool g_craftingOpen{false};
static std::atomic_bool g_craftingPendingUIRead{false};
static std::jthread     g_craftingPollThread;
static std::wstring     g_lastCraftingCat;
static std::wstring     g_lastCraftingItemAnnounce;
static std::wstring     g_lastCraftingDesc;
static std::atomic_bool g_craftingFirstReadDone{false};
// Mis à true par l'input handler clavier (Up/Down/W/S etc.) pour forcer la relecture
// même quand l'item résultant est textuellement identique (recettes dupliquées).
static std::atomic_bool g_craftingForceAnnounce{false};

struct CraftingSnapshot {
    std::wstring itemText;
    std::wstring catText;
    std::wstring descText;
    std::wstring ingredientsText;  // matériaux requis
    std::wstring valueText;
    std::wstring weightText;
    std::wstring weaponDamageText;
    std::wstring apparelArmorText;
    std::wstring enchantmentText;  // effets enchantements (ex: "Carry Weight +50") pour sacs à dos modés et items enchantés
    bool         inCategoryMode{true};  // true = dans les catégories, false = dans les items
};

// Lit le texte d'enchantement (effets) depuis l'ItemCard.
// L'ItemCard du crafting bascule via gotoAndStop entre frames Apparel/Weapon/Apparel_Enchanted/Weapon_Enchanted.
// On essaie les deux labels (armure/arme), htmlText puis text en fallback.
static void ReadCraftingEnchantment(RE::GFxMovieView* movie, std::wstring& out) {
    const char* paths[] = {
        "_root.Menu.ItemInfoHolder.ItemInfo.ApparelEnchantedLabel.htmlText",
        "_root.Menu.ItemInfoHolder.ItemInfo.ApparelEnchantedLabel.text",
        "_root.Menu.ItemInfoHolder.ItemInfo.WeaponEnchantedLabel.htmlText",
        "_root.Menu.ItemInfoHolder.ItemInfo.WeaponEnchantedLabel.text",
    };
    std::string raw;
    for (auto* p : paths) {
        if (GetGFxString(movie, p, raw) && !raw.empty()) {
            out = StripMarkupForSpeech(Utf8ToWString(raw));
            if (!out.empty()) return;
        }
    }
}

static std::atomic_bool g_craftingIsSimpleList{false};  // true = tannerie/meule/établi (pas de catégories)
static std::atomic_bool g_craftingModeDetected{false};

// Lecture sûre pour mode simple — ne lit QUE les textfields de ItemInfo (pas de getters AS2)
static bool ReadCraftingSnapshotSimple(CraftingSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::CraftingMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
    std::string tmp;

    // Lire le nom de l'item
    // SkyUI : CategoryList.itemList.selectedEntry.text
    // Vanilla : ItemListTweener.List_mc.selectedEntry.text
    const char* itemPaths[] = {
        skyui ? "_root.Menu.CategoryList.itemList.selectedEntry.text" : "_root.Menu.ItemListTweener.List_mc.selectedEntry.text",
        "_root.Menu.ItemInfoHolder.ItemInfo.ItemText.ItemTextField.text",
    };
    for (auto* p : itemPaths) {
        if (GetGFxString(movie, p, tmp) && !tmp.empty()) {
            snap.itemText = StripMarkupForSpeech(Utf8ToWString(tmp));
            break;
        }
    }

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

    // Effets enchantements (sacs à dos modés, items enchantés)
    ReadCraftingEnchantment(movie, snap.enchantmentText);

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

    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
    std::string tmp;

    // CategoryList fonctionne dans les deux modes (vérifié via logs)
    const char* catListBase = "_root.Menu.CategoryList";

    // Catégorie
    if (skyui) {
        std::string catLabelPath = std::string(catListBase) + ".categoryLabel.textField.text";
        std::string catEntryPath = std::string(catListBase) + ".CategoriesList.selectedEntry.text";
        if (GetGFxString(movie, catLabelPath.c_str(), tmp) && !tmp.empty())
            snap.catText = ResolveUIString(movie, tmp);
        else if (GetGFxString(movie, catEntryPath.c_str(), tmp) && !tmp.empty())
            snap.catText = ResolveUIString(movie, tmp);
    } else {
        if (GetGFxString(movie, "_root.Menu.CategoryList.CategoriesList.centeredEntry.text", tmp) && !tmp.empty())
            snap.catText = ResolveUIString(movie, tmp);
    }

    // Item sélectionné — lire depuis la liste d'abord (vide si focus sur catégories)
    {
        std::string listText, itemCardText;
        std::string listTextPath = std::string(catListBase) + (skyui ? ".itemList.selectedEntry.text" : ".ItemsList.selectedEntry.text");
        GetGFxString(movie, listTextPath.c_str(), listText);

        if (!listText.empty()) {
            // Focus sur les items — lire depuis la liste
            snap.itemText = ResolveUIString(movie, listText);
            snap.inCategoryMode = false;
        } else {
            // Liste vide = focus sur les catégories — ne PAS lire l'ItemCard (elle garde l'ancien)
            snap.inCategoryMode = true;
        }

        // Quantité depuis la liste
        std::string listCountPath = std::string(catListBase) + (skyui ? ".itemList.selectedEntry.count" : ".ItemsList.selectedEntry.count");
        double countNum = 0;
        GetGFxNumber(movie, listCountPath.c_str(), countNum);
        if (static_cast<int>(countNum) > 1 && !snap.itemText.empty())
            snap.itemText += L" (" + std::to_wstring(static_cast<int>(countNum)) + L")";
    }

    // Vanilla : utiliser aussi panelState pour le mode catégorie
    if (!skyui) {
        double panelState = 0;
        std::string statePath = std::string(catListBase) + ".currentState";
        GetGFxNumber(movie, statePath.c_str(), panelState);
        snap.inCategoryMode = (static_cast<int>(panelState) != 2);
    }

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

    // Effets enchantements (sacs à dos modés, items enchantés)
    ReadCraftingEnchantment(movie, snap.enchantmentText);

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


    // Détection : si currentState >= 1 → forge/tannerie (avec catégories), sinon simple (meule, établi)
    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
    const char* statePath = "_root.Menu.CategoryList.currentState";
    double panelState = 0;
    bool hasPanelState = GetGFxNumber(movie, statePath, panelState);
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
    // Force = touche de navigation clavier pressée → on relit même si texte identique
    // (cas des recettes dupliquées affichées plusieurs fois dans la liste)
    const bool forced = g_craftingForceAnnounce.exchange(false);
    const bool itemChanged = !snap.itemText.empty() && (snap.itemText != g_lastCraftingItemAnnounce || forced);

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
            if (!snap.enchantmentText.empty()) SpeakQueue(snap.enchantmentText);
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

    // En mode catégorie (vanilla uniquement) : lire seulement la catégorie
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
        g_lastCraftingDesc.clear();
        // Ne PAS clear g_lastCraftingItemAnnounce — l'ItemCard garde l'ancien texte
        // pendant quelques ticks. On évite de le relire en gardant le dernier annoncé.
    }
    if (itemChanged) {
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
        if (!snap.enchantmentText.empty()) SpeakQueue(snap.enchantmentText);
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
            auto detected = std::make_shared<std::atomic_bool>(false);
            auto* task = SKSE::GetTaskInterface();
            if (task) {
                task->AddUITask([detected]() {
                    DetectCraftingMode();
                    detected->store(true);
                });
            }
            // Attendre que la détection soit faite (max 2 secondes)
            for (int i = 0; i < 40 && !detected->load() && !st.stop_requested(); i++)
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
