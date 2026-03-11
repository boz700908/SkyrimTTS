#pragma once

// ---------------- Main Menu UI read (verrouillage du chemin) ----------------

static constexpr const char* MAIN_BASES[] = {"_root.MenuHolder.Menu_mc", "_root.Menu_mc", "_root"};

static RE::GFxMovieView* GetMainMenuMovie(std::string& outMenuName) {
    outMenuName.clear();

    auto ui = RE::UI::GetSingleton();
    if (!ui) return nullptr;

    if (auto m = ui->GetMenu(RE::MainMenu::MENU_NAME); m) {
        outMenuName = RE::MainMenu::MENU_NAME;
        return m->uiMovie.get();
    }

    if (auto m = ui->GetMenu(RE::TitleSequenceMenu::MENU_NAME); m) {
        outMenuName = RE::TitleSequenceMenu::MENU_NAME;
        return m->uiMovie.get();
    }

    return nullptr;
}

static std::string g_mainLockedPath;
static std::string g_mainLockedMenuName;

static bool TryReadMainMenuAtPath(RE::GFxMovieView* movie, const std::string& path, std::wstring& outItem) {
    outItem.clear();
    std::string raw;
    if (!GetGFxString(movie, path.c_str(), raw) || raw.empty()) {
        return false;
    }

    std::wstring w = ResolveUIString(movie, raw);

    if (w.empty()) return false;

    outItem = w;
    static std::string lastRaw;
    if (raw != lastRaw) {
        LOG("MainMenu path hit: {} → raw='{}'", path, raw);
        lastRaw = raw;
    }
    return true;
}

static bool ReadMainMenuUI(std::wstring& outItem, std::string& outWhichPath) {
    outItem.clear();
    outWhichPath.clear();

    std::string menuName;
    RE::GFxMovieView* movie = GetMainMenuMovie(menuName);
    if (!movie) return false;

    // 1) locked path = stable
    if (!g_mainLockedPath.empty()) {
        std::wstring item;
        if (TryReadMainMenuAtPath(movie, g_mainLockedPath, item)) {
            outItem = item;
            outWhichPath = std::string("MENU=") + menuName + " | PATH=" + g_mainLockedPath;
            return true;
        }
        g_mainLockedPath.clear();
        g_mainLockedMenuName.clear();
    }

    // 2) initial scan
    const auto& bases = MAIN_BASES;

    const char* candidates[] = {".MainListHolder.MainList",
                                ".MainListHolder.MainList.List_mc",
                                ".MainListHolder.List_mc",
                                ".MainList",
                                ".MainList.List_mc",
                                ".List_mc",
                                ".MenuList",
                                ".menuList",
                                ".list_mc",
                                ".list",
                                ".List",
                                ".Container_mc.MainList",
                                ".Container_mc.List_mc"};

    const char* suffixDirect[] = {".selectedTextString", ".selectedEntry.label", ".selectedEntry.text",
                                  ".selectedEntry"};

    for (auto b : bases) {
        for (auto c : candidates) {
            std::string path = std::string(b) + c;

            std::wstring item;
            if (TryReadMainMenuAtPath(movie, path, item)) {
                g_mainLockedPath = path;
                g_mainLockedMenuName = menuName;
                outItem = item;
                outWhichPath = std::string("MENU=") + menuName + " | PATH=" + path;
                return true;
            }
        }
    }

    for (auto b : bases) {
        for (auto c : candidates) {
            for (auto s : suffixDirect) {
                std::string path = std::string(b) + c + s;

                std::wstring item;
                if (TryReadMainMenuAtPath(movie, path, item)) {
                    g_mainLockedPath = path;
                    g_mainLockedMenuName = menuName;
                    outItem = item;
                    outWhichPath = std::string("MENU=") + menuName + " | PATH=" + path;
                    return true;
                }
            }
        }
    }

    return false;
}

// ---------------- Save/Load panel & Confirm panel ----------------

static std::string ReadMainMenuState(RE::GFxMovieView* movie) {
    const auto& bases = MAIN_BASES;
    for (auto b : bases) {
        std::string path = std::string(b) + ".strCurrentState";
        std::string val;
        if (GetGFxString(movie, path.c_str(), val) && !val.empty())
            return val;
    }
    return "";
}

static std::wstring TryReadSaveLoadEntry(RE::GFxMovieView* movie, bool isCharacterSelection = false) {
    const auto& bases = MAIN_BASES;
    const char* listPaths[] = {".SaveLoadPanel_mc.List_mc", ".SaveLoadListHolder.List_mc"};

    for (auto b : bases) {
        for (auto lp : listPaths) {
            std::string eb = std::string(b) + lp + ".selectedEntry";

            // fileNum: primary differentiator between saves of the same character
            double fileNumD = 0.0;
            bool hasFileNum = GetGFxNumber(movie, (eb + ".fileNum").c_str(), fileNumD);

            // text: list display string (truncated to 20 chars by AS — same for all saves of one char)
            std::string textStr;
            GetGFxString(movie, (eb + ".text").c_str(), textStr);
            std::wstring textW = textStr.empty() ? L"" : ResolveUIString(movie, textStr);

            if (!hasFileNum && textW.empty()) continue;

            // Build "Save 042: " prefix from fileNum
            std::wstring prefix;
            if (hasFileNum) {
                std::wostringstream ss;
                ss << L"Save " << std::setw(3) << std::setfill(L'0') << static_cast<int>(fileNumD);
                prefix = ss.str() + L": ";
            }

            // corrupt / obsolete: full path reads (same mechanism as .text — traverses selectedEntry getter)
            auto readBool = [&](const std::string& path) -> bool {
                RE::GFxValue val;
                if (!movie->GetVariable(&val, path.c_str())) return false;
                if (val.IsBool())   return val.GetBool();
                if (val.IsNumber()) return val.GetNumber() != 0.0;
                return false;
            };
            if (readBool(eb + ".corrupt"))  { LOG("SaveLoad entry: corrupt");  return prefix + L"Corrupt save"; }
            if (readBool(eb + ".obsolete")) { LOG("SaveLoad entry: obsolete"); return prefix + L"Obsolete save"; }

            // name: full character name (preferred over truncated text)
            std::string nameStr;
            GetGFxString(movie, (eb + ".name").c_str(), nameStr);
            std::wstring charName = nameStr.empty() ? L"" : ResolveUIString(movie, nameStr);

            // In SaveLoad state: name not yet loaded → skip, wait for full data.
            // In CharacterSelection state: text IS the character name, name field is unused → announce.
            if (!isCharacterSelection && nameStr.empty())
                continue;

            if (charName.empty()) charName = textW;

            std::wstring msg = prefix + charName;

            // Include location from text field when it differs from character name.
            // Strip [X] save type prefix: "[M]"=Manual, "[Q]"=Quick, "[A]"=Auto, "[R]"=Autosave
            if (!textW.empty() && textW != charName) {
                std::wstring location = textW;
                if (location.size() >= 3 && location[0] == L'[' && location[2] == L']')
                    location = location.substr(3);
                if (!location.empty())
                    msg += L", " + location;
            }

            // Remaining confirmed fields from SaveLoadPanel.as::ShowScreenshot — all via full paths
            std::string raceStr;
            GetGFxString(movie, (eb + ".raceName").c_str(), raceStr);
            if (!raceStr.empty()) msg += L", " + ResolveUIString(movie, raceStr);

            double levelD = 0.0;
            if (GetGFxNumber(movie, (eb + ".level").c_str(), levelD) && levelD > 0.0)
                msg += L", level " + std::to_wstring(static_cast<int>(levelD));

            std::string playTimeStr;
            GetGFxString(movie, (eb + ".playTime").c_str(), playTimeStr);  // capital T — confirmed in AS
            if (!playTimeStr.empty()) msg += L", " + ResolveUIString(movie, playTimeStr);

            std::string dateStr;
            GetGFxString(movie, (eb + ".dateString").c_str(), dateStr);
            if (!dateStr.empty()) msg += L", " + ResolveUIString(movie, dateStr);

            static std::wstring lastLoggedEntry;
            if (msg != lastLoggedEntry) {
                LOG("SaveLoad entry: '{}'", WStringToUtf8(msg));
                lastLoggedEntry = msg;
            }
            return msg;
        }
    }
    return L"";
}

static std::wstring TryReadConfirmText(RE::GFxMovieView* movie, const std::string& state) {
    const auto& bases = MAIN_BASES;

    // MAIN_CONFIRM_STATE ("MainConfirm") uses ConfirmPanel_mc
    // SaveLoadConfirm / DeleteSaveConfirm / MarketplaceConfirm use SaveLoadConfirmText
    const bool isMainConfirm = (state.find("MainConfirm") != std::string::npos);

    const char* suffixes[][2] = {
        // primary path,                          nested textField fallback
        {".ConfirmPanel_mc.textField.text",        ".ConfirmPanel_mc.textField.textField.text"},
        {".SaveLoadConfirmText.textField.text",    ".SaveLoadConfirmText.textField.textField.text"},
    };
    int idx = isMainConfirm ? 0 : 1;

    for (auto b : bases) {
        for (int s = 0; s < 2; ++s) {
            std::string fullPath = std::string(b) + suffixes[idx][s];
            std::string raw;
            if (GetGFxString(movie, fullPath.c_str(), raw) && !raw.empty()) {
                std::wstring w = ResolveUIString(movie, raw);
                if (!w.empty()) {
                    // Cache the built string — only resolve $Accept/$Back when text changes
                    static std::string  lastRawConfirm;
                    static std::wstring lastBuiltConfirm;
                    if (raw != lastRawConfirm) {
                        std::wstring accept = ResolveUIString(movie, "$Accept");
                        std::wstring back   = ResolveUIString(movie, "$Back");
                        if (!accept.empty() || !back.empty()) {
                            w += L". ";
                            if (!accept.empty()) w += accept;
                            if (!accept.empty() && !back.empty()) w += L", ";
                            if (!back.empty()) w += back;
                        }
                        LOG("Confirm text hit: {} → '{}'", fullPath, raw);
                        lastRawConfirm  = raw;
                        lastBuiltConfirm = w;
                    } else {
                        w = lastBuiltConfirm;
                    }
                    return w;
                }
            }
        }
    }
    return L"";
}

// ---------------- State (menu principal) ----------------

static std::atomic_bool g_mainOpen{false};
static std::atomic_bool g_mainPendingUIRead{false};
static std::jthread g_mainPollThread;
static std::wstring g_lastMainItem;
static std::string  g_lastMainPathInfo;
static std::wstring g_lastSaveLoadItem;
static std::wstring g_lastConfirmText;

static void AnnounceMainMenuChange() {
    std::string menuName;
    RE::GFxMovieView* movie = GetMainMenuMovie(menuName);
    if (!movie) return;

    std::string state = ReadMainMenuState(movie);

    // 1. Confirm overlay (MainConfirm, SaveLoadConfirm, DeleteSaveConfirm, MarketplaceConfirm)
    if (!state.empty() && state.find("Confirm") != std::string::npos) {
        std::wstring text = TryReadConfirmText(movie, state);
        if (!text.empty() && text != g_lastConfirmText) {
            Speak(text);
            g_lastConfirmText = text;
        }
        g_lastSaveLoadItem.clear();
        return;
    }
    g_lastConfirmText.clear();

    // 2. Save/Load or Character selection panel
    if (!state.empty() &&
        (state.find("SaveLoad") != std::string::npos ||
         state.find("CharacterSelection") != std::string::npos ||
         state.find("CharacterLoad") != std::string::npos)) {
        bool isCharSel = state.find("CharacterSelection") != std::string::npos ||
                         state.find("CharacterLoad") != std::string::npos;
        std::wstring item = TryReadSaveLoadEntry(movie, isCharSel);
        if (!item.empty() && item != g_lastSaveLoadItem) {
            Speak(item);
            g_lastSaveLoadItem = item;
        }
        return;
    }
    g_lastSaveLoadItem.clear();

    // 3. Default: main list (existing behaviour)
    std::wstring item;
    std::string which;
    if (!ReadMainMenuUI(item, which)) return;
    g_lastMainPathInfo = which;
    if (!item.empty() && item != g_lastMainItem) {
        // Première lecture après ouverture du menu : s'enchaîne après "Plugin loaded" et "Main menu open"
        const bool firstRead = g_lastMainItem.empty();
        if (firstRead) SpeakQueue(item);
        else           Speak(item);
        g_lastMainItem = item;
    }
}

static void QueueMainMenuRead() {
    if (!g_mainOpen.load(std::memory_order_relaxed)) return;
    if (g_mainPendingUIRead.exchange(true)) return;

    auto* task = SKSE::GetTaskInterface();
    if (!task) {
        g_mainPendingUIRead.store(false);
        return;
    }

    task->AddUITask([]() {
        g_mainPendingUIRead.store(false);
        if (g_mainOpen.load()) {
            AnnounceMainMenuChange();
        }
    });
}

static void StartMainMenuPolling() {
    if (g_mainPollThread.joinable()) {
        g_mainPollThread.request_stop();
        g_mainPollThread.join();
    }
    g_mainPollThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_mainOpen.load(std::memory_order_relaxed)) {
                QueueMainMenuRead();
            }
        }
    });
}

static void StopMainMenuPolling() {
    if (g_mainPollThread.joinable()) {
        g_mainPollThread.request_stop();
        g_mainPollThread.join();
    }
}

static void DiagnoseMainMenuNow() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        std::string menuName;
        RE::GFxMovieView* movie = GetMainMenuMovie(menuName);

        if (!movie) {
            Speak(L"Main menu not found");
            return;
        }

        std::wstring item;
        std::string which;
        if (!ReadMainMenuUI(item, which)) {
            Speak(L"Path not found");
            return;
        }

        Speak(L"Main menu OK");
        SpeakQueue(L"Selection");
        SpeakQueue(item);

        if (!which.empty()) {
            SpeakQueue(L"Path OK");
        }
    });
}
