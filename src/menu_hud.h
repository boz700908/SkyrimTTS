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
static std::string g_hudPrevLocation;
static std::string g_hudPrevTutorial;
static std::string g_hudPrevMessage;   // dernier message d'item (Gold added, etc.)
static std::set<std::string> g_hudReadObjectives; // objectifs de quête déjà lus
static std::string g_hudPrevArrowCount;  // dernier compteur de flèches
static std::string g_hudPrevStealth;     // dernier statut furtivité
static bool g_wasSneaking = false;       // état accroupi précédent
static bool g_wasFirstPerson = true;     // état caméra précédent
static bool g_cameraInitialized = false; // éviter annonce au lancement

// Notifications : QuestName stocké sur AnimatedLetter_mc avant animation lettre par lettre
static constexpr const char* HUD_NOTIF     = "_root.HUDMovieBaseInstance.QuestUpdateBaseInstance.AnimatedLetter_mc.QuestName";
// Sous-titres de dialogue (SubtitleText = SubtitleTextHolder.textField, ligne 146)
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
    // On reset g_hudPrevNotif quand le texte disparaît, pour relire si ça réapparaît identique
    std::string notif;
    if (GetGFxString(movie, HUD_NOTIF, notif)) {
        if (!notif.empty() && notif != g_hudPrevNotif) {
            LOG("HUD notif: '{}'", notif);
            g_hudPrevNotif = notif;
            Speak(StripMarkupForSpeech(Utf8ToWString(notif)));
        }
    } else if (!g_hudPrevNotif.empty()) {
        g_hudPrevNotif.clear();
    }

    // Objectifs de quête (texte sous la notification "Quest updated")
    {
        bool anyObjectiveVisible = false;
        for (int i = 0; i < 3; i++) {
            std::string path = "_root.HUDMovieBaseInstance.QuestUpdateBaseInstance.objective"
                + std::to_string(i) + ".ObjectiveTextFieldInstance.TextFieldInstance.text";
            std::string objText;
            if (GetGFxString(movie, path.c_str(), objText) && !objText.empty()) {
                anyObjectiveVisible = true;
                if (g_hudReadObjectives.find(objText) == g_hudReadObjectives.end()) {
                    g_hudReadObjectives.insert(objText);
                    LOG("HUD objective: '{}'", objText);
                    SpeakQueue(StripMarkupForSpeech(Utf8ToWString(objText)));
                }
            }
        }
        // Quand tous les objectifs disparaissent, on reset le set pour pouvoir relire
        if (!anyObjectiveVisible && !g_hudReadObjectives.empty()) {
            g_hudReadObjectives.clear();
        }
    }

    // Messages HUD (item ajouté, or reçu, etc.)
    // Reset quand le tableau est vide pour relire un message identique qui réapparaît
    {
        RE::GFxValue messagesBlock;
        if (SafeGetVariable(movie, messagesBlock, "_root.HUDMovieBaseInstance.MessagesBlock") && SafeIsObject(messagesBlock)) {
            RE::GFxValue shownArray;
            if (messagesBlock.GetMember("ShownMessageArray", &shownArray) && SafeIsArray(shownArray)) {
                uint32_t len = SafeGetArraySize(shownArray);
                if (len == 0) {
                    g_hudPrevMessage.clear();
                } else {
                    for (uint32_t i = 0; i < len; i++) {
                        RE::GFxValue entry;
                        if (!shownArray.GetElement(i, &entry) || !SafeIsObject(entry)) continue;
                        RE::GFxValue textClip;
                        if (!entry.GetMember("TextFieldClip", &textClip) || !SafeIsObject(textClip)) continue;
                        RE::GFxValue tf1;
                        if (!textClip.GetMember("tf1", &tf1) || !SafeIsObject(tf1)) continue;
                        RE::GFxValue htmlText;
                        if (!tf1.GetMember("htmlText", &htmlText) || !SafeIsString(htmlText)) continue;
                        std::string msg = SafeGetString(htmlText);
                        if (!msg.empty() && msg != g_hudPrevMessage) {
                            g_hudPrevMessage = msg;
                            std::wstring wmsg = StripMarkupForSpeech(Utf8ToWString(msg));
                            if (!wmsg.empty()) {
                                // Filtrer "Aucune quête active" / "No active quest" (spam du mod Dio)
                                std::wstring lower = wmsg;
                                for (auto& c : lower) c = towlower(c);
                                bool mute = (lower.find(L"aucune qu") != std::wstring::npos) ||
                                            (lower.find(L"no active quest") != std::wstring::npos) ||
                                            (lower.find(L"no quest") != std::wstring::npos);
                                if (!mute) SpeakQueue(wmsg);
                            }
                        }
                        break;  // lire seulement le premier (le plus récent)
                    }
                }
            }
        } else if (!g_hudPrevMessage.empty()) {
            g_hudPrevMessage.clear();
        }
    }

    // Compteur de flèches (annonce seulement quand le type de flèche change, pas le nombre)
    {
        std::string arrowCount;
        if (GetGFxString(movie, "_root.HUDMovieBaseInstance.ArrowInfoInstance.ArrowCountInstance.ArrowNumInstance.text", arrowCount)) {
            if (!arrowCount.empty()) {
                // Extraire le nom de la flèche (avant le "(")
                std::string arrowName = arrowCount;
                auto paren = arrowName.find('(');
                if (paren != std::string::npos) arrowName = arrowName.substr(0, paren);
                // Ne vocaliser que si le type de flèche change
                if (arrowName != g_hudPrevArrowCount) {
                    g_hudPrevArrowCount = arrowName;
                    SpeakQueue(Utf8ToWString(arrowCount));
                }
            }
        } else if (!g_hudPrevArrowCount.empty()) {
            g_hudPrevArrowCount.clear();
        }
    }

    // Détection accroupi / debout
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            bool sneaking = player->IsSneaking();
            if (sneaking != g_wasSneaking) {
                g_wasSneaking = sneaking;
                Speak(sneaking ? L"Sneaking" : L"Standing");
            }
        }
    }

    // Détection première / troisième personne — mise à jour silencieuse de l'état
    // L'annonce vocale se fait uniquement sur appui de F dans InputListener (plugin.cpp)
    {
        auto* camera = RE::PlayerCamera::GetSingleton();
        if (camera) {
            bool firstPerson = camera->IsInFirstPerson();
            g_wasFirstPerson = firstPerson;
            g_cameraInitialized = true;
        }
    }

    // Statut furtivité (Hidden / Detected / Caution) — suspendre pendant le crafting, désactivable via MCM
    if (g_mcmStealthAnnounce.load() && !RE::UI::GetSingleton()->IsMenuOpen(RE::CraftingMenu::MENU_NAME)) {
        std::string stealth;
        if (GetGFxString(movie, "_root.HUDMovieBaseInstance.StealthMeterInstance.SneakTextHolder.SneakTextClip.SneakTextInstance.text", stealth)) {
            if (!stealth.empty() && stealth != g_hudPrevStealth) {
                g_hudPrevStealth = stealth;
                Speak(Utf8ToWString(stealth));
            }
        } else if (!g_hudPrevStealth.empty()) {
            g_hudPrevStealth.clear();
        }
    }

    // Sous-titres de dialogue — désactivé, les PNJ ont déjà des voix
    // Le champ GFx peut être rempli même si les sous-titres sont désactivés dans les options

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
