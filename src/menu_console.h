#pragma once

// VOCALISATION CONSOLE DE COMMANDE - DEBUT

static std::atomic_bool g_consoleOpen{false};
static std::string g_lastConsoleMessage;
static std::string g_lastConsoleEntry;

using ConsoleAdvanceMovie_t = void(RE::IMenu*, float, std::uint32_t);
static ConsoleAdvanceMovie_t* g_origConsoleAdvanceMovie = nullptr;

// Lire un champ texte GFx via GetMember (GetVariable ne marche pas pour .text sur TextField)
static bool GetTextFieldText(RE::GFxMovieView* movie, const char* instancePath, const char* fieldName, std::string& out) {
    RE::GFxValue instance;
    if (!SafeGetVariable(movie, instance, instancePath)) return false;
    if (!instance.IsObject() && !instance.IsDisplayObject()) return false;

    RE::GFxValue field;
    if (!instance.GetMember(fieldName, &field)) return false;
    if (!field.IsObject() && !field.IsDisplayObject()) return false;

    RE::GFxValue textVal;
    if (!field.GetMember("text", &textVal)) return false;
    if (!textVal.IsString()) return false;

    out = textVal.GetString();
    return true;
}

static void ConsoleAdvanceMovie_Hook(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime) {
    if (g_origConsoleAdvanceMovie) g_origConsoleAdvanceMovie(a_this, a_interval, a_currentTime);

    if (!g_consoleOpen.load(std::memory_order_relaxed)) return;

    // Throttle : toutes les ~3 frames
    static int s_frameSkip = 0;
    if (++s_frameSkip < 3) return;
    s_frameSkip = 0;

    // --- Résultat des commandes via ConsoleLog (le plus fiable) ---
    auto* consoleLog = RE::ConsoleLog::GetSingleton();
    if (consoleLog) {
        std::string lastMsg(consoleLog->lastMessage);
        if (!lastMsg.empty() && lastMsg != g_lastConsoleMessage) {
            g_lastConsoleMessage = lastMsg;
            SpeakQueue(Utf8ToWString(lastMsg));
            LOG("Console: output='{}'", lastMsg);
        }
    }

    // --- Texte tapé (CommandEntry) via GetMember ---
    RE::GFxMovieView* movie = a_this->uiMovie.get();
    if (!movie) return;

    // Essayer de lire CommandEntry.text via les différents chemins d'instance
    std::string entry;
    static const char* instancePaths[] = {
        "_root.instance1",
        "_root.Console.ConsoleInstance",
        "_root.ConsoleMovie",
    };
    static int s_foundPath = -1;

    if (s_foundPath >= 0) {
        GetTextFieldText(movie, instancePaths[s_foundPath], "CommandEntry", entry);
    } else {
        for (int i = 0; i < 3; i++) {
            if (GetTextFieldText(movie, instancePaths[i], "CommandEntry", entry)) {
                s_foundPath = i;
                LOG("Console: found instance path '{}'", instancePaths[i]);
                break;
            }
        }
    }

    if (entry != g_lastConsoleEntry) {
        if (entry.empty() && !g_lastConsoleEntry.empty()) {
            // Commande envoyée (entry vidée) — ne rien dire
        } else if (entry.size() > g_lastConsoleEntry.size() &&
                   entry.substr(0, g_lastConsoleEntry.size()) == g_lastConsoleEntry) {
            // Nouveau caractère ajouté
            std::string newChars = entry.substr(g_lastConsoleEntry.size());
            Speak(Utf8ToWString(newChars));
        } else if (!entry.empty()) {
            // Texte complètement changé (flèche haut/bas = commande précédente)
            Speak(Utf8ToWString(entry));
        }
        g_lastConsoleEntry = entry;
    }
}

static bool InstallConsoleAdvanceMovieHook() {
    REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_Console[0]};
    g_origConsoleAdvanceMovie = reinterpret_cast<ConsoleAdvanceMovie_t*>(vtbl.write_vfunc(0x05, &ConsoleAdvanceMovie_Hook));
    LOG("Console AdvanceMovie hook installed");
    return true;
}

// VOCALISATION CONSOLE DE COMMANDE - FIN
