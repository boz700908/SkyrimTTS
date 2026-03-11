#pragma once

// VOCALISATION INVENTAIRE - DEBUT

// --- État ---
static std::atomic_bool g_invOpen{false};
static std::atomic_bool g_invPendingUIRead{false};
static std::jthread     g_invPollThread;
static std::wstring     g_lastInvCat;
static std::wstring     g_lastInvItemAnnounce;
static std::wstring     g_lastInvDesc;
static std::wstring     g_lastInvItemName;
static int              g_lastInvItemCount{0};
static int              g_lastInvFavorite{-1};  // -1=inconnu, 0=non, 1=oui

// --- Inventory snapshot data ---
struct InventorySnapshot {
    std::wstring itemText;
    int          count{0};
    int          equipState{0};
    std::wstring valueText;
    std::wstring weightText;
    std::wstring weaponDamageText;
    std::wstring apparelArmorText;
    std::wstring catText;
    std::wstring descText;
    bool         favorite{false};
};

// --- Lecture GFx ---
static bool ReadInventorySnapshot(InventorySnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::InventoryMenu::MENU_NAME);
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
    {
        RE::GFxValue favVal;
        if (movie->GetVariable(&favVal, "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.favorite"))
            snap.favorite = favVal.IsBool() ? favVal.GetBool()
                          : favVal.IsNumber() ? (favVal.GetNumber() != 0.0) : false;
    }
    if (GetGFxString(movie, "_root.Menu_mc.InventoryLists_mc.CategoriesList.centeredEntry.text", tmp) && !tmp.empty())
        snap.catText = ResolveUIString(movie, tmp);

    // Read description/effects from ItemCard::infoText (C++ side, more reliable than GFx)
    {
        auto* invMenu = static_cast<RE::InventoryMenu*>(menu.get());
        if (invMenu) {
            auto& rd = invMenu->GetRuntimeData();
            if (rd.itemCard && rd.itemCard->infoText.c_str()) {
                std::string raw = rd.itemCard->infoText.c_str();
                if (!raw.empty()) {
                    std::string norm;
                    for (size_t i = 0; i < raw.size(); ) {
                        if (raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n') { norm += ", "; i += 2; }
                        else if (raw[i] == '\n' || raw[i] == '\r') { norm += ", "; ++i; }
                        else { norm += raw[i++]; }
                    }
                    auto trimComma = [](std::string& s) {
                        size_t start = 0;
                        while (start < s.size() && (s[start] == ' ' || s[start] == ',')) ++start;
                        s = s.substr(start);
                        while (!s.empty() && (s.back() == ' ' || s.back() == ',')) s.pop_back();
                    };
                    trimComma(norm);
                    for (size_t p = norm.find(", ,"); p != std::string::npos; p = norm.find(", ,"))
                        norm.replace(p, 3, ",");
                    trimComma(norm);
                    if (!norm.empty()) {
                        std::wstring descW = Utf8ToWString(norm);
                        {
                            static std::wstring lastRawDesc;
                            if (descW != lastRawDesc &&
                                (descW.find(L'<') != std::wstring::npos ||
                                 descW.find(L'&') != std::wstring::npos ||
                                 descW.find(L"color") != std::wstring::npos)) {
                                LOG("INV RAW desc='{}'", WStringToUtf8(descW)); lastRawDesc = descW;
                            }
                        }
                        snap.descText = StripMarkupForSpeech(descW);
                        static std::string lastDesc;
                        if (norm != lastDesc) { LOG("INV desc='{}'", WStringToUtf8(snap.descText)); lastDesc = norm; }
                    }
                }
            }
        }
    }

    // Read value/weight from ItemCard TextFields
    auto readItemCard = [&](const char* field, std::wstring& out) {
        std::string s;
        const char* prefixes[] = {"_root.Menu_mc.ItemCard_mc.", "_root.ItemCard_mc."};
        const char* suffixes[] = {".text", ""};
        for (auto pfx : prefixes) {
            for (auto sfx : suffixes) {
                if (GetGFxString(movie, (std::string(pfx) + field + sfx).c_str(), s) && !s.empty()) {
                    out = Utf8ToWString(s);
                    return;
                }
            }
        }
    };
    readItemCard("ItemValueText.text",     snap.valueText);
    readItemCard("ItemWeightText.text",    snap.weightText);
    readItemCard("WeaponDamageValue.text", snap.weaponDamageText);
    readItemCard("ApparelArmorValue.text", snap.apparelArmorText);

    snap.valueText  = SanitizeNumericText(snap.valueText);
    snap.weightText = SanitizeNumericText(snap.weightText);

    {
        auto hasMarkup = [](const std::wstring& w) {
            return w.find(L'<') != std::wstring::npos ||
                   w.find(L'&') != std::wstring::npos ||
                   w.find(L"color") != std::wstring::npos;
        };
        static std::wstring lastRawDmg, lastRawArmor;
        if (hasMarkup(snap.weaponDamageText) && snap.weaponDamageText != lastRawDmg) {
            LOG("INV RAW dmg='{}'", WStringToUtf8(snap.weaponDamageText)); lastRawDmg = snap.weaponDamageText;
        }
        if (hasMarkup(snap.apparelArmorText) && snap.apparelArmorText != lastRawArmor) {
            LOG("INV RAW armor='{}'", WStringToUtf8(snap.apparelArmorText)); lastRawArmor = snap.apparelArmorText;
        }
    }
    snap.weaponDamageText = StripMarkupForSpeech(snap.weaponDamageText);
    snap.apparelArmorText = StripMarkupForSpeech(snap.apparelArmorText);

    {
        static std::wstring lastLogVal, lastLogWt;
        if ((!snap.valueText.empty() || !snap.weightText.empty()) &&
            (snap.valueText != lastLogVal || snap.weightText != lastLogWt)) {
            LOG("INV ItemCard value='{}' weight='{}'", WStringToUtf8(snap.valueText), WStringToUtf8(snap.weightText));
            lastLogVal = snap.valueText;
            lastLogWt  = snap.weightText;
        }
    }

    return !snap.itemText.empty() || !snap.catText.empty();
}

// --- Formatters ---
static std::wstring FormatEquipState(int state) {
    switch (state) {
        case 1: return L"equipped";
        case 2: return L"left hand";
        case 4: return L"right hand";
        case 6: return L"both hands";
        default: return L"";
    }
}


static std::wstring BuildItemAnnouncement(const InventorySnapshot& snap) {
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
    if (snap.favorite)
        msg += L", favorite";
    return msg;
}

// FormatCarryWeight, FormatWeight, SanitizeNumericText → common.h

// --- Vocalization ---

static void AnnounceInventoryChangeImpl() {
    if (!g_invOpen.load()) return;
    InventorySnapshot snap;
    if (!ReadInventorySnapshot(snap)) return;

    const bool catChanged = !snap.catText.empty() && snap.catText != g_lastInvCat;
    const std::wstring announce = BuildItemAnnouncement(snap);
    const bool itemChanged = !announce.empty() && announce != g_lastInvItemAnnounce;
    const bool sameItem    = !itemChanged && !snap.itemText.empty() && snap.itemText == g_lastInvItemName;
    const int  favInt      = snap.favorite ? 1 : 0;
    const bool favChanged  = sameItem && g_lastInvFavorite >= 0 && favInt != g_lastInvFavorite;

    const bool firstRead = g_lastInvCat.empty() && g_lastInvItemAnnounce.empty();
    if (catChanged) {
        if (firstRead) SpeakQueue(snap.catText); else Speak(snap.catText);
        g_lastInvCat = snap.catText;
    }
    if (itemChanged) {
        // Si même objet mais seul le count a changé → dire juste le nombre restant
        if (!firstRead && !snap.itemText.empty() && snap.itemText == g_lastInvItemName && snap.count != g_lastInvItemCount) {
            if (snap.count > 1)
                Speak(std::to_wstring(snap.count));
            else
                Speak(L"1");
        } else {
            if (firstRead) SpeakQueue(announce); else Speak(announce);
        }
        g_lastInvItemAnnounce = announce;
        g_lastInvItemName = snap.itemText;
        g_lastInvItemCount = snap.count;
        g_lastInvFavorite = favInt;
        g_lastInvDesc.clear();
    } else if (favChanged) {
        Speak(snap.favorite ? L"added to favorites" : L"removed from favorites");
        g_lastInvFavorite = favInt;
        g_lastInvItemAnnounce = announce;  // mettre à jour pour refléter le changement
    }
    if (!snap.descText.empty() && snap.descText != g_lastInvDesc) {
        SpeakQueue(snap.descText); // s'enchaîne après le nom sans l'interrompre
        g_lastInvDesc = snap.descText;
    }
}

// Announces gold and carry weight (H key)
static void AnnounceInventoryStats() {
    if (!g_invOpen.load(std::memory_order_relaxed)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        if (!g_invOpen.load()) return;
        auto ui = RE::UI::GetSingleton();
        if (!ui) return;
        auto menu = ui->GetMenu(RE::InventoryMenu::MENU_NAME);
        if (!menu) return;
        RE::GFxMovieView* movie = menu->uiMovie.get();
        if (!movie) return;

        std::string gold, carry;
        GetGFxString(movie, "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.PlayerGoldValue.text", gold);
        GetGFxString(movie, "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.CarryWeightValue.text", carry);

        std::wstring msg;
        if (!gold.empty())
            msg += Utf8ToWString(gold) + L" gold";
        if (!carry.empty()) {
            if (!msg.empty()) msg += L", weight: ";
            else              msg += L"weight: ";
            msg += FormatCarryWeight(Utf8ToWString(carry));
        }
        if (!msg.empty()) Speak(msg);
    });
}

static void DiagnoseInventoryNow() {
    if (!g_invOpen.load(std::memory_order_relaxed)) {
        Speak(L"Inventory closed");
        return;
    }
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        InventorySnapshot snap;
        if (!ReadInventorySnapshot(snap)) { Speak(L"UI selection not found"); return; }
        if (!snap.catText.empty())  { Speak(L"Category"); SpeakQueue(snap.catText); }
        if (!snap.itemText.empty()) { SpeakQueue(L"Item");     SpeakQueue(snap.itemText); }
    });
}

// --- Polling ---

static void QueueInventoryRead() {
    if (!g_invOpen.load(std::memory_order_relaxed)) return;
    if (g_invPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_invPendingUIRead.store(false); return; }
    task->AddUITask([]() {
        g_invPendingUIRead.store(false);
        if (g_invOpen.load()) AnnounceInventoryChangeImpl();
    });
}

static void StartInventoryPolling() {
    if (g_invPollThread.joinable()) { g_invPollThread.request_stop(); g_invPollThread.join(); }
    g_invPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_invOpen.load(std::memory_order_relaxed)) QueueInventoryRead();
        }
    });
}

static void StopInventoryPolling() {
    if (g_invPollThread.joinable()) { g_invPollThread.request_stop(); g_invPollThread.join(); }
}

// VOCALISATION INVENTAIRE - FIN
