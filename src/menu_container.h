#pragma once

// VOCALISATION MENU CONTENEUR - DEBUT

static std::atomic_bool g_containerOpen{false};
static std::atomic_bool g_containerPendingUIRead{false};
static std::jthread     g_containerPollThread;
static std::wstring     g_lastContainerCat;
static std::wstring     g_lastContainerItemAnnounce;
static std::wstring     g_lastContainerSide;
static std::wstring     g_lastContainerItemName;
static int              g_lastContainerItemCount{0};

struct ContainerSnapshot {
    std::wstring itemText;
    int          count{0};
    int          equipState{0};
    std::wstring valueText;
    std::wstring weightText;
    std::wstring weaponDamageText;
    std::wstring apparelArmorText;
    std::wstring catText;
    bool         isContainerSide{true};
    bool         atDivider{false};
};

static bool ReadContainerSnapshot(ContainerSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::ContainerMenu::MENU_NAME);
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

    // Determine if viewing container side or player inventory side
    double divider = -1.0, catIdx = -1.0;
    bool hasDivider = GetGFxNumber(movie, "_root.Menu_mc.InventoryLists_mc.CategoriesList.dividerIndex", divider);
    bool hasCatIdx  = GetGFxNumber(movie, "_root.Menu_mc.iSelectedCategory", catIdx);
    if (hasDivider && hasCatIdx && divider > 0) {
        snap.isContainerSide = (catIdx < divider);
        snap.atDivider       = (catIdx == divider);
    }

    // Read value/weight/damage/armor from ItemCard — try multiple prefixes
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

    snap.valueText        = SanitizeNumericText(snap.valueText);
    snap.weightText       = SanitizeNumericText(snap.weightText);
    snap.weaponDamageText = StripMarkupForSpeech(snap.weaponDamageText);
    snap.apparelArmorText = StripMarkupForSpeech(snap.apparelArmorText);

    return !snap.itemText.empty() || !snap.catText.empty();
}

static std::wstring BuildContainerItemAnnouncement(const ContainerSnapshot& snap) {
    if (snap.itemText.empty()) return L"";
    std::wstring msg = snap.itemText;
    if (snap.count > 1)
        msg += L", " + std::to_wstring(snap.count);
    const std::wstring eq = FormatEquipState(snap.equipState);
    if (!eq.empty())
        msg += L", " + eq;
    if (!snap.valueText.empty())
        msg += L", value " + snap.valueText;
    if (!snap.weightText.empty() && snap.weightText != L"0")
        msg += L", weight " + snap.weightText;
    if (!snap.weaponDamageText.empty() && snap.weaponDamageText != L"0")
        msg += L", damage " + snap.weaponDamageText;
    if (!snap.apparelArmorText.empty() && snap.apparelArmorText != L"0")
        msg += L", armor " + snap.apparelArmorText;
    return msg;
}

static void AnnounceContainerChangeImpl() {
    if (!g_containerOpen.load()) return;
    ContainerSnapshot snap;
    if (!ReadContainerSnapshot(snap)) return;

    const std::wstring side = snap.isContainerSide ? L"container" : L"inventory";
    const bool sideChanged = (side != g_lastContainerSide);
    const bool catChanged  = !snap.catText.empty() && (sideChanged || snap.catText != g_lastContainerCat);
    const std::wstring announce = BuildContainerItemAnnouncement(snap);
    const bool itemChanged = !announce.empty() && announce != g_lastContainerItemAnnounce;

    const bool firstRead = g_lastContainerCat.empty() && g_lastContainerItemAnnounce.empty();
    if (catChanged) {
        std::wstring catMsg = snap.atDivider ? (L"Your inventory: " + snap.catText) : (side + L": " + snap.catText);
        if (firstRead) SpeakQueue(catMsg); else Speak(catMsg);
        g_lastContainerCat  = snap.catText;
        g_lastContainerSide = side;
    }
    if (itemChanged) {
        // Si même objet mais seul le count a changé → dire juste le nombre restant
        if (!firstRead && !snap.itemText.empty() && snap.itemText == g_lastContainerItemName && snap.count != g_lastContainerItemCount) {
            if (snap.count > 1)
                Speak(std::to_wstring(snap.count));
            else
                Speak(L"1");
        } else {
            if (firstRead) SpeakQueue(announce); else Speak(announce);
        }
        g_lastContainerItemAnnounce = announce;
        g_lastContainerItemName = snap.itemText;
        g_lastContainerItemCount = snap.count;
    }
}

static void AnnounceContainerStats() {
    if (!g_containerOpen.load(std::memory_order_relaxed)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        if (!g_containerOpen.load()) return;
        auto ui = RE::UI::GetSingleton();
        if (!ui) return;
        auto menu = ui->GetMenu(RE::ContainerMenu::MENU_NAME);
        if (!menu) return;
        RE::GFxMovieView* movie = menu->uiMovie.get();
        if (!movie) return;

        std::string gold, carry;
        const char* goldPaths[]  = {
            "_root.Menu_mc.BottomBar_mc.PlayerGoldValue.text",
            "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.PlayerGoldValue.text"
        };
        const char* carryPaths[] = {
            "_root.Menu_mc.BottomBar_mc.CarryWeightValue.text",
            "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.CarryWeightValue.text"
        };
        for (auto p : goldPaths)  { if (GetGFxString(movie, p, gold)  && !gold.empty())  break; }
        for (auto p : carryPaths) { if (GetGFxString(movie, p, carry) && !carry.empty()) break; }

        std::wstring msg;
        if (!gold.empty())
            msg += Utf8ToWString(gold) + L" gold";
        if (!carry.empty()) {
            if (!msg.empty()) msg += L", weight: ";
            else              msg  = L"weight: ";
            msg += FormatCarryWeight(Utf8ToWString(carry));
        }
        if (!msg.empty()) Speak(msg);
    });
}

static void QueueContainerRead() {
    if (!g_containerOpen.load(std::memory_order_relaxed)) return;
    if (g_containerPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_containerPendingUIRead.store(false); return; }
    task->AddUITask([]() {
        g_containerPendingUIRead.store(false);
        if (g_containerOpen.load()) AnnounceContainerChangeImpl();
    });
}

static void StartContainerPolling() {
    if (g_containerPollThread.joinable()) { g_containerPollThread.request_stop(); g_containerPollThread.join(); }
    g_containerPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_containerOpen.load(std::memory_order_relaxed)) QueueContainerRead();
        }
    });
}

static void StopContainerPolling() {
    if (g_containerPollThread.joinable()) { g_containerPollThread.request_stop(); g_containerPollThread.join(); }
}

static void DiagnoseContainerNow() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        auto ui = RE::UI::GetSingleton();
        if (!ui) return;
        auto menu = ui->GetMenu(RE::ContainerMenu::MENU_NAME);
        if (!menu) { LOG("Container diag: menu not open"); return; }
        RE::GFxMovieView* movie = menu->uiMovie.get();
        if (!movie) { LOG("Container diag: no movie"); return; }

        double divider = -1, catIdx = -1, itemCount = -1;
        std::string catText, itemText;

        GetGFxNumber(movie, "_root.Menu_mc.InventoryLists_mc.CategoriesList.dividerIndex", divider);
        GetGFxNumber(movie, "_root.Menu_mc.iSelectedCategory", catIdx);
        GetGFxNumber(movie, "_root.Menu_mc.InventoryLists_mc.ItemsList.totalCount", itemCount);
        GetGFxString(movie, "_root.Menu_mc.InventoryLists_mc.CategoriesList.centeredEntry.text", catText);
        GetGFxString(movie, "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.text", itemText);

        LOG("Container diag: divider={} catIdx={} catText='{}' itemCount={} selectedItem='{}'",
            (int)divider, (int)catIdx, catText, (int)itemCount, itemText);
    });
}

// VOCALISATION MENU CONTENEUR - FIN
