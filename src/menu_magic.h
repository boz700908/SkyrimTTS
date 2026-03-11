#pragma once

// VOCALISATION MAGIE - DEBUT

// --- Constantes ICT (InventoryDefines.as) ---
static constexpr int MAGIC_ICT_SPELL         = 7;   // Sort régulier (école de magie)
static constexpr int MAGIC_ICT_SHOUT         = 10;  // Cri de dragon
static constexpr int MAGIC_ICT_ACTIVE_EFFECT = 11;  // Effet actif
static constexpr int MAGIC_ICT_SPELL_DEFAULT = 13;  // Pouvoir / pouvoir mineur

// --- Constantes équipement (InventoryDefines.as) ---
static constexpr int MAGIC_ES_NONE           = 0;
static constexpr int MAGIC_ES_EQUIPPED       = 1;
static constexpr int MAGIC_ES_LEFT           = 2;
static constexpr int MAGIC_ES_RIGHT          = 3;
static constexpr int MAGIC_ES_BOTH           = 4;

// --- Chemins GFx (MagicMenu extends ItemMenu → même InventoryLists_mc que l'inventaire) ---
static constexpr const char* MAGIC_ITEM_TEXT    = "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.text";
static constexpr const char* MAGIC_ITEM_ENABLED = "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.enabled";
static constexpr const char* MAGIC_ITEM_EQUIP    = "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.equipState";
static constexpr const char* MAGIC_ITEM_FAVORITE = "_root.Menu_mc.InventoryLists_mc.ItemsList.selectedEntry.favorite";
static constexpr const char* MAGIC_CAT_TEXT      = "_root.Menu_mc.InventoryLists_mc.CategoriesList.centeredEntry.text";
static constexpr const char* MAGIC_CARD_PREFIX  = "_root.Menu_mc.ItemCardFadeHolder_mc.ItemCard_mc.itemInfo.";

// --- État ---
static std::atomic_bool g_magicOpen{false};
static std::atomic_bool g_magicPendingUIRead{false};
static std::jthread     g_magicPollThread;
static std::wstring     g_lastMagicCat;
static std::wstring     g_lastMagicItem;
static int              g_lastMagicEquipState{-1};
static int              g_lastMagicFavorite{-1};  // -1=inconnu, 0=non, 1=oui
static std::wstring     g_lastMagicEffects;

// --- Snapshot ---
struct MagicSnapshot {
    std::wstring category;
    std::wstring itemName;
    bool         itemEnabled{true};
    int          equipState{-1};
    bool         favorite{false};
    // itemInfo
    int          itemType{-1};
    std::wstring effects;
    double       spellCost{0.0};
    // ICT_SPELL
    double       castLevel{0.0};
    double       castTime{0.0};    // 0 = sort de concentration
    // ICT_SHOUT
    struct ShoutWord { std::wstring name; bool unlocked{false}; };
    ShoutWord shoutWords[3];
    // ICT_ACTIVE_EFFECT
    double       timeRemaining{0.0};
};

static bool ReadMagicSnapshot(MagicSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::MagicMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    std::string tmp;

    if (GetGFxString(movie, MAGIC_CAT_TEXT, tmp) && !tmp.empty())
        snap.category = ResolveUIString(movie, tmp);

    if (GetGFxString(movie, MAGIC_ITEM_TEXT, tmp) && !tmp.empty())
        snap.itemName = ResolveUIString(movie, tmp);

    RE::GFxValue enabledVal;
    if (movie->GetVariable(&enabledVal, MAGIC_ITEM_ENABLED))
        snap.itemEnabled = enabledVal.IsBool() ? enabledVal.GetBool()
                         : (enabledVal.IsNumber() && enabledVal.GetNumber() != 0.0);

    double equipD = 0.0;
    if (GetGFxNumber(movie, MAGIC_ITEM_EQUIP, equipD))
        snap.equipState = static_cast<int>(equipD);

    RE::GFxValue favVal;
    if (movie->GetVariable(&favVal, MAGIC_ITEM_FAVORITE))
        snap.favorite = favVal.IsBool() ? favVal.GetBool()
                      : (favVal.IsNumber() && favVal.GetNumber() != 0.0);

    // --- itemInfo ---
    auto cardField = [](const char* field) {
        return std::string(MAGIC_CARD_PREFIX) + field;
    };

    double typeD = -1.0;
    if (GetGFxNumber(movie, cardField("type").c_str(), typeD))
        snap.itemType = static_cast<int>(typeD);

    if (GetGFxString(movie, cardField("effects").c_str(), tmp) && !tmp.empty())
        snap.effects = StripMarkupForSpeech(ResolveUIString(movie, tmp));

    GetGFxNumber(movie, cardField("spellCost").c_str(), snap.spellCost);

    if (snap.itemType == MAGIC_ICT_SPELL) {
        GetGFxNumber(movie, cardField("castLevel").c_str(), snap.castLevel);
        GetGFxNumber(movie, cardField("castTime").c_str(), snap.castTime);

    } else if (snap.itemType == MAGIC_ICT_SHOUT) {
        for (int i = 0; i < 3; ++i) {
            std::string wordPath = cardField(("word" + std::to_string(i)).c_str());
            if (GetGFxString(movie, wordPath.c_str(), tmp) && !tmp.empty())
                snap.shoutWords[i].name = ResolveUIString(movie, tmp);

            std::string unlockedPath = cardField(("unlocked" + std::to_string(i)).c_str());
            RE::GFxValue unlocked;
            if (movie->GetVariable(&unlocked, unlockedPath.c_str()))
                snap.shoutWords[i].unlocked = unlocked.IsBool() ? unlocked.GetBool()
                    : (unlocked.IsNumber() && unlocked.GetNumber() != 0.0);
        }

    } else if (snap.itemType == MAGIC_ICT_ACTIVE_EFFECT) {
        GetGFxNumber(movie, cardField("timeRemaining").c_str(), snap.timeRemaining);
    }

    return !snap.itemName.empty() || !snap.category.empty();
}

static const wchar_t* MagicEquipText(int state) {
    switch (state) {
        case MAGIC_ES_EQUIPPED: return L", equipped";
        case MAGIC_ES_LEFT:     return L", equipped left";
        case MAGIC_ES_RIGHT:    return L", equipped right";
        case MAGIC_ES_BOTH:     return L", equipped both hands";
        default:                return L"";
    }
}

static void AnnounceMagicChangeImpl() {
    if (!g_magicOpen.load()) return;
    MagicSnapshot snap;
    if (!ReadMagicSnapshot(snap)) return;

    const bool catChanged      = !snap.category.empty() && snap.category != g_lastMagicCat;
    const bool itemChanged     = !snap.itemName.empty() && snap.itemName != g_lastMagicItem;
    const bool sameItem        = !itemChanged && !snap.itemName.empty() && snap.itemName == g_lastMagicItem;
    const bool equipChanged    = sameItem && snap.equipState >= 0 && snap.equipState != g_lastMagicEquipState;
    const int  favInt          = snap.favorite ? 1 : 0;
    const bool favoriteChanged = sameItem && g_lastMagicFavorite >= 0 && favInt != g_lastMagicFavorite;

    const bool effectsChanged  = sameItem && !snap.effects.empty() && snap.effects != g_lastMagicEffects;

    const bool firstRead = g_lastMagicCat.empty() && g_lastMagicItem.empty();
    if (catChanged) {
        if (firstRead) SpeakQueue(snap.category); else Speak(snap.category);
        g_lastMagicCat        = snap.category;
        g_lastMagicItem.clear();
        g_lastMagicEffects.clear();
        g_lastMagicEquipState = -1;
        g_lastMagicFavorite   = -1;
    }

    if (itemChanged) {
        std::wstring announce = snap.itemName;
        announce += MagicEquipText(snap.equipState);
        if (snap.favorite)      announce += L", favorite";
        if (!snap.itemEnabled)  announce += L", locked";
        if (firstRead) SpeakQueue(announce); else Speak(announce);
        g_lastMagicItem       = snap.itemName;
        g_lastMagicEquipState = snap.equipState;
        g_lastMagicFavorite   = favInt;
        g_lastMagicEffects.clear();  // réinitialise pour laisser arriver les effets async
    } else if (equipChanged) {
        std::wstring announce = snap.itemName;
        announce += MagicEquipText(snap.equipState);
        Speak(announce);
        g_lastMagicEquipState = snap.equipState;
    } else if (favoriteChanged) {
        Speak(snap.favorite ? L"added to favorites" : L"removed from favorites");
        g_lastMagicFavorite = favInt;
    }

    // Effets chargés de façon asynchrone → annoncés dès qu'ils arrivent (+ coût/recharge)
    if (effectsChanged) {
        SpeakQueue(snap.effects);
        if (snap.spellCost > 0) {
            if (snap.itemType == MAGIC_ICT_SHOUT)
                SpeakQueue(L"recovery " + std::to_wstring(static_cast<int>(snap.spellCost)) + L" seconds");
            else if (snap.itemType != MAGIC_ICT_ACTIVE_EFFECT)
                SpeakQueue(L"cost " + std::to_wstring(static_cast<int>(snap.spellCost)));
        }
        if (snap.itemType == MAGIC_ICT_SHOUT) {
            for (int i = 0; i < 3; ++i) {
                if (snap.shoutWords[i].name.empty()) continue;
                SpeakQueue(snap.shoutWords[i].name +
                           (snap.shoutWords[i].unlocked ? L", known" : L", locked"));
            }
        }
        g_lastMagicEffects = snap.effects;
    }
}

static void QueueMagicRead() {
    if (!g_magicOpen.load(std::memory_order_relaxed)) return;
    if (g_magicPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_magicPendingUIRead.store(false); return; }
    task->AddUITask([]() {
        g_magicPendingUIRead.store(false);
        if (g_magicOpen.load()) AnnounceMagicChangeImpl();
    });
}

static void StartMagicPolling() {
    if (g_magicPollThread.joinable()) { g_magicPollThread.request_stop(); g_magicPollThread.join(); }
    g_magicPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_magicOpen.load(std::memory_order_relaxed)) QueueMagicRead();
        }
    });
}

static void StopMagicPolling() {
    if (g_magicPollThread.joinable()) { g_magicPollThread.request_stop(); g_magicPollThread.join(); }
}

static void DiagnoseMagicNow() {
    if (!g_magicOpen.load(std::memory_order_relaxed)) {
        Speak(L"Magic menu closed");
        return;
    }
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        MagicSnapshot snap;
        if (!ReadMagicSnapshot(snap)) { Speak(L"Magic: no data"); return; }

        if (!snap.category.empty()) Speak(snap.category);

        if (snap.itemName.empty()) { SpeakQueue(L"no item selected"); return; }

        std::wstring announce = snap.itemName;
        announce += MagicEquipText(snap.equipState);
        if (snap.favorite)     announce += L", favorite";
        if (!snap.itemEnabled) announce += L", locked";
        SpeakQueue(announce);

        if (snap.itemType == MAGIC_ICT_SPELL) {
            if (snap.castLevel > 0)
                SpeakQueue(L"skill level " + std::to_wstring(static_cast<int>(snap.castLevel)));
            if (snap.spellCost > 0)
                SpeakQueue(L"cost " + std::to_wstring(static_cast<int>(snap.spellCost)));
            if (snap.castTime == 0.0)
                SpeakQueue(L"concentration");

        } else if (snap.itemType == MAGIC_ICT_SPELL_DEFAULT) {
            if (snap.spellCost > 0)
                SpeakQueue(L"cost " + std::to_wstring(static_cast<int>(snap.spellCost)));

        } else if (snap.itemType == MAGIC_ICT_SHOUT) {
            for (int i = 0; i < 3; ++i) {
                if (snap.shoutWords[i].name.empty()) continue;
                SpeakQueue(snap.shoutWords[i].name +
                      (snap.shoutWords[i].unlocked ? L", known" : L", locked"));
            }

        } else if (snap.itemType == MAGIC_ICT_ACTIVE_EFFECT && snap.timeRemaining > 0) {
            int secs = static_cast<int>(snap.timeRemaining);
            std::wstring timeStr;
            if (secs >= 3600) {
                int h = secs / 3600;
                timeStr = std::to_wstring(h) + (h == 1 ? L" hour" : L" hours");
            } else if (secs >= 60) {
                int m = secs / 60;
                timeStr = std::to_wstring(m) + (m == 1 ? L" minute" : L" minutes");
            } else {
                timeStr = std::to_wstring(secs) + (secs == 1 ? L" second" : L" seconds");
            }
            SpeakQueue(timeStr);
        }

        if (!snap.effects.empty()) SpeakQueue(snap.effects);
    });
}

// VOCALISATION MAGIE - FIN
