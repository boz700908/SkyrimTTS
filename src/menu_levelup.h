#pragma once

// VOCALISATION MENU LEVEL UP - DEBUT

static std::atomic_bool g_levelUpOpen{false};
static std::atomic_int  g_levelUpSelection{0}; // 0=Health, 1=Magicka, 2=Stamina

static std::wstring LevelUpSelectionName(int sel) {
    switch (sel) {
        case 0: return TR("Health");
        case 1: return TR("Magicka");
        case 2: return TR("Stamina");
        default: return L"";
    }
}

static void AnnounceLevelUpSelection(bool queue = false) {
    std::wstring name = LevelUpSelectionName(g_levelUpSelection.load());
    if (queue) SpeakQueue(name); else Speak(name);
}

static void ConfirmLevelUpSelectionImpl() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::LevelUpMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    const char* fn = nullptr;
    switch (g_levelUpSelection.load()) {
        case 0: fn = "_root.LevelUpMenu_mc.addHealth";  break;
        case 1: fn = "_root.LevelUpMenu_mc.addMagicka"; break;
        case 2: fn = "_root.LevelUpMenu_mc.addStamina"; break;
    }
    if (fn) movie->Invoke(fn, nullptr, nullptr, 0);
}

static void QueueConfirmLevelUp() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        if (g_levelUpOpen.load()) ConfirmLevelUpSelectionImpl();
    });
}

static void DiagnoseLevelUpImpl() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::LevelUpMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    LOG("=== LevelUp diagnostic ===");
    static const char* candidates[] = {
        "_root.LevelUpMenu_mc.Title",
        "_root.LevelUpMenu_mc.Title.text",
        "_root.LevelUpMenu_mc.TitleText",
        "_root.LevelUpMenu_mc.TitleText.text",
        "_root.LevelUpMenu_mc.Header",
        "_root.LevelUpMenu_mc.Header.text",
        "_root.LevelUpMenu_mc.HeaderText",
        "_root.LevelUpMenu_mc.HeaderText.text",
        "_root.LevelUpMenu_mc.Message",
        "_root.LevelUpMenu_mc.Message.text",
        "_root.LevelUpMenu_mc.MessageText",
        "_root.LevelUpMenu_mc.MessageText.text",
        "_root.LevelUpMenu_mc.Description",
        "_root.LevelUpMenu_mc.Description.text",
        "_root.LevelUpMenu_mc.DescriptionText",
        "_root.LevelUpMenu_mc.DescriptionText.text",
        "_root.LevelUpMenu_mc.LevelText",
        "_root.LevelUpMenu_mc.LevelText.text",
        "_root.LevelUpMenu_mc.SubText",
        "_root.LevelUpMenu_mc.SubText.text",
        "_root.LevelUpMenu_mc.HealthButton.label",
        "_root.LevelUpMenu_mc.HealthButton.ButtonText.text",
        "_root.LevelUpMenu_mc.MagickaButton.label",
        "_root.LevelUpMenu_mc.MagickaButton.ButtonText.text",
        "_root.LevelUpMenu_mc.StaminaButton.label",
        "_root.LevelUpMenu_mc.StaminaButton.ButtonText.text",
        "_root.Title",
        "_root.Title.text",
        "_root.TitleText",
        "_root.TitleText.text",
        "_root.Message",
        "_root.Message.text",
    };

    std::string val;
    for (auto path : candidates) {
        if (GetGFxString(movie, path, val) && !val.empty())
            LOG("  [HIT] {} = \"{}\"", path, val);
        else
            LOG("  [   ] {}", path);
    }
    LOG("=== fin diagnostic ===");
}

static void QueueDiagnoseLevelUp() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() { DiagnoseLevelUpImpl(); });
}

// VOCALISATION MENU LEVEL UP - FIN
