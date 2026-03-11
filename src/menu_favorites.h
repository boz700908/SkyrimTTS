#pragma once

// VOCALISATION MENU FAVORIS - DEBUT

static std::atomic_bool g_favOpen{false};
static std::atomic_bool g_favPendingUIRead{false};
static std::jthread     g_favPollThread;
static std::wstring     g_lastFavItemAnnounce;

static std::wstring FavEquipStateText(int state) {
    switch (state) {
        case 1: return L"equipped";
        case 2: return L"left hand";
        case 3: return L"right hand";
        case 4: return L"both hands";
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

    std::string itemName;
    double equipState = 0.0;
    double hotkey = -1.0;

    GetGFxString(movie, "_root.MenuHolder.Menu_mc.List_mc.selectedEntry.text", itemName);
    GetGFxNumber(movie, "_root.MenuHolder.Menu_mc.List_mc.selectedEntry.equipState", equipState);
    GetGFxNumber(movie, "_root.MenuHolder.Menu_mc.List_mc.selectedEntry.hotkey", hotkey);

    if (itemName.empty()) return;

    std::wstring announce = ResolveUIString(movie, itemName);
    int eq = static_cast<int>(equipState);
    std::wstring eqText = FavEquipStateText(eq);
    if (!eqText.empty())
        announce += L", " + eqText;
    int hk = static_cast<int>(hotkey);
    if (hk >= 0 && hk <= 7)
        announce += L", hotkey " + std::to_wstring(hk + 1);

    if (announce == g_lastFavItemAnnounce) return;

    const bool firstRead = g_lastFavItemAnnounce.empty();
    if (firstRead) SpeakQueue(announce); else Speak(announce);
    g_lastFavItemAnnounce = announce;
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

// VOCALISATION MENU FAVORIS - FIN
