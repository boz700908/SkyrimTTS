#pragma once

// VOCALISATION MENU RACESEX (CREATION PERSONNAGE) - DEBUT

// Convertit un keycode clavier Skyrim (DirectInput scan code) en nom lisible
static std::wstring GetKeyName(std::uint32_t keyCode)
{
    static const std::unordered_map<std::uint32_t, std::wstring> kKeyNames = {
        {0x01, L"Escape"}, {0x02, L"1"}, {0x03, L"2"}, {0x04, L"3"}, {0x05, L"4"},
        {0x06, L"5"}, {0x07, L"6"}, {0x08, L"7"}, {0x09, L"8"}, {0x0A, L"9"}, {0x0B, L"0"},
        {0x0E, L"Backspace"}, {0x0F, L"Tab"},
        {0x10, L"Q"}, {0x11, L"W"}, {0x12, L"E"}, {0x13, L"R"}, {0x14, L"T"},
        {0x15, L"Y"}, {0x16, L"U"}, {0x17, L"I"}, {0x18, L"O"}, {0x19, L"P"},
        {0x1C, L"Enter"}, {0x1D, L"Ctrl"},
        {0x1E, L"A"}, {0x1F, L"S"}, {0x20, L"D"}, {0x21, L"F"}, {0x22, L"G"},
        {0x23, L"H"}, {0x24, L"J"}, {0x25, L"K"}, {0x26, L"L"},
        {0x2A, L"Shift"},
        {0x2C, L"Z"}, {0x2D, L"X"}, {0x2E, L"C"}, {0x2F, L"V"},
        {0x30, L"B"}, {0x31, L"N"}, {0x32, L"M"},
        {0x36, L"Right Shift"}, {0x38, L"Alt"}, {0x39, L"Space"},
        {0x3B, L"F1"}, {0x3C, L"F2"}, {0x3D, L"F3"}, {0x3E, L"F4"}, {0x3F, L"F5"},
        {0x40, L"F6"}, {0x41, L"F7"}, {0x42, L"F8"}, {0x43, L"F9"}, {0x44, L"F10"},
        {0x57, L"F11"}, {0x58, L"F12"},
        // Pavé numérique
        {0x47, L"Numpad7"}, {0x48, L"Numpad8"}, {0x49, L"Numpad9"},
        {0x4B, L"Numpad4"}, {0x4C, L"Numpad5"}, {0x4D, L"Numpad6"},
        {0x4F, L"Numpad1"}, {0x50, L"Numpad2"}, {0x51, L"Numpad3"},
        {0x52, L"Numpad0"}, {0x53, L"Numpad."},
        // mouse buttons (codes >= 256 in Skyrim = 0x100+)
        {0x100, L"Left Click"}, {0x101, L"Right Click"}, {0x102, L"Middle Click"},
    };
    auto it = kKeyNames.find(keyCode);
    if (it != kKeyNames.end()) return it->second;
    return TR("Key") + L" " + std::to_wstring(keyCode);
}

// Retourne les hints de navigation pour le menu RaceSex
// Numpad 5 / Numpad 8 pour changer de catégorie (touches confirmées en jeu)
// R (XButton) pour confirmer
static std::wstring BuildRaceSexHints()
{
    std::wstring cached;
    cached = L". " + TR("Ctrl Left / Right: change category");
    auto* cm = RE::ControlMap::GetSingleton();
    if (cm) {
        constexpr std::uint32_t kInvalid = 0xFF;
        const std::uint32_t doneKey = cm->GetMappedKey("XButton", RE::INPUT_DEVICE::kKeyboard, RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu);
        if (doneKey != kInvalid)
            cached += L", " + GetKeyName(doneKey) + L": " + TR("confirm");
    }
    return cached;
}

static std::atomic_bool g_raceSexOpen{false};
static std::atomic_bool g_raceSexPendingRead{false};
static std::jthread     g_raceSexPollThread;
static std::wstring     g_lastRaceSexCat;
static std::wstring     g_lastRaceSexRace;
static std::wstring     g_lastRaceSexSliderLabel;
static int              g_raceSexTickCount{0};  // compteur de ticks pour délai nom
static double           g_lastRaceSexSliderValue{-1.0};
static std::wstring     g_lastRaceSexName;
static std::wstring     g_lastRaceSexRaceDesc;
static int              g_lastRaceSexSex{-1};
static bool             g_lastRaceSexNameEntryActive{false};

// Detection RaceMenu vs vanilla : on tente les chemins RaceMenu en premier.
// Si le mod RaceMenu est installe, la structure GFx est completement differente :
//   RaceMenu: _root.RaceSexMenuBaseInstance.RaceSexPanelsInstance est un MovieClip "RaceMenu"
//     .racePanel.slidingCategoryList.categoryList.selectedEntry.text  (categorie)
//     .racePanel.itemList.selectedEntry.text                          (item/slider label)
//     .racePanel.itemList.selectedEntry.position                      (valeur du slider)
//     .raceDescription.textField.text                                 (description race)
//     .bottomBar.playerInfo.PlayerName.text                           (nom du joueur)
//   Vanilla: chemins avec CagetoryLockBaseInstance, PanelTwoNarrowInstance, etc.
static std::atomic_bool g_raceSexIsRaceMenu{false};

// Prefixe commun pour eviter la repetition
static constexpr const char* RM_BASE = "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance";

static void AnnounceRaceSexChangeImpl() {
    if (!g_raceSexOpen.load()) return;

    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu<RE::RaceSexMenu>();
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    std::string tmp;

    const std::wstring hints = BuildRaceSexHints();
    const bool firstRead = g_lastRaceSexSex < 0;
    g_raceSexTickCount++;

    // Detection auto RaceMenu vs vanilla (une seule fois)
    // RaceMenu expose .racePanel.itemList, vanilla expose .PanelTwoWideInstance
    if (firstRead) {
        RE::GFxValue testVal;
        std::string rmTestPath = std::string(RM_BASE) + ".racePanel.itemList";
        if (movie->GetVariable(&testVal, rmTestPath.c_str()) && testVal.IsObject()) {
            g_raceSexIsRaceMenu.store(true);
            LOG("RaceSex: RaceMenu mod detected (racePanel.itemList found)");
        } else {
            g_raceSexIsRaceMenu.store(false);
            LOG("RaceSex: vanilla mode (racePanel.itemList not found)");
        }
    }

    const bool isRM = g_raceSexIsRaceMenu.load(std::memory_order_relaxed);

    // 1. Sexe depuis les donnees C++ (source fiable, pas le slider GFx)
    const int sex = (menu->GetRuntimeData().sex == RE::SEX::kFemale) ? 1 : 0;
    if (sex != g_lastRaceSexSex) {
        LOG("RaceSex: sex={} firstRead={}", sex == 1 ? "Female" : "Male", firstRead);
        g_lastRaceSexSex = sex;
    }

    // 2. Categorie active
    bool catChanged = false;
    {
        // RaceMenu: racePanel.slidingCategoryList.categoryList.selectedEntry.text
        // Vanilla: CagetoryLockBaseInstance.CategoryInstance.List_mc.SelectedEntry.textField.text
        const char* catPaths[] = {
            isRM ? "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.racePanel.slidingCategoryList.categoryList.selectedEntry.text"
                 : "_root.RaceSexMenuBaseInstance.CagetoryLockBaseInstance.CategoryInstance.List_mc.SelectedEntry.textField.text",
            nullptr
        };
        for (auto p = catPaths; *p; ++p) {
            if (GetGFxString(movie, *p, tmp) && !tmp.empty()) break;
        }
        if (!tmp.empty()) {
            const std::wstring cat = ResolveUIString(movie, tmp);
            if (!cat.empty() && cat != g_lastRaceSexCat) {
                LOG("RaceSex: cat='{}' firstRead={} rm={}", WStringToUtf8(cat), firstRead, isRM);
                g_lastRaceSexCat = cat;
                catChanged = true;
                g_lastRaceSexSliderLabel.clear();
                g_lastRaceSexSliderValue = -1.0;
                if (firstRead) SpeakQueue(cat + hints); else Speak(cat + hints);
            }
        }
    }

    // 3. Race selectionnee / item selectionne
    {
        // RaceMenu: racePanel.itemList.selectedEntry.text (contient race OU slider label)
        // Vanilla: PanelTwoNarrowInstance.List_mc.SelectedEntry.textField.text
        tmp.clear();
        if (isRM) {
            GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.racePanel.itemList.selectedEntry.text", tmp);
        } else {
            GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.PanelTwoNarrowInstance.List_mc.SelectedEntry.textField.text", tmp);
        }
        if (!tmp.empty()) {
            bool isNumeric = true;
            for (auto c : tmp) { if (c < '0' || c > '9') { isNumeric = false; break; } }
            if (!isNumeric) {
                const std::wstring race = ResolveUIString(movie, tmp);
                if (!race.empty() && race != g_lastRaceSexRace) {
                    LOG("RaceSex: race/item='{}' catChanged={}", WStringToUtf8(race), catChanged);
                    if (firstRead || catChanged) SpeakQueue(race); else Speak(race);
                    g_lastRaceSexRace = race;
                    g_lastRaceSexRaceDesc.clear();
                }
            }
        }
    }

    // 4. Description de la race
    {
        tmp.clear();
        // RaceMenu: raceDescription.textField.text
        // Vanilla: RaceDescriptionInstance.RaceTextInstance.text
        if (isRM)
            GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.raceDescription.textField.text", tmp);
        else
            GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.RaceDescriptionInstance.RaceTextInstance.text", tmp);

        if (!tmp.empty()) {
            const std::wstring desc = StripMarkupForSpeech(Utf8ToWString(tmp));
            if (!desc.empty() && desc != g_lastRaceSexRaceDesc) {
                SpeakQueue(desc);
                g_lastRaceSexRaceDesc = desc;
            }
        }
    }

    // 5. Slider actif
    {
        std::wstring sliderLabel;
        double sliderVal = -1.0;

        if (isRM) {
            // RaceMenu: le slider label et la position sont dans itemList.selectedEntry
            tmp.clear();
            if (GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.racePanel.itemList.selectedEntry.text", tmp) && !tmp.empty())
                sliderLabel = ResolveUIString(movie, tmp);
            GetGFxNumber(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.racePanel.itemList.selectedEntry.position", sliderVal);
        } else {
            // Vanilla: PanelTwoWideInstance
            tmp.clear();
            if (GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.PanelTwoWideInstance.List_mc.SelectedEntry.textField.text", tmp) && !tmp.empty())
                sliderLabel = ResolveUIString(movie, tmp);
            GetGFxNumber(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.PanelTwoWideInstance.List_mc.SelectedEntry.SliderInstance.position", sliderVal);
        }

        // Ignorer les sliders parasites
        if (!sliderLabel.empty() && WStringToUtf8(sliderLabel) == "Selected Text")
            sliderLabel.clear();

        // Slider "Sexe"/"Sex" : lire Male/Female au lieu de 0/1
        bool isSexSlider = false;
        if (!sliderLabel.empty()) {
            std::string lbl = WStringToUtf8(sliderLabel);
            if (lbl == "Sexe" || lbl == "Sex") isSexSlider = true;
        }

        if (!sliderLabel.empty() && (sliderLabel != g_lastRaceSexSliderLabel || sliderVal != g_lastRaceSexSliderValue)) {
            std::wstring msg;
            if (isSexSlider) {
                msg = (sex == 1) ? TR("Female") : TR("Male");
            } else {
                msg = sliderLabel;
                if (sliderVal >= 0.0)
                    msg += L", " + std::to_wstring(static_cast<int>(std::round(sliderVal)));
            }
            LOG("RaceSex: slider='{}' val={:.1f} -> '{}'", WStringToUtf8(sliderLabel), sliderVal, WStringToUtf8(msg));
            if (firstRead || catChanged) SpeakQueue(msg); else Speak(msg);
            g_lastRaceSexSliderLabel = sliderLabel;
            g_lastRaceSexSliderValue = sliderVal;
        }
    }

    // 6. Nom du joueur
    {
        std::string nameTmp;
        if (isRM)
            GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.bottomBar.playerInfo.PlayerName.text", nameTmp);
        else
            GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.NameEntryInstance.TextInputInstance.text", nameTmp);

        if (!nameTmp.empty()) {
            const std::wstring name = Utf8ToWString(nameTmp);
            if (name != g_lastRaceSexName) {
                LOG("RaceSex: name='{}'", nameTmp);
                Speak(name);
                g_lastRaceSexName = name;
            }
        }
    }
}

static void QueueRaceSexRead() {
    if (!g_raceSexOpen.load(std::memory_order_relaxed)) return;
    if (g_raceSexPendingRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_raceSexPendingRead.store(false); return; }
    task->AddUITask([]() {
        g_raceSexPendingRead.store(false);
        if (g_raceSexOpen.load()) AnnounceRaceSexChangeImpl();
    });
}

static void StartRaceSexPolling() {
    if (g_raceSexPollThread.joinable()) { g_raceSexPollThread.request_stop(); g_raceSexPollThread.join(); }
    g_raceSexPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (g_raceSexOpen.load(std::memory_order_relaxed)) QueueRaceSexRead();
        }
    });
}

static void StopRaceSexPolling() {
    if (g_raceSexPollThread.joinable()) { g_raceSexPollThread.request_stop(); g_raceSexPollThread.join(); }
}

static void DiagnoseRaceSexNow() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        auto ui = RE::UI::GetSingleton();
        if (!ui) return;
        auto menu = ui->GetMenu<RE::RaceSexMenu>();
        if (!menu) { LOG("RaceSex diag: menu not open"); return; }
        RE::GFxMovieView* movie = menu->uiMovie.get();
        if (!movie) { LOG("RaceSex diag: no movie"); return; }

        std::string cat, race, raceDesc, sliderLabel, name;
        double sliderVal = -1.0;
        const int sex = (menu->GetRuntimeData().sex == RE::SEX::kFemale) ? 1 : 0;

        GetGFxString(movie, "_root.RaceSexMenuBaseInstance.CagetoryLockBaseInstance.CategoryInstance.List_mc.SelectedEntry.textField.text", cat);
        GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.PanelTwoNarrowInstance.List_mc.SelectedEntry.textField.text", race);
        GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.RaceDescriptionInstance.RaceTextInstance.text", raceDesc);
        GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.PanelTwoWideInstance.List_mc.SelectedEntry.textField.text", sliderLabel);
        GetGFxNumber(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.PanelTwoWideInstance.List_mc.SelectedEntry.SliderInstance.position", sliderVal);
        GetGFxString(movie, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.NameEntryInstance.TextInputInstance.text", name);

        LOG("RaceSex diag: sex={} cat='{}' race='{}' raceDesc='{}' sliderLabel='{}' sliderVal={} name='{}'",
            sex == 1 ? "Female" : "Male", cat, race, raceDesc, sliderLabel, (int)sliderVal, name);

        // Log des keycodes pour vérifier les mappings
        auto* cm = RE::ControlMap::GetSingleton();
        if (cm) {
            using D = RE::INPUT_DEVICE;
            using C = RE::UserEvents::INPUT_CONTEXT_ID;
            LOG("RaceSex keys: XButton(ItemMenu)={:#x} Accept(Menu)={:#x} Cancel(Menu)={:#x} PrevPage(ItemMenu)={:#x} NextPage(ItemMenu)={:#x}",
                cm->GetMappedKey("XButton",  D::kKeyboard, C::kItemMenu),
                cm->GetMappedKey("Accept",   D::kKeyboard, C::kMenuMode),
                cm->GetMappedKey("Cancel",   D::kKeyboard, C::kMenuMode),
                cm->GetMappedKey("PrevPage", D::kKeyboard, C::kItemMenu),
                cm->GetMappedKey("NextPage", D::kKeyboard, C::kItemMenu));
        }
    });
}

// VOCALISATION MENU RACESEX (CREATION PERSONNAGE) - FIN
