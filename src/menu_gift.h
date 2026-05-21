#pragma once

// VOCALISATION MENU CADEAU (donner/prendre objets à un compagnon) - DEBUT

// GFx paths (from GiftMenu.as — hérite d'ItemMenu, même structure que inventaire/conteneur):
// Vanilla:  _root.Menu_mc.InventoryLists_mc.ItemsList / CategoriesList.centeredEntry
// SkyUI:    _root.Menu_mc.inventoryLists.itemList / categoryList.selectedEntry
// ItemCard: Vanilla: ItemCardFadeHolder_mc.ItemCard_mc / SkyUI: itemCard

static std::atomic_bool g_giftOpen{false};
static std::atomic_bool g_giftPendingUIRead{false};
static std::jthread     g_giftPollThread;
static std::wstring     g_lastGiftCat;
static std::wstring     g_lastGiftItemAnnounce;
static std::wstring     g_lastGiftItemName;
static bool             g_giftQuantityOpen{false};
static int              g_lastGiftQuantity{0};
static int              g_lastGiftItemCount{0};
// Pointeur Item* de la selection — cf. menu_inventory.h pour l'explication.
static const void*      g_lastGiftItemPtr = nullptr;

struct GiftSnapshot {
    std::wstring itemText;
    int          count{0};
    int          equipState{0};
    std::wstring valueText;
    std::wstring weightText;
    std::wstring weaponDamageText;
    std::wstring apparelArmorText;
    std::wstring catText;
    std::wstring soulLevelText;
    bool         stolen{false};     // item appartenant a un PNJ/faction (pas au joueur)
    int          chargePercent{-1}; // charge restante d'une arme enchantee [0..100], -1 si non applicable
    const void*  itemPtr{nullptr};
};

static bool ReadGiftSnapshot(GiftSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::GiftMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);

    const char* itemTextP  = skyui ? "_root.Menu_mc.inventoryLists.itemList.selectedEntry.text"
                                   : "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.text";
    const char* itemCountP = skyui ? "_root.Menu_mc.inventoryLists.itemList.selectedEntry.count"
                                   : "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.count";
    const char* itemEquipP = skyui ? "_root.Menu_mc.inventoryLists.itemList.selectedEntry.equipState"
                                   : "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.equipState";
    const char* catTextP   = skyui ? "_root.Menu_mc.inventoryLists.categoryList.selectedEntry.text"
                                   : "_root.Menu_mc.InventoryLists_mc.CategoriesList.centeredEntry.text";

    std::string tmp;
    double num = 0.0;

    if (GetGFxString(movie, itemTextP, tmp) && !tmp.empty())
        snap.itemText = ResolveUIString(movie, tmp);
    if (GetGFxNumber(movie, itemCountP, num))
        snap.count = static_cast<int>(num);
    if (GetGFxNumber(movie, itemEquipP, num))
        snap.equipState = static_cast<int>(num);
    if (GetGFxString(movie, catTextP, tmp) && !tmp.empty())
        snap.catText = ResolveUIString(movie, tmp);

    // ItemCard values
    auto readItemCard = [&](const char* field, std::wstring& out) {
        std::string s;
        const char* prefixes[] = {
            skyui ? "_root.Menu_mc.itemCard." : "_root.Menu_mc.ItemCardFadeHolder_mc.ItemCard_mc.",
            skyui ? "_root.Menu_mc.itemCardFadeHolder.ItemCard_mc." : "_root.Menu_mc.ItemCard_mc.",
            "_root.ItemCard_mc."
        };
        for (auto pfx : prefixes) {
            if (GetGFxString(movie, (std::string(pfx) + field).c_str(), s) && !s.empty()) {
                out = Utf8ToWString(s);
                return;
            }
        }
    };
    readItemCard("ItemValueText.text",     snap.valueText);
    readItemCard("ItemWeightText.text",    snap.weightText);
    readItemCard("WeaponDamageValue.text", snap.weaponDamageText);
    readItemCard("ApparelArmorValue.text", snap.apparelArmorText);
    readItemCard("SoulLevel.text",        snap.soulLevelText);

    snap.valueText        = SanitizeNumericText(snap.valueText);
    snap.weightText       = SanitizeNumericText(snap.weightText);
    snap.weaponDamageText = StripMarkupForSpeech(snap.weaponDamageText);
    snap.apparelArmorText = StripMarkupForSpeech(snap.apparelArmorText);

    // Pointeur Item* de la selection — cf. menu_inventory.h.
    // Lecture du flag stolen via IsItemStolen (cf. common.h).
    {
        auto* giftMenu = static_cast<RE::GiftMenu*>(menu.get());
        if (giftMenu) {
            auto& rd = giftMenu->GetRuntimeData();
            if (rd.itemList) {
                auto* sel = rd.itemList->GetSelectedItem();
                snap.itemPtr = sel;
                if (sel && sel->data.objDesc) {
                    snap.stolen        = IsItemStolen(sel->data.objDesc);
                    snap.chargePercent = GetEnchantmentChargePercent(sel->data.objDesc);
                }
            }
        }
    }

    return !snap.itemText.empty() || !snap.catText.empty();
}

static std::wstring BuildGiftItemAnnouncement(const GiftSnapshot& snap) {
    if (snap.itemText.empty()) return L"";
    std::wstring msg = snap.itemText;
    if (snap.stolen)
        msg += L", " + TR("stolen");
    if (snap.count > 1)
        msg += L", " + std::to_wstring(snap.count);
    const std::wstring eq = FormatEquipState(snap.equipState);
    if (!eq.empty())
        msg += L", " + eq;
    if (!snap.weaponDamageText.empty() && snap.weaponDamageText != L"0")
        msg += L", " + TR("damage") + L" " + snap.weaponDamageText;
    if (!snap.apparelArmorText.empty() && snap.apparelArmorText != L"0")
        msg += L", " + TR("armor") + L" " + snap.apparelArmorText;
    if (!snap.valueText.empty() && !isZero(snap.valueText))
        msg += L", " + TR("value") + L" " + snap.valueText;
    if (!snap.weightText.empty() && !isZero(snap.weightText))
        msg += L", " + TR("weight") + L" " + snap.weightText;
    // Charge restante d'une arme enchantee (en %). -1 = non applicable.
    if (snap.chargePercent >= 0)
        msg += L", " + TR("charge") + L" " + std::to_wstring(snap.chargePercent) + L"%";
    if (!snap.soulLevelText.empty())
        msg += L", " + snap.soulLevelText;
    return msg;
}

static void AnnounceGiftChangeImpl() {
    if (!g_giftOpen.load()) return;

    // Vérifier le slider de quantité
    {
        auto ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu(RE::GiftMenu::MENU_NAME) : nullptr;
        auto* movie = (menu && menu->uiMovie) ? menu->uiMovie.get() : nullptr;
        if (movie) {
            const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
            const char* sliderPath = skyui
                ? "_root.Menu_mc.itemCardFadeHolder.ItemCard_mc.QuantitySlider_mc.value"
                : "_root.Menu_mc.ItemCardFadeHolder_mc.ItemCard_mc.QuantitySlider_mc.value";
            const char* sliderAlphaPath = skyui
                ? "_root.Menu_mc.itemCardFadeHolder.ItemCard_mc.QuantitySlider_mc._alpha"
                : "_root.Menu_mc.ItemCardFadeHolder_mc.ItemCard_mc.QuantitySlider_mc._alpha";

            double alpha = 0.0;
            GetGFxNumber(movie, sliderAlphaPath, alpha);
            bool sliderVisible = (alpha >= 50.0);

            if (sliderVisible) {
                double val = 0.0;
                GetGFxNumber(movie, sliderPath, val);
                int qty = static_cast<int>(val);
                if (!g_giftQuantityOpen) {
                    g_giftQuantityOpen = true;
                    g_lastGiftQuantity = qty;
                    Speak(TR("Quantity") + L": " + std::to_wstring(qty));
                } else if (qty != g_lastGiftQuantity) {
                    g_lastGiftQuantity = qty;
                    Speak(std::to_wstring(qty));
                }
                return;
            } else if (g_giftQuantityOpen) {
                g_giftQuantityOpen = false;
                g_lastGiftQuantity = 0;
            }
        }
    }

    GiftSnapshot snap;
    if (!ReadGiftSnapshot(snap)) return;

    const bool catChanged  = !snap.catText.empty() && snap.catText != g_lastGiftCat;
    const std::wstring announce = BuildGiftItemAnnouncement(snap);
    // Changement detecte soit par le texte, soit par le pointeur Item*.
    const bool ptrChanged = snap.itemPtr != nullptr && g_lastGiftItemPtr != nullptr &&
                            snap.itemPtr != g_lastGiftItemPtr;
    const bool itemChanged = !announce.empty() && (announce != g_lastGiftItemAnnounce || ptrChanged);

    // Si on est dans les catégories (pas d'item), reset pour forcer la relecture au retour
    if (snap.itemText.empty() && !g_lastGiftItemAnnounce.empty()) {
        g_lastGiftItemAnnounce.clear();
        g_lastGiftItemName.clear();
        g_lastGiftItemCount = 0;
        g_lastGiftItemPtr = nullptr;
        g_lastGiftCat.clear();  // relire la catégorie quand on revient avec flèche gauche
    }

    const bool firstRead = g_lastGiftCat.empty() && g_lastGiftItemAnnounce.empty();
    if (catChanged) {
        if (firstRead) SpeakQueue(snap.catText); else Speak(snap.catText);
        g_lastGiftCat = snap.catText;
    }
    if (itemChanged) {
        // Si MEME item (meme pointeur) mais seul le count a changé → dire juste
        // le nombre. Sinon relire tout (cas homonyme avec count different).
        const bool sameItemPtr = snap.itemPtr != nullptr && snap.itemPtr == g_lastGiftItemPtr;
        if (!firstRead && sameItemPtr &&
            !snap.itemText.empty() && snap.itemText == g_lastGiftItemName && snap.count != g_lastGiftItemCount) {
            if (snap.count > 1)
                Speak(std::to_wstring(snap.count));
            else
                Speak(L"1");
        } else {
            if (firstRead) SpeakQueue(announce); else Speak(announce);
        }
        g_lastGiftItemAnnounce = announce;
        g_lastGiftItemName = snap.itemText;
        g_lastGiftItemCount = snap.count;
        g_lastGiftItemPtr = snap.itemPtr;
    }
}

static void QueueGiftRead() {
    if (!g_giftOpen.load(std::memory_order_relaxed)) return;
    if (g_giftPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_giftPendingUIRead.store(false); return; }
    task->AddUITask([]() {
        g_giftPendingUIRead.store(false);
        if (g_giftOpen.load()) AnnounceGiftChangeImpl();
    });
}

static void StartGiftPolling() {
    if (g_giftPollThread.joinable()) { g_giftPollThread.request_stop(); g_giftPollThread.join(); }
    g_giftPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_giftOpen.load(std::memory_order_relaxed)) QueueGiftRead();
        }
    });
}

static void StopGiftPolling() {
    if (g_giftPollThread.joinable()) { g_giftPollThread.request_stop(); g_giftPollThread.join(); }
}

// VOCALISATION MENU CADEAU - FIN
