#pragma once

// =============================================================================
// PERK TREE INDEX & HELPERS
//
// Construit en mémoire un index de l'arbre de perks pour CHAQUE compétence
// disponible dans le StatsMenu (kSmithing, kOneHanded, ... + Beast Skills si
// présents). Permet ensuite de :
//   - Retrouver un BGSPerk* à partir du nom affiché à l'écran
//   - Lister les parents (prérequis) et enfants (perks débloqués)
//   - Savoir si le joueur a appris un perk (et à quel rang)
//   - Savoir si le joueur peut l'apprendre maintenant
//
// L'index est reconstruit à chaque ouverture du StatsMenu (les mods de perks
// type Ordinator/Vokrii sont donc supportés automatiquement, on lit ce que le
// moteur a chargé).
// =============================================================================

#include <map>
#include <cwctype>

#include "RE/A/ActorValueInfo.h"
#include "RE/A/ActorValueList.h"
#include "RE/A/ActorValueOwner.h"
#include "RE/A/ActorValues.h"
#include "RE/B/BGSPerk.h"
#include "RE/B/BGSPerkRankArray.h"
#include "RE/B/BGSSkillPerkTreeNode.h"
#include "RE/B/BSString.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/S/StatsMenu.h"
#include "RE/T/TESCondition.h"
#include "RE/T/TESDescription.h"
#include "RE/T/TESFullName.h"
#include "RE/T/TESGlobal.h"

namespace perks {

// Une entrée de l'index : pointe vers le perk (rang 1) et son nœud d'arbre.
struct PerkIndexEntry {
    RE::BGSPerk*               perk{nullptr};
    RE::BGSSkillPerkTreeNode*  node{nullptr};
    RE::ActorValue             skill{RE::ActorValue::kNone};
    std::wstring               nameLower;  // pour matching insensible à la casse
};

// Index global, reconstruit à chaque ouverture du menu.
//   - g_perkByName   : nom (lowercased) -> entrée (pour retrouver le perk
//                      sélectionné à partir du nom affiché à l'écran).
//                      MULTIMAP car certains perks de mods peuvent partager
//                      un nom.
//   - g_perkByPtr    : pointeur perk    -> entrée (pour parents/enfants/rang)
//   - g_perksBySkill : ActorValue       -> liste des entrées de cette skill
static std::unordered_multimap<std::wstring, PerkIndexEntry>     g_perkByName;
static std::unordered_map<RE::BGSPerk*, PerkIndexEntry>          g_perkByPtr;
// std::map (pas unordered) car RE::ActorValue n'a pas de hasher std par défaut.
static std::map<RE::ActorValue, std::vector<PerkIndexEntry>>     g_perksBySkill;
static bool g_perkIndexBuilt = false;

// --------------------------------------------------------------------
// Conversion utilitaires
// --------------------------------------------------------------------

static std::wstring ToLowerW(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) out.push_back((wchar_t)std::towlower((wint_t)c));
    return out;
}

// Strip HTML tags (\<...>) — les noms de perks dans PerkNamesA sont parfois
// wrappés en HTML par le moteur.
static std::wstring StripHtml(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    bool inTag = false;
    for (wchar_t c : s) {
        if (c == L'<') { inTag = true; continue; }
        if (c == L'>') { inTag = false; continue; }
        if (!inTag) out.push_back(c);
    }
    return out;
}

// Trim espaces début/fin
static std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}

// --------------------------------------------------------------------
// Construction de l'index
// --------------------------------------------------------------------

// Parcours récursif du DAG de l'arbre de perks pour une compétence donnée.
static void WalkPerkTree(RE::BGSSkillPerkTreeNode* node,
                         RE::ActorValue skill,
                         std::set<RE::BGSSkillPerkTreeNode*>& visited,
                         std::vector<PerkIndexEntry>& outForSkill) {
    if (!node) return;
    if (!visited.insert(node).second) return;

    if (node->perk) {
        // Récupérer le nom (TESFullName)
        const char* rawName = node->perk->GetFullName();
        if (rawName && *rawName) {
            std::wstring wname = Trim(StripHtml(Utf8ToWString(rawName)));
            if (!wname.empty()) {
                PerkIndexEntry e;
                e.perk      = node->perk;
                e.node      = node;
                e.skill     = skill;
                e.nameLower = ToLowerW(wname);

                outForSkill.push_back(e);
                g_perkByName.emplace(e.nameLower, e);
                // Pour g_perkByPtr : on garde l'entrée qui PORTE le bon nœud.
                // Si plusieurs nœuds pointent vers le même perk (rare), on
                // garde le premier rencontré.
                g_perkByPtr.emplace(node->perk, e);
            }
        }
    }

    for (auto* child : node->children) {
        WalkPerkTree(child, skill, visited, outForSkill);
    }
}

// Construit l'index complet pour TOUTES les compétences listées dans le
// StatsMenu courant (skillTrees inclut Beast Skills si applicable).
static void BuildIndex() {
    g_perkByName.clear();
    g_perkByPtr.clear();
    g_perksBySkill.clear();
    g_perkIndexBuilt = false;

    auto* avList = RE::ActorValueList::GetSingleton();
    if (!avList) {
        LOG("[perks] BuildIndex: ActorValueList singleton null");
        return;
    }

    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::StatsMenu::MENU_NAME);
    if (!menu) {
        LOG("[perks] BuildIndex: StatsMenu not open");
        return;
    }
    auto* sm = static_cast<RE::StatsMenu*>(menu.get());
    if (!sm) return;

    auto& rd = sm->GetRuntimeData();
    if (rd.skillTrees.empty()) {
        LOG("[perks] BuildIndex: skillTrees array empty");
        return;
    }

    int totalPerks = 0;
    for (auto av : rd.skillTrees) {
        auto* avInfo = avList->GetActorValue(av);
        if (!avInfo) continue;
        if (!avInfo->perkTree) continue;

        std::vector<PerkIndexEntry> entries;
        std::set<RE::BGSSkillPerkTreeNode*> visited;
        WalkPerkTree(avInfo->perkTree, av, visited, entries);

        if (!entries.empty()) {
            totalPerks += (int)entries.size();
            g_perksBySkill.emplace(av, std::move(entries));
        }
    }

    g_perkIndexBuilt = true;
    LOG("[perks] BuildIndex done : {} skills, {} perks total", g_perksBySkill.size(), totalPerks);
}

static void ResetIndex() {
    g_perkByName.clear();
    g_perkByPtr.clear();
    g_perksBySkill.clear();
    g_perkIndexBuilt = false;
}

// --------------------------------------------------------------------
// Lookups
// --------------------------------------------------------------------

// Cherche un perk par son nom affiché. Si plusieurs perks ont le même nom
// (rare, mais possible avec mods), on PRÉFÈRE celui qui appartient à
// preferredSkill (la compétence courante du menu) si fourni.
static const PerkIndexEntry* FindPerkByDisplayName(const std::wstring& displayName,
                                                   RE::ActorValue preferredSkill = RE::ActorValue::kNone) {
    if (displayName.empty()) return nullptr;
    std::wstring key = ToLowerW(Trim(StripHtml(displayName)));
    if (key.empty()) return nullptr;

    auto range = g_perkByName.equal_range(key);
    if (range.first == range.second) return nullptr;

    // Préférence par skill
    if (preferredSkill != RE::ActorValue::kNone) {
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.skill == preferredSkill) return &it->second;
        }
    }
    return &range.first->second;
}

// --------------------------------------------------------------------
// État du joueur sur un perk
// --------------------------------------------------------------------

// Renvoie le rang acquis par le joueur sur ce perk (en suivant la chaîne
// nextPerk). 0 = pas appris, sinon nombre de rangs cumulés.
static std::int8_t GetPlayerRankOn(RE::BGSPerk* basePerk) {
    if (!basePerk) return 0;
    auto* pc = RE::PlayerCharacter::GetSingleton();
    if (!pc) return 0;

    std::int8_t rank = 0;
    auto* p = basePerk;
    while (p) {
        if (pc->HasPerk(p)) ++rank;
        else break;  // les rangs sont chaînés, on s'arrête au 1er manquant
        p = p->nextPerk;
    }
    return rank;
}

// Nombre TOTAL de rangs disponibles pour ce perk (suit nextPerk).
static std::int8_t GetTotalRanks(RE::BGSPerk* basePerk) {
    if (!basePerk) return 0;
    // PerkData.numRanks stocke déjà le total quand bien rempli, mais sur
    // certains mods c'est 0 ou 1 et la chaîne nextPerk est utilisée à la
    // place. On compte par sécurité.
    int n = 0;
    auto* p = basePerk;
    while (p) { ++n; p = p->nextPerk; }
    return (std::int8_t)n;
}

// Le joueur peut-il acheter le PROCHAIN rang de ce perk maintenant ?
//   missing : si non, contient une raison lisible ("level too low", "needs X")
struct CanLearnResult {
    bool          canLearn{false};
    std::wstring  reason;     // texte lisible si canLearn == false
};

static CanLearnResult CanLearnPerk(const PerkIndexEntry& e) {
    CanLearnResult r;
    auto* pc = RE::PlayerCharacter::GetSingleton();
    if (!pc || !e.perk) { r.reason = Utf8ToWString("indisponible"); return r; }

    // 1. Déjà au rang max ?
    std::int8_t curRank   = GetPlayerRankOn(e.perk);
    std::int8_t totalRank = GetTotalRanks(e.perk);
    if (totalRank > 0 && curRank >= totalRank) {
        r.reason = Utf8ToWString("already at max rank");
        return r;
    }

    // 2. Le perk en cours d'achat = celui de rang (curRank+1).
    //    On remonte la chaîne nextPerk de curRank pas pour trouver le bon.
    auto* nextRankPerk = e.perk;
    for (std::int8_t i = 0; i < curRank && nextRankPerk; ++i) {
        nextRankPerk = nextRankPerk->nextPerk;
    }
    if (!nextRankPerk) { r.reason = Utf8ToWString("no next rank"); return r; }

    // 3. Niveau de compétence
    if (e.skill != RE::ActorValue::kNone) {
        auto* avo = pc->AsActorValueOwner();
        if (avo) {
            float baseLvl = avo->GetBaseActorValue(e.skill);
            std::int8_t reqLvl = nextRankPerk->data.level;
            if (baseLvl < (float)reqLvl) {
                std::string s = "requires skill level ";
                s += std::to_string((int)reqLvl);
                s += ", you have ";
                s += std::to_string((int)baseLvl);
                r.reason = Utf8ToWString(s);
                return r;
            }
        }
    }

    // 4. Prérequis (parents) — pour le rang 1 uniquement (les rangs >1 ne
    //    dépendent que du rang précédent du même perk).
    if (curRank == 0 && e.node) {
        for (auto* parent : e.node->parents) {
            if (!parent || !parent->perk) continue;
            if (!pc->HasPerk(parent->perk)) {
                r.reason = Utf8ToWString("missing prerequisite: ");
                const char* pn = parent->perk->GetFullName();
                if (pn) r.reason += Utf8ToWString(pn);
                return r;
            }
        }
    }

    // 5. Conditions globales du perk
    if (nextRankPerk->perkConditions.head) {
        if (!nextRankPerk->perkConditions(pc, pc)) {
            r.reason = Utf8ToWString("conditions not met");
            return r;
        }
    }

    r.canLearn = true;
    return r;
}

// --------------------------------------------------------------------
// Type topologique du nœud (feuille / linéaire / embranchement)
// --------------------------------------------------------------------

enum class NodeShape {
    kLeaf,        // 0 enfants
    kLinear,      // 1 enfant
    kBranch,      // 2+ enfants
    kUnknown
};

static NodeShape GetNodeShape(const PerkIndexEntry& e) {
    if (!e.node) return NodeShape::kUnknown;
    // Compter les enfants qui ont effectivement un perk (les nœuds vides
    // ne nous intéressent pas pour le joueur).
    int n = 0;
    for (auto* c : e.node->children) {
        if (c && c->perk) ++n;
    }
    if (n == 0) return NodeShape::kLeaf;
    if (n == 1) return NodeShape::kLinear;
    return NodeShape::kBranch;
}

// Compte les enfants utiles (avec perk).
static int CountUsefulChildren(const PerkIndexEntry& e) {
    if (!e.node) return 0;
    int n = 0;
    for (auto* c : e.node->children) {
        if (c && c->perk) ++n;
    }
    return n;
}

// Récupère les enfants utiles (perks débloqués par celui-ci).
static std::vector<RE::BGSPerk*> GetChildPerks(const PerkIndexEntry& e) {
    std::vector<RE::BGSPerk*> out;
    if (!e.node) return out;
    for (auto* c : e.node->children) {
        if (c && c->perk) out.push_back(c->perk);
    }
    return out;
}

// Récupère les parents utiles (prérequis directs).
static std::vector<RE::BGSPerk*> GetParentPerks(const PerkIndexEntry& e) {
    std::vector<RE::BGSPerk*> out;
    if (!e.node) return out;
    for (auto* p : e.node->parents) {
        if (p && p->perk) out.push_back(p->perk);
    }
    return out;
}

// --------------------------------------------------------------------
// Lecture native depuis le perk (pas depuis le SWF) — synchrone, fiable.
// --------------------------------------------------------------------
//
// Ces deux fonctions remplacent la lecture des champs cardDesc/cardReq du
// SWF qui sont mis à jour de manière asynchrone par le moteur (80-300ms
// après que le perk centré change). Lues directement depuis BGSPerk*, les
// données sont valides instantanément dès qu'on a identifié le perk.

// Retourne la description native localisée du perk (lue depuis le STRINGS
// file via TESDescription::GetDescription, exactement comme le moteur le
// fait pour l'afficher dans la card du StatsMenu).
//
// Renvoie wstring vide si pas de description.
static std::wstring GetPerkDescriptionNative(RE::BGSPerk* perk) {
    if (!perk) return L"";

    RE::BSString desc;
    perk->GetDescription(desc, perk);  // 'CSED' par défaut (DESC record)

    if (desc.empty()) return L"";

    return Utf8ToWString(std::string(desc.c_str()));
}

// Retourne le niveau de compétence requis pour ce perk en parcourant ses
// conditions CTDA (méthode utilisée par Bethesda dans Skyrim — data.level
// est généralement 0 dans les records vanilla, le seuil est encodé dans
// une condition GetBaseActorValue).
//
// Renvoie -1 si rien de trouvé.
static int GetRequiredSkillLevelNative(RE::BGSPerk* perk, RE::ActorValue forSkill) {
    if (!perk) return -1;

    int bestLevel = -1;

    for (auto* item = perk->perkConditions.head; item != nullptr; item = item->next) {
        const auto& cond = item->data;
        const auto fn = cond.functionData.function.get();

        // On cible les 3 variantes ActorValue utilisées pour les prérequis
        // de niveau (Skyrim utilise quasi exclusivement GetBaseActorValue).
        if (fn != RE::FUNCTION_DATA::FunctionID::kGetActorValue &&
            fn != RE::FUNCTION_DATA::FunctionID::kGetBaseActorValue &&
            fn != RE::FUNCTION_DATA::FunctionID::kGetPermanentActorValue) {
            continue;
        }

        // params[0] = ActorValue encodé directement dans le pointeur
        // (convention Bethesda pour les conditions à argument numérique)
        auto av = static_cast<RE::ActorValue>(
            reinterpret_cast<std::intptr_t>(cond.functionData.params[0]));
        if (av != forSkill) continue;

        // Si flags.global == true, comparisonValue est un TESGlobal* ;
        // sinon c'est un float direct.
        float threshold = 0.0f;
        if (cond.flags.global) {
            if (!cond.comparisonValue.g) continue;
            threshold = cond.comparisonValue.g->value;
        } else {
            threshold = cond.comparisonValue.f;
        }

        // Garde la valeur la plus restrictive si plusieurs conditions matchent
        const int lvl = static_cast<int>(threshold);
        if (lvl > bestLevel) bestLevel = lvl;
    }

    // Fallback sur le champ DATA.level (rare en vanilla, mais possible)
    if (bestLevel < 0 && perk->data.level > 0) {
        bestLevel = (int)perk->data.level;
    }

    return bestLevel;
}

}  // namespace perks
