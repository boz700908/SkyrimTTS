#pragma once

// VOCALISATION MESSAGE BOX - DEBUT

static std::atomic_bool g_msgBoxOpen{false};
static std::atomic_int  g_msgBoxSelectedBtn{0};
static std::atomic_int  g_msgBoxBtnCount{0};

// Lit message + boutons, compte les boutons, annonce tout sur le thread UI
static void AnnounceMsgBoxImpl() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::MessageBoxMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    std::string tmp;
    std::wstring announce;

    // Texte du message
    if (GetGFxString(movie, "_root.MessageMenu.MessageText.text", tmp) && !tmp.empty())
        announce += ResolveUIString(movie, tmp);

    // Compte et lit les boutons
    int count = 0;
    for (int i = 0; i < 8; ++i) {
        std::string path = "_root.MessageMenu.Buttons.Button" + std::to_string(i) + ".ButtonText.text";
        if (!GetGFxString(movie, path.c_str(), tmp) || tmp.empty()) break;
        if (!announce.empty()) announce += L", ";
        announce += ResolveUIString(movie, tmp);
        count = i + 1;
    }

    g_msgBoxBtnCount.store(count);
    g_msgBoxSelectedBtn.store(0);

    if (!announce.empty()) Speak(announce);
}

// Vocalise le bouton actuellement sélectionné
static void AnnounceMsgBoxBtnImpl() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::MessageBoxMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    int sel = g_msgBoxSelectedBtn.load();
    std::string path = "_root.MessageMenu.Buttons.Button" + std::to_string(sel) + ".ButtonText.text";
    std::string tmp;
    if (GetGFxString(movie, path.c_str(), tmp) && !tmp.empty())
        Speak(ResolveUIString(movie, tmp));
}

// Appuie sur un bouton par index via GFxValue::Invoke sur l'objet bouton
static void MsgBoxPressButtonImpl(int buttonIdx) {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::MessageBoxMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    std::string path = "_root.MessageMenu.Buttons.Button" + std::to_string(buttonIdx);
    RE::GFxValue button;
    if (!SafeGetVariable(movie, button, path.c_str()) || !button.IsObject()) {
        LOG("MsgBox: button {} not found at '{}'", buttonIdx, path);
        return;
    }

    LOG("MsgBox: invoking handleMousePress on Button{}", buttonIdx);
    button.Invoke("handleMousePress", nullptr, nullptr, 0);
}

// Presse le bouton actuellement sélectionné
static void MsgBoxPressSelectedImpl() {
    MsgBoxPressButtonImpl(g_msgBoxSelectedBtn.load());
}

static void MsgBoxPressCancelImpl() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::MessageBoxMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    double isCancellable = 0.0;
    GetGFxNumber(movie, "_root.MessageMenu.IsCancellable", isCancellable);
    if (isCancellable == 0.0) {
        LOG("MsgBox: not cancellable, ignoring Escape");
        return;
    }

    double cancelIdx = 0.0;
    if (!GetGFxNumber(movie, "_root.MessageMenu.CancelOptionIndex", cancelIdx)) {
        LOG("MsgBox: CancelOptionIndex not readable, skipping cancel");
        return;
    }

    MsgBoxPressButtonImpl(static_cast<int>(cancelIdx));
}

static void QueueMsgBoxAnnounce() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() { if (g_msgBoxOpen.load()) AnnounceMsgBoxImpl(); });
}

static void QueueAnnounceMsgBoxBtn() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() { if (g_msgBoxOpen.load()) AnnounceMsgBoxBtnImpl(); });
}

static void QueueMsgBoxPress() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() { if (g_msgBoxOpen.load()) MsgBoxPressSelectedImpl(); });
}

static void QueueMsgBoxCancel() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() { if (g_msgBoxOpen.load()) MsgBoxPressCancelImpl(); });
}

// VOCALISATION MESSAGE BOX - FIN
