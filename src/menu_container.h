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
static std::wstring     g_lastContainerDesc;
static bool             g_containerQuantityOpen{false};
static int              g_lastContainerQuantity{0};
// Pointeur RE::ItemList::Item* de l'entree selectionnee — permet de detecter
// un changement de selection entre deux items homonymes (ex: gemmes vide/
// pleine). Cf. commentaire equivalent dans menu_inventory.h.
static const void*      g_lastContainerItemPtr = nullptr;

struct ContainerSnapshot {
    std::wstring itemText;
    int          count{0};
    int          equipState{0};
    std::wstring valueText;
    std::wstring weightText;
    std::wstring weaponDamageText;
    std::wstring apparelArmorText;
    std::wstring catText;
    std::wstring soulLevelText;
    bool         isContainerSide{true};
    bool         atDivider{false};
    bool         stolen{false};     // item appartenant a un PNJ/faction (pas au joueur)
    const void*  itemPtr{nullptr};  // cf. g_lastContainerItemPtr
    std::wstring descText;          // description / effets / enchantements (depuis ItemCard.infoText)
};

static bool ReadContainerSnapshot(ContainerSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::ContainerMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);

    const char* itemText  = skyui ? "_root.Menu_mc.inventoryLists.itemList.selectedEntry.text"
                                  : "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.text";
    const char* itemCount = skyui ? "_root.Menu_mc.inventoryLists.itemList.selectedEntry.count"
                                  : "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.count";
    const char* itemEquip = skyui ? "_root.Menu_mc.inventoryLists.itemList.selectedEntry.equipState"
                                  : "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.equipState";
    const char* catPath   = skyui ? "_root.Menu_mc.inventoryLists.categoryList.selectedEntry.text"
                                  : "_root.Menu_mc.InventoryLists_mc.CategoriesList.centeredEntry.text";
    const char* dividerPath = skyui ? "_root.Menu_mc.inventoryLists.categoryList.dividerIndex"
                                    : "_root.Menu_mc.InventoryLists_mc.CategoriesList.dividerIndex";
    const char* catIdxPath  = skyui ? "_root.Menu_mc.inventoryLists.categoryList.selectedIndex"
                                    : "_root.Menu_mc.InventoryLists_mc.CategoriesList.selectedIndex";

    std::string tmp;
    double num = 0.0;

    if (GetGFxString(movie, itemText, tmp) && !tmp.empty())
        snap.itemText = ResolveUIString(movie, tmp);
    if (GetGFxNumber(movie, itemCount, num))
        snap.count = static_cast<int>(num);
    if (GetGFxNumber(movie, itemEquip, num))
        snap.equipState = static_cast<int>(num);
    if (GetGFxString(movie, catPath, tmp) && !tmp.empty())
        snap.catText = ResolveUIString(movie, tmp);

    // Determine if viewing container side or player inventory side
    if (skyui) {
        // SkyUI uses activeSegment: 0 = container, 1 = player inventory
        double segment = 0.0;
        if (GetGFxNumber(movie, "_root.Menu_mc.inventoryLists.categoryList.activeSegment", segment)) {
            snap.isContainerSide = (static_cast<int>(segment) == 0);
            snap.atDivider = false;
        }
    } else {
        double divider = -1.0, catIdx = -1.0;
        bool hasDivider = GetGFxNumber(movie, dividerPath, divider);
        bool hasCatIdx  = GetGFxNumber(movie, catIdxPath, catIdx);
        if (hasDivider && hasCatIdx && divider > 0) {
            snap.isContainerSide = (catIdx < divider);
            snap.atDivider       = (catIdx == divider);
        }
    }

    // Read description/effects/enchantments from ItemCard infoText (C++ side)
    {
        auto* contMenu = static_cast<RE::ContainerMenu*>(menu.get());
        if (contMenu) {
            auto& rd = contMenu->GetRuntimeData();
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

    // Read value/weight/damage/armor from ItemCard — try multiple prefixes
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

    // Pointeur Item* de la selection courante — permet de detecter un
    // changement entre deux items homonymes (gemmes vide/pleine, etc.).
    // Lecture du flag stolen via IsItemStolen (cf. common.h).
    {
        auto* contMenu = static_cast<RE::ContainerMenu*>(menu.get());
        if (contMenu) {
            auto& rd = contMenu->GetRuntimeData();
            if (rd.itemList) {
                auto* sel = rd.itemList->GetSelectedItem();
                snap.itemPtr = sel;
                if (sel && sel->data.objDesc) {
                    snap.stolen = IsItemStolen(sel->data.objDesc);
                }
            }
        }
    }

    return !snap.itemText.empty() || !snap.catText.empty();
}

static std::wstring BuildContainerItemAnnouncement(const ContainerSnapshot& snap) {
    if (snap.itemText.empty()) return L"";
    std::wstring msg = snap.itemText;
    if (snap.stolen)
        msg += L", stolen";
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

static void AnnounceContainerChangeImpl() {
    if (!g_containerOpen.load()) return;

    // Vérifier le slider de quantité
    {
        auto ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu(RE::ContainerMenu::MENU_NAME) : nullptr;
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
                if (!g_containerQuantityOpen) {
                    g_containerQuantityOpen = true;
                    g_lastContainerQuantity = qty;
                    Speak(L"Quantity: " + std::to_wstring(qty));
                } else if (qty != g_lastContainerQuantity) {
                    g_lastContainerQuantity = qty;
                    Speak(std::to_wstring(qty));
                }
                return;
            } else if (g_containerQuantityOpen) {
                g_containerQuantityOpen = false;
                g_lastContainerQuantity = 0;
            }
        }
    }

    ContainerSnapshot snap;
    if (!ReadContainerSnapshot(snap)) return;

    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
    const std::wstring side = snap.isContainerSide ? L"container" : L"inventory";
    const bool sideChanged = (side != g_lastContainerSide);
    const bool catChanged  = !snap.catText.empty() && (sideChanged || snap.catText != g_lastContainerCat);
    // Log uniquement quand quelque chose change
    if (sideChanged || catChanged || (!snap.itemText.empty() && snap.itemText != g_lastContainerItemName)) {
        // Log les valeurs brutes du divider
        auto ui2 = RE::UI::GetSingleton();
        double rawDiv = -1, rawCatIdx = -1;
        if (ui2) {
            auto m2 = ui2->GetMenu(RE::ContainerMenu::MENU_NAME);
            if (m2 && m2->uiMovie) {
                const char* divP = skyui ? "_root.Menu_mc.inventoryLists.categoryList.dividerIndex"
                                         : "_root.Menu_mc.InventoryLists_mc.CategoriesList.dividerIndex";
                GetGFxNumber(m2->uiMovie.get(), divP, rawDiv);
                GetGFxNumber(m2->uiMovie.get(), "_root.Menu_mc.iSelectedCategory", rawCatIdx);
            }
        }
        LOG("Container: side='{}' cat='{}' item='{}' dividerIdx={} catIdx={} isContainerSide={} sideChanged={} catChanged={}",
            WStringToUtf8(side), WStringToUtf8(snap.catText), WStringToUtf8(snap.itemText),
            rawDiv, rawCatIdx, snap.isContainerSide, sideChanged, catChanged);
    }
    const std::wstring announce = BuildContainerItemAnnouncement(snap);
    // Changement detecte soit par le texte, soit par le pointeur d'item
    // selectionne (cas de deux items homonymes adjacents).
    const bool ptrChanged = snap.itemPtr != nullptr && g_lastContainerItemPtr != nullptr &&
                            snap.itemPtr != g_lastContainerItemPtr;
    const bool itemChanged = !announce.empty() && (announce != g_lastContainerItemAnnounce || ptrChanged);

    // Si on est dans les catégories (pas d'item), reset pour forcer la relecture au retour
    if (snap.itemText.empty() && !g_lastContainerItemAnnounce.empty()) {
        g_lastContainerItemAnnounce.clear();
        g_lastContainerItemName.clear();
        g_lastContainerItemCount = 0;
        g_lastContainerItemPtr = nullptr;
        g_lastContainerCat.clear();  // relire la catégorie quand on revient avec flèche gauche
    }

    // Si on change de côté, reset la catégorie pour forcer la relecture avec le bon préfixe
    if (sideChanged) {
        g_lastContainerCat.clear();
    }

    const bool firstRead = g_lastContainerCat.empty() && g_lastContainerItemAnnounce.empty();
    if (catChanged) {
        std::wstring catMsg = snap.atDivider ? (L"Your inventory: " + snap.catText) : (side + L": " + snap.catText);
        if (firstRead) SpeakQueue(catMsg); else Speak(catMsg);
        g_lastContainerCat  = snap.catText;
        g_lastContainerSide = side;
    }
    if (itemChanged) {
        // Si MEME item (meme pointeur) mais seul le count a changé → dire juste
        // le nombre restant. On exige le meme pointeur pour ne PAS tomber dans
        // cette branche quand on navigue vers un item homonyme avec count
        // different (ex: Grand Soul Gem vide 1 -> pleine 3).
        const bool sameItemPtr = snap.itemPtr != nullptr && snap.itemPtr == g_lastContainerItemPtr;
        if (!firstRead && sameItemPtr &&
            !snap.itemText.empty() && snap.itemText == g_lastContainerItemName && snap.count != g_lastContainerItemCount) {
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
        g_lastContainerItemPtr = snap.itemPtr;
        g_lastContainerDesc.clear();
    }
    if (!snap.descText.empty() && snap.descText != g_lastContainerDesc) {
        SpeakQueue(snap.descText);
        g_lastContainerDesc = snap.descText;
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
        const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
        const char* goldPaths[]  = {
            skyui ? "_root.Menu_mc.bottomBar.playerInfoCard.PlayerGoldValue.text"
                  : "_root.Menu_mc.BottomBar_mc.PlayerGoldValue.text",
            skyui ? "_root.Menu_mc.bottomBar.PlayerGoldValue.text"
                  : "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.PlayerGoldValue.text"
        };
        const char* carryPaths[] = {
            skyui ? "_root.Menu_mc.bottomBar.playerInfoCard.CarryWeightValue.text"
                  : "_root.Menu_mc.BottomBar_mc.CarryWeightValue.text",
            skyui ? "_root.Menu_mc.bottomBar.CarryWeightValue.text"
                  : "_root.Menu_mc.BottomBar_mc.PlayerInfoCard_mc.CarryWeightValue.text"
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

        // Si le conteneur cible est un compagnon (PlayerTeammate), on lit
        // aussi son poids porte / capacite. Pour un coffre ou un cadavre, on
        // ne dit rien de plus (GetTargetRefHandle retournera un ref non-actor
        // ou un actor non-teammate).
        RE::RefHandle targetHandle = RE::ContainerMenu::GetTargetRefHandle();
        RE::NiPointer<RE::Actor> targetActor;
        if (RE::LookupReferenceByHandle(targetHandle, targetActor) && targetActor &&
            targetActor->IsPlayerTeammate()) {
            float maxCarry = targetActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);
            float currentWeight = targetActor->GetWeightInContainer();
            const char* rawName = targetActor->GetDisplayFullName();
            std::wstring followerName = (rawName && rawName[0]) ? Utf8ToWString(rawName) : L"Follower";
            if (!msg.empty()) msg += L", ";
            msg += followerName + L": " + std::to_wstring(static_cast<int>(currentWeight))
                 + L" of " + std::to_wstring(static_cast<int>(maxCarry));
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

        const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
        double divider = -1, catIdx = -1, itemCount = -1;
        std::string catText, itemText;

        GetGFxNumber(movie, skyui ? "_root.Menu_mc.inventoryLists.categoryList.dividerIndex"
                                  : "_root.Menu_mc.InventoryLists_mc.CategoriesList.dividerIndex", divider);
        GetGFxNumber(movie, "_root.Menu_mc.iSelectedCategory", catIdx);
        GetGFxNumber(movie, skyui ? "_root.Menu_mc.inventoryLists.itemList.selectedIndex"
                                  : "_root.Menu_mc.InventoryLists_mc.ItemsList.totalCount", itemCount);
        GetGFxString(movie, skyui ? "_root.Menu_mc.inventoryLists.categoryList.selectedEntry.text"
                                  : "_root.Menu_mc.InventoryLists_mc.CategoriesList.centeredEntry.text", catText);
        GetGFxString(movie, skyui ? "_root.Menu_mc.inventoryLists.itemList.selectedEntry.text"
                                  : "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.text", itemText);

        LOG("Container diag: divider={} catIdx={} catText='{}' itemCount={} selectedItem='{}'",
            (int)divider, (int)catIdx, catText, (int)itemCount, itemText);
    });
}

// VOCALISATION MENU CONTENEUR - FIN
