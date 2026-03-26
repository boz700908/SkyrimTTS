#pragma once

// VOCALISATION MENU MARCHAND (BARTER) - DEBUT

static std::atomic_bool g_barterOpen{false};
static std::atomic_bool g_barterPendingUIRead{false};
static std::jthread     g_barterPollThread;
static std::wstring     g_lastBarterCat;
static std::wstring     g_lastBarterItemAnnounce;
static std::wstring     g_lastBarterSide;
static std::wstring     g_lastBarterItemName;
static std::wstring     g_lastBarterDesc;
static int              g_lastBarterItemCount{0};

struct BarterSnapshot {
    std::wstring itemText;
    int          count{0};
    int          equipState{0};
    std::wstring valueText;
    std::wstring weightText;
    std::wstring weaponDamageText;
    std::wstring apparelArmorText;
    std::wstring catText;
    std::wstring descText;
    std::wstring soulLevelText;
    bool         isVendorSide{true};
    bool         atDivider{false};
};

static bool ReadBarterSnapshot(BarterSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::BarterMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    std::string tmp;
    double num = 0.0;

    // Essayer les chemins du BarterMenu (peut différer du ContainerMenu)
    const char* itemPaths[] = {
        "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.text",
        "_root.Menu_mc.itemList.selectedEntry.text",
        "_root.Menu_mc.ItemsList.selectedEntry.text",
    };
    for (auto p : itemPaths) {
        if (GetGFxString(movie, p, tmp) && !tmp.empty()) {
            snap.itemText = ResolveUIString(movie, tmp);
            static bool logged = false;
            if (!logged) { LOG("Barter: item path found: '{}'", p); logged = true; }
            break;
        }
    }

    const char* countPaths[] = {
        "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.count",
        "_root.Menu_mc.itemList.selectedEntry.count",
    };
    for (auto p : countPaths) {
        if (GetGFxNumber(movie, p, num)) { snap.count = static_cast<int>(num); break; }
    }

    const char* equipPaths[] = {
        "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.equipState",
        "_root.Menu_mc.itemList.selectedEntry.equipState",
    };
    for (auto p : equipPaths) {
        if (GetGFxNumber(movie, p, num)) { snap.equipState = static_cast<int>(num); break; }
    }

    const char* catPaths[] = {
        "_root.Menu_mc.InventoryLists_mc.CategoriesList.centeredEntry.text",
        "_root.Menu_mc.categoriesList.centeredEntry.text",
    };
    for (auto p : catPaths) {
        if (GetGFxString(movie, p, tmp) && !tmp.empty()) {
            snap.catText = ResolveUIString(movie, tmp);
            break;
        }
    }

    // Determine if viewing vendor side or player inventory side
    // Utiliser selectedIndex (se met à jour en temps réel) au lieu de iSelectedCategory
    double divider = -1.0, catIdx = -1.0;
    bool hasDivider = GetGFxNumber(movie, "_root.Menu_mc.InventoryLists_mc.CategoriesList.dividerIndex", divider);
    bool hasCatIdx  = GetGFxNumber(movie, "_root.Menu_mc.InventoryLists_mc.CategoriesList.selectedIndex", catIdx);
    if (!hasCatIdx) {
        hasCatIdx = GetGFxNumber(movie, "_root.Menu_mc.iSelectedCategory", catIdx);
    }
    if (hasDivider && hasCatIdx && divider > 0) {
        snap.isVendorSide = (catIdx < divider);
        snap.atDivider    = (catIdx == divider);
    }

    // Read description/effects from ItemCard infoText (C++ side)
    {
        auto* barterMenu = static_cast<RE::BarterMenu*>(menu.get());
        if (barterMenu) {
            auto& rd = barterMenu->GetRuntimeData();
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
                    if (!norm.empty())
                        snap.descText = StripMarkupForSpeech(Utf8ToWString(norm));
                }
            }
        }
    }

    // Read value/weight/damage/armor from ItemCard
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

static std::wstring BuildBarterItemAnnouncement(const BarterSnapshot& snap) {
    if (snap.itemText.empty()) return L"";
    std::wstring msg = snap.itemText;
    if (snap.count > 1)
        msg += L", " + std::to_wstring(snap.count);
    const std::wstring eq = FormatEquipState(snap.equipState);
    if (!eq.empty())
        msg += L", " + eq;
    auto isZero = [](const std::wstring& s) {
        try { return std::stof(s) == 0.0f; } catch (...) { return s.empty(); }
    };
    if (!snap.weaponDamageText.empty() && !isZero(snap.weaponDamageText))
        msg += L", damage " + snap.weaponDamageText;
    if (!snap.apparelArmorText.empty() && !isZero(snap.apparelArmorText))
        msg += L", armor " + snap.apparelArmorText;
    if (!snap.valueText.empty() && !isZero(snap.valueText))
        msg += L", value " + snap.valueText;
    if (!snap.weightText.empty() && !isZero(snap.weightText))
        msg += L", weight " + snap.weightText;
    if (!snap.soulLevelText.empty())
        msg += L", " + snap.soulLevelText;
    return msg;
}

static void AnnounceBarterChangeImpl() {
    if (!g_barterOpen.load()) return;
    BarterSnapshot snap;
    if (!ReadBarterSnapshot(snap)) return;

    const std::wstring side = snap.isVendorSide ? L"vendor" : L"inventory";
    const bool sideChanged = (side != g_lastBarterSide);
    const bool catChanged  = !snap.catText.empty() && (sideChanged || snap.catText != g_lastBarterCat);
    const std::wstring announce = BuildBarterItemAnnouncement(snap);
    const bool itemChanged = !announce.empty() && announce != g_lastBarterItemAnnounce;

    // Si on est dans les catégories (pas d'item), reset pour forcer la relecture au retour
    if (snap.itemText.empty() && !g_lastBarterItemAnnounce.empty()) {
        g_lastBarterItemAnnounce.clear();
        g_lastBarterItemName.clear();
        g_lastBarterItemCount = 0;
        g_lastBarterCat.clear();  // relire la catégorie quand on revient avec flèche gauche
    }

    const bool firstRead = g_lastBarterCat.empty() && g_lastBarterItemAnnounce.empty();
    if (catChanged) {
        std::wstring catMsg = snap.atDivider ? (L"Your inventory: " + snap.catText) : (side + L": " + snap.catText);
        if (firstRead) SpeakQueue(catMsg); else Speak(catMsg);
        g_lastBarterCat  = snap.catText;
        g_lastBarterSide = side;
    }
    if (itemChanged) {
        if (!firstRead && !snap.itemText.empty() && snap.itemText == g_lastBarterItemName && snap.count != g_lastBarterItemCount) {
            if (snap.count > 1)
                Speak(std::to_wstring(snap.count));
            else
                Speak(L"1");
        } else {
            if (firstRead) SpeakQueue(announce); else Speak(announce);
        }
        g_lastBarterItemAnnounce = announce;
        g_lastBarterItemName = snap.itemText;
        g_lastBarterItemCount = snap.count;
        g_lastBarterDesc.clear();
    }
    if (!snap.descText.empty() && snap.descText != g_lastBarterDesc) {
        SpeakQueue(snap.descText);
        g_lastBarterDesc = snap.descText;
    }
}

// H key: announce player gold, vendor gold, carry weight
static void AnnounceBarterStats() {
    if (!g_barterOpen.load(std::memory_order_relaxed)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        if (!g_barterOpen.load()) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        LOG("BarterStats: step 1 - reading gold from GFx");
        std::wstring msg;
        auto ui = RE::UI::GetSingleton();
        if (ui) {
            auto menu = ui->GetMenu(RE::BarterMenu::MENU_NAME);
            if (menu) {
                auto* barterMenu = static_cast<RE::BarterMenu*>(menu.get());
                if (barterMenu) {
                    // Lire l'or du marchand via GFx au lieu du C++
                    auto* movie = menu->uiMovie.get();
                    if (movie) {
                        // Probe tous les chemins possibles du BottomBar
                        const char* allPaths[] = {
                            "_root.Menu_mc.BottomBar_mc.PlayerGoldValue.text",
                            "_root.Menu_mc.BottomBar_mc.ContainerGoldValue.text",
                            "_root.Menu_mc.BottomBar_mc.CarryWeightValue.text",
                            "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.PlayerGoldValue.text",
                            "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.CarryWeightValue.text",
                        };
                        for (auto p : allPaths) {
                            std::string val;
                            bool ok = GetGFxString(movie, p, val);
                            if (ok && !val.empty()) {
                                LOG("BarterStats: '{}' = '{}'", p, val);
                            }
                        }

                        // Lire l'or du joueur
                        std::string playerGold;
                        if (GetGFxString(movie, "_root.Menu_mc.BottomBar_mc.PlayerGoldValue.text", playerGold) && !playerGold.empty()) {
                            msg += L"Your gold: " + Utf8ToWString(playerGold);
                        } else if (GetGFxString(movie, "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.PlayerGoldValue.text", playerGold) && !playerGold.empty()) {
                            msg += L"Your gold: " + Utf8ToWString(playerGold);
                        }

                        // Lire l'or du marchand
                        std::string vendorGold;
                        const char* vendorPaths[] = {
                            "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.VendorGoldValue.text",
                            "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.VendorGoldValue.htmlText",
                        };
                        for (auto p : vendorPaths) {
                            if (GetGFxString(movie, p, vendorGold) && !vendorGold.empty()) {
                                LOG("BarterStats: vendor gold at '{}' = '{}'", p, vendorGold);
                                if (!msg.empty()) msg += L", ";
                                msg += L"Vendor gold: " + StripMarkupForSpeech(Utf8ToWString(vendorGold));
                                break;
                            }
                        }

                        // Lire le poids — en mode Barter, il n'y a peut-être pas de CarryWeight
                        // Lisons-le depuis le C++ à la place
                        auto* av = player->AsActorValueOwner();
                        if (av) {
                            float carryMax = av->GetActorValue(RE::ActorValue::kCarryWeight);
                            float carryCur = player->GetWeightInContainer();
                            if (!msg.empty()) msg += L", ";
                            msg += L"weight: " + std::to_wstring(static_cast<int>(carryCur)) + L" / " + std::to_wstring(static_cast<int>(carryMax));
                        }
                    }
                }
            }
        }

        LOG("BarterStats: step 3 - speaking");
        Speak(msg);
    });
}

static void QueueBarterRead() {
    if (!g_barterOpen.load(std::memory_order_relaxed)) return;
    if (g_barterPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_barterPendingUIRead.store(false); return; }
    task->AddUITask([]() {
        g_barterPendingUIRead.store(false);
        if (g_barterOpen.load()) AnnounceBarterChangeImpl();
    });
}

static void StartBarterPolling() {
    if (g_barterPollThread.joinable()) { g_barterPollThread.request_stop(); g_barterPollThread.join(); }
    g_barterPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_barterOpen.load(std::memory_order_relaxed)) QueueBarterRead();
        }
    });
}

static void StopBarterPolling() {
    if (g_barterPollThread.joinable()) { g_barterPollThread.request_stop(); g_barterPollThread.join(); }
}

// VOCALISATION MENU MARCHAND (BARTER) - FIN
