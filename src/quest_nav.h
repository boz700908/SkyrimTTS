#pragma once

// QUEST AUDIO NAVIGATION
// Inspiré du système de Diokiri (clairvoyance) + LethalAccess (sons directionnels).
// Toutes les 0.5 sec, calcule un chemin A* sur le navmesh vers la quête active,
// puis joue un son selon la direction du PROCHAIN waypoint à atteindre.
// Comme la clairvoyance vanilla, ça contourne les murs et passe par les portes.
//
// IMPORTANT : ce header DOIT être inclus APRÈS scanner.h, autowalk.h et pathfinding.h
// car il utilise GetActiveQuestNavTarget() et PF_AStarPath().

#include "common.h"

// ---------------- État du système ----------------
static std::atomic_bool g_questNavEnabled{false};
static std::jthread     g_questNavThread;

// Constantes (mêmes valeurs que LethalAccess)
static constexpr float QUEST_NAV_PING_INTERVAL_MS = 500.0f;  // 0.5 sec entre chaque ping
static constexpr float QUEST_NAV_DOT_THRESHOLD    = 0.3f;    // ±73° devant/derrière
static constexpr float QUEST_NAV_ARRIVAL_DIST     = 200.0f;  // ~200 unités = environ 3m

// État pour éviter de réannoncer "arrivé" en boucle
static std::atomic_bool g_questNavArrived{false};

// FormIDs des sons SOUN dans SkyrimTTS_AutoWalk.esp (générés par SkyrimCK-MCP)
static constexpr RE::FormID g_soundNavOnTargetID     = 0x80E;
static constexpr RE::FormID g_soundNavAlmostTargetID = 0x80F;
static constexpr RE::FormID g_soundNavOffTargetID    = 0x810;
static constexpr RE::FormID g_soundNavReachedID      = 0x811;

// ---------------- Calcul du facing du joueur ----------------
// Skyrim utilise data.angle.z pour le yaw (rotation horizontale)
// 0 = nord, π/2 = est, π = sud, 3π/2 = ouest
static RE::NiPoint3 GetPlayerForwardHorizontal(RE::PlayerCharacter* player) {
    float yaw = player->data.angle.z;
    return RE::NiPoint3(std::sin(yaw), std::cos(yaw), 0.0f);
}

// ---------------- Le ping principal (DOIT être appelé depuis le thread UI du jeu) ----------------
static void QuestNavPing() {
    static int s_debugTick = 0;
    s_debugTick++;
    bool logThisTick = (s_debugTick % 4 == 0);  // log toutes les 2 secondes (4 * 0.5s)

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        if (logThisTick) LOG("QuestNav: skip - no player");
        return;
    }

    // Si un menu est ouvert, on ne fait rien (pause auto)
    auto* ui = RE::UI::GetSingleton();
    if (ui && (ui->GameIsPaused() ||
               ui->IsMenuOpen(RE::MapMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::JournalMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::MagicMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::MainMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::TweenMenu::MENU_NAME))) {
        if (logThisTick) LOG("QuestNav: skip - menu open");
        return;
    }

    // Récupérer la cible de quête active (peut échouer si la quête est très loin)
    RE::NiPoint3 questPos{0, 0, 0};
    std::wstring questName = L"Quest";
    bool haveQuestPos = GetActiveQuestNavTarget(player, questPos, questName);

    auto playerPos = player->GetPosition();
    float distToFinal = haveQuestPos ? (questPos - playerPos).Length() : 999999.0f;

    // Vérifier si on est arrivé à destination finale (seulement si on a une vraie position)
    if (haveQuestPos && distToFinal <= QUEST_NAV_ARRIVAL_DIST) {
        if (!g_questNavArrived.exchange(true)) {
            LOG("QuestNav: arrived at '{}' dist={:.0f}", WStringToUtf8(questName), distToFinal);
            PlaySoundOneShot(g_soundNavReachedID, 1.0f);
        }
        return;
    }
    g_questNavArrived.store(false);

    // === Détermination de la direction vers la quête (3 niveaux de fallback) ===
    // Niveau 1 : pathfinding A* sur le navmesh → next waypoint (le plus précis)
    // Niveau 2 : compass heading depuis le HUD → direction authentique de Skyrim
    // Niveau 3 : ligne droite vers la position résolue (fallback ultime)
    float dirX = 0.0f, dirY = 0.0f;
    bool haveDirection = false;
    const char* dirSource = "none";
    size_t pathSize = 0;

    // Niveau 1 : Pathfinding A* (uniquement si on a une vraie position cible)
    auto meshes = haveQuestPos ? PF_GetLoadedNavmeshes() : std::vector<RE::NavMesh*>{};
    if (haveQuestPos && !meshes.empty()) {
        auto path = PF_AStarPath(playerPos, questPos, meshes);
        pathSize = path.size();
        if (!path.empty()) {
            // Trouver le prochain waypoint pas encore atteint
            RE::NiPoint3 nextWP = path.back().position;
            for (auto& wp : path) {
                float d = (wp.position - playerPos).Length();
                if (d > QUEST_NAV_ARRIVAL_DIST) { nextWP = wp.position; break; }
            }
            float dx = nextWP.x - playerPos.x;
            float dy = nextWP.y - playerPos.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len > 1.0f) {
                dirX = dx / len;
                dirY = dy / len;
                haveDirection = true;
                dirSource = "navmesh";
            }
        }
    }

    // Niveau 2 : Boussole HUD (la direction authentique de Skyrim)
    if (!haveDirection) {
        float compassHeading = -1.0f;
        if (ReadQuestCompassHeading(compassHeading) && compassHeading >= 0) {
            float rad = compassHeading * 3.14159265f / 180.0f;
            dirX = std::sin(rad);
            dirY = std::cos(rad);
            haveDirection = true;
            dirSource = "compass";
        }
    }

    // Niveau 3 : ligne droite vers la cible résolue (uniquement si on a une position)
    if (!haveDirection && haveQuestPos) {
        float dx = questPos.x - playerPos.x;
        float dy = questPos.y - playerPos.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 1.0f) {
            dirX = dx / len;
            dirY = dy / len;
            haveDirection = true;
            dirSource = "straight";
        }
    }

    if (!haveDirection) {
        if (logThisTick) LOG("QuestNav: no direction available");
        return;
    }

    // Calcul du dot product entre le facing du joueur et la direction de la quête
    RE::NiPoint3 forward = GetPlayerForwardHorizontal(player);
    float dot = forward.x * dirX + forward.y * dirY;
    float distToWP = distToFinal;  // pour le log

    // Choisir le son selon la zone
    RE::FormID soundID;
    const char* tag;
    if (dot > QUEST_NAV_DOT_THRESHOLD) {
        soundID = g_soundNavOnTargetID;
        tag = "ON";
    } else if (dot < -QUEST_NAV_DOT_THRESHOLD) {
        soundID = g_soundNavOffTargetID;
        tag = "OFF";
    } else {
        soundID = g_soundNavAlmostTargetID;
        tag = "ALMOST";
    }

    LOG("QuestNav: ping {} dot={:.2f} dist={:.0f} src={} pathSize={} target='{}'",
        tag, dot, distToFinal, dirSource, pathSize, WStringToUtf8(questName));
    PlaySoundOneShot(soundID, 1.0f);
}

// ---------------- Thread de polling ----------------
// Le thread se contente de tick et d'envoyer un ping sur le thread UI du jeu.
// Tout l'accès aux structures du jeu (player, quêtes...) est fait sur le thread UI.
static void StartQuestNavPolling() {
    if (g_questNavThread.joinable()) return;  // déjà actif
    g_questNavEnabled.store(true);
    g_questNavArrived.store(false);
    g_questNavThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(QUEST_NAV_PING_INTERVAL_MS)));
            if (st.stop_requested()) break;
            // Toujours exécuter le ping sur le thread UI pour la sécurité thread
            auto* task = SKSE::GetTaskInterface();
            if (task) task->AddTask([]() { QuestNavPing(); });
        }
        LOG("QuestNav: polling thread ended");
    });
    LOG("QuestNav: polling thread started");
}

static void StopQuestNavPolling() {
    g_questNavEnabled.store(false);
    if (g_questNavThread.joinable()) {
        g_questNavThread.request_stop();
        g_questNavThread.join();
    }
}

// ---------------- Toggle (appelé depuis l'InputListener) ----------------
static void ToggleQuestNav() {
    if (g_questNavEnabled.load()) {
        StopQuestNavPolling();
        Speak(L"Quest navigation off");
        LOG("QuestNav: disabled");
    } else {
        StartQuestNavPolling();
        Speak(L"Quest navigation on");
        LOG("QuestNav: enabled");
    }
}
