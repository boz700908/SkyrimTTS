#pragma once

// VOCALISATION MENU DORMIR/ATTENDRE - DEBUT

// GFx paths (from SleepWaitMenu.as):
// Question : SleepWaitMenu_mc.QuestionInstance.text  ("Rest how long?" / "Wait how long?")
// Heures :   SleepWaitMenu_mc.HoursText.text
// Heure :    SleepWaitMenu_mc.CurrentTime.text

static std::atomic_bool g_sleepWaitOpen{false};
static std::string g_lastSleepWaitHours;
static std::string g_lastSleepWaitTime;

static constexpr const char* SW_QUESTION = "_root.SleepWaitMenu_mc.QuestionInstance.text";
static constexpr const char* SW_HOURS    = "_root.SleepWaitMenu_mc.HoursText.text";
static constexpr const char* SW_TIME     = "_root.SleepWaitMenu_mc.CurrentTime.text";

// Hook AdvanceMovie pour suivre les changements de slider
using SleepWaitAdvanceMovie_t = void(RE::IMenu*, float, std::uint32_t);
static SleepWaitAdvanceMovie_t* g_origSleepWaitAdvanceMovie = nullptr;

static void SleepWaitAdvanceMovie_Hook(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime) {
    if (g_origSleepWaitAdvanceMovie) g_origSleepWaitAdvanceMovie(a_this, a_interval, a_currentTime);

    if (!g_sleepWaitOpen.load(std::memory_order_relaxed)) return;

    // Throttle : toutes les ~5 frames (~83ms) pour être réactif au slider
    static int s_frameSkip = 0;
    if (++s_frameSkip < 5) return;
    s_frameSkip = 0;

    RE::GFxMovieView* movie = a_this->uiMovie.get();
    if (!movie) return;

    // Lire les heures sélectionnées (change avec le slider)
    std::string hours;
    if (GetGFxString(movie, SW_HOURS, hours) && !hours.empty() && hours != g_lastSleepWaitHours) {
        g_lastSleepWaitHours = hours;
        std::wstring whours = Utf8ToWString(hours);
        Speak(whours + L" hours");
    }

    // Lire l'heure actuelle (change pendant que le temps passe)
    std::string time;
    if (GetGFxString(movie, SW_TIME, time) && !time.empty() && time != g_lastSleepWaitTime) {
        bool firstRead = g_lastSleepWaitTime.empty();
        g_lastSleepWaitTime = time;
        if (!firstRead) {
            SpeakQueue(Utf8ToWString(time));
        }
    }
}

static void AnnounceSleepWaitOpen(RE::GFxMovieView* movie) {
    std::string question;
    if (GetGFxString(movie, SW_QUESTION, question) && !question.empty()) {
        std::wstring wq = StripMarkupForSpeech(ResolveUIString(movie, question));
        Speak(wq);
    }

    // Annoncer l'heure actuelle
    std::string time;
    if (GetGFxString(movie, SW_TIME, time) && !time.empty()) {
        g_lastSleepWaitTime = time;
        SpeakQueue(Utf8ToWString(time));
    }

    // Annoncer les heures sélectionnées
    std::string hours;
    if (GetGFxString(movie, SW_HOURS, hours) && !hours.empty()) {
        g_lastSleepWaitHours = hours;
        SpeakQueue(Utf8ToWString(hours) + L" hours");
    }
}

static bool InstallSleepWaitAdvanceMovieHook() {
    REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_SleepWaitMenu[0]};
    g_origSleepWaitAdvanceMovie = reinterpret_cast<SleepWaitAdvanceMovie_t*>(vtbl.write_vfunc(0x05, &SleepWaitAdvanceMovie_Hook));
    LOG("SleepWait Menu AdvanceMovie hook installed");
    return true;
}

// VOCALISATION MENU DORMIR/ATTENDRE - FIN
