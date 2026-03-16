#pragma once

// VOCALISATION HUD (crosshair, notifications, sous-titres) - DEBUT

// --- Crosshair : événement SKSE + lecture GFx pour le texte d'action complet ---
static std::atomic<RE::FormID> g_lastCrosshairFormID{0};
static std::wstring             g_lastCrosshairName;
static constexpr const char* HUD_CROSSHAIR = "_root.HUDMovieBaseInstance.RolloverNameInstance.text";

class CrosshairListener : public RE::BSTEventSink<SKSE::CrosshairRefEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* e,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override {
        if (!e) return RE::BSEventNotifyControl::kContinue;

        RE::FormID newID = e->crosshairRef ? e->crosshairRef->GetFormID() : 0;
        RE::FormID oldID = g_lastCrosshairFormID.exchange(newID);

        if (newID == 0) {
            g_lastCrosshairName.clear();
            return RE::BSEventNotifyControl::kContinue;
        }

        if (newID == oldID) return RE::BSEventNotifyControl::kContinue;

        // Nouvel objet sous le crosshair → lire le texte d'action depuis GFx (ex: "Voler Tonneau")
        auto* task = SKSE::GetTaskInterface();
        if (!task) return RE::BSEventNotifyControl::kContinue;
        task->AddUITask([]() {
            auto ui = RE::UI::GetSingleton();
            if (!ui) return;
            auto menu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
            if (!menu) return;
            RE::GFxMovieView* movie = menu->uiMovie.get();
            if (!movie) return;

            std::string text;
            if (GetGFxString(movie, HUD_CROSSHAIR, text) && !text.empty() && text != " ") {
                std::wstring wtext = StripMarkupForSpeech(Utf8ToWString(text));
                if (wtext != g_lastCrosshairName) {
                    g_lastCrosshairName = wtext;
                    Speak(wtext);
                }
            }
        });

        return RE::BSEventNotifyControl::kContinue;
    }
};

static CrosshairListener g_crosshairListener;

// --- HUD AdvanceMovie hook (notifications, sous-titres, lieu, tutoriel) ---
static std::string g_hudPrevNotif;
static std::string g_hudPrevSubtitle;
static std::string g_hudPrevLocation;
static std::string g_hudPrevTutorial;

// Notifications : QuestName stocké sur AnimatedLetter_mc avant animation lettre par lettre
static constexpr const char* HUD_NOTIF     = "_root.HUDMovieBaseInstance.QuestUpdateBaseInstance.AnimatedLetter_mc.QuestName";
// Sous-titres de dialogue (SubtitleText = SubtitleTextHolder.textField, ligne 146)
static constexpr const char* HUD_SUBTITLE  = "_root.HUDMovieBaseInstance.SubtitleTextHolder.textField.text";
// Nom de lieu (SetLocationName, HUDMenu.as ligne 375)
static constexpr const char* HUD_LOCATION  = "_root.HUDMovieBaseInstance.LocationLockBase.LocationNameBase.LocationTextBase.LocationTextInstance.text";
// Tutoriel (ShowTutorialHintText, HUDMenu.as ligne 147)
static constexpr const char* HUD_TUTORIAL      = "_root.HUDMovieBaseInstance.TutorialLockInstance.TutorialHintsInstance.FadeHolder.TutorialHintsTextInstance.text";
static constexpr const char* HUD_TUTORIAL_HTML = "_root.HUDMovieBaseInstance.TutorialLockInstance.TutorialHintsInstance.FadeHolder.TutorialHintsTextInstance.htmlText";

// Original AdvanceMovie function pointer (saved before hook)
using AdvanceMovie_t = void(RE::IMenu*, float, std::uint32_t);
static AdvanceMovie_t* g_origHUDAdvanceMovie = nullptr;

static void HUDAdvanceMovie_Hook(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime) {
    // Appeler l'original d'abord (mise à jour normale du HUD)
    if (g_origHUDAdvanceMovie) g_origHUDAdvanceMovie(a_this, a_interval, a_currentTime);

    // Throttle : vérifier toutes les ~3 frames (~50ms à 60fps)
    static int s_frameSkip = 0;
    if (++s_frameSkip < 3) return;
    s_frameSkip = 0;

    RE::GFxMovieView* movie = a_this->uiMovie.get();
    if (!movie) return;

    // Notifications HUD (compétence augmentée, quête mise à jour, niveau HUD)
    std::string notif;
    if (GetGFxString(movie, HUD_NOTIF, notif)) {
        if (!notif.empty() && notif != g_hudPrevNotif) {
            LOG("HUD notif: '{}'", notif);
            g_hudPrevNotif = notif;
            Speak(StripMarkupForSpeech(Utf8ToWString(notif)));
        }
    }

    // Sous-titres de dialogue
    std::string subtitle;
    if (GetGFxString(movie, HUD_SUBTITLE, subtitle)) {
        if (subtitle.empty() || subtitle == " ") {
            g_hudPrevSubtitle.clear();
        } else if (subtitle != g_hudPrevSubtitle) {
            g_hudPrevSubtitle = subtitle;
            Speak(StripMarkupForSpeech(Utf8ToWString(subtitle)));
        }
    }

    // Nom de lieu quand on entre dans une nouvelle zone
    std::string location;
    if (GetGFxString(movie, HUD_LOCATION, location) && !location.empty() && location != g_hudPrevLocation) {
        g_hudPrevLocation = location;
        Speak(StripMarkupForSpeech(Utf8ToWString(location)));
    }

    // Tutoriel (hints de début de jeu / nouveaux joueurs)
    // Try htmlText first to capture <img src='KeyName.png'> keybind icons
    std::string tutorial;
    bool tutoFromHtml = false;
    std::string htmlRaw;
    if (GetGFxString(movie, HUD_TUTORIAL_HTML, htmlRaw) && !htmlRaw.empty()) {
        if (htmlRaw.find("<img") != std::string::npos || htmlRaw.find("<IMG") != std::string::npos) {
            tutorial = htmlRaw;
            tutoFromHtml = true;
        } else {
            // htmlText exists but no <img> — log it to see what's there
            static std::string lastLoggedHtml;
            if (htmlRaw != lastLoggedHtml) {
                LOG("HUD tutorial htmlText (no img)='{}'", htmlRaw);
                lastLoggedHtml = htmlRaw;
            }
        }
    }
    if (!tutoFromHtml) {
        tutorial.clear();
        GetGFxString(movie, HUD_TUTORIAL, tutorial);
    }
    if (!tutorial.empty() && tutorial != g_hudPrevTutorial) {
        g_hudPrevTutorial = tutorial;
        std::wstring tutoW = Utf8ToWString(tutorial);
        if (tutoFromHtml)
            tutoW = ReplaceImgTagsWithKeyNames(tutoW);
        tutoW = StripMarkupForSpeech(tutoW);
        LOG("HUD tutorial speech='{}' (fromHtml={})", WStringToUtf8(tutoW), tutoFromHtml);
        if (!tutoW.empty())
            Speak(tutoW);
    }
}

static bool InstallHUDAdvanceMovieHook() {
    REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_HUDMenu[0]};
    g_origHUDAdvanceMovie = reinterpret_cast<AdvanceMovie_t*>(vtbl.write_vfunc(0x05, &HUDAdvanceMovie_Hook));
    LOG("HUD AdvanceMovie hook installed");
    return true;
}

static void RegisterCrosshairListener() {
    auto* source = SKSE::GetCrosshairRefEventSource();
    if (source) {
        source->AddEventSink(&g_crosshairListener);
        LOG("CrosshairRefEvent listener registered");
    } else {
        LOG("WARNING: CrosshairRefEvent source not available");
    }
}

// H en jeu : annonce santé, magicka, vigueur
static void AnnouncePlayerVitals() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        auto* av = player->AsActorValueOwner();
        if (!av) return;

        int curH = static_cast<int>(av->GetActorValue(RE::ActorValue::kHealth));
        int maxH = static_cast<int>(av->GetPermanentActorValue(RE::ActorValue::kHealth));
        int curM = static_cast<int>(av->GetActorValue(RE::ActorValue::kMagicka));
        int maxM = static_cast<int>(av->GetPermanentActorValue(RE::ActorValue::kMagicka));
        int curS = static_cast<int>(av->GetActorValue(RE::ActorValue::kStamina));
        int maxS = static_cast<int>(av->GetPermanentActorValue(RE::ActorValue::kStamina));

        std::wstring msg = std::to_wstring(curH) + L" / " + std::to_wstring(maxH) + L" health";
        msg += L", " + std::to_wstring(curM) + L" / " + std::to_wstring(maxM) + L" magicka";
        msg += L", " + std::to_wstring(curS) + L" / " + std::to_wstring(maxS) + L" stamina";
        Speak(msg);
    });
}

// VOCALISATION HUD - FIN
