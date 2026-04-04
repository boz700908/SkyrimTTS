#pragma once
#include "common.h"

// UIExtensions UIListMenu — vocalisation du menu V/L du mod d'accessibilité gameplay
// Le menu GFx s'appelle "CustomMenu", les items sont ajoutés après ouverture via Papyrus

static constexpr const char* UILIST_MENU_NAME = "CustomMenu";

// Structure : _root.listMenu.itemView.itemList.ItemListEntry{i}.textField.text
// L'item sélectionné a selectIndicator._visible = true

static std::atomic_bool g_uiListMenuOpen{false};
static std::atomic_bool g_uiListPendingRead{false};
static std::wstring     g_lastUIListItem;
static std::jthread     g_uiListPollThread;

static void AnnounceUIListChangeImpl() {
    g_uiListPendingRead.store(false);

    auto ui = RE::UI::GetSingleton(); if (!ui) return;
    auto menu = ui->GetMenu(UILIST_MENU_NAME); if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get(); if (!movie) return;

    const bool firstRead = g_lastUIListItem.empty();

    // Lire les entrées visibles jusqu'à trouver celle qui est sélectionnée
    // Les clips sont nommés ItemListEntry0..N dans itemList
    for (int i = 0; i < 50; i++) {
        std::string visPath = "_root.listMenu.itemView.itemList.ItemListEntry" + std::to_string(i) + ".selectIndicator._visible";
        std::string txtPath = "_root.listMenu.itemView.itemList.ItemListEntry" + std::to_string(i) + ".textField.text";

        RE::GFxValue vis;
        RE::GFxValue txt;
        if (!SafeGetVariable(movie, vis, visPath.c_str())) continue;
        if (!vis.IsBool() || !vis.GetBool()) continue;
        if (!SafeGetVariable(movie, txt, txtPath.c_str()) || !txt.IsString()) continue;

        std::string s = txt.GetString();
        if (s.empty()) continue;
        // Ignorer les placeholders par défaut du SWF (avant que Papyrus remplisse les items)
        if (s == "text" || s == "texte" || s == "Text") continue;

        std::wstring ws = StripMarkupForSpeech(Utf8ToWString(s));
        if (ws != g_lastUIListItem) {
            g_lastUIListItem = ws;
            if (firstRead)
                SpeakQueue(ws);
            else
                Speak(ws);
            LOG("UIListMenu selected: '{}'", s);
        }
        return;
    }
}

static void QueueUIListRead() {
    if (g_uiListPendingRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_uiListPendingRead.store(false); return; }
    task->AddUITask([]() { AnnounceUIListChangeImpl(); });
}

static void StartUIListPolling() {
    if (g_uiListPollThread.joinable()) g_uiListPollThread.request_stop();
    g_uiListPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!g_uiListMenuOpen.load(std::memory_order_relaxed)) break;
            QueueUIListRead();
        }
    });
}

static void StopUIListPolling() {
    if (g_uiListPollThread.joinable()) {
        g_uiListPollThread.request_stop();
        g_uiListPollThread.join();
    }
}
