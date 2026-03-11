#pragma once

// VOCALISATION MENU EN CROIX - DEBUT

// --- Chemins GFx (niveau + date, injectés par StartOpenMenuAnim) ---
static constexpr const char* TWEEN_LEVEL_TEXT  = "_root.TweenMenu_mc.BottomBarTweener_mc.BottomBar_mc.LevelNumberLabel.text";
static constexpr const char* TWEEN_DATE_TEXT   = "_root.TweenMenu_mc.BottomBarTweener_mc.BottomBar_mc.DateText.text";
static constexpr const char* TWEEN_SKILLS_TEXT = "_root.TweenMenu_mc.Selections_mc.SkillsText_mc.textField.text";

// --- État ---
static std::atomic_bool g_tweenOpen{false};       // TweenMenu sur la stack (reste vrai en fond)
static std::atomic_bool g_tweenForeground{false}; // croix visuellement active (aucun sous-menu par-dessus)
static std::atomic_bool g_tweenPendingRead{false};
static std::atomic_bool g_tweenLevelAnnounced{false};
static std::jthread     g_tweenPollThread;
static std::atomic<int> g_lastTweenFrame{-1};

// UP=2(Skills) LEFT=3(Magic) RIGHT=4(Items) DOWN=5(Map)
static const wchar_t* TweenDirectionSuffix(int frame) {
    switch (frame) {
        case 2: return L", up";
        case 3: return L", left";
        case 4: return L", right";
        case 5: return L", down";
        default: return L"";
    }
}

// Appelé directement depuis InputListener — pas besoin de GFx
static void AnnounceTweenNavKey(int frame) {
    std::wstring announce;
    switch (frame) {
        case 2: announce = L"Skills"; break;
        case 3: announce = L"Magic";  break;
        case 4: announce = L"Items";  break;
        case 5: announce = L"Map";    break;
        default: return;
    }
    announce += TweenDirectionSuffix(frame);
    Speak(announce);
}

// Lit niveau + date sur le thread UI (appelé via polling après StartOpenMenuAnim)
static void AnnounceTweenLevelImpl() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::TweenMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    std::string tmp;
    std::wstring announce;

    if (GetGFxString(movie, TWEEN_LEVEL_TEXT, tmp) && !tmp.empty())
        announce += L"Level " + ResolveUIString(movie, tmp);

    if (GetGFxString(movie, TWEEN_DATE_TEXT, tmp) && !tmp.empty()) {
        if (!announce.empty()) announce += L", ";
        announce += ResolveUIString(movie, tmp);
    }

    std::string skillsRaw;
    if (GetGFxString(movie, TWEEN_SKILLS_TEXT, skillsRaw) && skillsRaw == "$LEVEL UP") {
        if (!announce.empty()) announce += L", ";
        announce += ResolveUIString(movie, skillsRaw);
    }

    LOG("TweenMenu level read: announce='{}'",
        WStringToUtf8(announce.empty() ? L"(empty)" : announce));

    if (!announce.empty()) {
        Speak(announce);
        g_tweenLevelAnnounced.store(true);
    }
}

static void QueueTweenLevelRead() {
    if (g_tweenLevelAnnounced.load(std::memory_order_relaxed)) return;
    if (g_tweenPendingRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_tweenPendingRead.store(false); return; }
    task->AddUITask([]() {
        g_tweenPendingRead.store(false);
        if (g_tweenOpen.load()) AnnounceTweenLevelImpl();
    });
}

static void StartTweenPolling() {
    if (g_tweenPollThread.joinable()) { g_tweenPollThread.request_stop(); g_tweenPollThread.join(); }
    g_tweenPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_tweenOpen.load(std::memory_order_relaxed))
                QueueTweenLevelRead();
        }
    });
}

static void StopTweenPolling() {
    if (g_tweenPollThread.joinable()) { g_tweenPollThread.request_stop(); g_tweenPollThread.join(); }
}

// VOCALISATION MENU EN CROIX - FIN
