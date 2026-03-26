#pragma once

// VOCALISATION MENU CADEAU (donner/prendre objets à un compagnon) - DEBUT

// GFx paths (from GiftMenu.as — hérite d'ItemMenu, même structure que inventaire/conteneur):
// Item sélectionné : _root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.text
// Catégorie :        _root.Menu_mc.InventoryLists_mc.CategoriesList.centeredEntry.text
// Label cadeau :     _root.Menu_mc.InventoryLists_mc.CategoriesList._parent.CategoryLabel.textField.text
// ItemCard :         _root.Menu_mc.ItemCardFadeHolder_mc.ItemCard_mc.*

static std::atomic_bool g_giftOpen{false};
static std::atomic_bool g_giftPendingUIRead{false};
static std::jthread     g_giftPollThread;
static std::wstring     g_lastGiftCat;
static std::wstring     g_lastGiftItemAnnounce;
static std::wstring     g_lastGiftItemName;
static int              g_lastGiftItemCount{0};

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
};

static bool ReadGiftSnapshot(GiftSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::GiftMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    std::string tmp;
    double num = 0.0;

    if (GetGFxString(movie, "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.text", tmp) && !tmp.empty())
        snap.itemText = ResolveUIString(movie, tmp);
    if (GetGFxNumber(movie, "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.count", num))
        snap.count = static_cast<int>(num);
    if (GetGFxNumber(movie, "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.equipState", num))
        snap.equipState = static_cast<int>(num);
    if (GetGFxString(movie, "_root.Menu_mc.InventoryLists_mc.CategoriesList.centeredEntry.text", tmp) && !tmp.empty())
        snap.catText = ResolveUIString(movie, tmp);

    // ItemCard values
    auto readItemCard = [&](const char* field, std::wstring& out) {
        std::string s;
        const char* prefixes[] = {
            "_root.Menu_mc.ItemCardFadeHolder_mc.ItemCard_mc.",
            "_root.Menu_mc.ItemCard_mc.",
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

    return !snap.itemText.empty() || !snap.catText.empty();
}

static std::wstring BuildGiftItemAnnouncement(const GiftSnapshot& snap) {
    if (snap.itemText.empty()) return L"";
    std::wstring msg = snap.itemText;
    if (snap.count > 1)
        msg += L", " + std::to_wstring(snap.count);
    const std::wstring eq = FormatEquipState(snap.equipState);
    if (!eq.empty())
        msg += L", " + eq;
    if (!snap.weaponDamageText.empty() && snap.weaponDamageText != L"0")
        msg += L", damage " + snap.weaponDamageText;
    if (!snap.apparelArmorText.empty() && snap.apparelArmorText != L"0")
        msg += L", armor " + snap.apparelArmorText;
    if (!snap.valueText.empty() && !isZero(snap.valueText))
        msg += L", value " + snap.valueText;
    if (!snap.weightText.empty() && !isZero(snap.weightText))
        msg += L", weight " + snap.weightText;
    if (!snap.soulLevelText.empty())
        msg += L", " + snap.soulLevelText;
    return msg;
}

static void AnnounceGiftChangeImpl() {
    if (!g_giftOpen.load()) return;
    GiftSnapshot snap;
    if (!ReadGiftSnapshot(snap)) return;

    const bool catChanged  = !snap.catText.empty() && snap.catText != g_lastGiftCat;
    const std::wstring announce = BuildGiftItemAnnouncement(snap);
    const bool itemChanged = !announce.empty() && announce != g_lastGiftItemAnnounce;

    // Si on est dans les catégories (pas d'item), reset pour forcer la relecture au retour
    if (snap.itemText.empty() && !g_lastGiftItemAnnounce.empty()) {
        g_lastGiftItemAnnounce.clear();
        g_lastGiftItemName.clear();
        g_lastGiftItemCount = 0;
        g_lastGiftCat.clear();  // relire la catégorie quand on revient avec flèche gauche
    }

    const bool firstRead = g_lastGiftCat.empty() && g_lastGiftItemAnnounce.empty();
    if (catChanged) {
        if (firstRead) SpeakQueue(snap.catText); else Speak(snap.catText);
        g_lastGiftCat = snap.catText;
    }
    if (itemChanged) {
        if (!firstRead && !snap.itemText.empty() && snap.itemText == g_lastGiftItemName && snap.count != g_lastGiftItemCount) {
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
