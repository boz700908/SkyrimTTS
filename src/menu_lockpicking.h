#pragma once

#include <cmath>

// MENU LOCKPICKING — Accessibilité du minijeu de crochetage
//
// Principe : bip à intervalle variable (style sonar) qui indique au joueur
// la distance entre son crochet et le sweet spot (zone d'or invisible). Plus
// le bip est rapide, plus on est proche. Quand on est DANS le sweet spot, le
// bip atteint sa cadence maximale.
//
// Données :
//   - currentAngle (angle du crochet) lu en C++ via LockpickingMenu::GetRuntimeData()
//   - sweet spot center + length : extraits du SWF au démarrage du menu
//     (parser les TextField debug : SweetSpotText = "SWEET SPOT: 30°" pour la
//      length, et SweetSpotRects_mc.SweetSpotRect._x + ._width pour la position
//      du centre, convertie en angle via fPickMinAngle/fPickMaxAngle).
//   - Le sweet spot change à chaque nouveau cadenas (et à chaque pick break),
//     donc on le recalcule chaque fois que la position du rectangle bouge.

static std::atomic_bool g_lockpickOpen{false};
static std::jthread     g_lockpickBipThread;
static std::jthread     g_lockpickSamplerThread;

// Sweet spot courant (en degres, dans le repere du jeu : currentAngle est dans
// la meme unite). Mis a jour quand on detecte un changement de position du
// rectangle dans le SWF.
static std::atomic<float> g_lockpickSweetCenter{0.0f};
static std::atomic<float> g_lockpickSweetHalfWidth{0.0f};
static std::atomic_bool   g_lockpickSweetValid{false};

// Angle courant du crochet, echantillonne sur le thread principal et expose
// au thread bip via cet atomic. Lecture sans verrou cote thread bip, pas de
// race possible.
static std::atomic<float> g_lockpickCurrentAngle{0.0f};

// Position x cache pour detecter quand le sweet spot change (nouveau cadenas
// ou pick break -> le moteur appelle UpdateSweetSpot avec une nouvelle valeur).
static std::atomic<double> g_lockpickLastRectX{-99999.0};

// Constantes geometriques calculees a chaque ReadLockpickSweetSpot, utilisees
// pour convertir PickIndicator_mc._x (pixels) en angle (degres) dans le repere
// de fPickMinAngle/fPickMaxAngle. Stocke en doubles non-atomic : seul le
// thread sampler les lit, et l'ecriture se fait aussi via AddUITask donc meme
// thread. Pas besoin d'atomicite stricte.
static double g_lockpickCalcFPickMin     = 0.0;
static double g_lockpickCalcHolderX      = 0.0;
static double g_lockpickCalcDegPerPixel  = 0.0;

// Flag d'annonce d'ouverture : true quand on a deja annonce niveau + crochets
// pour la session courante du menu. Reset a chaque ouverture.
static std::atomic_bool g_lockpickOpenAnnounced{false};

// Parametres du bip
// Note : le WAV fait 60 ms. Si l'intervalle entre deux bips egale 60 ms, on
// enchaine sans silence audible -> ressenti d'un son continu. C'est ce qu'on
// veut quand le joueur est DANS le sweet spot. En dehors, plus c'est loin
// plus l'intervalle est grand (style sonar).
//
// Deux threads :
//   - sampler : 33 ms (~30 Hz). Lit currentAngle depuis le menu via AddUITask
//     et le stocke dans un atomic. Refresh le sweet spot a la meme cadence
//     (a moindre cout : un seul AddUITask qui lit les 2 valeurs).
//   - bip : 30 ms. Lit l'atomic, calcule la cadence, joue le bip si l'intervalle
//     est ecoule. Pas de GFx ni d'acces moteur depuis ce thread = zero race.
static constexpr int   kLockpickBipPollMs        = 30;   // tick du thread bip
static constexpr int   kLockpickSamplerPollMs    = 33;   // tick du sampler (~30 Hz)
static constexpr int   kLockpickBipMinIntervalMs = 60;   // continu quand dans sweet spot
static constexpr int   kLockpickBipMaxIntervalMs = 900;  // bip le plus lent (loin)
static constexpr float kLockpickBipVolumeNear    = 1.0f;
static constexpr float kLockpickBipVolumeFar     = 0.6f;

// Pour le panning stereo : on positionne la source du bip a une distance laterale
// du joueur. Plus l'ecart |currentAngle - sweetCenter| est grand, plus on pousse
// la source loin sur le cote (panning plus marque). 300 units lateraux donnent
// un panning quasi-extreme (le moteur HRTF de Skyrim atteint son max bien avant).
static constexpr float kLockpickPanMaxLateralUnits = 300.0f;
static constexpr float kLockpickPanRangeDeg        = 30.0f;  // ecart au-dela duquel panning max

// Joue le bip lockpick 3D avec panning stereo gauche/droite.
// signedOffsetAngle = currentAngle - sweetCenter (signe = direction) :
//   - negatif (crochet a gauche du spot) -> source a DROITE du joueur (guide vers droite)
//   - positif (crochet a droite du spot) -> source a GAUCHE du joueur (guide vers gauche)
//   - zero (dans le sweet spot) -> son centre
static void PlayLockpickBipPanned(float signedOffsetAngle, float volume) {
    auto* mgr = RE::BSAudioManager::GetSingleton();
    if (!mgr) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    RE::BSSoundHandle handle;
    mgr->BuildSoundDataFromEditorID(handle, "SkyrimTTS_LockpickBip3D", 0x1A);
    if (!handle.IsValid()) return;

    handle.SetVolume(volume);

    // Calculer la position laterale dans le worldspace, dans le repere du joueur.
    // Skyrim yaw : 0 = nord (axe Y+), pi/2 = est (axe X+). Le vecteur "droite"
    // du joueur est donc (sin(yaw + pi/2), cos(yaw + pi/2), 0) = (cos(yaw), -sin(yaw), 0).
    const auto playerPos = player->GetPosition();
    const float yaw = player->GetAngleZ();
    const float rightX =  std::cos(yaw);
    const float rightY = -std::sin(yaw);

    // Normaliser l'offset signe en [-1 ; +1]. Au-dela de kLockpickPanRangeDeg,
    // panning maximal (les sons ne deviennent pas "plus a droite" qu'a droite).
    float t = signedOffsetAngle / kLockpickPanRangeDeg;
    if (t >  1.0f) t =  1.0f;
    if (t < -1.0f) t = -1.0f;

    // Inverser le signe : si crochet a gauche du sweet (signedOffset < 0), on
    // veut que le son sorte a DROITE pour guider le joueur. Donc lateral = -t.
    const float lateralUnits = -t * kLockpickPanMaxLateralUnits;

    RE::NiPoint3 sourcePos{
        playerPos.x + rightX * lateralUnits,
        playerPos.y + rightY * lateralUnits,
        playerPos.z + 60.0f  // hauteur des oreilles approximative
    };

    handle.SetPosition(sourcePos);
    handle.Play();
}

// Parse une chaine de la forme "SWEET SPOT: 30" ou "SWEET SPOT: 30°" en float.
// Retourne true si parse OK.
static bool ParseAngleFromText(const std::string& text, float& outAngle) {
    auto pos = text.find(':');
    if (pos == std::string::npos) return false;
    std::string num = text.substr(pos + 1);
    // Trim espaces et le symbole degre
    while (!num.empty() && (num.front() == ' ' || num.front() == '\t')) num.erase(num.begin());
    while (!num.empty() && (num.back() == ' '  || num.back() == '\t' ||
                            num.back() == '\xC2' || num.back() == '\xB0')) num.pop_back();
    try {
        outAngle = std::stof(num);
        return true;
    } catch (...) {
        return false;
    }
}

// Lit le sweet spot depuis le SWF. A appeler une seule fois a l'ouverture, puis
// a chaque fois qu'on detecte un changement de SweetSpotRect._x (nouveau cadenas).
// Renvoie true si on a pu obtenir un sweet spot coherent.
static bool ReadLockpickSweetSpot(RE::GFxMovieView* movie) {
    if (!movie) return false;

    // 1) Length : parser le texte "SWEET SPOT: 30"
    std::string sweetText;
    if (!GetGFxString(movie, "_root.LockpickingMenu_mc.DebugDisplay_mc.SweetSpotText.text", sweetText)) {
        LOG("Lockpick: failed to read SweetSpotText");
        return false;
    }
    float sweetLengthDeg = 0.0f;
    if (!ParseAngleFromText(sweetText, sweetLengthDeg) || sweetLengthDeg <= 0.0f) {
        LOG("Lockpick: invalid sweet length from '{}'", sweetText);
        return false;
    }

    // 2) Position du rectangle dans le holder (pixels)
    double rectX = 0.0, rectWidth = 0.0, holderX = 0.0;
    if (!GetGFxNumber(movie, "_root.LockpickingMenu_mc.DebugDisplay_mc.SweetSpotRects_mc.SweetSpotRect._x", rectX) ||
        !GetGFxNumber(movie, "_root.LockpickingMenu_mc.DebugDisplay_mc.SweetSpotRects_mc.SweetSpotRect._width", rectWidth) ||
        !GetGFxNumber(movie, "_root.LockpickingMenu_mc.DebugDisplay_mc.SweetSpotRects_mc._x", holderX)) {
        LOG("Lockpick: failed to read rect coordinates");
        return false;
    }

    if (rectWidth <= 0.0) {
        LOG("Lockpick: rect width is zero or negative");
        return false;
    }

    // 3) Lire fPickMinAngle (variable d'instance posee par le moteur via
    //    SetPickMinMax avant UpdateSweetSpot). C'est cette valeur qui permet
    //    de calculer le centre du sweet spot dans le repere de currentAngle.
    //    Sans elle on tombe dans un autre repere et l'ecart est faux.
    double fPickMinAngle = 0.0;
    bool gotMin = GetGFxNumber(movie, "_root.LockpickingMenu_mc.fPickMinAngle", fPickMinAngle);
    if (!gotMin) {
        LOG("Lockpick: failed to read fPickMinAngle, falling back to symmetric range");
    }

    // 4) Reconstruire le centre. Le SWF fait :
    //      rect._x = holderX + 1000 * (angle - fPickMinAngle) / (fPickMaxAngle - fPickMinAngle)
    //    Et degPerPixel = sweetLengthDeg / rectWidth = (max - min) / 1000.
    //    Donc :
    //      angle = fPickMinAngle + (rect._x - holderX) * degPerPixel
    const double degPerPixel = sweetLengthDeg / rectWidth;
    const double centerPx    = rectX + rectWidth / 2.0;
    double centerAngle = 0.0;
    if (gotMin) {
        centerAngle = fPickMinAngle + (centerPx - holderX) * degPerPixel;
    } else {
        // Fallback : range symetrique [-half ; +half]. Plus risque que le vrai
        // fPickMinAngle mais mieux que rien.
        const double holderHalfDeg = (1000.0 * degPerPixel) / 2.0;
        centerAngle = (centerPx - holderX) * degPerPixel - holderHalfDeg;
    }

    g_lockpickSweetCenter.store(static_cast<float>(centerAngle));
    g_lockpickSweetHalfWidth.store(sweetLengthDeg / 2.0f);
    g_lockpickSweetValid.store(true);
    g_lockpickLastRectX.store(rectX);

    // Stocker les constantes geometriques utilisees ensuite pour convertir la
    // position du PickIndicator (pixels) en angle (degres).
    g_lockpickCalcFPickMin    = gotMin ? fPickMinAngle : -(1000.0 * degPerPixel) / 2.0;
    g_lockpickCalcHolderX     = holderX;
    g_lockpickCalcDegPerPixel = degPerPixel;

    LOG("Lockpick: sweet center={:.2f} half={:.2f} length={:.2f} rectX={:.1f} holderX={:.1f} degPerPx={:.4f} fPickMin={:.2f} ({})",
        centerAngle, sweetLengthDeg / 2.0f, sweetLengthDeg, rectX, holderX, degPerPixel,
        fPickMinAngle, gotMin ? "OK" : "fallback");
    return true;
}

// Calcule l'intervalle entre deux bips (ms) en fonction de l'ecart |currentAngle - sweetCenter|.
// - Dans le sweet spot (ecart <= halfWidth) : intervalle minimum (bip continu)
// - En dehors : courbe en t^1.5 (entre lineaire et quadratique). t^2 etait
//   trop genereux pres du sweet spot (bip trop espace a mi-distance) ;
//   lineaire ne donne pas assez d'acceleration en se rapprochant. t^1.5 est
//   un bon compromis qui donne un ressenti "Geiger" naturel.
//   Range complete : 60 deg (au-dela t=1, bip au max).
static int ComputeBipIntervalMs(float currentAngle, float sweetCenter, float sweetHalfWidth) {
    const float diff = std::abs(currentAngle - sweetCenter);
    if (diff <= sweetHalfWidth) return kLockpickBipMinIntervalMs;

    const float outsideDist = diff - sweetHalfWidth;
    const float maxDist = 60.0f;
    const float linearT = std::min(1.0f, outsideDist / maxDist);
    const float t = std::pow(linearT, 1.5f);
    return static_cast<int>(kLockpickBipMinIntervalMs +
        t * (kLockpickBipMaxIntervalMs - kLockpickBipMinIntervalMs));
}

// Thread sampler : echantillonne le menu C++ + le SWF sur le thread principal
// via AddUITask. Met a jour les atomics :
//   - g_lockpickCurrentAngle (lu chaque cycle, peu cher)
//   - sweet spot (relu seulement quand rectX change = nouveau cadenas / pick break)
//
// Toutes les operations qui touchent au moteur (menus, GFx) sont confinees a
// la lambda AddUITask. Le thread bip lit uniquement des atomics -> zero race.
static void StartLockpickSamplerThread() {
    if (g_lockpickSamplerThread.joinable()) {
        g_lockpickSamplerThread.request_stop();
        g_lockpickSamplerThread.join();
    }
    g_lockpickSamplerThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kLockpickSamplerPollMs));
            if (!g_lockpickOpen.load(std::memory_order_relaxed)) continue;

            auto* task = SKSE::GetTaskInterface();
            if (!task) continue;

            task->AddUITask([]() {
                if (!g_lockpickOpen.load()) return;
                auto* ui = RE::UI::GetSingleton();
                if (!ui) return;
                auto menu = ui->GetMenu<RE::LockpickingMenu>();
                if (!menu) return;

                // 1) Echantillonner currentAngle.
                //    rt.currentAngle (offset 0x98 dans le header CommonLibSSE) ne
                //    bouge PAS en jeu : le commentaire du header est incorrect.
                //    Le SWF debug texte ne bouge pas non plus quand DebugDisplay_mc
                //    est invisible (cas par defaut, ligne 42 du LockpickingMenu.as).
                //    MAIS : PickIndicator_mc._x est mis a jour a chaque
                //    UpdatePickAngle (ligne 63 de l'AS), independamment de la
                //    visibilite. On lit cette position en pixels et on la convertit
                //    en degres via l'inverse de PickAngleToX :
                //      angle = fPickMinAngle + (_x - holderX) * degPerPixel
                //    On a deja degPerPixel et fPickMinAngle en cache depuis
                //    ReadLockpickSweetSpot.
                RE::GFxMovieView* movie = menu->uiMovie.get();
                if (!movie) return;

                double pickX = 0.0;
                if (GetGFxNumber(movie, "_root.LockpickingMenu_mc.DebugDisplay_mc.PickIndicator_mc._x", pickX)) {
                    const float currentAngle = static_cast<float>(
                        g_lockpickCalcFPickMin + (pickX - g_lockpickCalcHolderX) * g_lockpickCalcDegPerPixel);
                    g_lockpickCurrentAngle.store(currentAngle);
                }

                // 2) Verifier si le sweet spot a change (rectX bouge a chaque
                //    nouveau cadenas / pick break). Cout : 1 GetVariable + 1
                //    comparaison float. Si change, relire les 3 valeurs.
                double rectX = 0.0;
                if (GetGFxNumber(movie,
                        "_root.LockpickingMenu_mc.DebugDisplay_mc.SweetSpotRects_mc.SweetSpotRect._x",
                        rectX)) {
                    double cached = g_lockpickLastRectX.load();
                    if (std::abs(rectX - cached) > 0.5) {
                        LOG("Lockpick: sweet spot rect moved ({:.1f} -> {:.1f}), refreshing",
                            cached, rectX);
                        ReadLockpickSweetSpot(movie);
                    }
                }

                // 3) Annonce d'ouverture : "Crochetage. Cadenas <niveau>. <N> crochets."
                //    - Niveau : recupere via ExtraLock C++ (independant de la traduction
                //      vanilla qui ne donne pas le mot "Crochets" complet en FR)
                //    - Nombre : parse du SWF NumLockpicksText (format "X : 12" ou "X : 99+"),
                //      on garde juste le nombre apres le ':'.
                if (!g_lockpickOpenAnnounced.load()) {
                    // Niveau du cadenas via l'API C++.
                    auto* targetRef = RE::LockpickingMenu::GetTargetReference();
                    auto* extraLock = targetRef ? targetRef->extraList.GetByType<RE::ExtraLock>() : nullptr;
                    RE::LOCK_LEVEL level = RE::LOCK_LEVEL::kUnlocked;
                    if (extraLock && extraLock->lock) {
                        level = extraLock->lock->GetLockLevel(targetRef);
                    }

                    // Nombre de crochets : parser le SWF (la BottomBar est remplie
                    // par SetLockInfo, donc on doit attendre qu'elle soit prete).
                    std::string numPicksRaw;
                    bool gotPicks = GetGFxString(movie,
                        "_root.LockpickingMenu_mc.BottomBar_mc.InfoRect_mc.NumLockpicksText.text",
                        numPicksRaw);

                    if (level != RE::LOCK_LEVEL::kUnlocked && gotPicks && !numPicksRaw.empty()) {
                        // Traduction du niveau
                        std::wstring levelW;
                        switch (level) {
                            case RE::LOCK_LEVEL::kVeryEasy:    levelW = TR("Novice");     break;
                            case RE::LOCK_LEVEL::kEasy:        levelW = TR("Apprentice"); break;
                            case RE::LOCK_LEVEL::kAverage:     levelW = TR("Adept");      break;
                            case RE::LOCK_LEVEL::kHard:        levelW = TR("Expert");     break;
                            case RE::LOCK_LEVEL::kVeryHard:    levelW = TR("Master");     break;
                            case RE::LOCK_LEVEL::kRequiresKey: levelW = TR("Requires key"); break;
                            default: levelW = L""; break;
                        }

                        // Extraire le nombre apres le ':' dans NumLockpicksText
                        std::string picksNum;
                        auto colonPos = numPicksRaw.find(':');
                        if (colonPos != std::string::npos) {
                            picksNum = numPicksRaw.substr(colonPos + 1);
                            while (!picksNum.empty() && (picksNum.front() == ' ' || picksNum.front() == '\t'))
                                picksNum.erase(picksNum.begin());
                            while (!picksNum.empty() && (picksNum.back() == ' ' || picksNum.back() == '\t'))
                                picksNum.pop_back();
                        }

                        if (!levelW.empty() && !picksNum.empty()) {
                            std::wstring picksW = Utf8ToWString(picksNum.c_str());
                            std::wstring msg = TR("Lockpicking") + L". " +
                                               TR("Lock") + L" " + levelW + L". " +
                                               picksW + L" " + TR("lockpicks") + L".";
                            SpeakQueue(msg);
                            LOG("Lockpick: announced '{}'", WStringToUtf8(msg));
                            g_lockpickOpenAnnounced.store(true);
                        }
                    }
                }
            });
        }
    });
}

// Thread bip : tourne en boucle, lit uniquement des atomics, joue le bip a la
// cadence calculee. Ne touche jamais au moteur ni au SWF.
static void StartLockpickBipThread() {
    if (g_lockpickBipThread.joinable()) {
        g_lockpickBipThread.request_stop();
        g_lockpickBipThread.join();
    }
    g_lockpickBipThread = std::jthread([](std::stop_token st) {
        int64_t lastBipMs = 0;
        auto nowMs = []() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        };

        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kLockpickBipPollMs));
            if (!g_lockpickOpen.load(std::memory_order_relaxed)) continue;
            if (!g_lockpickSweetValid.load()) continue;

            const float currentAngle = g_lockpickCurrentAngle.load();
            const float sweetCenter  = g_lockpickSweetCenter.load();
            const float sweetHalf    = g_lockpickSweetHalfWidth.load();

            const int intervalMs = ComputeBipIntervalMs(currentAngle, sweetCenter, sweetHalf);
            const int64_t t = nowMs();

            if (t - lastBipMs >= intervalMs) {
                lastBipMs = t;
                // Volume final = volume interne (near/far selon distance au
                // sweet spot) * multiplicateur MCM utilisateur.
                const float baseVolume = (intervalMs == kLockpickBipMinIntervalMs)
                                ? kLockpickBipVolumeNear
                                : kLockpickBipVolumeFar;
                const float volume = baseVolume * g_volumeLockpickBip;

                // signedOffset = currentAngle - sweetCenter. Le panning est
                // calcule dans PlayLockpickBipPanned a partir de ce signe.
                const float signedOffset = currentAngle - sweetCenter;

                // BSAudioManager doit etre appele sur le thread principal.
                auto* task = SKSE::GetTaskInterface();
                if (task) {
                    task->AddTask([signedOffset, volume]() {
                        PlayLockpickBipPanned(signedOffset, volume);
                    });
                }
            }
        }
    });
}

static void StopLockpickThreads() {
    if (g_lockpickBipThread.joinable()) {
        g_lockpickBipThread.request_stop();
        g_lockpickBipThread.join();
    }
    if (g_lockpickSamplerThread.joinable()) {
        g_lockpickSamplerThread.request_stop();
        g_lockpickSamplerThread.join();
    }
}

// Appele a l'ouverture du Lockpicking Menu (depuis MenuListener)
//
// On demarre le sampler tout de suite : sa premiere iteration (apres 33 ms)
// detectera automatiquement que rectX est different de la valeur sentinelle
// (-99999) et appellera ReadLockpickSweetSpot. Plus besoin de thread differe
// avec sleep(300) — le sampler converge naturellement quand le SWF a recu
// UpdateSweetSpot du moteur (peut prendre quelques frames).
static void OnLockpickMenuOpen() {
    g_lockpickOpen.store(true);
    g_lockpickSweetValid.store(false);
    g_lockpickLastRectX.store(-99999.0);
    g_lockpickCurrentAngle.store(0.0f);
    g_lockpickOpenAnnounced.store(false);

    StartLockpickSamplerThread();
    StartLockpickBipThread();
}

static void OnLockpickMenuClose() {
    g_lockpickOpen.store(false);
    g_lockpickSweetValid.store(false);
    StopLockpickThreads();
}
