#pragma once

// VOCALISATION MENU DE CHARGEMENT - DEBUT

// GFx paths (from LoadingMenu.as):
// Texte de chargement : Menu_mc.LoadingTextFader.LoadingText.textField.text
// Niveau : Menu_mc.LevelMeterRect.LevelNumberLabel.text

static std::atomic_bool g_loadingOpen{false};
static std::string g_lastLoadingText;

static constexpr const char* LOADING_TEXT = "_root.Menu_mc.LoadingTextFader.LoadingText.textField.text";
static constexpr const char* LOADING_LEVEL = "_root.Menu_mc.LevelMeterRect.LevelNumberLabel.text";

// Hook AdvanceMovie pour lire le texte de chargement quand il change
using LoadingAdvanceMovie_t = void(RE::IMenu*, float, std::uint32_t);
static LoadingAdvanceMovie_t* g_origLoadingAdvanceMovie = nullptr;

static void LoadingAdvanceMovie_Hook(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime) {
    if (g_origLoadingAdvanceMovie) g_origLoadingAdvanceMovie(a_this, a_interval, a_currentTime);

    if (!g_loadingOpen.load(std::memory_order_relaxed)) return;

    // Throttle : pas besoin de checker chaque frame, toutes les ~10 frames (~166ms)
    static int s_frameSkip = 0;
    if (++s_frameSkip < 10) return;
    s_frameSkip = 0;

    RE::GFxMovieView* movie = a_this->uiMovie.get();
    if (!movie) return;

    std::string text;
    if (GetGFxString(movie, LOADING_TEXT, text) && !text.empty() && text != " " && text != g_lastLoadingText) {
        g_lastLoadingText = text;
        Speak(StripMarkupForSpeech(Utf8ToWString(text)));
    }
}

static bool InstallLoadingAdvanceMovieHook() {
    REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_LoadingMenu[0]};
    g_origLoadingAdvanceMovie = reinterpret_cast<LoadingAdvanceMovie_t*>(vtbl.write_vfunc(0x05, &LoadingAdvanceMovie_Hook));
    LOG("Loading Menu AdvanceMovie hook installed");
    return true;
}

// VOCALISATION MENU DE CHARGEMENT - FIN
