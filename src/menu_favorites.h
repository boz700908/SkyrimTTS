#pragma once

#include <mutex>
#include <unordered_map>

// VOCALISATION MENU FAVORIS - DEBUT

static std::atomic_bool g_favOpen{false};
static std::atomic_bool g_favPendingUIRead{false};
static std::jthread     g_favPollThread;
static std::wstring     g_lastFavItemAnnounce;
static std::wstring     g_lastFavCategory;

// Extended Hotkey System (EHS) — detecte si le mod est charge
// Permet d'annoncer les raccourcis F1-F12 assignes via Ctrl+F<N>
// sans generer de faux positifs pour les utilisateurs sans EHS.
static std::atomic_bool g_ehsInstalled{false};

// Map interne des raccourcis EHS : nom de l'item -> label du raccourci
// (ex: "3" pour la touche 3, "F1" pour Ctrl+F1). EHS stocke TOUTES ses
// assignations (numero ET F-keys) dans son propre co-save SKSE, invisible
// depuis GFx : il remplace favoritesmenu.swf et intercepte toutes les
// touches avant que SkyUI ne les voie, donc le champ .hotkey du
// dataProvider n'est jamais mis a jour. On maintient notre propre mapping
// pour pouvoir annoncer le raccourci a l'assignation ET au retour sur
// l'item.
static std::mutex g_ehsMapMutex;
static std::unordered_map<std::wstring, std::wstring> g_ehsHotkeyMap;

static std::wstring FavEquipStateText(int state) {
    switch (state) {
        case 1: return TR("equipped");
        case 2: return TR("left hand");
        case 3: return TR("right hand");
        case 4: return TR("both hands");
        default: return L"";
    }
}

static void AnnounceFavChangeImpl() {
    if (!g_favOpen.load()) return;
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::FavoritesMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
    const char* entryBase = skyui ? "_root.MenuHolder.Menu_mc.itemList.selectedEntry"
                                  : "_root.MenuHolder.Menu_mc.List_mc.selectedEntry";

    std::string itemName;
    double equipState = 0.0;
    double hotkey = -1.0;

    GetGFxString(movie, (std::string(entryBase) + ".text").c_str(), itemName);
    GetGFxNumber(movie, (std::string(entryBase) + ".equipState").c_str(), equipState);
    GetGFxNumber(movie, (std::string(entryBase) + ".hotkey").c_str(), hotkey);

    // SkyUI: lire la catégorie depuis headerText
    if (skyui) {
        std::string catStr;
        if (GetGFxString(movie, "_root.MenuHolder.Menu_mc.headerText.text", catStr) && !catStr.empty()) {
            std::wstring cat = ResolveUIString(movie, catStr);
            if (!cat.empty() && cat != g_lastFavCategory) {
                const bool firstCat = g_lastFavItemAnnounce.empty() && g_lastFavCategory.empty();
                if (firstCat) SpeakQueue(cat); else Speak(cat);
                g_lastFavCategory = cat;
            }
        }
    }

    if (itemName.empty()) return;

    std::wstring item = ResolveUIString(movie, itemName);
    std::wstring announce = item;
    int eq = static_cast<int>(equipState);
    std::wstring eqText = FavEquipStateText(eq);
    if (!eqText.empty())
        announce += L", " + eqText;

    // Raccourci :
    // - Si EHS est installe, c'est lui qui gere TOUTES les assignations
    //   (y compris 1-8). Le champ GFx .hotkey n'est jamais mis a jour par EHS.
    //   On utilise donc notre map interne comme source de verite.
    // - Si EHS n'est pas installe, SkyUI met a jour .hotkey normalement.
    if (g_ehsInstalled.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(g_ehsMapMutex);
        auto it = g_ehsHotkeyMap.find(item);
        if (it != g_ehsHotkeyMap.end())
            announce += L", " + TR("hotkey") + L" " + it->second;
    } else {
        int hk = static_cast<int>(hotkey);
        if (hk >= 0 && hk <= 7)
            announce += L", " + TR("hotkey") + L" " + std::to_wstring(hk + 1);
    }

    if (announce == g_lastFavItemAnnounce) return;

    const bool firstRead = g_lastFavItemAnnounce.empty();
    if (firstRead) SpeakQueue(announce); else Speak(announce);
    g_lastFavItemAnnounce = announce;
    LOG("Fav read: item={} hk_gfx={} ehs={}",
        WToNarrow(item), static_cast<int>(hotkey),
        g_ehsInstalled.load() ? "on" : "off");
}

static void QueueFavRead() {
    if (!g_favOpen.load(std::memory_order_relaxed)) return;
    if (g_favPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_favPendingUIRead.store(false); return; }
    task->AddUITask([]() {
        g_favPendingUIRead.store(false);
        if (g_favOpen.load()) AnnounceFavChangeImpl();
    });
}

static void StartFavPolling() {
    if (g_favPollThread.joinable()) { g_favPollThread.request_stop(); g_favPollThread.join(); }
    g_favPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_favOpen.load(std::memory_order_relaxed)) QueueFavRead();
        }
    });
}

static void StopFavPolling() {
    if (g_favPollThread.joinable()) { g_favPollThread.request_stop(); g_favPollThread.join(); }
}

// --- Extended Hotkey System : gestion des raccourcis dans le menu favoris ---
// EHS intercepte TOUTES les assignations (numeros 1-8 ET F1-F12 via Ctrl+F<N>)
// avant SkyUI et les stocke dans son propre co-save SKSE, jamais visible dans
// GFx. On maintient donc notre propre map comme source de verite quand EHS
// est installe.
//
// label = "1".."8" pour les touches numeriques vanilla, "F1".."F12" pour les
// raccourcis etendus via Ctrl+F<N>.
static void AnnounceEHSHotkeyImpl(std::wstring label) {
    if (!g_favOpen.load()) return;
    if (!g_ehsInstalled.load()) return;
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::FavoritesMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
    const char* entryBase = skyui ? "_root.MenuHolder.Menu_mc.itemList.selectedEntry"
                                  : "_root.MenuHolder.Menu_mc.List_mc.selectedEntry";

    std::string itemName;
    if (!GetGFxString(movie, (std::string(entryBase) + ".text").c_str(), itemName) || itemName.empty()) {
        LOG("EHS assign: selectedEntry.text empty or unreadable, skipping");
        return;
    }

    std::wstring item = ResolveUIString(movie, itemName);

    // Enregistre ou met a jour l'assignation dans notre map interne.
    // Si ce meme label etait deja assigne a un autre item, on le retire.
    {
        std::lock_guard<std::mutex> lock(g_ehsMapMutex);
        for (auto it = g_ehsHotkeyMap.begin(); it != g_ehsHotkeyMap.end();) {
            if (it->second == label && it->first != item)
                it = g_ehsHotkeyMap.erase(it);
            else
                ++it;
        }
        g_ehsHotkeyMap[item] = label;
    }

    // Reconstruction de l'annonce dans le meme format que AnnounceFavChangeImpl
    // pour que le cache colle avec ce que le prochain poll lira.
    double equipState = 0.0;
    GetGFxNumber(movie, (std::string(entryBase) + ".equipState").c_str(), equipState);
    std::wstring announce = item;
    std::wstring eqText = FavEquipStateText(static_cast<int>(equipState));
    if (!eqText.empty())
        announce += L", " + eqText;
    announce += L", " + TR("hotkey") + L" " + label;

    Speak(announce);
    // Synchronise le cache pour eviter un double announce au prochain poll.
    g_lastFavItemAnnounce = announce;
    LOG("EHS assign: {} -> hotkey {}", WToNarrow(item), WToNarrow(label));
}

static void QueueEHSHotkeyAnnounce(const std::wstring& label) {
    if (!g_favOpen.load(std::memory_order_relaxed)) return;
    if (!g_ehsInstalled.load(std::memory_order_relaxed)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    std::wstring labelCopy = label;
    task->AddUITask([labelCopy]() mutable {
        AnnounceEHSHotkeyImpl(std::move(labelCopy));
    });
}

// VOCALISATION MENU FAVORIS - FIN
