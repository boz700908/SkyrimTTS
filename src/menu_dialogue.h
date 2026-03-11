#pragma once

// VOCALISATION MENU DIALOGUE - DEBUT

static std::atomic_bool g_dialogueOpen{false};
static std::atomic_bool g_dialoguePendingRead{false};
static std::jthread     g_dialoguePollThread;
static std::wstring     g_lastDialogueOption;

static void AnnounceDialogueChangeImpl() {
    if (!g_dialogueOpen.load()) return;

    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu<RE::DialogueMenu>();
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    std::string tmp;

    // Option sélectionnée par le joueur — uniquement quand la liste est visible (state 1)
    double menuStateVal = 0.0;
    GetGFxNumber(movie, "_root.DialogueMenu_mc.eMenuState", menuStateVal);
    if (static_cast<int>(menuStateVal) != 1) return;

    if (GetGFxString(movie, "_root.DialogueMenu_mc.TopicListHolder.List_mc.selectedEntry.text", tmp) && !tmp.empty()) {
        const std::wstring option = ResolveUIString(movie, tmp);
        if (!option.empty() && option != g_lastDialogueOption) {
            Speak(option);
            g_lastDialogueOption = option;
        }
    }
}

static void QueueDialogueRead() {
    if (!g_dialogueOpen.load(std::memory_order_relaxed)) return;
    if (g_dialoguePendingRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_dialoguePendingRead.store(false); return; }
    task->AddUITask([]() {
        g_dialoguePendingRead.store(false);
        if (g_dialogueOpen.load()) AnnounceDialogueChangeImpl();
    });
}

static void StartDialoguePolling() {
    if (g_dialoguePollThread.joinable()) { g_dialoguePollThread.request_stop(); g_dialoguePollThread.join(); }
    g_dialoguePollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (g_dialogueOpen.load(std::memory_order_relaxed)) QueueDialogueRead();
        }
    });
}

static void StopDialoguePolling() {
    if (g_dialoguePollThread.joinable()) { g_dialoguePollThread.request_stop(); g_dialoguePollThread.join(); }
}

// VOCALISATION MENU DIALOGUE - FIN
