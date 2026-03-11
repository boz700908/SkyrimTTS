#pragma once

// VOCALISATION STATS MENU (compétences / perks) - DEBUT

static std::atomic_bool g_statsOpen{false};
static std::atomic_bool g_statsPendingUIRead{false};
static std::jthread     g_statsPollThread;
static std::string      g_statsPrevKey;

static constexpr const char* STATS_ROOT      = "_root.StatsMenuBaseInstance";
static constexpr const char* STATS_CARD_DESC = "_root.StatsMenuBaseInstance.DescriptionCardInstance.CardDescriptionTextInstance.text";
static constexpr const char* STATS_CARD_REQ  = "_root.StatsMenuBaseInstance.DescriptionCardInstance.SkillRequirementText.text";
static constexpr const char* STATS_LEVEL     = "_root.StatsMenuBaseInstance.TopPlayerInfo.LevelNumberLabel.text";
static constexpr const char* STATS_PERKS     = "_root.StatsMenuBaseInstance.AddPerkTextInstance.AddPerkTextField.text";

static constexpr int STATS_SKILL_COUNT = 18;
static constexpr const char* STATS_RING_BASE = "_root.StatsMenuBaseInstance.AnimatingSkillTextInstance.SkillText";

// Cache des noms de compétences lus depuis l'anneau GFx
static std::string g_statsRingNames[STATS_SKILL_COUNT];
static bool g_statsRingCached = false;

// Lit et cache les 18 noms de compétences de l'anneau
static void CacheStatsRingNames(RE::GFxMovieView* movie) {
    if (g_statsRingCached) return;
    for (int i = 0; i < STATS_SKILL_COUNT; ++i) {
        std::string path = std::string(STATS_RING_BASE) + std::to_string(i) + ".LabelInstance.text";
        GetGFxString(movie, path.c_str(), g_statsRingNames[i]);
    }
    g_statsRingCached = true;
}

// Trouve la compétence sélectionnée via _xscale (> 100 = centrée)
static std::string GetSelectedSkillName(RE::GFxMovieView* movie) {
    CacheStatsRingNames(movie);
    double bestScale = 0;
    int bestIdx = -1;
    for (int i = 0; i < STATS_SKILL_COUNT; ++i) {
        std::string path = std::string(STATS_RING_BASE) + std::to_string(i) + "._xscale";
        double scale = 0;
        if (GetGFxNumber(movie, path.c_str(), scale) && scale > bestScale) {
            bestScale = scale;
            bestIdx = i;
        }
    }
    if (bestIdx >= 0 && bestScale > 100.0) {
        return g_statsRingNames[bestIdx];
    }
    return {};
}

// Annonce à l'ouverture : "Skills, Level N, N perks"
static void AnnounceStatsOpenImpl() {
    if (g_levelUpOpen.load()) return;
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::StatsMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    std::string tmp;
    std::wstring announce = L"Skills";

    if (GetGFxString(movie, STATS_LEVEL, tmp) && !tmp.empty())
        announce += L", level " + Utf8ToWString(tmp);

    if (GetGFxString(movie, STATS_PERKS, tmp) && !tmp.empty())
        announce += L", " + ResolveUIString(movie, tmp);

    Speak(announce);
}

// Annonce la sélection courante si elle a changé
static void AnnounceStatsSelectionImpl() {
    if (g_levelUpOpen.load()) return;
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::StatsMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    // Lire la compétence sélectionnée depuis l'anneau (via _xscale)
    std::string skillName = GetSelectedSkillName(movie);

    std::string cardDesc;
    GetGFxString(movie, STATS_CARD_DESC, cardDesc);

    std::string key = skillName + "|" + cardDesc;
    if (key == g_statsPrevKey || key == "|") return;
    g_statsPrevKey = key;

    LOG("Stats selection: skill='{}', cardDesc='{}'", skillName, cardDesc);

    std::wstring announce;

    // Nom de la compétence sélectionnée (ex: "FORGEAGE 60")
    if (!skillName.empty()) {
        announce += StripMarkupForSpeech(Utf8ToWString(skillName));
    }

    // Prérequis (en mode perk : "FORGE          REQUIRES: ...")
    std::string req;
    if (GetGFxString(movie, STATS_CARD_REQ, req) && !req.empty()) {
        std::wstring wreq = StripMarkupForSpeech(ResolveUIString(movie, req));
        if (!wreq.empty()) {
            if (!announce.empty()) announce += L", ";
            announce += wreq;
        }
    }

    // Description de la compétence ou du perk
    if (!cardDesc.empty()) {
        std::wstring wdesc = StripMarkupForSpeech(ResolveUIString(movie, cardDesc));
        if (!wdesc.empty()) {
            if (!announce.empty()) announce += L". ";
            announce += wdesc;
        }
    }

    if (!announce.empty()) Speak(announce);
}

static void QueueStatsRead() {
    if (g_statsPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        g_statsPendingUIRead.store(false);
        if (g_statsOpen.load()) AnnounceStatsSelectionImpl();
    });
}

static void QueueStatsOpen() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        if (g_statsOpen.load()) AnnounceStatsOpenImpl();
    });
}

static void StopStatsPoll();

static void StartStatsPoll() {
    StopStatsPoll();
    g_statsPrevKey.clear();
    g_statsRingCached = false;
    g_statsPollThread = std::jthread([](std::stop_token st) {
        // Attend que le jeu ait initialisé le menu (SetPerkCount, etc.)
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        if (!st.stop_requested() && g_statsOpen.load()) {
            auto* task = SKSE::GetTaskInterface();
            if (task) task->AddUITask([]() { if (g_statsOpen.load()) AnnounceStatsOpenImpl(); });
        }
        // Délai supplémentaire pour que l'annonce d'ouverture se termine
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_statsOpen.load(std::memory_order_relaxed))
                QueueStatsRead();
        }
    });
}

static void StopStatsPoll() {
    if (g_statsPollThread.joinable()) {
        g_statsPollThread.request_stop();
        g_statsPollThread.join();
    }
}

// VOCALISATION STATS MENU - FIN
