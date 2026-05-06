#pragma once

// VOCALISATION STATS MENU (compétences / perks) - DEBUT

#include "menu_perks.h"

static std::atomic_bool g_statsOpen{false};
static std::atomic_bool g_statsPendingUIRead{false};
static std::jthread     g_statsPollThread;
static std::string      g_statsPrevKey;
// Mémorise le dernier état "zoomed" pour détecter les transitions
// anneau <-> arbre.
static bool             g_statsLastZoomed = false;

// Debounce du mode arbre : on attend que le perk centré soit resté STABLE
// pendant un petit délai (anti-flicker / anti-snap-back de la caméra)
// avant d'annoncer. Si pendant ce délai le perk change, on remet à zéro.
//
// g_statsCandidatePerk : nom du perk candidat (sera annoncé si stable)
// g_statsCandidateSince : timestamp (en ms) du moment où ce candidat est apparu
// STATS_TREE_STABLE_MS : durée de stabilité requise avant annonce
static std::string                       g_statsCandidatePerk;
static std::chrono::steady_clock::time_point g_statsCandidateSince;
static constexpr int STATS_TREE_STABLE_MS = 150;

static constexpr const char* STATS_ROOT      = "_root.StatsMenuBaseInstance";
static constexpr const char* STATS_CARD_DESC = "_root.StatsMenuBaseInstance.DescriptionCardInstance.CardDescriptionTextInstance.text";
static constexpr const char* STATS_CARD_REQ  = "_root.StatsMenuBaseInstance.DescriptionCardInstance.SkillRequirementText.text";
static constexpr const char* STATS_LEVEL     = "_root.StatsMenuBaseInstance.TopPlayerInfo.LevelNumberLabel.text";
static constexpr const char* STATS_PERKS     = "_root.StatsMenuBaseInstance.AddPerkTextInstance.AddPerkTextField.text";

static constexpr int STATS_SKILL_COUNT = 18;
static constexpr const char* STATS_RING_BASE = "_root.StatsMenuBaseInstance.AnimatingSkillTextInstance.SkillText";

// L'arbre 3D affiche jusqu'à 64 perks visibles à l'écran via PerkName0..PerkName63.
static constexpr int STATS_PERK_SLOT_COUNT = 64;
static constexpr const char* STATS_PERK_SLOT_BASE = "_root.StatsMenuBaseInstance.PerkName";

// Cache des noms de compétences lus depuis l'anneau GFx
static std::string g_statsRingNames[STATS_SKILL_COUNT];
static bool g_statsRingCached = false;

// ---------------- Helpers texte (UTF-8 safe) ----------------
//
// Les littéraux L"..." dans MSVC sont mal encodés quand le fichier source
// n'a pas de BOM UTF-8 (les caractères accentués sortent en garbage).
// Par cohérence on passe systématiquement par Utf8ToWString() au runtime.
// Convention : on garde le préfixe "FR" historique des helpers, mais nos
// textes scriptés sont maintenant en anglais (les textes localisés du jeu
// restent évidemment dans la langue du joueur).

static std::wstring FR(const char* utf8) {
    return Utf8ToWString(std::string(utf8));
}

// FR avec un nombre intégré : FRn("rank ", 3, " of 5")
static std::wstring FRn(const char* utf8Prefix, int value, const char* utf8Suffix = "") {
    std::string s = utf8Prefix;
    s += std::to_string(value);
    s += utf8Suffix;
    return Utf8ToWString(s);
}

// FR avec deux nombres : FRnn("rank ", 3, " of ", 5)
static std::wstring FRnn(const char* utf8Prefix, int value1, const char* utf8Mid,
                         int value2, const char* utf8Suffix = "") {
    std::string s = utf8Prefix;
    s += std::to_string(value1);
    s += utf8Mid;
    s += std::to_string(value2);
    s += utf8Suffix;
    return Utf8ToWString(s);
}

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

// Critères pour qu'un perk soit considéré "vraiment centré" :
//   - scale > MIN_SCALE       → grand (couche centrale, pas adjacent)
//   - |x| le plus petit       → physiquement le plus proche du centre horizontal
//
// DÉCOUVERTES EMPIRIQUES (logs des tests utilisateur, 6 mai 2026) :
//
// 1. Le perk CENTRÉ a TOUJOURS x ≈ 0 (et non 640). Le perk à droite est
//    à x ≈ +618, celui à gauche à x ≈ -618. Le centre Flash est donc x=0.
//
// 2. Les perks vraiment centrés ont scale ≥ 113. Les perks "à l'horizon"
//    pendant les rotations de caméra ont scale 101-110 (juste au-dessus
//    du seuil, mais pas vraiment centrés). Pour éliminer ces faux positifs
//    qui causaient des annonces parasites de perks brièvement traversés,
//    on remonte le seuil à 110.
//
// 3. La constellation peut faire apparaître des perks d'AUTRES compétences
//    pendant les transitions (ex: "Forge elfique" annoncé en mode
//    Enchantement). On filtre par appartenance à la compétence courante
//    plus loin dans le pipeline.
static constexpr double STATS_TREE_MIN_SCALE = 110.0;

// Lit le nom du perk VRAIMENT centré dans la constellation 3D.
//
// Algorithme :
//   1. Filtre les slots avec scale > MIN_SCALE (vraiment grands)
//   2. Parmi ceux-là, retourne celui dont |x| est le plus petit (le plus
//      proche de l'axe vertical central de l'écran)
//   3. Si aucun ne remplit le critère scale, retourne ""
//
// Retourne "" pendant les phases d'ouverture du menu et les transitions
// très rapides où aucun perk n'a encore atteint sa taille finale.
static std::string GetSelectedPerkName(RE::GFxMovieView* movie,
                                       double* outBestScale = nullptr,
                                       std::string* outTop3Debug = nullptr) {
    if (!movie) return {};

    struct SlotInfo {
        double      scale;
        double      x, y;
        double      absX;  // pour tri
        std::string name;
    };
    // Top 3 trié par |x| (croissant) → le plus proche du centre horizontal en [0]
    constexpr double INF = 1e9;
    SlotInfo top3[3] = {{0,0,0,INF,""}, {0,0,0,INF,""}, {0,0,0,INF,""}};

    for (int i = 0; i < STATS_PERK_SLOT_COUNT; ++i) {
        std::string base = std::string(STATS_PERK_SLOT_BASE) + std::to_string(i);

        double scale = 0;
        if (!GetGFxNumber(movie, (base + "._xscale").c_str(), scale)) continue;
        // Filtre principal : on n'accepte que les "grands" perks (couche
        // centrale de la constellation). Les perks adjacents font scale 50-75,
        // les placeholders font scale 100 exact.
        if (scale <= STATS_TREE_MIN_SCALE) continue;

        double x = 0, y = 0;
        GetGFxNumber(movie, (base + "._x").c_str(), x);
        GetGFxNumber(movie, (base + "._y").c_str(), y);

        std::string name;
        if (!GetGFxString(movie, (base + ".PerkNameClipInstance.NameText.text").c_str(), name)) continue;
        if (name.empty()) continue;

        double absX = std::abs(x);
        if (absX >= top3[2].absX) continue;  // pire que le pire du top 3

        // Insertion triée dans le top 3 (croissant par |x|)
        SlotInfo s = {scale, x, y, absX, name};
        if (absX < top3[0].absX) {
            top3[2] = top3[1];
            top3[1] = top3[0];
            top3[0] = s;
        } else if (absX < top3[1].absX) {
            top3[2] = top3[1];
            top3[1] = s;
        } else {
            top3[2] = s;
        }
    }

    if (outBestScale) *outBestScale = top3[0].scale;
    if (outTop3Debug) {
        char buf[768];
        if (top3[0].name.empty()) {
            snprintf(buf, sizeof(buf), "<no centered perk> (no slot with scale > %.0f)",
                     STATS_TREE_MIN_SCALE);
        } else {
            snprintf(buf, sizeof(buf),
                     "[1] '%s' s=%.1f xy=%.0f,%.0f | [2] '%s' s=%.1f xy=%.0f,%.0f | [3] '%s' s=%.1f xy=%.0f,%.0f",
                     top3[0].name.c_str(), top3[0].scale, top3[0].x, top3[0].y,
                     top3[1].name.c_str(), top3[1].scale, top3[1].x, top3[1].y,
                     top3[2].name.c_str(), top3[2].scale, top3[2].x, top3[2].y);
        }
        *outTop3Debug = buf;
    }

    return top3[0].name;
}

// Renvoie le ActorValue de la compétence courante du menu (pour préférer le
// matching dans cette skill quand on cherche le perk par nom).
static RE::ActorValue GetCurrentMenuSkill() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return RE::ActorValue::kNone;
    auto menu = ui->GetMenu(RE::StatsMenu::MENU_NAME);
    if (!menu) return RE::ActorValue::kNone;
    auto* sm = static_cast<RE::StatsMenu*>(menu.get());
    if (!sm) return RE::ActorValue::kNone;
    auto& rd = sm->GetRuntimeData();
    if (rd.skillTrees.empty()) return RE::ActorValue::kNone;
    if (rd.selectedTree >= rd.skillTrees.size()) return RE::ActorValue::kNone;
    return rd.skillTrees[rd.selectedTree];
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

// Construit DEUX annonces séparées pour le perk sélectionné.
//   - urgent : nom + statut + niveau + dispo + topo (court, prioritaire, Speak())
//   - queued : description complète (longue, SpeakQueue() — coupable si on
//              navigue plus loin)
struct PerkAnnouncement {
    std::wstring urgent;
    std::wstring queued;
    bool         valid = false;  // false si le perk n'est pas dans l'index
};

// Supprime un suffixe " (X/Y)" du nom d'un perk (le SWF l'ajoute pour les
// perks à rangs multiples : "Regain (0/2)"). Le nom dans notre index C++ est
// juste "Regain", donc on retire le suffixe avant de matcher.
static std::wstring StripRankSuffix(const std::wstring& name) {
    auto pos = name.rfind(L" (");
    if (pos == std::wstring::npos) return name;
    if (name.empty() || name.back() != L')') return name;
    // Vérifie le contenu entre parenthèses : doit contenir un '/'
    std::wstring inside = name.substr(pos + 2, name.size() - pos - 3);
    if (inside.find(L'/') == std::wstring::npos) return name;
    return name.substr(0, pos);
}

// Construit l'annonce d'un perk depuis les données NATIVES du moteur (pas
// depuis le SWF). Cette approche est synchrone et fiable : aucune
// dépendance à l'état d'animation du SWF, donc plus de désynchronisation
// possible entre le nom du perk et sa description / son niveau requis.
static PerkAnnouncement BuildPerkAnnouncement(const std::string& rawPerkName) {
    PerkAnnouncement out;

    // Trouver le perk dans l'index. Si introuvable (placeholder "Perk Name"
    // ou autre nom transitoire), on retourne valid=false : l'appelant
    // ignorera cette lecture sans rien annoncer.
    std::wstring wnameRaw = StripMarkupForSpeech(Utf8ToWString(rawPerkName));
    std::wstring wname    = StripRankSuffix(wnameRaw);  // "Regain (0/2)" -> "Regain"
    auto* entry = perks::FindPerkByDisplayName(wname, GetCurrentMenuSkill());
    if (!entry) {
        out.valid = false;
        return out;
    }
    out.valid = true;

    // 1. Nom (depuis l'index, garanti propre)
    out.urgent += wname;

    // 2. Statut acquis ou non
    std::int8_t curRank   = perks::GetPlayerRankOn(entry->perk);
    std::int8_t totalRank = perks::GetTotalRanks(entry->perk);

    if (curRank > 0) {
        if (totalRank > 1) {
            out.urgent += L". ";
            out.urgent += FRnn("already learned, rank ", (int)curRank, " of ", (int)totalRank);
        } else {
            out.urgent += L". ";
            out.urgent += FR("already learned");
        }
    } else {
        out.urgent += L". ";
        out.urgent += FR("not learned");
    }

    // 3. Niveau de compétence requis — lecture native depuis les conditions
    //    CTDA du perk (garanti synchro, peu importe l'état du SWF).
    auto* nextRankPerk = entry->perk;
    for (std::int8_t i = 0; i < curRank && nextRankPerk; ++i) {
        nextRankPerk = nextRankPerk->nextPerk;
    }
    bool levelOK = true;
    if (nextRankPerk && entry->skill != RE::ActorValue::kNone) {
        int reqLvl = perks::GetRequiredSkillLevelNative(nextRankPerk, entry->skill);
        if (reqLvl > 0) {
            auto* pc = RE::PlayerCharacter::GetSingleton();
            float baseLvl = 0.0f;
            if (pc) {
                auto* avo = pc->AsActorValueOwner();
                if (avo) baseLvl = avo->GetBaseActorValue(entry->skill);
            }
            if (baseLvl >= (float)reqLvl) {
                out.urgent += L". ";
                out.urgent += FRn("requires skill level ", reqLvl);
            } else {
                out.urgent += L". ";
                out.urgent += FRnn("requires skill level ", reqLvl,
                                   ", you have ", (int)baseLvl);
                levelOK = false;
            }
        }
    }

    // 4. Prérequis manquants (parents) — pour le rang 1 uniquement.
    //    Les rangs > 1 ne dépendent que du rang précédent du même perk.
    bool prereqOK = true;
    if (curRank == 0 && entry->node) {
        for (auto* parent : entry->node->parents) {
            if (!parent || !parent->perk) continue;
            auto* pc = RE::PlayerCharacter::GetSingleton();
            if (pc && !pc->HasPerk(parent->perk)) {
                out.urgent += L". ";
                out.urgent += FR("missing prerequisite: ");
                const char* pn = parent->perk->GetFullName();
                if (pn) out.urgent += Utf8ToWString(pn);
                prereqOK = false;
                break;  // un seul suffit
            }
        }
    }

    // 5. Achetable maintenant ?
    if (curRank < totalRank && levelOK && prereqOK) {
        out.urgent += L". ";
        out.urgent += FR("can be learned now");
    }

    // 6. Type topologique (uniquement si ça apporte de l'info)
    perks::NodeShape shape = perks::GetNodeShape(*entry);
    int childCount = perks::CountUsefulChildren(*entry);
    if (shape == perks::NodeShape::kBranch) {
        out.urgent += L". ";
        out.urgent += FRn("branches into ", childCount, " perks");
    } else if (shape == perks::NodeShape::kLeaf) {
        out.urgent += L". ";
        out.urgent += FR("end of branch");
    }
    // kLinear : on ne dit rien (cas le plus banal)

    // 7. Description — lecture native depuis le perk (garanti synchro).
    //    Mise dans la file d'attente : enchaîne après l'urgent sans être
    //    coupée. Si on navigue plus loin, le Speak() suivant annulera
    //    cette description naturellement.
    //    Note : on lit la description du nextRankPerk pour les rangs > 1
    //    (chaque rang a sa propre description "+25%", "+50%", etc.).
    RE::BGSPerk* descPerk = nextRankPerk ? nextRankPerk : entry->perk;
    std::wstring wdesc = perks::GetPerkDescriptionNative(descPerk);
    if (!wdesc.empty()) out.queued = StripMarkupForSpeech(wdesc);

    return out;
}

// =============================================================================
// FONCTIONNALITÉ 3 — Raccourcis clavier de navigation logique dans l'arbre
// =============================================================================
//
// Quatre touches permettent à l'aveugle d'explorer l'arbre logique sans
// avoir à errer dans la constellation 3D :
//
//   Touche 1 : enfants du perk centré (où je peux aller depuis ici)
//   Touche 2 : parents du perk centré (ses prérequis)
//   Touche 3 : perks achetables maintenant dans cet arbre
//   Touche 4 : perks déjà acquis dans cet arbre
//
// Toutes ces fonctions utilisent l'index perks::g_perksBySkill construit
// à l'ouverture du menu (compatible mods Ordinator/Vokrii).

// Récupère le perk centré actuel (perk + skill courante).
// Retourne nullptr si rien de centré ou si on n'est pas en mode arbre.
static const perks::PerkIndexEntry* GetCurrentCenteredPerk() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) {
        LOG("[stats] hotkey: UI singleton null");
        return nullptr;
    }
    auto menu = ui->GetMenu(RE::StatsMenu::MENU_NAME);
    if (!menu) {
        LOG("[stats] hotkey: StatsMenu not open");
        return nullptr;
    }
    auto* sm = static_cast<RE::StatsMenu*>(menu.get());
    if (!sm) return nullptr;
    if (!sm->GetRuntimeData().zoomed) {
        LOG("[stats] hotkey: not in zoomed mode (ring), ignoring");
        return nullptr;
    }
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return nullptr;

    std::string rawName = GetSelectedPerkName(movie);
    if (rawName.empty()) {
        LOG("[stats] hotkey: no centered perk (camera in transition?)");
        return nullptr;
    }

    std::wstring wname = StripRankSuffix(StripMarkupForSpeech(Utf8ToWString(rawName)));
    auto* entry = perks::FindPerkByDisplayName(wname, GetCurrentMenuSkill());
    if (!entry) {
        LOG("[stats] hotkey: perk '{}' not found in index", rawName);
    }
    return entry;
}

// Touche 1 — Annonce les enfants (où peut-on aller depuis ce perk).
static void AnnouncePerkChildren() {
    auto* entry = GetCurrentCenteredPerk();
    if (!entry) {
        Speak(FR("No perk selected"));
        return;
    }

    auto children = perks::GetChildPerks(*entry);
    LOG("[stats] hotkey 1: perk has {} children", children.size());

    if (children.empty()) {
        Speak(FR("End of branch, no successor"));
        return;
    }

    std::wstring announce = FRn("This perk leads to ", (int)children.size(),
                                children.size() > 1 ? " perks: " : " perk: ");
    for (size_t i = 0; i < children.size(); ++i) {
        if (i > 0) announce += L", ";
        const char* name = children[i]->GetFullName();
        if (name && *name) announce += Utf8ToWString(name);
    }
    Speak(announce);
}

// Touche 2 — Annonce les parents (prérequis directs).
static void AnnouncePerkParents() {
    auto* entry = GetCurrentCenteredPerk();
    if (!entry) {
        Speak(FR("No perk selected"));
        return;
    }

    auto parents = perks::GetParentPerks(*entry);
    LOG("[stats] hotkey 2: perk has {} parents", parents.size());

    if (parents.empty()) {
        Speak(FR("This is a root perk with no prerequisite"));
        return;
    }

    std::wstring announce = FRn("This perk requires ", (int)parents.size(),
                                parents.size() > 1 ? " prerequisites: " : " prerequisite: ");
    for (size_t i = 0; i < parents.size(); ++i) {
        if (i > 0) announce += L", ";
        const char* name = parents[i]->GetFullName();
        if (name && *name) announce += Utf8ToWString(name);
    }
    Speak(announce);
}

// Vérifie si le joueur peut acheter (le rang suivant de) ce perk maintenant :
// niveau de compétence OK ET tous les parents (prérequis) acquis.
static bool IsPerkBuyableNow(const perks::PerkIndexEntry& e) {
    auto* pc = RE::PlayerCharacter::GetSingleton();
    if (!pc || !e.perk) return false;

    std::int8_t curRank   = perks::GetPlayerRankOn(e.perk);
    std::int8_t totalRank = perks::GetTotalRanks(e.perk);
    if (curRank >= totalRank) return false;  // déjà au max

    // Trouver le prochain rang à acheter
    auto* nextRankPerk = e.perk;
    for (std::int8_t i = 0; i < curRank && nextRankPerk; ++i) {
        nextRankPerk = nextRankPerk->nextPerk;
    }
    if (!nextRankPerk) return false;

    // Niveau de compétence requis
    int reqLvl = perks::GetRequiredSkillLevelNative(nextRankPerk, e.skill);
    if (reqLvl > 0 && e.skill != RE::ActorValue::kNone) {
        auto* avo = pc->AsActorValueOwner();
        if (!avo) return false;
        if (avo->GetBaseActorValue(e.skill) < (float)reqLvl) return false;
    }

    // Prérequis (parents) — uniquement pour le rang 1
    if (curRank == 0 && e.node) {
        for (auto* parent : e.node->parents) {
            if (!parent || !parent->perk) continue;
            if (!pc->HasPerk(parent->perk)) return false;
        }
    }

    return true;
}

// Touche 3 — Liste les perks ACHETABLES MAINTENANT dans la skill courante.
static void AnnouncePerksBuyable() {
    auto skill = GetCurrentMenuSkill();
    if (skill == RE::ActorValue::kNone) {
        LOG("[stats] hotkey 3: no current skill");
        Speak(FR("No skill selected"));
        return;
    }

    auto it = perks::g_perksBySkill.find(skill);
    if (it == perks::g_perksBySkill.end() || it->second.empty()) {
        LOG("[stats] hotkey 3: no perks in tree for skill");
        Speak(FR("No perks in this tree"));
        return;
    }

    std::vector<std::wstring> buyable;
    for (const auto& e : it->second) {
        if (IsPerkBuyableNow(e)) {
            const char* name = e.perk->GetFullName();
            if (name && *name) buyable.push_back(Utf8ToWString(name));
        }
    }
    LOG("[stats] hotkey 3: {} buyable perks (out of {})", buyable.size(), it->second.size());

    if (buyable.empty()) {
        Speak(FR("No perks can be learned now in this tree"));
        return;
    }

    std::wstring announce = FRn("",  (int)buyable.size(),
                                buyable.size() > 1 ? " perks can be learned: " : " perk can be learned: ");
    for (size_t i = 0; i < buyable.size(); ++i) {
        if (i > 0) announce += L", ";
        announce += buyable[i];
    }
    Speak(announce);
}

// Touche 4 — Liste les perks DÉJÀ ACQUIS dans la skill courante.
static void AnnouncePerksOwned() {
    auto skill = GetCurrentMenuSkill();
    if (skill == RE::ActorValue::kNone) {
        LOG("[stats] hotkey 4: no current skill");
        Speak(FR("No skill selected"));
        return;
    }

    auto it = perks::g_perksBySkill.find(skill);
    if (it == perks::g_perksBySkill.end() || it->second.empty()) {
        LOG("[stats] hotkey 4: no perks in tree for skill");
        Speak(FR("No perks in this tree"));
        return;
    }

    auto* pc = RE::PlayerCharacter::GetSingleton();
    if (!pc) {
        LOG("[stats] hotkey 4: PlayerCharacter null");
        Speak(FR("Error: player not found"));
        return;
    }

    // On collecte les perks acquis avec leur rang.
    struct OwnedInfo {
        std::wstring name;
        int          curRank;
        int          totalRank;
    };
    std::vector<OwnedInfo> owned;
    for (const auto& e : it->second) {
        std::int8_t cur = perks::GetPlayerRankOn(e.perk);
        if (cur <= 0) continue;
        std::int8_t total = perks::GetTotalRanks(e.perk);
        const char* name = e.perk->GetFullName();
        if (!name || !*name) continue;
        owned.push_back({Utf8ToWString(name), (int)cur, (int)total});
    }
    LOG("[stats] hotkey 4: {} owned perks (out of {})", owned.size(), it->second.size());

    if (owned.empty()) {
        Speak(FR("You have no perks in this tree"));
        return;
    }

    std::wstring announce = FRn("You have ", (int)owned.size(),
                                owned.size() > 1 ? " perks in this tree: " : " perk in this tree: ");
    for (size_t i = 0; i < owned.size(); ++i) {
        if (i > 0) announce += L", ";
        announce += owned[i].name;
        if (owned[i].totalRank > 1) {
            announce += L" ";
            announce += FRnn("rank ", owned[i].curRank, " of ", owned[i].totalRank);
        }
    }
    Speak(announce);
}

// Annonce la sélection courante si elle a changé.
// Deux modes :
//   - zoomed == false : on est sur l'anneau des compétences (comportement
//                       original, inchangé)
//   - zoomed == true  : on est dans la constellation 3D, on annonce le
//                       perk centré avec toutes ses infos
static void AnnounceStatsSelectionImpl() {
    if (g_levelUpOpen.load()) return;
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::StatsMenu::MENU_NAME);
    if (!menu) return;
    auto* sm = static_cast<RE::StatsMenu*>(menu.get());
    if (!sm) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    bool zoomed = sm->GetRuntimeData().zoomed;

    // Détection de transition anneau <-> constellation : on RESET la clé
    // de dédup ET le candidat en cours pour repartir avec un état propre.
    if (zoomed != g_statsLastZoomed) {
        LOG("[stats] mode change: zoomed={} -> {}", g_statsLastZoomed, zoomed);
        g_statsLastZoomed = zoomed;
        g_statsPrevKey.clear();
        g_statsCandidatePerk.clear();
    }

    if (!zoomed) {
        // ----- MODE ANNEAU (comportement original) -----
        std::string skillName = GetSelectedSkillName(movie);
        std::string cardDesc;
        GetGFxString(movie, STATS_CARD_DESC, cardDesc);

        std::string key = "RING|" + skillName + "|" + cardDesc;
        if (key == g_statsPrevKey || skillName.empty()) return;
        g_statsPrevKey = key;

        LOG("[stats] ring selection: skill='{}', cardDesc len={}", skillName, cardDesc.size());

        std::wstring announce;
        announce += StripMarkupForSpeech(Utf8ToWString(skillName));

        // Prérequis (peut contenir le niveau actuel de la compétence)
        std::string req;
        if (GetGFxString(movie, STATS_CARD_REQ, req) && !req.empty()) {
            std::wstring wreq = StripMarkupForSpeech(ResolveUIString(movie, req));
            if (!wreq.empty()) {
                announce += L", ";
                announce += wreq;
            }
        }

        if (!cardDesc.empty()) {
            std::wstring wdesc = StripMarkupForSpeech(ResolveUIString(movie, cardDesc));
            if (!wdesc.empty()) {
                announce += L". ";
                announce += wdesc;
            }
        }

        if (!announce.empty()) Speak(announce);
        return;
    }

    // ----- MODE ARBRE -----
    //
    // Stratégie : DEBOUNCE basé sur la stabilité du nom du perk centré.
    // On annonce uniquement quand le perk centré reste le MÊME pendant au
    // moins STATS_TREE_STABLE_MS (150 ms).
    //
    // DIAGNOSTIC : on trace toutes les TRANSITIONS (changement de perk
    // observé par le polling) + un HEARTBEAT toutes les ~1 seconde même si
    // rien ne change, pour vérifier que le polling tourne et voir le top 3
    // des perks visibles à chaque heartbeat.

    static std::string g_diagLastSeenPerk;
    static std::string g_diagLastSkippedReason;
    static std::chrono::steady_clock::time_point g_diagLastHeartbeat;

    double bestScale = 0;
    std::string top3Debug;
    std::string perkName = GetSelectedPerkName(movie, &bestScale, &top3Debug);

    // HEARTBEAT : toutes les 1000 ms, écrit l'état complet (top 3 des perks
    // avec leur scale + le perk choisi comme "centré" par notre algo).
    auto nowHb = std::chrono::steady_clock::now();
    auto hbMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    nowHb - g_diagLastHeartbeat).count();
    if (hbMs >= 1000) {
        LOG("[stats] DIAG HEARTBEAT: chosen='{}' (scale={:.1f}) | top3: {}",
            perkName, bestScale, top3Debug);
        g_diagLastHeartbeat = nowHb;
    }

    // ---- CAS 1 : aucun perk vraiment centré (caméra en transition) ----
    //
    // GetSelectedPerkName retourne "" quand aucun perk ne remplit les
    // critères stricts (scale > 100 ET distance < 400). Cela arrive pendant
    // les déplacements de caméra. On NE TOUCHE PAS au candidat — la caméra
    // va se poser sur un perk d'ici quelques ticks, on attend.
    if (perkName.empty()) {
        if (g_diagLastSeenPerk != "<empty>") {
            LOG("[stats] DIAG: now seeing <no centered perk> | {}", top3Debug);
            g_diagLastSeenPerk = "<empty>";
            g_diagLastSkippedReason.clear();
        }
        return;
    }

    // Trace : nouveau perk vu (différent du tick précédent).
    bool firstObservationOfThisPerk = (g_diagLastSeenPerk != perkName);
    if (firstObservationOfThisPerk) {
        LOG("[stats] DIAG: now seeing perk='{}' (scale={:.1f})", perkName, bestScale);
        g_diagLastSeenPerk = perkName;
        g_diagLastSkippedReason.clear();
    }

    // ---- CAS 2 : nom hors index (placeholder, transitoire) ----
    {
        std::wstring wnameRaw = StripMarkupForSpeech(Utf8ToWString(perkName));
        std::wstring wname    = StripRankSuffix(wnameRaw);
        if (perks::FindPerkByDisplayName(wname, GetCurrentMenuSkill()) == nullptr) {
            if (g_diagLastSkippedReason != "not_in_index") {
                LOG("[stats] DIAG: skip perk='{}' reason=not_in_index", perkName);
                g_diagLastSkippedReason = "not_in_index";
            }
            return;
        }
    }

    // ---- CAS 3 : dédup (déjà annoncé, et toujours sur le même perk) ----
    //
    // La dédup empêche de réannoncer si l'utilisateur reste immobile sur le
    // perk déjà annoncé. Elle se "lâche" automatiquement dès que mon code
    // voit un AUTRE perk centré (le `key != g_statsPrevKey` ci-dessous
    // est faux uniquement tant que c'est le même perk que celui annoncé).
    std::string key = "TREE|" + perkName;
    if (key == g_statsPrevKey) {
        if (g_diagLastSkippedReason != "dedup") {
            LOG("[stats] DIAG: skip perk='{}' reason=dedup (already announced)", perkName);
            g_diagLastSkippedReason = "dedup";
        }
        return;
    }

    // ---- CAS 4 : DEBOUNCE en cours ----
    auto now = std::chrono::steady_clock::now();
    if (g_statsCandidatePerk != perkName) {
        // Nouveau candidat — démarre le timer
        g_statsCandidatePerk  = perkName;
        g_statsCandidateSince = now;
        LOG("[stats] DIAG: candidate started for perk='{}' (timer 0ms / target {}ms)",
            perkName, STATS_TREE_STABLE_MS);
        g_diagLastSkippedReason = "stable_pending";
        return;
    }
    auto stableMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - g_statsCandidateSince).count();
    if (stableMs < STATS_TREE_STABLE_MS) {
        // En attente — ne pas spammer le log, on a déjà loggé "candidate started"
        return;
    }

    // ---- CAS 5 : on annonce ----
    PerkAnnouncement ann = BuildPerkAnnouncement(perkName);
    if (!ann.valid) {
        // Sécurité : ne devrait pas arriver, on a déjà filtré plus haut.
        LOG("[stats] DIAG: invalid perk='{}' (race?)", perkName);
        g_statsCandidatePerk.clear();
        return;
    }

    g_statsPrevKey = key;
    g_statsCandidatePerk.clear();  // candidat consommé
    g_diagLastSkippedReason.clear();
    LOG("[stats] tree selection: perk='{}', queued len={} (stable {}ms)",
        perkName, ann.queued.size(), stableMs);

    if (!ann.urgent.empty()) Speak(ann.urgent);
    if (!ann.queued.empty()) SpeakQueue(ann.queued);
}

static void QueueStatsRead() {
    // Guard : si le menu Stats n'est plus ouvert, on ne pose pas la main sur
    // le flag. Sinon on risque que le polling (ou un déclenchement clavier
    // résiduel) pose g_statsPendingUIRead à true juste après que StopStatsPoll
    // ait été appelé, ce qui laisse le flag coincé à true et empêche toute
    // future ouverture de fonctionner correctement.
    if (!g_statsOpen.load(std::memory_order_relaxed)) return;
    if (g_statsPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_statsPendingUIRead.store(false); return; }
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
    g_statsCandidatePerk.clear();
    g_statsRingCached = false;
    g_statsLastZoomed = false;

    // Construit l'index de perks (dans le thread UI pour être sûr que le
    // moteur a fini d'initialiser StatsMenu).
    auto* task = SKSE::GetTaskInterface();
    if (task) {
        task->AddUITask([]() {
            if (g_statsOpen.load()) {
                perks::BuildIndex();
            }
        });
    }

    g_statsPollThread = std::jthread([](std::stop_token st) {
        // Attend que le jeu ait initialisé le menu (SetPerkCount, etc.)
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        if (!st.stop_requested() && g_statsOpen.load()) {
            auto* t = SKSE::GetTaskInterface();
            if (t) t->AddUITask([]() { if (g_statsOpen.load()) AnnounceStatsOpenImpl(); });
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
    perks::ResetIndex();
}

// VOCALISATION STATS MENU - FIN
