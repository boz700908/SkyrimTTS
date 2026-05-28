#pragma once

#include "loot_tracker.h"

// SCANNER D'OBJETS — Navigation par catégories pour joueurs aveugles

// --- Catégories ---
enum ScanCategory : int {
    kCatAll = 0,
    kCatNPCs,
    kCatDoors,
    kCatContainers,
    kCatItems,
    kCatActivators,
    kCatCorpses,
    kCatCompanions,
    kCatQuests,
    kCatLocations,
    kCatCOUNT
};

// Cles techniques anglaises (servent aussi de cle de traduction).
// Toujours en anglais : utilisees pour comparaisons et logs.
static const wchar_t* g_categoryNames[] = {
    L"All",
    L"NPCs",
    L"Doors",
    L"Containers",
    L"Items",
    L"Activators",
    L"Corpses",
    L"Companions",
    L"Quests",
    L"Locations"
};

// Helper : retourne le libelle traduit d'une categorie (pour vocalisation).
static std::wstring GetCategoryNameSpoken(int cat) {
    if (cat < 0 || cat >= static_cast<int>(sizeof(g_categoryNames) / sizeof(g_categoryNames[0])))
        return L"";
    // On passe le wchar_t* a travers std::string pour TR() qui prend une cle UTF-8.
    return TR(WStringToUtf8(g_categoryNames[cat]));
}

// --- Sous-catégories ---
// Les valeurs TypeA/TypeB sont utilisées pour les catégories génériques
// (containers, doors, corpses, activators). Les valeurs plus spécifiques
// sont pour la catégorie Items (weapons, armor, etc.).
enum class ScanSubcategory : int {
    All = 0,
    // Génériques (dépendent de la catégorie)
    TypeA,
    TypeB,
    // Items — sous-types
    ItemWeapons,
    ItemArmor,
    ItemPotions,
    ItemFood,
    ItemIngredients,
    ItemScrolls,
    ItemBooks,
    ItemSoulGems,
    ItemMisc,
    // Activators — sous-types spécifiques
    ActivatorDestructible,  // toiles d'araignée, barricades, racines, portes fragiles
    // NPCs — sous-types spécifiques
    NpcDialogue,   // PNJ avec qui on peut ouvrir le menu de dialogue
    NpcHostile,    // PNJ qui veulent nous tuer
    NpcMerchant,   // PNJ marchands actuellement en service
    COUNT
};

// --- Objet scanné ---
struct ScannedObject {
    RE::FormID  formID{0};
    std::wstring name;
    float       distance{0.0f};
    float       zDiff{0.0f};       // positif = au-dessus, négatif = en-dessous
    RE::NiPoint3 lastKnownPos{0, 0, 0};  // position en cache pour recalcul
    ScanCategory category{kCatAll};
    bool        locked{false};
    bool        empty{false};
    bool        dead{false};
    bool        isCellDoor{false};     // porte avec chargement de cellule
    bool        isFurniture{false};    // meuble (chaise, lit, etc.)
    bool        isCraftingStation{false};  // forge, enchanteur, alchimie, meule, tannerie, four, cookpot, atelier d'armurier
    // PNJ — flags pour le filtrage de la categorie NPCs.
    bool        npcCanTalk{false};     // CanTalkToPlayer() : ouvre une boite de dialogue
    bool        npcHostile{false};     // IsHostileToActor(player) : veut nous tuer
    bool        npcMerchant{false};    // GetVendorFaction() != null : marchand en service
    RE::FormType formType{RE::FormType::None};  // type Bethesda du base form (Weapon, Armor, AlchemyItem, etc.)
    std::wstring doorDestination;   // destination d'une porte (nom de la cellule)
    // Obstacles destructibles (toiles d'araignée, barricades, racines, portes
    // fragiles, etc.). isDestructible est stable pour un FormID donné (propriété
    // du base form), healthPercent peut diminuer à chaque scan si l'objet a été
    // partiellement endommagé. destructibleLabel pointe vers une chaîne statique
    // (pas de gestion de lifetime nécessaire).
    bool           isDestructible{false};
    int            destructibleHealthPercent{100};  // 100 = intact, 0 = détruit
    const wchar_t* destructibleLabel{nullptr};      // "Spider web", "Barricade", "Eldergleam root", etc.
};

// --- État global du scanner ---
// Table des symboles pour les portes à Griffe (anneaux)
struct DoorSymbols { RE::FormID keyhole; const char* s1; const char* s2; const char* s3; };
static const DoorSymbols g_doorSymbolTable[] = {
    {0x0004E2B9, "Bear", "Moth", "Owl"},         // Bleak Falls Barrow (Golden Claw)
    {0x000DB883, "Moth", "Owl", "Wolf"},          // Shroud Hearth Barrow (Sapphire Claw)
    {0x000F3986, "Wolf", "Hawk", "Wolf"},          // Dead Men's Respite
    {0x000E4ECE, "Hawk", "Hawk", "Dragon"},        // Folgunthur (Ivory Claw)
    {0x000B89F2, "Bear", "Whale", "Snake"},        // Reachwater Rock (Emerald Claw)
    {0x000A46D6, "Wolf", "Moth", "Dragon"},        // Korvanjund (Ebony Claw)
    {0x0007C536, "Fox", "Owl", "Snake"},           // Forelhost (Glass Claw)
    {0x000B634C, "Snake", "Wolf", "Moth"},         // Yngol Barrow (Coral Claw)
    {0x000FC2DC, "Fox", "Moth", "Dragon"},         // Skuldafn (Diamond Claw)
};

static std::vector<ScannedObject>  g_scannedAll;        // tous les objets scannés
static std::vector<ScannedObject*> g_scannedFiltered;   // filtrés par catégorie courante
static ScanCategory                g_scanCategory{kCatAll};
static ScanSubcategory             g_scanSubcategory{ScanSubcategory::All};
static int                         g_scanIndex{-1};
// Pas de rayon de scan — on scanne toutes les cellules chargées (comme FO4 Access)
static constexpr float             RESCAN_DISTANCE = 100.0f; // auto-rescan si joueur bouge de >100 unités
static RE::NiPoint3                g_lastScanPos{0, 0, 0};

// Cache d'analyse des scripts Papyrus attachés aux refs. Les scripts attachés à
// une ref sont stables à l'exécution (ils ne changent pas pendant une session
// de jeu), donc on peut mémoriser le résultat d'une analyse VM et la réutiliser
// à chaque scan. Gain énorme dans les donjons : avant le cache, on faisait 9
// FindBoundObject par activator x N activators par scan (pour détecter les Murs
// des Mots), plus jusqu'à 4 par pilier nommé. Avec le cache, on ne paie ce coût
// qu'UNE seule fois par FormID, puis on sait immédiatement quoi faire.
// L'état live du pilier (position01/02/03) continue d'être lu à chaque scan
// car lui change à l'exécution.
enum class ScriptTypeKind : uint8_t {
    Unknown = 0,                // Pas encore analysé
    NotSpecial,                 // Analysé : rien de spécial (pas un Mur ni un pilier)
    WordWall,                   // Mur des Mots (un des 9 scripts WordWallTrigger*)
    PillarStandard,             // defaultPuzzlePillarScript / DefaultPuzzlePillarScript
    PillarInt,                  // intPuzzlePillarScript
    RingHallOfStories,          // HallofStoriesDiskScript (anneaux de la Gorge du Monde)
    // --- Obstacles destructibles (scripts Papyrus connus) ---
    WebObstacle,                // toile d'araignée (MGRWebObstacleScript)
    WebEggSac,                  // sac d'œufs (MGREggSackScript)
    Barricade,                  // barricade guerre civile (CWBarricadeScript)
    EldergleamRoot,             // racine d'Aubéternel, nécessite Nettlebane (DA16EldergleamRootScript)
    BreakableDoor,              // porte fragile (defaultBreakableDoorSCRIPT)
    GenericDestructible         // destructible mais script non identifié (fallback Tier 1)
};
static std::unordered_map<RE::FormID, ScriptTypeKind> g_scriptTypeCache;

// --- Quêtes actives (lues depuis le journal GFx) ---
static std::set<RE::FormID> g_activeQuestFormIDs;    // FormIDs des quêtes traquées dans le journal
static bool                 g_questFilterInitialized{false}; // true après la première lecture du journal
static bool                 g_miscQuestsActive{false};       // true si "Divers" est coché dans le journal

// Lire les quêtes actives depuis le journal GFx quand il se ferme
static void ReadActiveQuestsFromJournal() {
    auto* ui = RE::UI::GetSingleton();
    if (!ui) {
        LOG("Scanner: ReadActiveQuests - UI singleton null");
        return;
    }
    auto journal = ui->GetMenu(RE::JournalMenu::MENU_NAME);
    if (!journal) {
        LOG("Scanner: ReadActiveQuests - journal menu null");
        return;
    }
    if (!journal->uiMovie) {
        LOG("Scanner: ReadActiveQuests - journal uiMovie null");
        return;
    }

    auto* movie = journal->uiMovie.get();
    LOG("Scanner: journal movie found, probing GFx paths...");

    // Probe plusieurs chemins possibles pour trouver la liste de quêtes
    const char* listPaths[] = {
        "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.TitleList_mc.List_mc.EntriesA",
        "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.TitleList_mc.List_mc.entryList",
        "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.TitleList_mc.EntriesA",
        "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.TitleList_mc.entryList",
    };

    RE::GFxValue questList;
    bool found = false;
    for (auto* path : listPaths) {
        if (SafeGetVariable(movie, questList, path) && SafeIsArray(questList)) {
            LOG("Scanner: journal quest list found at '{}'  size={}", path, SafeGetArraySize(questList));
            found = true;
            break;
        }
    }
    if (!found) {
        LOG("Scanner: journal GFx quest list not found in any path");
        return;
    }

    uint32_t count = SafeGetArraySize(questList);
    g_activeQuestFormIDs.clear();
    g_miscQuestsActive = false;  // reset, sera mis à true si une entrée Misc est active

    LOG("Scanner: journal list has {} entries, dumping all:", count);
    for (uint32_t i = 0; i < count; i++) {
        RE::GFxValue entry;
        if (!questList.GetElement(i, &entry) || !SafeIsObject(entry)) continue;

        RE::GFxValue activeVal, formIDVal;
        bool isActive = false;
        uint32_t formID = 0;

        if (entry.GetMember("active", &activeVal)) {
            isActive = SafeIsBool(activeVal) ? SafeGetBool(activeVal) : (SafeIsNumber(activeVal) && SafeGetNumber(activeVal) != 0.0);
        }
        if (entry.GetMember("formID", &formIDVal) && SafeIsNumber(formIDVal)) {
            formID = static_cast<uint32_t>(SafeGetNumber(formIDVal));
        }

        // Log chaque entrée
        RE::GFxValue textVal;
        std::string entryText = "?";
        if (entry.GetMember("text", &textVal) && SafeIsString(textVal)) entryText = SafeGetString(textVal);
        LOG("Scanner: journal entry[{}] text='{}' formID={:08X} active={}", i, entryText, formID, isActive);

        if (formID == 0) {
            // Entrée "Divers" — formID=0
            if (isActive) g_miscQuestsActive = true;
            LOG("Scanner: journal Misc entry active={}", isActive);
        } else if (isActive) {
            g_activeQuestFormIDs.insert(formID);
            LOG("Scanner: journal quest active FormID={:08X}", formID);
        }
    }

    // Lire les quêtes misc individuelles depuis objectiveList
    // Quand "Divers" est dans le journal, objectiveList contient les quêtes misc avec leurs vrais formIDs
    RE::GFxValue objList;
    if (SafeGetVariable(movie, objList, "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.objectiveList.entryList") && SafeIsArray(objList)) {
        uint32_t objCount = SafeGetArraySize(objList);
        for (uint32_t i = 0; i < objCount; i++) {
            RE::GFxValue objEntry;
            if (!objList.GetElement(i, &objEntry) || !SafeIsObject(objEntry)) continue;

            RE::GFxValue objFormIDVal, objActiveVal;
            uint32_t objFormID = 0;
            bool objActive = false;

            if (objEntry.GetMember("formID", &objFormIDVal) && SafeIsNumber(objFormIDVal))
                objFormID = static_cast<uint32_t>(SafeGetNumber(objFormIDVal));
            if (objEntry.GetMember("active", &objActiveVal))
                objActive = SafeIsBool(objActiveVal) ? SafeGetBool(objActiveVal) : (SafeIsNumber(objActiveVal) && SafeGetNumber(objActiveVal) != 0.0);

            // formID != 0 = quête misc individuelle (formID=0 serait l'entrée Divers elle-même)
            if (objFormID != 0 && objActive) {
                g_activeQuestFormIDs.insert(objFormID);
                RE::GFxValue objTextVal;
                std::string objText = "?";
                if (objEntry.GetMember("text", &objTextVal) && SafeIsString(objTextVal)) objText = SafeGetString(objTextVal);
                LOG("Scanner: misc quest active FormID={:08X} text='{}'", objFormID, objText);
            }
        }
    }

    g_questFilterInitialized = true;
    LOG("Scanner: {} active quests read from journal, misc={}", g_activeQuestFormIDs.size(), g_miscQuestsActive);
}

static bool IsDragon(RE::Actor* actor) {
    return actor && actor->HasKeywordString("ActorTypeDragon");
}

// --- Detection station de craft ---
// Une station de craft est techniquement une Furniture avec :
//   - soit workBenchData.benchType != kNone (couvre meule, atelier d'armurier,
//     enchanteur, alchimie, et leurs variantes "experiment")
//   - soit un keyword vanilla : CraftingSmithingForge, CraftingCookpot,
//     CraftingSmelter, CraftingTanningRack (couvre forge, cookpot, smelter,
//     tanning rack — vanilla et Hearthfire/Dragonborn confondus).
//
// On detecte les keywords via HasKeywordString sur le base form : c'est
// l'API native du moteur, marche partout, et evite l'assertion buggee dans
// BGSDefaultObjectManager::GetObject(DefaultObjectID) qui plante en debug
// multi-target (l'assert verifie la valeur encodee SE|VR au lieu de l'index
// decode, donc tout DefaultObjectID > 183 fait sauter l'assertion).
static bool IsCraftingStation(RE::TESObjectREFR& ref) {
    auto* base = ref.GetBaseObject();
    if (!base) return false;
    auto* furn = base->As<RE::TESFurniture>();
    if (!furn) return false;

    // 1) Workbench data (meule, atelier d'armurier, enchanteur, alchimie et
    //    leurs variantes Experiment).
    if (furn->workBenchData.benchType.get() != RE::TESFurniture::WorkBenchData::BenchType::kNone) {
        return true;
    }

    // 2) Keywords vanilla (forge, cookpot, smelter, tanning rack). Les editor
    //    IDs sont stables entre toutes les versions de Skyrim et Hearthfire/
    //    Dragonborn reutilisent les memes.
    if (furn->HasKeywordString("CraftingSmithingForge")) return true;
    if (furn->HasKeywordString("CraftingCookpot"))       return true;
    if (furn->HasKeywordString("CraftingSmelter"))       return true;
    if (furn->HasKeywordString("CraftingTanningRack"))   return true;

    return false;
}

// --- Déterminer la catégorie d'une référence ---
static ScanCategory CategorizeRef(RE::TESObjectREFR& ref) {
    // Acteur ?
    if (auto* actor = ref.As<RE::Actor>()) {
        if (actor->IsDead()) return kCatCorpses;
        if (actor->IsPlayerTeammate()) return kCatCompanions;
        return kCatNPCs;
    }

    auto* base = ref.GetBaseObject();
    if (!base) return kCatAll;

    auto type = base->GetFormType();

    if (type == RE::FormType::Door) return kCatDoors;
    if (type == RE::FormType::Container) return kCatContainers;
    if (type == RE::FormType::Furniture || type == RE::FormType::Activator)
        return kCatActivators;

    // Cas particulier : "Pousse du Primarbor" (Eldergleam root, base 0x1CB81)
    // est un FormType::Tree (32) en vanilla. On la categorise comme Activator
    // pour qu'elle apparaisse dans le sous-filtre Destructibles du scanner.
    if (base->GetFormID() == 0x0001CB81)
        return kCatActivators;

    // Items au sol
    if (type == RE::FormType::Weapon || type == RE::FormType::Armor ||
        type == RE::FormType::AlchemyItem || type == RE::FormType::Ingredient ||
        type == RE::FormType::Book || type == RE::FormType::KeyMaster ||
        type == RE::FormType::Scroll || type == RE::FormType::Ammo ||
        type == RE::FormType::Misc || type == RE::FormType::SoulGem ||
        type == RE::FormType::Flora || type == RE::FormType::Light)
        return kCatItems;

    return kCatAll;
}

// --- Sous-catégories : quelles catégories en ont ---
static bool HasSubcategories(ScanCategory cat) {
    return cat == kCatAll || cat == kCatActivators || cat == kCatContainers ||
           cat == kCatDoors || cat == kCatCorpses || cat == kCatItems ||
           cat == kCatNPCs;
}

// --- Liste ordonnée des sous-catégories applicables à une catégorie ---
// Le cycle (touche +/- pour avancer dans les sous-cats) utilise cette liste.
static const std::vector<ScanSubcategory>& GetSubcategoriesFor(ScanCategory cat) {
    static const std::vector<ScanSubcategory> empty = {ScanSubcategory::All};
    static const std::vector<ScanSubcategory> twoTypes = {
        ScanSubcategory::All, ScanSubcategory::TypeA, ScanSubcategory::TypeB
    };
    static const std::vector<ScanSubcategory> activatorTypes = {
        ScanSubcategory::All,
        ScanSubcategory::TypeA,
        ScanSubcategory::TypeB,
        ScanSubcategory::ActivatorDestructible
    };
    static const std::vector<ScanSubcategory> npcTypes = {
        ScanSubcategory::All,
        ScanSubcategory::NpcDialogue,
        ScanSubcategory::NpcHostile,
        ScanSubcategory::NpcMerchant
    };
    static const std::vector<ScanSubcategory> items = {
        ScanSubcategory::All,
        ScanSubcategory::ItemWeapons,
        ScanSubcategory::ItemArmor,
        ScanSubcategory::ItemPotions,
        ScanSubcategory::ItemFood,
        ScanSubcategory::ItemIngredients,
        ScanSubcategory::ItemScrolls,
        ScanSubcategory::ItemBooks,
        ScanSubcategory::ItemSoulGems,
        ScanSubcategory::ItemMisc
    };
    // kCatAll et kCatItems partagent les memes sous-filtres items.
    // Difference : dans kCatAll le sous-filtre "All" affiche TOUT (pas juste items),
    // alors que dans kCatItems il n'y a que des items de toute facon.
    if (cat == kCatItems || cat == kCatAll) return items;
    if (cat == kCatActivators) return activatorTypes;
    if (cat == kCatNPCs) return npcTypes;
    if (cat == kCatContainers || cat == kCatDoors || cat == kCatCorpses)
        return twoTypes;
    return empty;
}

// --- Nom de la sous-catégorie (dépend de la catégorie courante, traduit) ---
static std::wstring GetSubcategoryName(ScanSubcategory sub) {
    if (g_scanCategory == kCatContainers) {
        switch (sub) {
            case ScanSubcategory::All: return TR("All");
            case ScanSubcategory::TypeA: return TR("Non-empty");
            case ScanSubcategory::TypeB: return TR("Empty");
            default: return L"?";
        }
    } else if (g_scanCategory == kCatDoors) {
        switch (sub) {
            case ScanSubcategory::All: return TR("All");
            case ScanSubcategory::TypeA: return TR("Locked");
            case ScanSubcategory::TypeB: return TR("Cell doors");
            default: return L"?";
        }
    } else if (g_scanCategory == kCatCorpses) {
        switch (sub) {
            case ScanSubcategory::All: return TR("All");
            case ScanSubcategory::TypeA: return TR("Unlooted");
            case ScanSubcategory::TypeB: return TR("Looted");
            default: return L"?";
        }
    } else if (g_scanCategory == kCatActivators) {
        switch (sub) {
            case ScanSubcategory::All: return TR("All");
            case ScanSubcategory::TypeA: return TR("Crafting");
            case ScanSubcategory::TypeB: return TR("Other");
            case ScanSubcategory::ActivatorDestructible: return TR("Destructible");
            default: return L"?";
        }
    } else if (g_scanCategory == kCatNPCs) {
        switch (sub) {
            case ScanSubcategory::All:         return TR("All");
            case ScanSubcategory::NpcDialogue: return TR("Dialogue");
            case ScanSubcategory::NpcHostile:  return TR("Hostile");
            case ScanSubcategory::NpcMerchant: return TR("Merchants");
            default: return L"?";
        }
    } else if (g_scanCategory == kCatItems || g_scanCategory == kCatAll) {
        // kCatAll partage les memes sous-filtres items que kCatItems.
        // Dans kCatAll, "All" affiche tous les objets (pas juste items).
        // Dans kCatItems, "All" n'affiche que les items.
        switch (sub) {
            case ScanSubcategory::All:             return TR("All");
            case ScanSubcategory::ItemWeapons:     return TR("Weapons");
            case ScanSubcategory::ItemArmor:       return TR("Armor");
            case ScanSubcategory::ItemPotions:     return TR("Potions");
            case ScanSubcategory::ItemFood:        return TR("Food");
            case ScanSubcategory::ItemIngredients: return TR("Ingredients");
            case ScanSubcategory::ItemScrolls:     return TR("Scrolls");
            case ScanSubcategory::ItemBooks:       return TR("Books");
            case ScanSubcategory::ItemSoulGems:    return TR("Soul Gems");
            case ScanSubcategory::ItemMisc:        return TR("Miscellaneous");
            default: return L"?";
        }
    }
    return TR("All");
}

// --- Vérifier si un objet correspond à la sous-catégorie ---
static bool MatchesSubcategory(const ScannedObject& obj) {
    if (g_scanSubcategory == ScanSubcategory::All) return true;

    if (g_scanCategory == kCatContainers) {
        if (g_scanSubcategory == ScanSubcategory::TypeA) return !obj.empty;
        if (g_scanSubcategory == ScanSubcategory::TypeB) return obj.empty;
    } else if (g_scanCategory == kCatDoors) {
        if (g_scanSubcategory == ScanSubcategory::TypeA) return obj.locked;
        if (g_scanSubcategory == ScanSubcategory::TypeB) return obj.isCellDoor;
    } else if (g_scanCategory == kCatCorpses) {
        // Sous-filtre cadavres : base sur l'etat "deja fouille par le joueur"
        // (via LootTracker) plutot que sur "conteneur vide" (obj.empty).
        // Un cadavre peut etre non-vide mais deja fouille (le joueur a laisse
        // des objets sans valeur) — il compte comme "Looted". Inversement un
        // cadavre vide peut ne jamais avoir ete ouvert (mort custom, event) —
        // il compte comme "Unlooted".
        bool looted = LootTracker::GetSingleton()->IsLooted(obj.formID);
        if (g_scanSubcategory == ScanSubcategory::TypeA) return !looted;  // Unlooted
        if (g_scanSubcategory == ScanSubcategory::TypeB) return looted;   // Looted
    } else if (g_scanCategory == kCatActivators) {
        if (g_scanSubcategory == ScanSubcategory::TypeA) return obj.isCraftingStation;
        // "Other" : tout ce qui n'est PAS une station de craft (leviers, chaines,
        // mais aussi les meubles ordinaires : chaises, lits, etabli a cire, etc.).
        if (g_scanSubcategory == ScanSubcategory::TypeB) return !obj.isCraftingStation;
        if (g_scanSubcategory == ScanSubcategory::ActivatorDestructible) return obj.isDestructible;
    } else if (g_scanCategory == kCatNPCs) {
        if (g_scanSubcategory == ScanSubcategory::NpcDialogue) return obj.npcCanTalk;
        if (g_scanSubcategory == ScanSubcategory::NpcHostile)  return obj.npcHostile;
        if (g_scanSubcategory == ScanSubcategory::NpcMerchant) return obj.npcMerchant;
    } else if (g_scanCategory == kCatItems || g_scanCategory == kCatAll) {
        // Sous-filtres items communs a kCatItems et kCatAll.
        // Dans kCatAll + sous-filtre item : seuls les items correspondants sont
        // retenus (les PNJ/portes/conteneurs/etc. sont filtres out). Le cas
        // "ScanSubcategory::All" est deja renvoye true tout en haut de la fonction,
        // donc en mode kCatAll "All" on voit bien tous les types.
        // AlchemyItem couvre potions ET nourriture — on distingue via IsFood().
        switch (g_scanSubcategory) {
            case ScanSubcategory::ItemWeapons:     return obj.formType == RE::FormType::Weapon || obj.formType == RE::FormType::Ammo;
            case ScanSubcategory::ItemArmor:       return obj.formType == RE::FormType::Armor;
            case ScanSubcategory::ItemPotions: {
                if (obj.formType != RE::FormType::AlchemyItem) return false;
                auto* form = RE::TESForm::LookupByID(obj.formID);
                auto* ref = form ? form->AsReference() : nullptr;
                auto* base = ref ? ref->GetBaseObject() : nullptr;
                auto* alch = base ? base->As<RE::AlchemyItem>() : nullptr;
                return alch && !alch->IsFood();
            }
            case ScanSubcategory::ItemFood: {
                if (obj.formType != RE::FormType::AlchemyItem) return false;
                auto* form = RE::TESForm::LookupByID(obj.formID);
                auto* ref = form ? form->AsReference() : nullptr;
                auto* base = ref ? ref->GetBaseObject() : nullptr;
                auto* alch = base ? base->As<RE::AlchemyItem>() : nullptr;
                return alch && alch->IsFood();
            }
            case ScanSubcategory::ItemIngredients: return obj.formType == RE::FormType::Ingredient;
            case ScanSubcategory::ItemScrolls:     return obj.formType == RE::FormType::Scroll;
            case ScanSubcategory::ItemBooks:       return obj.formType == RE::FormType::Book;
            case ScanSubcategory::ItemSoulGems:    return obj.formType == RE::FormType::SoulGem;
            case ScanSubcategory::ItemMisc:        return obj.formType == RE::FormType::Misc ||
                                                          obj.formType == RE::FormType::KeyMaster ||
                                                          obj.formType == RE::FormType::Light ||
                                                          obj.formType == RE::FormType::Flora;
            default: break;
        }
    }
    return true;
}

// --- Filtrer les résultats par catégorie ---
static void ApplyCategoryFilter() {
    // Retenir l'objet courant (formID + catégorie) pour le retrouver après filtrage.
    // On sauvegarde la catégorie aussi car un même formID peut apparaître deux fois
    // en mode "All" (ex: un cadavre est à la fois cible de quête ET conteneur).
    RE::FormID currentFormID = 0;
    ScanCategory currentCategory = kCatAll;
    if (g_scanIndex >= 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
        currentFormID = g_scannedFiltered[g_scanIndex]->formID;
        currentCategory = g_scannedFiltered[g_scanIndex]->category;
    }

    g_scannedFiltered.clear();
    for (auto& obj : g_scannedAll) {
        if (obj.formID == 0) continue;  // objet invalidé (ramassé/supprimé)
        // En mode "All" : exclure les objectifs de quete (ils sont accessibles
        // uniquement via la categorie Quests dediee, pour ne pas polluer la liste
        // generale d'exploration). Meme logique pourrait s'appliquer aux Locations
        // mais on les garde car le joueur s'attend a voir les lieux dans All.
        if (g_scanCategory == kCatAll && obj.category == kCatQuests) continue;
        if (g_scanCategory == kCatAll || obj.category == g_scanCategory) {
            // Toujours appeler MatchesSubcategory (meme en kCatAll) pour que les
            // sous-filtres items (Weapons, Armor, etc.) s'appliquent aussi en All.
            // Quand g_scanSubcategory == All, MatchesSubcategory retourne true
            // immediatement (short-circuit en tete), donc kCatAll + All affiche tout.
            if (MatchesSubcategory(obj)) {
                g_scannedFiltered.push_back(&obj);
            }
        }
    }

    // Essayer de retrouver l'objet courant
    g_scanIndex = g_scannedFiltered.empty() ? -1 : 0;
    if (currentFormID != 0) {
        // Passe 1 : match strict (formID + catégorie) — évite de rester bloqué sur
        // la première copie quand un même formID a deux entrées dans la liste
        int foundIdx = -1;
        for (int i = 0; i < static_cast<int>(g_scannedFiltered.size()); i++) {
            if (g_scannedFiltered[i]->formID == currentFormID &&
                g_scannedFiltered[i]->category == currentCategory) {
                foundIdx = i;
                break;
            }
        }
        // Passe 2 (fallback) : match par formID seul, au cas où l'objet aurait
        // été recatégorisé entre-temps (PNJ mort, etc.)
        if (foundIdx < 0) {
            for (int i = 0; i < static_cast<int>(g_scannedFiltered.size()); i++) {
                if (g_scannedFiltered[i]->formID == currentFormID) {
                    foundIdx = i;
                    break;
                }
            }
        }
        if (foundIdx >= 0) g_scanIndex = foundIdx;
    }
}

// --- Résoudre les <Alias=XXX> dans le texte d'un objectif de quête ---
// Remplace les balises d'alias d'une quete par le nom reel du PNJ/lieu/item.
// Le moteur Skyrim utilise plusieurs formats dans le displayText des objectifs :
//   <Alias=Name>             : alias standard, on remplace par le nom complet
//   <alias=Name>             : meme chose en minuscules (utilise par certaines
//                              quetes modees, ex "La soeur de Belethor")
//   <Alias.ShortName=Name>   : variante "nom court" (vanilla, ex "Sels de givre
//                              de Dravynea"). On utilise le meme nom — le
//                              moteur, lui, extrait probablement le prenom.
//   <Alias.X=Name>           : autres suffixes (.Race, .Type, etc.) — meme
//                              traitement, on prend le nom du ref.
//
// L'ancienne implementation cherchait litteralement "<Alias=" (case-sensitive,
// sans suffixe), donc tous les autres formats laissaient un texte brut comme
// "Tuez <Alias.ShortName=QuestGiver>" en sortie — d'ou les annonces foireuses
// du scanner ("apportez machin truc alias short name questgiver").
static std::string ResolveQuestAliases(const std::string& text, RE::TESQuest* quest) {
    std::string result = text;
    size_t pos = 0;
    while (pos < result.size()) {
        // Cherche le prochain "<Alias" ou "<alias" (case-insensitive sur le mot
        // "Alias", mais on garde la position de "<").
        size_t openPos = std::string::npos;
        for (size_t i = pos; i + 6 < result.size(); ++i) {
            if (result[i] != '<') continue;
            char c1 = result[i + 1];
            if (c1 != 'A' && c1 != 'a') continue;
            // Comparer "lias" en case-insensitive
            if ((result[i + 2] == 'l' || result[i + 2] == 'L') &&
                (result[i + 3] == 'i' || result[i + 3] == 'I') &&
                (result[i + 4] == 'a' || result[i + 4] == 'A') &&
                (result[i + 5] == 's' || result[i + 5] == 'S')) {
                openPos = i;
                break;
            }
        }
        if (openPos == std::string::npos) break;

        size_t end = result.find('>', openPos);
        if (end == std::string::npos) break;

        // Trouver le '=' a l'interieur de la balise. Tout ce qui est apres
        // est le nom de l'alias ; ce qui est entre "Alias" et "=" est un
        // eventuel suffixe (.ShortName, .Race, etc.) qu'on ignore pour le
        // remplacement.
        size_t eq = result.find('=', openPos);
        std::string aliasName;
        if (eq != std::string::npos && eq < end) {
            aliasName = result.substr(eq + 1, end - eq - 1);
        }

        // Chercher l'alias dans la quete
        std::string replacement = aliasName;  // fallback : nom brut de l'alias
        if (!aliasName.empty() && quest) {
            for (auto* alias : quest->aliases) {
                if (!alias) continue;
                if (alias->aliasName == RE::BSFixedString(aliasName.c_str())) {
                    auto* refAlias = skyrim_cast<RE::BGSRefAlias*>(alias);
                    if (refAlias) {
                        auto* ref = refAlias->GetReference();
                        if (ref) {
                            const char* name = ref->GetDisplayFullName();
                            if (name && *name) {
                                replacement = name;
                            }
                        }
                    }
                    break;
                }
            }
        }

        result.replace(openPos, end - openPos + 1, replacement);
        pos = openPos + replacement.size();
    }
    return result;
}

// --- Vérifier si le joueur a bougé assez pour rescanner ---
static bool NeedsRescan() {
    if (g_scannedAll.empty()) return true;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return true;
    auto diff = player->GetPosition() - g_lastScanPos;
    return diff.Length() > RESCAN_DISTANCE;
}

// --- Direction cardinale (8 directions) ---
static std::wstring GetObjectDirection(const ScannedObject& obj) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return L"";
    if (obj.lastKnownPos.x == 0 && obj.lastKnownPos.y == 0) return L"";

    auto playerPos = player->GetPosition();
    float dx = obj.lastKnownPos.x - playerPos.x;
    float dy = obj.lastKnownPos.y - playerPos.y;
    float angle = std::atan2(dx, dy) * 180.0f / 3.14159265f;
    if (angle < 0) angle += 360.0f;

    if (angle >= 337.5f || angle < 22.5f)   return TR("north");
    if (angle >= 22.5f  && angle < 67.5f)   return TR("northeast");
    if (angle >= 67.5f  && angle < 112.5f)  return TR("east");
    if (angle >= 112.5f && angle < 157.5f)  return TR("southeast");
    if (angle >= 157.5f && angle < 202.5f)  return TR("south");
    if (angle >= 202.5f && angle < 247.5f)  return TR("southwest");
    if (angle >= 247.5f && angle < 292.5f)  return TR("west");
    return TR("northwest");
}

// Helper : renvoie ", above" / ", below" / "" selon l'ecart vertical entre deux
// positions. Seuil 256 unites (~1 etage Skyrim), meme valeur que dans
// FormatObjectAnnounce. Utilise par la visee auto et l'annonce des ennemis.
static std::wstring FormatElevationSuffix(float zDiff) {
    if (zDiff > 256.0f) return L", " + TR("above");
    if (zDiff < -256.0f) return L", " + TR("below");
    return L"";
}

// --- Formater l'annonce d'un objet ---
static std::wstring FormatObjectAnnounce(const ScannedObject& obj) {
    std::wstring msg = obj.name;

    if (!obj.doorDestination.empty()) msg += L", " + TR("to") + L" " + obj.doorDestination;
    if (obj.locked) msg += L", " + TR("locked");
    if (obj.empty) msg += L", " + TR("empty");

    // Flag "deja fouille par le joueur" : s'applique aux conteneurs et cadavres.
    // Indique au joueur qu'il a deja ouvert ce conteneur/cadavre au moins une
    // fois, meme s'il n'en a rien pris (utile pour ne pas le re-visiter
    // inutilement). Independant de obj.empty (qui regarde le contenu actuel).
    // Voir src/loot_tracker.h pour la logique de persistance et respawn.
    if ((obj.category == kCatContainers || obj.category == kCatCorpses) &&
        LootTracker::GetSingleton()->IsLooted(obj.formID)) {
        msg += L", " + TR("looted");
    }

    // Flag destructible : on ajoute le label ("Spider web", "Barricade", etc.)
    // seulement si différent du nom affiché (pour éviter "Spider web, spider web").
    // Puis la santé restante si < 100% (intact = pas d'info = plein par défaut).
    if (obj.isDestructible && obj.destructibleLabel) {
        // Cle anglaise stable -> traduction au moment de l'annonce.
        std::wstring lblEn = obj.destructibleLabel;
        std::wstring lbl = TR(WStringToUtf8(lblEn));
        // Comparaison insensible à la casse pour éviter la redondance
        auto toLower = [](std::wstring s) {
            for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
            return s;
        };
        if (toLower(obj.name).find(toLower(lbl)) == std::wstring::npos) {
            msg += L", " + lbl;
        } else {
            msg += L", " + TR("destructible");
        }
        if (obj.destructibleHealthPercent < 100) {
            msg += L" " + std::to_wstring(obj.destructibleHealthPercent) + L"%";
        }
    }

    msg += L", " + std::to_wstring(static_cast<int>(obj.distance)) + L" " + TR("units");

    // Direction seulement pour les piliers puzzle
    std::wstring nameCheck = obj.name;
    if (nameCheck.find(L"Pilier") != std::wstring::npos || nameCheck.find(L"Pillar") != std::wstring::npos) {
        std::wstring dir = GetObjectDirection(obj);
        if (!dir.empty()) msg += L" " + dir;
    }

    if (obj.zDiff > 256.0f) msg += L", " + TR("above");
    else if (obj.zDiff < -256.0f) msg += L", " + TR("below");

    return msg;
}

// Forward declarations
static void ScannerNextCategory();
static void ScannerPrevCategory();
static void ScannerNextCategoryImpl();
static void ScannerPrevCategoryImpl();
static void ScannerNextObject();
static void ScannerPrevObject();
static void ScannerNextObjectImpl();
static void ScannerPrevObjectImpl();

// Callback après scan automatique
enum ScanAction { kScanOnly, kScanThenNextCat, kScanThenPrevCat, kScanThenNextObj, kScanThenPrevObj };
static ScanAction g_pendingScanAction{kScanOnly};

// Détermine le type de script spécial attaché à une référence (Mur des Mots,
// Pilier puzzle, Anneau de la Gorge du Monde, ou rien de spécial). Utilise
// g_scriptTypeCache : une seule requête VM par FormID pour toute la session,
// ensuite c'est un simple lookup hash map. Les requêtes VM sont coûteuses
// (~100μs chacune) et sans cache on en faisait 9 par activator à chaque scan,
// ce qui donnait un gros freeze en donjons.
//
// IMPORTANT : on ne retourne PAS l'état live du pilier ici (position01/02/03) ;
// ça doit être lu à chaque scan car ça change à l'exécution. Ici on ne dit que
// "quelle FAMILLE de script est attachée", et c'est stable.
static ScriptTypeKind GetScriptTypeCached(RE::TESObjectREFR& ref) {
    RE::FormID fid = ref.GetFormID();
    auto it = g_scriptTypeCache.find(fid);
    if (it != g_scriptTypeCache.end()) return it->second;

    ScriptTypeKind result = ScriptTypeKind::NotSpecial;
    auto* vm = RE::SkyrimVM::GetSingleton();
    if (vm && vm->impl) {
        auto* policy = vm->impl->GetObjectHandlePolicy();
        if (policy) {
            auto vmH = policy->GetHandleForObject(
                static_cast<RE::VMTypeID>(RE::FormType::Reference), &ref);
            RE::BSTSmartPointer<RE::BSScript::Object> scriptObj;

            // 1) Mur des Mots : 9 variantes possibles selon les DLCs et addons
            static constexpr const char* kWordWallScripts[] = {
                "WordWallTriggerScript", "WordWallTriggerBleakFallsScript",
                "WordWallTrigger02Script", "DLC2WordWallTriggerScript",
                "DLC1WordWallTriggerScript", "DBSanctuaryWordWallTriggerScript",
                "DLC2WordWallTrigger02Script", "DLC2WordWallTriggerBendWillScript",
                "DLC1WordWallTrigger02Script"
            };
            for (auto* ws : kWordWallScripts) {
                if (vm->impl->FindBoundObject(vmH, ws, scriptObj) && scriptObj) {
                    result = ScriptTypeKind::WordWall;
                    break;
                }
            }

            // 2) Pilier standard / int / anneau (seulement si pas déjà identifié)
            if (result == ScriptTypeKind::NotSpecial) {
                if ((vm->impl->FindBoundObject(vmH, "defaultPuzzlePillarScript", scriptObj) && scriptObj) ||
                    (vm->impl->FindBoundObject(vmH, "DefaultPuzzlePillarScript", scriptObj) && scriptObj)) {
                    result = ScriptTypeKind::PillarStandard;
                } else if (vm->impl->FindBoundObject(vmH, "intPuzzlePillarScript", scriptObj) && scriptObj) {
                    result = ScriptTypeKind::PillarInt;
                } else if (vm->impl->FindBoundObject(vmH, "HallofStoriesDiskScript", scriptObj) && scriptObj) {
                    result = ScriptTypeKind::RingHallOfStories;
                }
            }

            // 3) Obstacles destructibles : toile, œufs, barricade, racine, porte fragile.
            // On teste plusieurs casings pour chaque nom (les scripts Papyrus vanilla
            // mélangent minuscules/majuscules selon la version du Creation Kit utilisée).
            if (result == ScriptTypeKind::NotSpecial) {
                auto try2 = [&](const char* a, const char* b) {
                    return (vm->impl->FindBoundObject(vmH, a, scriptObj) && scriptObj) ||
                           (vm->impl->FindBoundObject(vmH, b, scriptObj) && scriptObj);
                };
                if (try2("MGRWebObstacleScript", "mgrwebobstaclescript")) {
                    result = ScriptTypeKind::WebObstacle;
                } else if (try2("MGREggSackScript", "mgreggsackscript")) {
                    result = ScriptTypeKind::WebEggSac;
                } else if (try2("CWBarricadeScript", "cwbarricadescript")) {
                    result = ScriptTypeKind::Barricade;
                } else if (try2("T03EldergleamRootScript", "t03eldergleamrootscript")) {
                    // Nom de script vanilla CONFIRME : T03EldergleamRootScript
                    // (le prefixe T03 vient du nom interne du donjon Eldergleam
                    // Sanctuary, pas de DA16 qui etait une erreur). Cf script
                    // T03EldergleamRootScript.psc dans Skyrim.esm scripts.
                    // Verifie via repo digital-apple/TESVScripts 2026-05.
                    result = ScriptTypeKind::EldergleamRoot;
                } else if (try2("defaultBreakableDoorSCRIPT", "defaultBreakableDoorScript")) {
                    result = ScriptTypeKind::BreakableDoor;
                }
            }
        }
    }

    g_scriptTypeCache[fid] = result;
    return result;
}

// Retourne le label utilisateur pour un ScriptTypeKind destructible.
// NotSpecial et Unknown renvoient nullptr.
static const wchar_t* GetDestructibleLabel(ScriptTypeKind stk) {
    switch (stk) {
        case ScriptTypeKind::WebObstacle:          return L"Spider web";
        case ScriptTypeKind::WebEggSac:            return L"Spider egg sac";
        case ScriptTypeKind::Barricade:            return L"Wooden barricade";
        case ScriptTypeKind::EldergleamRoot:       return L"Eldergleam root";
        case ScriptTypeKind::BreakableDoor:        return L"Breakable door";
        case ScriptTypeKind::GenericDestructible:  return L"Destructible obstacle";
        default:                                    return nullptr;
    }
}

// Détecte si un base form a une "destruction data" (record DEST + stages).
// C'est le Tier 1 universel : capture tous les objets cassables, même ceux
// dont on ne connaît pas le script Papyrus spécifique.
static bool BaseIsDestructible(RE::TESBoundObject* base) {
    if (!base) return false;
    auto* dest = base->As<RE::BGSDestructibleObjectForm>();
    return dest && dest->data && dest->data->numStages > 0;
}

// Retourne le pourcentage de santé restante d'un ref destructible (0-100).
// Regarde d'abord ExtraObjectHealth (présent seulement si l'objet a été endommagé),
// sinon suppose 100% (intact).
static int ReadDestructibleHealthPercent(RE::TESObjectREFR& ref, RE::TESBoundObject* base) {
    // kDestroyed flag (bit 23) = détruit pour de bon
    if (ref.GetFormFlags() & RE::TESForm::RecordFlags::kDestroyed) return 0;
    auto* eh = ref.extraList.GetByType<RE::ExtraObjectHealth>();
    if (!eh) return 100;  // pas de dégât pris = plein
    auto* dest = base ? base->As<RE::BGSDestructibleObjectForm>() : nullptr;
    if (!dest || !dest->data || dest->data->health <= 0) return 100;
    float ratio = eh->health / static_cast<float>(dest->data->health);
    if (ratio > 1.0f) ratio = 1.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    return static_cast<int>(ratio * 100.0f + 0.5f);
}

// =============================================================================
// GetStablePosition : renvoie une position stable pour un ref, utilisable pour
// un calcul de distance fiable.
//
// Probleme : ref->GetPosition() renvoie le centre de masse Havok, qui bouge a
// chaque frame pour les objets physiques (epees jetees qui rebondissent, roulent,
// s'enfoncent dans le sol). La distance affichee peut sauter de 500 a 3000 sans
// que le joueur bouge.
//
// Solution : si le ref a son 3D charge et un worldBound valide, on utilise le
// centre du bounding box visuel (node->worldBound.center) — stable car aligne
// sur le mesh affiche, pas sur le centre de masse Havok. Sinon fallback sur
// GetPosition() (objets non charges, acteurs sans mesh, etc).
//
// Style f4access : position stable = distance stable.
// =============================================================================
static RE::NiPoint3 GetStablePosition(RE::TESObjectREFR* ref) {
    if (!ref) return {0, 0, 0};
    auto* node = ref->Get3D();
    if (node && node->worldBound.radius > 0.1f) {
        return node->worldBound.center;
    }
    return ref->GetPosition();
}

// =============================================================================
// IsRefContainerEmpty
// =============================================================================
// Détecte si un conteneur ou cadavre est vide DU POINT DE VUE DU JOUEUR
// (= "Take All" ne donnerait rien).
//
// Approche miroir de vanilla ContainerMenu::TakeAllItems :
// - GetInventory() retourne la map complète (base TESContainer + countDelta
//   appliqué). Pour chaque entrée, on filtre :
//   - count <= 0 → ignoré (item retiré ou entry sans items)
//   - IsQuestObject() → ignoré (le joueur ne peut pas le prendre)
//
// Si après filtrage il ne reste rien → vide.
//
// Cf. f4access/ObjectScanner.cpp::IsContainerEmpty (équivalent Fallout 4
// avec BGSInventoryList).
// =============================================================================
static bool IsRefContainerEmpty(RE::TESObjectREFR* ref) {
    if (!ref) return true;
    auto inv = ref->GetInventory();
    for (auto& [obj, data] : inv) {
        auto& [count, entry] = data;
        if (count <= 0) continue;
        if (!obj) continue;
        // Filtrer les quest items (le joueur ne peut pas les prendre)
        if (entry && entry->IsQuestObject()) continue;
        // Filtrer les LeveledItem non résolus (apparaissent dans le base
        // form mais ne sont pas de vrais items tant que le conteneur n'a
        // pas été ouvert ; après ouverture ils sont absents et seuls les
        // items rolés apparaissent).
        if (obj->GetFormType() == RE::FormType::LeveledItem) continue;
        return false;
    }
    return true;
}

// --- Scanner une cellule et ajouter ses références ---
static void ScanCell(RE::TESObjectCELL* cell, RE::PlayerCharacter* player, const RE::NiPoint3& playerPos) {
    if (!cell) return;

    auto& refList = cell->GetRuntimeData().references;
    for (auto& refHandle : refList) {
        auto refPtr = refHandle.get();
        if (!refPtr) continue;
        auto& ref = *refPtr;

        try {
            if (&ref == player) continue;

            // DEBUG TEMPORAIRE : log toute ref avec root/racine/primarbor/
            // eldergleam/t03 dans son nom, AVANT le filtre disabled, pour
            // diagnostiquer la quete T03 chez l'utilisateur. A retirer apres.
            {
                auto* base_dbg = ref.GetBaseObject();
                if (base_dbg) {
                    const char* dn = ref.GetDisplayFullName();
                    const char* be = base_dbg->GetFormEditorID();
                    const char* bn = base_dbg->GetName();
                    auto contains_ci = [](const char* hay, const char* needle) {
                        if (!hay || !*hay) return false;
                        std::string h = hay;
                        for (auto& c : h) c = static_cast<char>(std::tolower(c));
                        return h.find(needle) != std::string::npos;
                    };
                    bool match = false;
                    for (const char* s : {dn, be, bn}) {
                        if (contains_ci(s, "root") || contains_ci(s, "racine") ||
                            contains_ci(s, "primarbor") || contains_ci(s, "eldergleam") ||
                            contains_ci(s, "t03"))
                        { match = true; break; }
                    }
                    if (match) {
                        auto p = ref.GetPosition();
                        auto d = playerPos - p;
                        LOG("ELDERGLEAM-DEBUG refID=0x{:08X} baseID=0x{:08X} type={} "
                            "name='{}' base.name='{}' base.editorID='{}' "
                            "disabled={} deleted={} dist={:.0f} pos=({:.0f},{:.0f},{:.0f})",
                            ref.GetFormID(),
                            base_dbg->GetFormID(),
                            static_cast<int>(base_dbg->GetFormType()),
                            dn ? dn : "",
                            bn ? bn : "",
                            be ? be : "",
                            ref.IsDisabled(),
                            ref.IsDeleted(),
                            d.Length(),
                            p.x, p.y, p.z);
                    }
                }
            }

            // Exception : on laisse passer les Pousses du Primarbor (racines
            // de la quete T03 "Les Bienfaits de la Nature") meme si elles
            // sont disabled. Le moteur les laisse parfois disabled jusqu'a
            // ce que le joueur s'approche d'un trigger qui les enable, mais
            // l'utilisateur veut pouvoir les reperer a l'avance pour s'y
            // diriger. FormID base 0x0001CB81 (verifie via log capture).
            {
                auto* base_pre = ref.GetBaseObject();
                bool isEldergleamRoot =
                    base_pre && base_pre->GetFormID() == 0x0001CB81;
                if (!isEldergleamRoot) {
                    if (ref.IsDisabled() || ref.IsDeleted()) continue;
                } else {
                    // Pour une racine, on accepte disabled mais on skip
                    // tout de meme deleted (cadavre vraiment supprime).
                    if (ref.IsDeleted()) continue;
                }
            }

            auto* base = ref.GetBaseObject();
            if (!base) continue;

            // Log dragons pour diagnostic
            if (auto* actor = ref.As<RE::Actor>()) {
                if (IsDragon(actor)) {
                    auto dPos = ref.GetPosition();
                    auto dDiff = playerPos - dPos;
                    LOG("Scanner: DRAGON found '{}' FormID={:08X} dist={:.0f} z={:.0f} dead={} disabled={} 3D={}",
                        ref.GetDisplayFullName() ? ref.GetDisplayFullName() : "?",
                        ref.GetFormID(), dDiff.Length(), dPos.z - playerPos.z,
                        actor->IsDead(), actor->IsDisabled(), actor->Is3DLoaded());
                }
            }

            // Distance : on utilise GetStablePosition pour eviter les sauts
            // causes par GetPosition() qui renvoie le centre de masse Havok (bouge
            // a chaque frame pour les objets physiques jetes/rebondissants).
            auto refPos = GetStablePosition(&ref);
            auto diff = playerPos - refPos;
            float dist = diff.Length();

            // Filtre de distance MCM (0 = illimité)
            float maxRange = g_mcmScanRange.load();
            if (maxRange > 0.0f && dist > maxRange) continue;

            // Détecter les Murs des Mots (triggers invisibles sans nom).
            // Utilise le cache : première rencontre = 9 requêtes VM, ensuite 0.
            if (base && base->Is(RE::FormType::Activator) &&
                GetScriptTypeCached(ref) == ScriptTypeKind::WordWall) {
                ScannedObject obj;
                obj.formID = ref.GetFormID();
                obj.name = TR("Word Wall");
                obj.distance = dist;
                obj.zDiff = refPos.z - playerPos.z;
                obj.lastKnownPos = refPos;
                obj.category = kCatActivators;
                g_scannedAll.push_back(std::move(obj));
            }

            // Nom
            const char* rawName = ref.GetDisplayFullName();
            // Les obstacles destructibles (toile d'araignee, barricade, racine...)
            // n'ont souvent pas de nom affichable en jeu mais sont des elements
            // importants pour le joueur aveugle. On les garde s'ils ont une DEST
            // record et un type approprie. Le tri "vrai obstacle vs filon de fer"
            // se fera plus bas via la liste blanche (label assigne ou pas).
            bool hasNoName = (!rawName || !*rawName);
            if (hasNoName) {
                auto ft = base->GetFormType();
                bool candidateType = (ft == RE::FormType::Activator ||
                                      ft == RE::FormType::Door ||
                                      ft == RE::FormType::MovableStatic);
                if (!candidateType || !BaseIsDestructible(base) ||
                    (ref.GetFormFlags() & RE::TESForm::RecordFlags::kDestroyed)) {
                    continue;
                }
                // On continue : le scoring plus bas decidera si on garde l'objet
                // (s'il a un label connu) ou si on l'ignore via le check final.
                rawName = "";
            }

            // Diagnostic pilier puzzle : lire l'état via le script Papyrus.
            // Le TYPE de script est mis en cache (stable), mais l'ÉTAT live
            // (position01/02/03/busy) doit être relu à chaque scan car il change
            // quand le joueur tourne le pilier.
            std::string nameStr = rawName;
            ScriptTypeKind stk = ScriptTypeKind::NotSpecial;
            if (nameStr.find("Pilier") != std::string::npos ||
                nameStr.find("Pillar") != std::string::npos ||
                nameStr.find("pilier") != std::string::npos) {
                stk = GetScriptTypeCached(ref);
            }
            if (stk == ScriptTypeKind::PillarStandard ||
                stk == ScriptTypeKind::PillarInt ||
                stk == ScriptTypeKind::RingHallOfStories) {
                auto* vmSingleton = RE::SkyrimVM::GetSingleton();
                if (vmSingleton && vmSingleton->impl) {
                    auto* handlePolicy = vmSingleton->impl->GetObjectHandlePolicy();
                    if (handlePolicy) {
                        auto vmHandle = handlePolicy->GetHandleForObject(
                            static_cast<RE::VMTypeID>(RE::FormType::Reference), &ref);
                        // On cible directement le bon script — plus besoin de boucler sur 4 noms.
                        const char* sName = (stk == ScriptTypeKind::PillarStandard) ? "defaultPuzzlePillarScript"
                                          : (stk == ScriptTypeKind::PillarInt)      ? "intPuzzlePillarScript"
                                                                                    : "HallofStoriesDiskScript";
                        RE::BSTSmartPointer<RE::BSScript::Object> scriptObj;
                        if (vmSingleton->impl->FindBoundObject(vmHandle, sName, scriptObj) && scriptObj) {
                            std::string state = scriptObj->currentState.c_str();
                            int posNum = 0;
                            if (state == "position01") posNum = 1;
                            else if (state == "position02") posNum = 2;
                            else if (state == "position03") posNum = 3;

                            const char* symbol = "unknown";
                            if (state == "busy") {
                                symbol = "turning";
                            } else if (posNum > 0) {
                                if (stk == ScriptTypeKind::PillarStandard || stk == ScriptTypeKind::PillarInt) {
                                    if (posNum == 1) symbol = "Eagle";
                                    else if (posNum == 2) symbol = "Snake";
                                    else if (posNum == 3) symbol = "Whale";
                                } else if (stk == ScriptTypeKind::RingHallOfStories) {
                                    auto* linkedRef = ref.GetLinkedRef(nullptr);
                                    if (linkedRef) {
                                        RE::FormID keyholeID = linkedRef->GetFormID();
                                        for (size_t di = 0; di < sizeof(g_doorSymbolTable)/sizeof(g_doorSymbolTable[0]); di++) {
                                            if (g_doorSymbolTable[di].keyhole == keyholeID) {
                                                if (posNum == 1) symbol = g_doorSymbolTable[di].s1;
                                                else if (posNum == 2) symbol = g_doorSymbolTable[di].s2;
                                                else if (posNum == 3) symbol = g_doorSymbolTable[di].s3;
                                                break;
                                            }
                                        }
                                    }
                                    if (std::string(symbol) == "unknown") {
                                        if (posNum == 1) symbol = "Position 1";
                                        else if (posNum == 2) symbol = "Position 2";
                                        else if (posNum == 3) symbol = "Position 3";
                                    }
                                }
                            }

                            nameStr = std::string(rawName) + " (" + symbol + ")";
                            rawName = nullptr;
                        }
                    }
                }
            }

            // Filtrer les objets techniques/invisibles du jeu
            if (nameStr.find("Trigger") != std::string::npos ||
                nameStr.find("trigger") != std::string::npos ||
                nameStr.find("should not") != std::string::npos ||
                nameStr.find("DVAD") != std::string::npos ||
                nameStr.find("DefaultDummy") != std::string::npos ||
                nameStr.find("Marker") == 0 ||
                nameStr.find("XMarker") != std::string::npos)
                continue;

            // Catégoriser
            ScanCategory cat = CategorizeRef(ref);

            float zDiff = refPos.z - playerPos.z;

            // Status
            bool locked = false;
            if (base->Is(RE::FormType::Door) || base->Is(RE::FormType::Container)) {
                locked = ref.IsLocked();
            }

            // Destination des portes
            std::wstring doorDest;
            if (base->GetFormType() == RE::FormType::Door) {
                auto* extraTeleport = ref.extraList.GetByType<RE::ExtraTeleport>();
                if (extraTeleport && extraTeleport->teleportData) {
                    auto linkedDoorPtr = extraTeleport->teleportData->linkedDoor.get();
                    if (linkedDoorPtr) {
                        auto* destCell = linkedDoorPtr->GetParentCell();
                        if (destCell) {
                            const char* destName = destCell->GetName();
                            if (destName && *destName) {
                                doorDest = Utf8ToWString(destName);
                            }
                        }
                        // Fallback : si la cellule n'est pas chargée, essayer le worldspace
                        if (doorDest.empty()) {
                            auto* ws = linkedDoorPtr->GetWorldspace();
                            if (ws) {
                                const char* wsName = ws->GetName();
                                if (wsName && *wsName) {
                                    doorDest = Utf8ToWString(wsName);
                                }
                            }
                        }
                    }
                }
            }

            // Détection conteneur/cadavre vide via base + ContainerChanges
            bool isEmpty = false;
            if (cat == kCatContainers || cat == kCatCorpses) {
                try { isEmpty = IsRefContainerEmpty(&ref); } catch (...) {}
            }

            // Détection porte de cellule (avec teleport)
            bool isCellDoor = !doorDest.empty();

            // Détection meuble (Furniture)
            bool isFurniture = (base->GetFormType() == RE::FormType::Furniture);

            // Détection station de craft (sous-ensemble de Furniture) : forge,
            // enchanteur, alchimie, meule, atelier d'armurier, four, cookpot,
            // tannerie. Seules les Furniture peuvent l'etre.
            bool isCraftingStation = isFurniture && IsCraftingStation(ref);

            // Détection destructible (Tier 1 : DEST record sur le base form,
            // Tier 2 : script Papyrus connu pour donner un label précis).
            // Filtre de type : seulement Activator / Door / MovableStatic — les
            // autres FormType n'ont pas de destruction data pertinente pour un
            // obstacle de passage (les armes/armures qui ont un DEST ne nous
            // intéressent pas ici).
            bool isDestructible = false;
            int  destructibleHealthPercent = 100;
            const wchar_t* destructibleLabel = nullptr;

            // CAS SPECIAL : "Pousses du Primarbor" (Eldergleam Roots en VO),
            // quete T03 "Les Bienfaits de la Nature". Ces racines bloquent
            // le chemin spiralé qui mene a l'arbre Primarbor (Eldergleam) et
            // doivent etre coupees avec Bouillure (Nettlebane).
            //
            // Confirme via log capture en jeu (FormID + base + type) :
            //   refID=0x0001CB83 base=0x0001CB81 type=Tree (32)
            //   name='Pousse du Primarbor' (FR) / 'Eldergleam Root' (EN)
            //
            // ATTENTION : ce sont des FormType::Tree (pas Activator comme
            // documente initialement). Et le base FormID est 0x1CB81 (pas
            // 0x1CD1A qui etait une fausse info).
            //
            // Pas de DEST record : la pousse fait juste Disable() quand le
            // joueur l'active avec Bouillure (script T03EldergleamRootScript).
            {
                if (base->GetFormID() == 0x0001CB81 &&
                    !(ref.GetFormFlags() & RE::TESForm::RecordFlags::kDestroyed))
                {
                    isDestructible = true;
                    destructibleLabel = L"Eldergleam root";
                    destructibleHealthPercent = 100;
                }
            }

            // Bloc destructible standard (DEST record requis). Skip si on a
            // deja flag comme racine Eldergleam ci-dessus.
            if (!isDestructible)
            {
                auto ft = base->GetFormType();
                bool candidateType = (ft == RE::FormType::Activator ||
                                      ft == RE::FormType::Door ||
                                      ft == RE::FormType::MovableStatic);
                if (candidateType && BaseIsDestructible(base) &&
                    !(ref.GetFormFlags() & RE::TESForm::RecordFlags::kDestroyed)) {
                    // LISTE BLANCHE STRICTE : on ne flag "destructible" que les
                    // vrais obstacles de passage (toiles, sacs d'oeufs, barricades,
                    // racines, portes fragiles). Les filons de fer, tonneaux
                    // explosifs, poteries cassables, etc. ont aussi un DEST record
                    // mais ne sont PAS des obstacles gameplay — on les ignore pour
                    // ne pas polluer le sous-filtre Destructibles du scanner.
                    ScriptTypeKind stk = GetScriptTypeCached(ref);
                    const wchar_t* lbl = GetDestructibleLabel(stk);

                    // 1) Detection par EditorID (necessite po3_Tweaks active)
                    if (!lbl) {
                        const char* bedid = base->GetFormEditorID();
                        if (bedid && *bedid) {
                            std::string e = bedid;
                            auto has = [&](const char* s) {
                                return e.find(s) != std::string::npos;
                            };
                            if (has("WebObstacle") || has("Webs") || has("SpiderWeb") ||
                                has("MGRWeb")) {
                                lbl = L"Spider web";
                            } else if (has("EggSack") || has("EggSac") || has("MGREgg")) {
                                lbl = L"Spider egg sac";
                            } else if (has("Barricade") || has("CWBarricade")) {
                                lbl = L"Wooden barricade";
                            } else if (has("Eldergleam")) {
                                lbl = L"Eldergleam root";
                            } else if (has("BreakableDoor") || has("BreakableWall")) {
                                lbl = L"Breakable door";
                            }
                        }
                    }

                    // 2) Fallback : detection par FormID vanilla du base form.
                    // Ces IDs sont stables entre toutes les versions de Skyrim
                    // (extrait des logs en jeu sur Bleak Falls Sanctum).
                    if (!lbl) {
                        RE::FormID baseID = base->GetFormID();
                        switch (baseID) {
                            // Toiles d'araignee (MGRWebObstacle variantes)
                            case 0x00061501:
                            case 0x000862CC:
                            case 0x00090EFC:
                            case 0x000EC3DE:
                            case 0x0008B317:
                                lbl = L"Spider web";
                                break;
                            default:
                                break;
                        }
                    }

                    // Seulement si on a un label reconnu on flag comme destructible.
                    // Sinon (filon de fer, autre DEST random) : pas de flag.
                    if (lbl) {
                        isDestructible = true;
                        destructibleHealthPercent = ReadDestructibleHealthPercent(ref, base);
                        destructibleLabel = lbl;
                    }
                }
            }

            // Filtre final : si l'objet n'a pas de nom ET n'est pas reconnu
            // comme destructible avec label, on le saute (sinon il apparaitrait
            // anonyme dans la liste, polluant le scanner).
            bool noUsableName = (!rawName || !*rawName) && nameStr.empty() && !destructibleLabel;
            if (noUsableName) continue;

            ScannedObject obj;
            obj.formID = ref.GetFormID();
            // Si le nom est vide (typiquement un destructible sans nom affichable),
            // utiliser le label destructible comme nom de substitution pour que le
            // joueur entende quelque chose a la lecture.
            if (rawName && *rawName) {
                obj.name = Utf8ToWString(rawName);
            } else if (!nameStr.empty()) {
                obj.name = Utf8ToWString(nameStr.c_str());
            } else {
                // Pas de nom natif -> on utilise le label destructible traduit.
                obj.name = TR(WStringToUtf8(std::wstring(destructibleLabel)));
            }
            obj.distance = dist;
            obj.zDiff = zDiff;
            obj.lastKnownPos = refPos;
            obj.category = cat;
            obj.locked = locked;
            obj.empty = isEmpty;
            obj.dead = (cat == kCatCorpses);
            obj.isCellDoor = isCellDoor;
            obj.isFurniture = isFurniture;
            obj.isCraftingStation = isCraftingStation;
            // Flags PNJ pour le filtrage de la categorie NPCs. Calcules
            // uniquement si l'objet est un acteur vivant non-compagnon.
            if (cat == kCatNPCs) {
                if (auto* actor = ref.As<RE::Actor>()) {
                    obj.npcCanTalk  = actor->CanTalkToPlayer();
                    obj.npcHostile  = actor->IsHostileToActor(player);
                    obj.npcMerchant = (actor->GetVendorFaction() != nullptr);
                }
            }
            obj.formType = base->GetFormType();
            obj.doorDestination = std::move(doorDest);
            obj.isDestructible = isDestructible;
            obj.destructibleHealthPercent = destructibleHealthPercent;
            obj.destructibleLabel = destructibleLabel;

            g_scannedAll.push_back(std::move(obj));
        } catch (...) {
            // Skip
        }
    }
}

// --- Exécuter le scan dans les cellules chargées ---
static void DoScanInternal() {
    LOG("Scanner: DoScanInternal START");
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) { LOG("Scanner: no player"); return; }

    auto* tes = RE::TES::GetSingleton();
    if (!tes) { LOG("Scanner: no TES"); return; }

    g_scannedAll.clear();
    g_scannedFiltered.clear();
    auto playerPos = player->GetPosition();
    g_lastScanPos = playerPos;

    auto* playerCell = player->GetParentCell();
    if (!playerCell) { LOG("Scanner: no parent cell"); return; }

    bool isInterior = playerCell->IsInteriorCell();

    if (isInterior) {
        // Intérieur : une seule cellule
        ScanCell(playerCell, player, playerPos);
        LOG("Scanner: interior cell scanned");
    } else {
        // Extérieur : scanner toutes les cellules chargées de la grille
        if (auto* gridCells = tes->gridCells) {
            auto gridLen = gridCells->length;
            int cellCount = 0;
            for (uint32_t x = 0; x < gridLen; x++) {
                for (uint32_t y = 0; y < gridLen; y++) {
                    auto* cell = gridCells->GetCell(x, y);
                    if (cell && cell->IsAttached()) {
                        ScanCell(cell, player, playerPos);
                        cellCount++;
                    }
                }
            }
            LOG("Scanner: exterior, scanned {} cells (grid {}x{})", cellCount, gridLen, gridLen);
        } else {
            // Fallback : cellule du joueur uniquement
            ScanCell(playerCell, player, playerPos);
            LOG("Scanner: exterior fallback, single cell");
        }
    }

    // Passage supplementaire pour les acteurs hors-cellule (dragons en vol,
    // notamment Parturnax au sommet de la Gorge du Monde qui vole avant
    // l'interaction). Ces acteurs ne sont pas dans cell->references car ils
    // ne sont rattaches a aucune cellule pendant le vol. On les trouve via
    // ProcessLists, le meme mecanisme que Shift+X utilise.
    //
    // IMPORTANT : on ne fait ce passage QU'EN EXTERIEUR. ProcessLists
    // contient tous les acteurs actifs du worldspace courant (et parfois
    // des worldspaces voisins), donc en interieur on verrait apparaitre
    // les PNJ qui sont dehors. Les dragons sont des creatures exterieures,
    // ils ne peuvent jamais voler dans une maison/donjon de toute facon.
    if (!isInterior) {
        std::set<RE::FormID> alreadyScanned;
        for (auto& obj : g_scannedAll) {
            if (obj.formID != 0) alreadyScanned.insert(obj.formID);
        }

        // Worldspace courant : on filtre les acteurs ProcessLists qui sont
        // dans un autre worldspace (peut arriver pres des bordures de zone).
        auto* playerWorldSpace = player->GetWorldspace();

        auto* procLists = RE::ProcessLists::GetSingleton();
        if (procLists) {
            int procAdded = 0;
            float maxRange = g_mcmScanRange.load();

            auto addFromProcess = [&](RE::Actor* actor) {
                if (!actor) return;
                if (actor == player) return;
                if (actor->IsDeleted()) return;
                if (actor->IsDisabled()) return;
                if (!actor->Is3DLoaded()) return;
                RE::FormID fid = actor->GetFormID();
                if (alreadyScanned.count(fid)) return;  // deja capte par la boucle cellule

                // Filtre worldspace : exclure les acteurs d'un autre monde.
                // GetWorldspace() retourne null pour les acteurs en interieur,
                // donc on les exclut aussi (on est en exterieur dans cette branche).
                auto* actorWorld = actor->GetWorldspace();
                if (actorWorld != playerWorldSpace) return;

                auto refPos = actor->GetPosition();
                auto diff = playerPos - refPos;
                float dist = diff.Length();
                if (maxRange > 0.0f && dist > maxRange) return;

                const char* rawName = actor->GetDisplayFullName();
                if (!rawName || !*rawName) return;

                ScannedObject obj;
                obj.formID = fid;
                obj.name = Utf8ToWString(rawName);
                obj.distance = dist;
                obj.zDiff = refPos.z - playerPos.z;
                obj.lastKnownPos = refPos;
                obj.category = CategorizeRef(*actor);
                obj.dead = actor->IsDead();
                auto* base = actor->GetBaseObject();
                obj.formType = base ? base->GetFormType() : RE::FormType::None;
                // Flags PNJ : memes que pour la boucle cellule.
                if (obj.category == kCatNPCs) {
                    obj.npcCanTalk  = actor->CanTalkToPlayer();
                    obj.npcHostile  = actor->IsHostileToActor(player);
                    obj.npcMerchant = (actor->GetVendorFaction() != nullptr);
                }

                if (IsDragon(actor)) {
                    LOG("Scanner: DRAGON via ProcessLists '{}' FormID={:08X} dist={:.0f} z={:.0f}",
                        rawName, fid, dist, refPos.z - playerPos.z);
                }

                g_scannedAll.push_back(std::move(obj));
                alreadyScanned.insert(fid);
                procAdded++;
            };

            for (auto& handle : procLists->highActorHandles) {
                auto p = handle.get();
                if (p) addFromProcess(p.get());
            }
            for (auto& handle : procLists->middleHighActorHandles) {
                auto p = handle.get();
                if (p) addFromProcess(p.get());
            }
            if (procAdded > 0) {
                LOG("Scanner: ProcessLists added {} actors not in cells (flying dragons, etc.)", procAdded);
            }
        }
    }

    // Scanner les objectifs de quête actifs
    try {
        auto& objectives = REL::RelocateMemberIfNewer<RE::BSTArray<RE::BGSInstancedQuestObjective>>(
            SKSE::RUNTIME_SSE_1_6_629, player, 0x580, 0x588);
        LOG("Scanner: checking {} quest objectives", objectives.size());
        for (auto& instObj : objectives) {
            if (!instObj.Objective) continue;
            if (instObj.InstanceState != RE::QUEST_OBJECTIVE_STATE::kDisplayed) continue;

            auto* questObj = instObj.Objective;
            auto* quest = questObj->ownerQuest;
            if (!quest) continue;

            const char* questName = quest->GetFullName();
            auto rawFlags = quest->data.flags.underlying();
            bool displayedInHUD = (rawFlags & 0x20) != 0;  // bit 5
            bool isActive = quest->IsActive();
            LOG("Scanner: objective idx={} quest='{}' text='{}' numTargets={} HUD={} active={} rawFlags={:04X}",
                questObj->index, questName ? questName : "?",
                questObj->displayText.c_str(), questObj->numTargets,
                displayedInHUD, isActive, rawFlags);

            // Filtrer : ne garder que les quêtes actives (cochées dans le journal)
            if (!isActive) continue;

            // Pre-scan : detecter si l'objectif contient au moins un target nomme
            // (ref avec DisplayFullName). Exemple : quete "Adressez-vous aux
            // Grises-barbes" a 3 targets — target[0]=Arngeir (nomme), target[1] et
            // target[2]=markers exterieurs anonymes. Sans pre-scan, on creait 3
            // entrees dans le scanner pour la meme cible logique.
            // Regle : si au moins un target est nomme, les targets anonymes sont
            // consideres comme des fallbacks (markers de location) et ignores.
            // Si aucun target n'est nomme, on garde le comportement original (on
            // ajoute tous les targets, meme anonymes, pour ne pas perdre la quete).
            // On compte les targets nommes (pas juste detection booleenne) : si 2+
            // targets ont un nom, on suffixera pour distinguer les entrees dans le
            // scanner (ex: Irileth + tour de guet). Si 0 ou 1 target nomme, pas de
            // suffixe necessaire car l'entree est unique.
            int namedTargetCount = 0;
            for (uint32_t t = 0; t < questObj->numTargets; t++) {
                auto* target = questObj->targets[t];
                if (!target) continue;
                RE::ObjectRefHandle probeHandle;
                quest->CreateRefHandleByAliasID(probeHandle, target->alias);
                if (!probeHandle) continue;
                auto probePtr = probeHandle.get();
                if (!probePtr) continue;
                auto* probeRef = probePtr.get();
                const char* probeName = probeRef ? probeRef->GetDisplayFullName() : nullptr;
                if (probeName && *probeName) namedTargetCount++;
            }
            const bool hasNamedTarget = (namedTargetCount > 0);

            // Parcourir les cibles de l'objectif pour trouver la référence.
            // Système en 2 passes (inchangé pour le cas normal) :
            //   PASS 0 : essayer les targets avec vérification CTDA (comportement historique)
            //   PASS 1 : si AUCUN target n'a passé les CTDA, retenter sans CTDA
            //            → filet de sécurité pour les quêtes dont tous les targets ont des
            //              CTDA qui échouent (ex: Pierre de dragon à Bleak Falls Barrow,
            //              où le target est une ref qui n'est pas encore spawn dans le monde)
            //            → pas de régression : le flag questTargetResolved passe à true dès
            //              qu'un target est pushé, ce qui empêche la boucle externe de
            //              passer au PASS 1. La boucle interne des targets, elle, continue
            //              jusqu'au bout comme avant (la déduplication par nom gère les
            //              doublons éventuels entre targets du même objective).
            bool questTargetResolved = false;
            for (int pass = 0; pass < 2 && !questTargetResolved; pass++) {
                const bool ignoreCTDA = (pass == 1);
                if (ignoreCTDA) {
                    LOG("Scanner: objective idx={} pass 0 failed for all targets, retry without CTDA", questObj->index);
                }

            for (uint32_t t = 0; t < questObj->numTargets; t++) {
                auto* target = questObj->targets[t];
                if (!target) {
                    LOG("Scanner: target[{}] is null", t);
                    continue;
                }

                // Résoudre l'alias pour obtenir la référence
                uint32_t aliasIdx = target->alias;

                // Résoudre l'alias via CreateRefHandleByAliasID (méthode du moteur)
                // Plus fiable que BGSRefAlias::GetReference() pour les refs distantes
                RE::ObjectRefHandle refHandle;
                quest->CreateRefHandleByAliasID(refHandle, aliasIdx);

                if (!refHandle) {
                    if (!ignoreCTDA) LOG("Scanner: alias {} - CreateRefHandleByAliasID returned empty handle", aliasIdx);
                    continue;
                }

                auto refSmartPtr = refHandle.get();
                if (!refSmartPtr) {
                    if (!ignoreCTDA) LOG("Scanner: alias {} - handle.get() returned null", aliasIdx);
                    continue;
                }
                auto* targetRef = refSmartPtr.get();

                // Skip les targets anonymes si l'objectif contient au moins un
                // target nomme (les anonymes sont alors des markers fallback).
                // Evite de polluer le scanner avec 3 entrees pour la meme cible.
                if (hasNamedTarget) {
                    const char* probeName = targetRef->GetDisplayFullName();
                    if (!probeName || !*probeName) {
                        if (!ignoreCTDA) {
                            LOG("Scanner: target[{}] FormID={:08X} skipped (anonymous fallback, named target exists)",
                                t, targetRef->GetFormID());
                        }
                        continue;
                    }
                }

                // PASS 0 : vérifier les CTDA (comportement historique).
                // PASS 1 : on saute cette vérification pour accepter n'importe quel alias résolu.
                if (!ignoreCTDA && target->conditions.head != nullptr) {
                    if (!target->conditions.IsTrue(player, targetRef)) {
                        LOG("Scanner: target[{}] conditions not met, skipping", t);
                        continue;
                    }
                }
                if (ignoreCTDA) {
                    LOG("Scanner: target[{}] accepted via PASS 1 (CTDA ignored) ref='{}' FormID={:08X}",
                        t, targetRef->GetDisplayFullName() ? targetRef->GetDisplayFullName() : "?",
                        targetRef->GetFormID());
                }

                auto refPos = GetStablePosition(targetRef);
                const char* refName = targetRef->GetDisplayFullName();
                auto* refCell = targetRef->GetParentCell();
                LOG("Scanner: target ref='{}' FormID={:08X} pos=({:.0f},{:.0f},{:.0f}) cell='{}' dist={:.0f}",
                    refName ? refName : "?", targetRef->GetFormID(),
                    refPos.x, refPos.y, refPos.z,
                    refCell ? refCell->GetName() : "no cell",
                    (playerPos - refPos).Length());

                // Si la cible est dans une cellule différente du joueur,
                // chercher la porte à prendre via la boussole (comme l'autowalk)
                RE::NiPoint3 actualPos = refPos;
                RE::FormID actualFormID = targetRef->GetFormID();
                bool compassFallbackUsed = false;  // pour distinguer orientation/distance apres fallback

                if (refCell && refCell != playerCell) {
                    LOG("Scanner: quest target in different cell ('{}' vs '{}'), searching entrance",
                        refCell->GetName() ? refCell->GetName() : "?",
                        playerCell->GetName() ? playerCell->GetName() : "?");

                    bool resolvedPos = false;

                    // === 1. Recherche de porte directe dans les cellules chargées ===
                    // Priorité au door search : si on trouve une porte qui mène DIRECTEMENT
                    // à la cellule cible dans les cellules actuellement attachées, c'est la
                    // destination la plus précise. Ex: joueur dans Whiterun, cible Jarl à
                    // Fort-Dragon → la porte de Fort-Dragon est dans une cellule attachée de
                    // Whiterun, on la trouve et on y va directement.
                    {
                        RE::TESObjectREFR* bestDoor = nullptr;
                        float bestDoorDist = 999999.0f;

                        auto searchDoorsForCell = [&](RE::TESObjectCELL* searchCell) {
                            if (!searchCell) return;
                            for (auto& doorHandle : searchCell->GetRuntimeData().references) {
                                auto doorPtr = doorHandle.get();
                                if (!doorPtr) continue;
                                auto* doorBase = doorPtr->GetBaseObject();
                                if (!doorBase || doorBase->GetFormType() != RE::FormType::Door) continue;
                                auto* extraTele = doorPtr->extraList.GetByType<RE::ExtraTeleport>();
                                if (!extraTele || !extraTele->teleportData) continue;
                                auto linkedDoor = extraTele->teleportData->linkedDoor.get();
                                if (!linkedDoor) continue;
                                auto* destCell = linkedDoor->GetParentCell();
                                if (!destCell) continue;

                                if (destCell == refCell) {
                                    auto doorPos = doorPtr->GetPosition();
                                    float doorDist = (playerPos - doorPos).Length();
                                    if (doorDist < bestDoorDist) {
                                        bestDoorDist = doorDist;
                                        bestDoor = doorPtr;
                                    }
                                }
                            }
                        };

                        searchDoorsForCell(playerCell);

                        if (!bestDoor && !isInterior) {
                            if (auto* gridCells = tes->gridCells) {
                                for (uint32_t gx = 0; gx < gridCells->length && !bestDoor; gx++) {
                                    for (uint32_t gy = 0; gy < gridCells->length && !bestDoor; gy++) {
                                        auto* gc = gridCells->GetCell(gx, gy);
                                        if (gc && gc->IsAttached() && gc != playerCell) searchDoorsForCell(gc);
                                    }
                                }
                            }
                            auto* ws = player->GetWorldspace();
                            if (!bestDoor && ws && ws->persistentCell) {
                                searchDoorsForCell(ws->persistentCell);
                            }
                        }

                        if (bestDoor) {
                            actualPos = bestDoor->GetPosition();
                            resolvedPos = true;
                            LOG("Scanner: quest redirected to door '{}' FormID={:08X} dist={:.0f}",
                                bestDoor->GetDisplayFullName() ? bestDoor->GetDisplayFullName() : "?",
                                bestDoor->GetFormID(), bestDoorDist);
                        }
                    }

                    // === 2. worldLocMarker : fallback quand la porte cible n'est pas chargée ===
                    // Utilisé quand le joueur est loin (ex: hors Whiterun, cible Jarl). Remonte
                    // la hiérarchie parentLoc jusqu'à un marker extérieur (ex: porte de Whiterun).
                    // On skip :
                    //   - les markers dans une cellule interieure (ex: marker de Fort-Dragon
                    //     interne)
                    //   - les markers dans un worldspace DIFFERENT du joueur (ex: marker des
                    //     quais de Solitude qui sont dans le sub-worldspace "Solitude Docks"
                    //     avec coords (1128,3014) au lieu du worldspace principal Tamriel)
                    //     -> sinon la distance calculee est gigantesque (~120000 unites).
                    if (!resolvedPos && !isInterior) {
                        auto* playerWS = player->GetWorldspace();
                        auto* targetLocation = refCell->GetLocation();
                        LOG("Scanner: DIAG worldLocMarker phase playerWS='{}' targetLocation='{}'",
                            playerWS ? (playerWS->GetName() ? playerWS->GetName() : "?") : "NULL",
                            targetLocation ? (targetLocation->GetFullName() ? targetLocation->GetFullName() : "?") : "NULL");
                        for (auto* loc = targetLocation; loc && !resolvedPos; loc = loc->parentLoc) {
                            if (loc->worldLocMarker) {
                                auto markerPtr = loc->worldLocMarker.get();
                                if (markerPtr) {
                                    auto* markerCell = markerPtr->GetParentCell();
                                    bool markerInterior = markerCell && markerCell->IsInteriorCell();
                                    if (markerInterior) {
                                        LOG("Scanner: skip interior worldLocMarker loc='{}' FormID={:08X} (markerCell='{}')",
                                            loc->GetFullName() ? loc->GetFullName() : "?",
                                            markerPtr->GetFormID(),
                                            markerCell->GetName() ? markerCell->GetName() : "?");
                                        continue;
                                    }
                                    auto markerCandidatePos = markerPtr->GetPosition();
                                    auto* markerWS = markerPtr->GetWorldspace();
                                    // DIAG : tracer chaque candidat worldLocMarker
                                    LOG("Scanner: DIAG candidate loc='{}' pos=({:.0f},{:.0f},{:.0f}) markerWS='{}' playerWS='{}' compatible={}",
                                        loc->GetFullName() ? loc->GetFullName() : "?",
                                        markerCandidatePos.x, markerCandidatePos.y, markerCandidatePos.z,
                                        markerWS ? (markerWS->GetName() ? markerWS->GetName() : "?") : "NULL",
                                        playerWS ? (playerWS->GetName() ? playerWS->GetName() : "?") : "NULL",
                                        (playerWS && markerWS) ? (AreWorldspacesCompatible(markerWS, playerWS) ? "YES" : "NO") : "skip");
                                    // Verifier que le marker est dans un worldspace
                                    // compatible avec celui du joueur. Si markerWS
                                    // est null (marker dans une cellule non chargee
                                    // dont le worldspace n'est pas resolu), on rejette
                                    // par defaut pour eviter d'accepter des coordonnees
                                    // dans un repere inconnu (cas Gulum-Ei dans
                                    // l'Entrepot apres teleportation hors Solitude).
                                    if (playerWS) {
                                        if (!markerWS) {
                                            LOG("Scanner: skip worldLocMarker loc='{}' has null worldspace (player in '{}')",
                                                loc->GetFullName() ? loc->GetFullName() : "?",
                                                playerWS->GetName() ? playerWS->GetName() : "?");
                                            continue;
                                        }
                                        if (!AreWorldspacesCompatible(markerWS, playerWS)) {
                                            auto* mRoot = GetCoordinateRoot(markerWS);
                                            auto* pRoot = GetCoordinateRoot(playerWS);
                                            LOG("Scanner: skip worldLocMarker loc='{}' in incompatible worldspace ('{}' root='{}' vs player '{}' root='{}')",
                                                loc->GetFullName() ? loc->GetFullName() : "?",
                                                markerWS->GetName() ? markerWS->GetName() : "?",
                                                (mRoot && mRoot->GetName()) ? mRoot->GetName() : "?",
                                                playerWS->GetName() ? playerWS->GetName() : "?",
                                                (pRoot && pRoot->GetName()) ? pRoot->GetName() : "?");
                                            continue;
                                        }
                                    }
                                    actualPos = markerCandidatePos;
                                    resolvedPos = true;
                                    LOG("Scanner: quest resolved via worldLocMarker loc='{}' FormID={:08X} pos=({:.0f},{:.0f},{:.0f})",
                                        loc->GetFullName() ? loc->GetFullName() : "?",
                                        markerPtr->GetFormID(),
                                        actualPos.x, actualPos.y, actualPos.z);
                                }
                            }
                        }
                    }

                    // === 3. Fallback boussole ===
                    if (!resolvedPos) {
                        LOG("Scanner: no door or marker for '{}', trying compass", refCell->GetName());
                        float compassHeading = -1.0f;
                        auto* ui = RE::UI::GetSingleton();
                        if (ui) {
                            auto hudMenu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
                            if (hudMenu && hudMenu->uiMovie) {
                                RE::GFxValue hudRoot;
                                if (hudMenu->uiMovie->GetVariable(&hudRoot, "_root.HUDMovieBaseInstance") && SafeIsObject(hudRoot)) {
                                    RE::GFxValue dataArr;
                                    if (hudRoot.GetMember("CompassTargetDataA", &dataArr) && SafeIsArray(dataArr)) {
                                        RE::GFxValue qtVal, qdVal;
                                        float qt = -1, qd = -1;
                                        if (hudRoot.GetMember("CompassMarkerQuest", &qtVal) && SafeIsNumber(qtVal))
                                            qt = static_cast<float>(SafeGetNumber(qtVal));
                                        if (hudRoot.GetMember("CompassMarkerQuestDoor", &qdVal) && SafeIsNumber(qdVal))
                                            qd = static_cast<float>(SafeGetNumber(qdVal));
                                        uint32_t arrSize = SafeGetArraySize(dataArr);
                                        for (uint32_t ci = 0; ci + 3 < arrSize; ci += 4) {
                                            RE::GFxValue hVal, tVal;
                                            dataArr.GetElement(ci, &hVal);
                                            dataArr.GetElement(ci + 2, &tVal);
                                            if (!SafeIsNumber(tVal)) continue;
                                            float tp = static_cast<float>(SafeGetNumber(tVal));
                                            if ((tp == qt || tp == qd) && SafeIsNumber(hVal)) {
                                                compassHeading = static_cast<float>(SafeGetNumber(hVal));
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (compassHeading >= 0) {
                            // Direction de la boussole, distance fictive
                            float compassRad = compassHeading * 3.14159265f / 180.0f;
                            actualPos.x = playerPos.x + std::sin(compassRad) * 50000.0f;
                            actualPos.y = playerPos.y + std::cos(compassRad) * 50000.0f;
                            actualPos.z = playerPos.z;
                            compassFallbackUsed = true;
                            LOG("Scanner: quest compass fallback heading={:.1f}°", compassHeading);
                        }
                    }
                }

                // Distance : en cas de fallback compass, actualPos est une position
                // fictive a 50000u dans la direction de la boussole (pour l'orientation
                // camera). On utilise alors refPos (vraie position de la cible) pour
                // la distance, sinon le joueur verrait "25000 unites" au scan puis
                // "4500 unites" au refresh quand le RefreshFilteredList recalcule avec
                // GetStablePosition. Plus de saut.
                auto distPos = compassFallbackUsed ? refPos : actualPos;
                auto diff = distPos - playerPos;
                float dist = diff.Length();
                float zDiff = distPos.z - playerPos.z;

                // Nom : texte de l'objectif (résoudre les <Alias=XXX>)
                std::string objText;
                if (questObj->displayText.size() > 0) {
                    objText = ResolveQuestAliases(questObj->displayText.c_str(), quest);
                } else if (quest->GetFullName()) {
                    objText = quest->GetFullName();
                } else {
                    continue;
                }

                ScannedObject obj;
                obj.formID = actualFormID;
                obj.name = Utf8ToWString(objText.c_str());
                obj.distance = dist;
                obj.zDiff = zDiff;
                // Stocker la vraie position (refPos) plutot que la position fictive
                // du compass, pour que RefreshFilteredList utilise la bonne distance
                // au lieu de recalculer 25000u. L'orientation camera (si necessaire)
                // est recalculee live via le HUD compass dans ScannerAnnounceCurrent.
                obj.lastKnownPos = compassFallbackUsed ? refPos : actualPos;
                obj.category = kCatQuests;
                obj.locked = false;
                obj.empty = false;
                obj.dead = false;

                // Suffixe desambiguisant seulement si l'objectif a AU MOINS 2 targets
                // nommes (ex: "Rejoignez Irileth a la tour de guet" : Irileth + tour).
                // Les targets anonymes sont deja filtres par hasNamedTarget en amont,
                // donc si namedTargetCount == 1, on a une seule entree dans le scanner
                // et le suffixe serait juste bruyant. Cas "Arngeir" : 1 nomme + 2 anonymes
                // filtres → namedTargetCount=1 → pas de suffixe "(Arngeir)".
                if (namedTargetCount >= 2) {
                    std::wstring suffix;
                    // Priorite : nom de la ref, sinon nom de l'alias
                    const char* refName = targetRef->GetDisplayFullName();
                    if (refName && *refName) {
                        suffix = Utf8ToWString(refName);
                    } else if (target->alias < quest->aliases.size()) {
                        auto* alias = quest->aliases[target->alias];
                        if (alias && !alias->aliasName.empty()) {
                            suffix = Utf8ToWString(alias->aliasName.c_str());
                        }
                    }
                    if (!suffix.empty()) {
                        obj.name += L" (" + suffix + L")";
                    }
                }

                // Déduplication par FormID + nom du target. Un objectif peut avoir
                // plusieurs targets = plusieurs entrees distinctes (ex: Irileth +
                // Tour de guet). Avant on dedupliquait par nom seul, ce qui fusionnait
                // les 2 targets d'un meme objectif. Avec le suffixe ajoute ci-dessus,
                // les noms sont differents et donc les deux targets coexistent.
                bool duplicate = false;
                for (auto& existing : g_scannedAll) {
                    if (existing.category == kCatQuests &&
                        existing.formID == obj.formID &&
                        existing.name == obj.name) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    LOG("Scanner: skipping duplicate quest target '{}' FormID={:08X}",
                        objText, obj.formID);
                    continue;
                }

                g_scannedAll.push_back(std::move(obj));
                questTargetResolved = true;  // au moins un target a abouti → pas de PASS 1 nécessaire
                LOG("Scanner: quest objective '{}' target[{}] FormID={:08X} dist={:.0f}",
                    objText, t, targetRef->GetFormID(), dist);
            }
            }  // fin de la boucle des 2 passes (CTDA puis fallback sans CTDA)
        }
    } catch (...) {
        LOG("Scanner: exception while scanning quest objectives");
    }

    // --- Marqueur personnalisé de la carte (touche P) ---
    if (g_customMarkerActive && (g_customMarkerPos.x != 0 || g_customMarkerPos.y != 0)) {
        // Distance 3D complete (style f4access), coherente avec le scanner principal.
        auto diff = g_customMarkerPos - playerPos;
        float dist = diff.Length();

        ScannedObject obj;
        obj.formID = g_customMarkerFormID;  // FormID réel du marqueur de carte
        obj.name = TR("Marker") + L": " + g_customMarkerName;
        obj.distance = dist;
        obj.zDiff = g_customMarkerPos.z - playerPos.z;
        obj.lastKnownPos = g_customMarkerPos;
        obj.category = kCatQuests;
        obj.locked = false;
        obj.empty = false;
        obj.dead = false;

        g_scannedAll.push_back(std::move(obj));
        LOG("Scanner: custom marker '{}' at dist={:.0f}", WStringToUtf8(g_customMarkerName), dist);
    }

    // --- Scanner les Locations (marqueurs de carte) en extérieur ---
    if (!isInterior) {
        try {
            auto* worldSpace = player->GetWorldspace();
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (worldSpace && dataHandler) {
                auto scanLocations = [&](RE::TESObjectCELL* persistentCell) {
                    if (!persistentCell) return;
                    for (auto& refPtr : persistentCell->GetRuntimeData().references) {
                        auto* ref = refPtr.get();
                        if (!ref) continue;
                        auto* extraMarker = ref->extraList.GetByType<RE::ExtraMapMarker>();
                        if (!extraMarker || !extraMarker->mapData) continue;

                        // Ignorer les markers desactives (kInitiallyDisabled + ExtraEnableStateParent).
                        // Camps CW, Fort Garde-l'Aube : disabled tant que leur quete n'a pas appele Enable().
                        if (ref->IsDisabled() || ref->IsMarkedForDeletion()) continue;

                        auto* mapData = extraMarker->mapData;
                        // On affiche les lieux même non découverts pour l'accessibilité :
                        // les joueurs aveugles ne peuvent pas explorer visuellement, donc le
                        // scanner doit leur révéler ce qu'il y a autour.
                        // "Decouvert" = kCanTravelTo (bascule a 1 quand le joueur entre dans
                        // le rayon de decouverte). kVisible est pre-set dans les ESM pour les
                        // grandes villes et camps, donc ne reflete pas l'etat de decouverte.
                        bool discovered = mapData->flags.any(RE::MapMarkerData::Flag::kCanTravelTo);

                        const char* rawName = mapData->locationName.GetFullName();
                        if (!rawName || !*rawName) continue;

                        auto refPos = GetStablePosition(ref);
                        // Distance 3D complete (style f4access), coherente avec le scanner principal.
                        auto diff = refPos - playerPos;
                        float dist = diff.Length();

                        // Seulement dans un rayon de ~15000 unités (comme la boussole étendue)
                        if (dist > 15000.0f) continue;

                        // Éviter les doublons par FormID
                        bool dup = false;
                        for (auto& existing : g_scannedAll) {
                            if (existing.category == kCatLocations && existing.formID == ref->GetFormID()) {
                                dup = true;
                                break;
                            }
                        }
                        if (dup) continue;

                        RE::MARKER_TYPE markerType = mapData->type.get();
                        std::wstring typeName = GetMarkerTypeName(markerType);

                        ScannedObject obj;
                        obj.formID = ref->GetFormID();
                        obj.name = Utf8ToWString(rawName);
                        // Si le type est generique ("Location"), on ne l'ajoute pas
                        // pour eviter le bruit. On compare contre la version traduite
                        // ET la version anglaise (cle source) pour rester robuste.
                        if (!typeName.empty() && typeName != TR("Location") && typeName != L"Location") {
                            obj.name += L" (";
                            obj.name += typeName;
                            obj.name += L")";
                        }
                        if (!discovered) {
                            obj.name += L" (undiscovered)";
                        }
                        obj.distance = dist;
                        obj.zDiff = refPos.z - playerPos.z;
                        obj.lastKnownPos = refPos;
                        obj.category = kCatLocations;
                        obj.isCellDoor = mapData->flags.any(RE::MapMarkerData::Flag::kCanTravelTo);

                        g_scannedAll.push_back(std::move(obj));
                    }
                };

                // Scanner le worldspace du joueur
                if (worldSpace->persistentCell) {
                    scanLocations(worldSpace->persistentCell);
                }

                // Calculer la racine du worldspace du joueur (Tamriel, Solstheim,
                // Sovngarde...). On ne scannera QUE les worldspaces qui partagent
                // la meme racine, pour eviter que des markers de Solstheim ou de
                // Tamriel apparaissent quand le joueur est dans Sovngarde (bug
                // rapporte par Josh).
                auto* playerRoot = worldSpace;
                while (playerRoot->parentWorld) playerRoot = playerRoot->parentWorld;

                // Scanner les autres worldspaces (villes, sous-worldspaces) de la
                // meme racine uniquement.
                auto& worldSpaces = dataHandler->GetFormArray<RE::TESWorldSpace>();
                for (auto* ws : worldSpaces) {
                    if (!ws || ws == worldSpace) continue;
                    if (!ws->persistentCell) continue;
                    // Remonter la hierarchie parentWorld du ws candidat
                    auto* wsRoot = ws;
                    while (wsRoot->parentWorld) wsRoot = wsRoot->parentWorld;
                    if (wsRoot != playerRoot) continue;  // racine differente → skip
                    scanLocations(ws->persistentCell);
                }
            }
        } catch (...) {
            LOG("Scanner: exception while scanning locations");
        }
    }

    // Trier : en intérieur, même niveau Z d'abord puis par distance
    std::sort(g_scannedAll.begin(), g_scannedAll.end(),
        [isInterior](const ScannedObject& a, const ScannedObject& b) {
            if (isInterior) {
                bool aSameLevel = std::abs(a.zDiff) <= 256.0f;
                bool bSameLevel = std::abs(b.zDiff) <= 256.0f;
                if (aSameLevel != bSameLevel) return aSameLevel;
            }
            return a.distance < b.distance;
        });

    // Appliquer le filtre de catégorie courante
    ApplyCategoryFilter();

    LOG("Scanner: {} total objects scanned", g_scannedAll.size());

    // Log détaillé : premiers objets de chaque catégorie
    for (int cat = 0; cat < kCatCOUNT; cat++) {
        int logged = 0;
        for (auto& obj : g_scannedAll) {
            if (obj.category == cat && logged < 5) {
                LOG("Scanner: [{}] '{}' dist={:.0f} formID={:08X}",
                    WStringToUtf8(g_categoryNames[cat]),
                    WStringToUtf8(obj.name),
                    obj.distance, obj.formID);
                logged++;
            }
        }
    }
}

// --- Scanner (depuis un input, via AddTask pour le game thread) ---
static void DoScan(ScanAction postAction = kScanOnly) {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;

    g_pendingScanAction = postAction;

    task->AddTask([]() {
        DoScanInternal();

        // Exécuter l'action demandée après le scan — on est déjà dans un AddTask
        // (thread principal) donc on appelle directement les Impl pour éviter de
        // re-queue inutilement un nouveau AddTask.
        switch (g_pendingScanAction) {
            case kScanThenNextCat: ScannerNextCategoryImpl(); break;
            case kScanThenPrevCat: ScannerPrevCategoryImpl(); break;
            case kScanThenNextObj: ScannerNextObjectImpl(); break;
            case kScanThenPrevObj: ScannerPrevObjectImpl(); break;
            default: {
                // Scan manuel : annoncer le résultat
                int count = static_cast<int>(g_scannedFiltered.size());
                std::wstring msg = std::to_wstring(count) + L" " + GetCategoryNameSpoken(g_scanCategory);
                if (count > 0) {
                    msg += L". " + FormatObjectAnnounce(*g_scannedFiltered[0]);
                } else {
                    msg += L" " + TR("nearby");
                }
                Speak(msg);
                break;
            }
        }
    });
}

// --- Vérification live d'un objet (comme FO4 Access) ---
// Retire les objets invalides ou qui ont changé de catégorie, met à jour empty/distance
static void RefreshFilteredList() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto playerPos = player ? player->GetPosition() : RE::NiPoint3{0, 0, 0};

    // Parcourir g_scannedAll et mettre à jour l'état live
    // Style f4access : les objets non charges sont RETIRES de la liste, pas affiches
    // avec une position en cache (qui peut etre obsolete de plusieurs minutes).
    // Exception : les quetes gardent leur lastKnownPos car la cible cross-cell est
    // souvent dans une cellule non chargee (intention : "entree du lieu" via worldLocMarker).
    for (auto& obj : g_scannedAll) {
        auto* form = RE::TESForm::LookupByID(obj.formID);
        if (!form) {
            // FormID invalide (FF* runtime supprime par le moteur, ou ref deleted)
            // On retire de la liste plutot que d'afficher un fantome.
            obj.category = kCatAll;
            obj.formID = 0;
            continue;
        }
        auto* ref = form->AsReference();
        if (!ref) continue;

        // Objet ramassé, supprimé ou désactivé → retirer de la liste
        // Exception : les quêtes ET les locations gardent leur cache.
        // - Quêtes : cible cross-cell souvent dans une cellule non chargée
        // - Locations : markers de carte distants (villes, donjons à 8000+ unites)
        //   par essence non-3D-loaded, on garde leur position via lastKnownPos.
        // - Pousses du Primarbor (0x1CB81) : disabled jusqu'a ce que le
        //   joueur s'approche d'un trigger (script T03), mais on veut que
        //   le joueur puisse s'y diriger a l'avance via le scanner.
        if (obj.category != kCatQuests && obj.category != kCatLocations) {
            const auto* base = ref->GetBaseObject();
            const bool isEldergleamRoot =
                base && base->GetFormID() == 0x0001CB81;
            if (!isEldergleamRoot) {
                if (ref->IsDisabled() || ref->IsDeleted() || !ref->Is3DLoaded()) {
                    obj.category = kCatAll;
                    obj.formID = 0;
                    continue;
                }
            } else {
                // Pour les racines Eldergleam, on tolere disabled.
                if (ref->IsDeleted()) {
                    obj.category = kCatAll;
                    obj.formID = 0;
                    continue;
                }
            }
        }

        // Recalculer la distance en temps réel
        if (player) {
            // Quetes et locations utilisent lastKnownPos (markers distants ou
            // refs cross-cell non-3D-loaded, lecture impossible via GetStablePosition).
            const bool useCache = (obj.category == kCatQuests || obj.category == kCatLocations) &&
                                  (obj.lastKnownPos.x != 0 || obj.lastKnownPos.y != 0);
            if (useCache) {
                auto diff = playerPos - obj.lastKnownPos;
                obj.distance = diff.Length();
                obj.zDiff = obj.lastKnownPos.z - playerPos.z;
            } else {
                // GetStablePosition evite les sauts causes par Havok pour les
                // objets jetes/rebondissants (centre de masse vs centre visuel).
                auto refPos = GetStablePosition(ref);
                auto diff = playerPos - refPos;
                obj.distance = diff.Length();
                obj.zDiff = refPos.z - playerPos.z;
            }
        }

        // Re-catégoriser (un PNJ vivant peut être mort maintenant)
        // Ne pas écraser kCatQuests ni kCatLocations — ces catégories sont gérées séparément
        if (obj.category != kCatQuests && obj.category != kCatLocations) {
            obj.category = CategorizeRef(*ref);
        }

        // Re-vérifier empty (base TESContainer + countDelta des ContainerChanges)
        if (obj.category == kCatContainers || obj.category == kCatCorpses) {
            try { obj.empty = IsRefContainerEmpty(ref); } catch (...) {}
        }

        // Mettre à jour le state des piliers/anneaux puzzle
        // Rafraîchir l'état des piliers / anneaux (position01/02/03 peut changer).
        // Le TYPE est mis en cache par GetScriptTypeCached, donc zéro requête VM pour
        // les refs qu'on a déjà identifiées comme non-pilier (la majorité).
        std::string objNameUtf8 = WStringToUtf8(obj.name);
        if (objNameUtf8.find("Pilier") != std::string::npos || objNameUtf8.find("Pillar") != std::string::npos ||
            objNameUtf8.find("nneau") != std::string::npos || objNameUtf8.find("Ring") != std::string::npos ||
            objNameUtf8.find("Disk") != std::string::npos) {
            ScriptTypeKind stk = GetScriptTypeCached(*ref);
            if (stk == ScriptTypeKind::PillarStandard ||
                stk == ScriptTypeKind::PillarInt ||
                stk == ScriptTypeKind::RingHallOfStories) {
                auto* vm = RE::SkyrimVM::GetSingleton();
                if (vm && vm->impl) {
                    auto* policy = vm->impl->GetObjectHandlePolicy();
                    if (policy) {
                        auto handle = policy->GetHandleForObject(
                            static_cast<RE::VMTypeID>(RE::FormType::Reference), ref);
                        const char* sName = (stk == ScriptTypeKind::PillarStandard) ? "defaultPuzzlePillarScript"
                                          : (stk == ScriptTypeKind::PillarInt)      ? "intPuzzlePillarScript"
                                                                                    : "HallofStoriesDiskScript";
                        RE::BSTSmartPointer<RE::BSScript::Object> scriptObj;
                        if (vm->impl->FindBoundObject(handle, sName, scriptObj) && scriptObj) {
                            std::string state = scriptObj->currentState.c_str();
                            int posNum = 0;
                            if (state == "position01") posNum = 1;
                            else if (state == "position02") posNum = 2;
                            else if (state == "position03") posNum = 3;

                            const char* symbol = "unknown";
                            if (state == "busy") {
                                symbol = "turning";
                            } else if (posNum > 0) {
                                if (stk == ScriptTypeKind::PillarStandard || stk == ScriptTypeKind::PillarInt) {
                                    if (posNum == 1) symbol = "Eagle";
                                    else if (posNum == 2) symbol = "Snake";
                                    else if (posNum == 3) symbol = "Whale";
                                } else if (stk == ScriptTypeKind::RingHallOfStories) {
                                    auto* linkedRef = ref->GetLinkedRef(nullptr);
                                    if (linkedRef) {
                                        RE::FormID kid = linkedRef->GetFormID();
                                        for (size_t di = 0; di < sizeof(g_doorSymbolTable)/sizeof(g_doorSymbolTable[0]); di++) {
                                            if (g_doorSymbolTable[di].keyhole == kid) {
                                                if (posNum == 1) symbol = g_doorSymbolTable[di].s1;
                                                else if (posNum == 2) symbol = g_doorSymbolTable[di].s2;
                                                else if (posNum == 3) symbol = g_doorSymbolTable[di].s3;
                                                break;
                                            }
                                        }
                                    }
                                    if (std::string(symbol) == "unknown") {
                                        if (posNum == 1) symbol = "Position 1";
                                        else if (posNum == 2) symbol = "Position 2";
                                        else if (posNum == 3) symbol = "Position 3";
                                    }
                                }
                            }

                            // Reconstruire le nom avec le symbole actuel
                            const char* baseName = ref->GetDisplayFullName();
                            if (baseName) {
                                obj.name = Utf8ToWString((std::string(baseName) + " (" + symbol + ")").c_str());
                            }
                        }
                    }
                }
            }
        }

        // Re-vérifier locked
        auto* base = ref->GetBaseObject();
        if (base && (base->Is(RE::FormType::Door) || base->Is(RE::FormType::Container))) {
            obj.locked = ref->IsLocked();
        }

        // Re-vérifier l'état destructible (santé peut avoir diminué depuis le
        // scan ; objet peut avoir été détruit entre temps).
        if (obj.isDestructible) {
            if (ref->GetFormFlags() & RE::TESForm::RecordFlags::kDestroyed) {
                obj.isDestructible = false;
                obj.destructibleLabel = nullptr;
                obj.destructibleHealthPercent = 0;
            } else {
                obj.destructibleHealthPercent = ReadDestructibleHealthPercent(*ref, base);
            }
        }
    }

    // Re-filtrer par catégorie courante
    ApplyCategoryFilter();
}

// --- Naviguer : objet suivant/précédent ---
//
// Wrapper public : appelé depuis le thread clavier. On délègue tout le travail
// (lecture de refs, cellules, mise à jour de g_scannedFiltered) au thread
// principal via AddTask pour éviter les races avec le cell streaming et le scan
// automatique qui peut réallouer g_scannedAll à tout moment.
static void ScannerNextObject() {
    LOG("InputDiag: ScannerNextObject ENTRY");
    auto* task = SKSE::GetTaskInterface();
    if (!task) { ScannerNextObjectImpl(); return; }
    task->AddTask([]() { ScannerNextObjectImpl(); });
}

static void ScannerNextObjectImpl() {
    if (NeedsRescan()) {
        DoScan(kScanThenNextObj);
        return;
    }
    // Rafraîchir l'état des objets (PNJ mort, conteneur vidé, etc.)
    RefreshFilteredList();
    if (g_scannedFiltered.empty()) {
        Speak(TR("No objects in this category"));
        return;
    }
    g_scanIndex++;
    if (g_scanIndex >= static_cast<int>(g_scannedFiltered.size()))
        g_scanIndex = 0;
    // Recalculer la distance en temps réel
    auto& nextObj = *g_scannedFiltered[g_scanIndex];
    auto* p = RE::PlayerCharacter::GetSingleton();
    if (p) {
        auto playerPos = p->GetPosition();
        auto* refForm = RE::TESForm::LookupByID(nextObj.formID);
        auto* ref = refForm ? refForm->AsReference() : nullptr;
        if (ref && ref->Is3DLoaded()) {
            auto refPos = GetStablePosition(ref);
            auto diff = playerPos - refPos;
            nextObj.distance = diff.Length();
            nextObj.zDiff = refPos.z - playerPos.z;
        } else if (nextObj.lastKnownPos.x != 0 || nextObj.lastKnownPos.y != 0) {
            auto diff = playerPos - nextObj.lastKnownPos;
            nextObj.distance = diff.Length();
            nextObj.zDiff = nextObj.lastKnownPos.z - playerPos.z;
        }
    }
    std::wstring pos = L". " + std::to_wstring(g_scanIndex + 1) + L" of " + std::to_wstring(g_scannedFiltered.size());
    LOG("Scanner NEXT: idx={}/{} cat={} name='{}' formID={:08X} dist={:.0f}",
        g_scanIndex + 1, g_scannedFiltered.size(),
        WStringToUtf8(g_categoryNames[g_scanCategory]),
        WStringToUtf8(nextObj.name), nextObj.formID, nextObj.distance);
    Speak(FormatObjectAnnounce(nextObj) + pos);
}

// Wrapper public : même pattern que ScannerNextObject, tout est délégué au
// thread principal via AddTask.
static void ScannerPrevObject() {
    LOG("InputDiag: ScannerPrevObject ENTRY");
    auto* task = SKSE::GetTaskInterface();
    if (!task) { ScannerPrevObjectImpl(); return; }
    task->AddTask([]() { ScannerPrevObjectImpl(); });
}

static void ScannerPrevObjectImpl() {
    if (NeedsRescan()) {
        DoScan(kScanThenPrevObj);
        return;
    }
    // Rafraîchir l'état des objets
    RefreshFilteredList();
    if (g_scannedFiltered.empty()) {
        Speak(TR("No objects in this category"));
        return;
    }
    g_scanIndex--;
    if (g_scanIndex < 0)
        g_scanIndex = static_cast<int>(g_scannedFiltered.size()) - 1;
    // Recalculer la distance en temps réel
    auto& prevObj = *g_scannedFiltered[g_scanIndex];
    auto* p = RE::PlayerCharacter::GetSingleton();
    if (p) {
        auto playerPos = p->GetPosition();
        auto* refForm = RE::TESForm::LookupByID(prevObj.formID);
        auto* ref = refForm ? refForm->AsReference() : nullptr;
        if (ref && ref->Is3DLoaded()) {
            auto refPos = GetStablePosition(ref);
            auto diff = playerPos - refPos;
            prevObj.distance = diff.Length();
            prevObj.zDiff = refPos.z - playerPos.z;
        } else if (prevObj.lastKnownPos.x != 0 || prevObj.lastKnownPos.y != 0) {
            auto diff = playerPos - prevObj.lastKnownPos;
            prevObj.distance = diff.Length();
            prevObj.zDiff = prevObj.lastKnownPos.z - playerPos.z;
        }
    }
    std::wstring pos = L". " + std::to_wstring(g_scanIndex + 1) + L" of " + std::to_wstring(g_scannedFiltered.size());
    LOG("Scanner PREV: idx={}/{} cat={} name='{}' formID={:08X} dist={:.0f}",
        g_scanIndex + 1, g_scannedFiltered.size(),
        WStringToUtf8(g_categoryNames[g_scanCategory]),
        WStringToUtf8(prevObj.name), prevObj.formID, prevObj.distance);
    Speak(FormatObjectAnnounce(prevObj) + pos);
}

// --- Changer de catégorie (rescan + filtre, saute les catégories vides) ---
static void ScannerNextCategory() {
    LOG("InputDiag: ScannerNextCategory ENTRY");
    // Toujours rescanner au changement de catégorie (un PNJ peut être mort entre-temps)
    DoScan(kScanThenNextCat);
}

static void ScannerNextCategoryImpl() {
    g_scanSubcategory = ScanSubcategory::All;  // reset sous-catégorie
    for (int i = 0; i < kCatCOUNT; i++) {
        g_scanCategory = static_cast<ScanCategory>((g_scanCategory + 1) % kCatCOUNT);
        ApplyCategoryFilter();
        if (!g_scannedFiltered.empty() || g_scanCategory == kCatAll) break;
    }

    // Toujours repartir du premier objet (le plus proche) au changement de catégorie
    g_scanIndex = g_scannedFiltered.empty() ? -1 : 0;

    std::wstring msg = GetCategoryNameSpoken(g_scanCategory);

    if (!g_scannedFiltered.empty()) {
        msg += L". " + FormatObjectAnnounce(*g_scannedFiltered[0]);
    }

    Speak(msg);
}

static void ScannerPrevCategory() {
    LOG("InputDiag: ScannerPrevCategory ENTRY");
    DoScan(kScanThenPrevCat);
}

static void ScannerPrevCategoryImpl() {
    g_scanSubcategory = ScanSubcategory::All;  // reset sous-catégorie
    for (int i = 0; i < kCatCOUNT; i++) {
        g_scanCategory = static_cast<ScanCategory>((g_scanCategory - 1 + kCatCOUNT) % kCatCOUNT);
        ApplyCategoryFilter();
        if (!g_scannedFiltered.empty() || g_scanCategory == kCatAll) break;
    }

    // Toujours repartir du premier objet (le plus proche) au changement de catégorie
    g_scanIndex = g_scannedFiltered.empty() ? -1 : 0;

    std::wstring msg = GetCategoryNameSpoken(g_scanCategory);

    if (!g_scannedFiltered.empty()) {
        msg += L". " + FormatObjectAnnounce(*g_scannedFiltered[0]);
    }

    Speak(msg);
}

// --- Annoncer l'objet courant (avec distance recalculée en temps réel) ---
// Forward declarations
static RE::NiPoint3 GetActorCenter(RE::Actor* actor);
static void AimAtPosition(RE::PlayerCharacter* player, const RE::NiPoint3& targetPos, bool compensateGravity, const RE::NiPoint3* targetVelocity);

// Rafraîchir le target de quête courant (conditions CTDA ont pu changer)
// IMPORTANT : utilise la MÊME logique que le scanner principal pour les distances :
//   - Cross-cell : résolution via worldLocMarker (l'entrée du lieu en extérieur)
//   - Distance 2D (X/Y) au lieu de 3D, comme la carte
// Sinon on mélange les coordonnées de cellules intérieures et extérieures (référentiels
// différents) → distances absurdes (47000+ unités au lieu de 5700).
static void RefreshQuestTarget(ScannedObject& obj) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    auto playerPos = player->GetPosition();
    auto* playerCell = player->GetParentCell();
    if (!playerCell) return;

    auto& objectives = REL::RelocateMemberIfNewer<RE::BSTArray<RE::BGSInstancedQuestObjective>>(
        SKSE::RUNTIME_SSE_1_6_629, player, 0x580, 0x588);

    for (auto& inst : objectives) {
        if (!inst.Objective) continue;
        if (inst.InstanceState != RE::QUEST_OBJECTIVE_STATE::kDisplayed) continue;
        auto* quest = inst.Objective->ownerQuest;
        if (!quest || !quest->IsActive()) continue;

        auto rawFlags = quest->data.flags.underlying();
        if ((rawFlags & 0x20) == 0) continue;

        auto* questObj = inst.Objective;
        std::string objText = questObj->displayText.c_str();
        std::wstring objNameW = Utf8ToWString(objText.c_str());
        if (objNameW != obj.name) continue;

        // Trouvé l'objectif correspondant — chercher le bon target
        // Système en 2 passes (identique au scanner principal) :
        //   PASS 0 : target avec CTDA vérifiées (comportement historique)
        //   PASS 1 : retenter sans CTDA si aucun target n'a passé (ex: Pierre de dragon)
        bool refreshed = false;
        for (int pass = 0; pass < 2 && !refreshed; pass++) {
            const bool ignoreCTDA = (pass == 1);
        for (uint32_t t = 0; t < questObj->numTargets; t++) {
            auto* target = questObj->targets[t];
            if (!target) continue;

            RE::ObjectRefHandle refHandle;
            quest->CreateRefHandleByAliasID(refHandle, target->alias);
            if (!refHandle) continue;
            auto refPtr = refHandle.get();
            if (!refPtr) continue;
            auto* targetRef = refPtr.get();
            if (!targetRef) continue;

            // Vérifier les conditions CTDA (sauf en PASS 1 fallback)
            if (!ignoreCTDA && target->conditions.head != nullptr) {
                if (!target->conditions.IsTrue(player, targetRef)) continue;
            }

            // === Résolution de la position : même logique que le scanner principal ===
            auto refPos = GetStablePosition(targetRef);
            auto* refCell = targetRef->GetParentCell();
            RE::NiPoint3 actualPos = refPos;

            // Cas piege : parentCell null quand la cellule du PNJ n'est pas
            // attachee cote joueur (PNJ dans un interieur lointain). Sans cellule,
            // on saute la resolution interior->worldLocMarker et on garde des
            // coordonnees locales aberrantes. Fix : utiliser GetEditorLocation
            // pour recuperer la cellule editeur du ref independamment de parentCell.
            if (!refCell) {
                RE::NiPoint3 edPos{}, edRot{};
                RE::TESForm* worldOrCell = nullptr;
                if (targetRef->GetEditorLocation(edPos, edRot, worldOrCell, nullptr)) {
                    if (worldOrCell) {
                        if (auto* ws = worldOrCell->As<RE::TESWorldSpace>()) {
                            // Position editeur deja en worldspace exterieur :
                            // utilisable directement.
                            actualPos = edPos;
                            (void)ws;
                            LOG("Scanner: refresh editor location in worldspace, pos=({:.0f},{:.0f},{:.0f})",
                                actualPos.x, actualPos.y, actualPos.z);
                        } else if (auto* cell = worldOrCell->As<RE::TESObjectCELL>()) {
                            // Cellule editeur trouvee : on l'utilise pour la
                            // resolution worldLocMarker ci-dessous.
                            refCell = cell;
                            LOG("Scanner: refresh parentCell null, using editor cell '{}'",
                                cell->GetName() ? cell->GetName() : "?");
                        }
                    }
                }
            }

            // Si la cible est dans une cellule différente, résoudre via la même logique
            // que le scan principal : PASS 1 door search → PASS 2 worldLocMarker.
            // On ne fait pas le compass fallback ici (seulement dans le scan principal).
            if (refCell && refCell != playerCell) {
                bool isInterior = refCell->IsInteriorCell();
                bool resolved = false;

                if (isInterior) {
                    // === PASS 1 : recherche de porte directe dans les cellules chargées ===
                    // Si le joueur est dans Whiterun et la cible dans Fort-Dragon, on trouve
                    // la porte de Fort-Dragon dans les cellules attachées → destination précise.
                    RE::TESObjectREFR* bestDoor = nullptr;
                    float bestDoorDist = 999999.0f;
                    auto searchDoorsForCell = [&](RE::TESObjectCELL* searchCell) {
                        if (!searchCell) return;
                        for (auto& doorHandle : searchCell->GetRuntimeData().references) {
                            auto doorPtr = doorHandle.get();
                            if (!doorPtr) continue;
                            auto* doorBase = doorPtr->GetBaseObject();
                            if (!doorBase || doorBase->GetFormType() != RE::FormType::Door) continue;
                            auto* extraTele = doorPtr->extraList.GetByType<RE::ExtraTeleport>();
                            if (!extraTele || !extraTele->teleportData) continue;
                            auto linkedDoor = extraTele->teleportData->linkedDoor.get();
                            if (!linkedDoor) continue;
                            auto* destCell = linkedDoor->GetParentCell();
                            if (!destCell || destCell != refCell) continue;
                            auto doorPos = doorPtr->GetPosition();
                            float doorDist = (playerPos - doorPos).Length();
                            if (doorDist < bestDoorDist) {
                                bestDoorDist = doorDist;
                                bestDoor = doorPtr;
                            }
                        }
                    };
                    searchDoorsForCell(playerCell);
                    bool playerInInterior = playerCell && playerCell->IsInteriorCell();
                    if (!bestDoor && !playerInInterior) {
                        if (auto* tes = RE::TES::GetSingleton()) {
                            if (auto* gridCells = tes->gridCells) {
                                for (uint32_t gx = 0; gx < gridCells->length && !bestDoor; gx++) {
                                    for (uint32_t gy = 0; gy < gridCells->length && !bestDoor; gy++) {
                                        auto* gc = gridCells->GetCell(gx, gy);
                                        if (gc && gc->IsAttached() && gc != playerCell) searchDoorsForCell(gc);
                                    }
                                }
                            }
                        }
                        auto* ws = player->GetWorldspace();
                        if (!bestDoor && ws && ws->persistentCell) {
                            searchDoorsForCell(ws->persistentCell);
                        }
                    }
                    if (bestDoor) {
                        actualPos = bestDoor->GetPosition();
                        resolved = true;
                        LOG("Scanner: refresh redirected to door FormID={:08X} dist={:.0f}",
                            bestDoor->GetFormID(), bestDoorDist);
                    }

                    // === PASS 2 : worldLocMarker (fallback quand porte pas chargée) ===
                    if (!resolved) {
                        auto* playerWS = player->GetWorldspace();
                        auto* targetLocation = refCell->GetLocation();
                        for (auto* loc = targetLocation; loc && !resolved; loc = loc->parentLoc) {
                            if (loc->worldLocMarker) {
                                auto markerPtr = loc->worldLocMarker.get();
                                if (markerPtr) {
                                    auto* markerCell = markerPtr->GetParentCell();
                                    if (markerCell && markerCell->IsInteriorCell()) {
                                        LOG("Scanner: refresh skip interior worldLocMarker loc='{}' (markerCell='{}')",
                                            loc->GetFullName() ? loc->GetFullName() : "?",
                                            markerCell->GetName() ? markerCell->GetName() : "?");
                                        continue;
                                    }
                                    // Skip si worldspace incompatible avec le joueur,
                                    // ou si le marker n'a pas de worldspace (cellule
                                    // non chargee : on ne peut pas valider la coherence
                                    // des coordonnees).
                                    auto* markerWS = markerPtr->GetWorldspace();
                                    if (playerWS) {
                                        if (!markerWS) {
                                            LOG("Scanner: refresh skip worldLocMarker loc='{}' has null worldspace",
                                                loc->GetFullName() ? loc->GetFullName() : "?");
                                            continue;
                                        }
                                        if (!AreWorldspacesCompatible(markerWS, playerWS)) {
                                            auto* mRoot = GetCoordinateRoot(markerWS);
                                            auto* pRoot = GetCoordinateRoot(playerWS);
                                            LOG("Scanner: refresh skip worldLocMarker loc='{}' in incompatible worldspace ('{}' root='{}' vs '{}' root='{}')",
                                                loc->GetFullName() ? loc->GetFullName() : "?",
                                                markerWS->GetName() ? markerWS->GetName() : "?",
                                                (mRoot && mRoot->GetName()) ? mRoot->GetName() : "?",
                                                playerWS->GetName() ? playerWS->GetName() : "?",
                                                (pRoot && pRoot->GetName()) ? pRoot->GetName() : "?");
                                            continue;
                                        }
                                    }
                                    actualPos = markerPtr->GetPosition();
                                    resolved = true;
                                    LOG("Scanner: refresh resolved via worldLocMarker loc='{}' pos=({:.0f},{:.0f},{:.0f})",
                                        loc->GetFullName() ? loc->GetFullName() : "?",
                                        actualPos.x, actualPos.y, actualPos.z);
                                }
                            }
                        }
                    }

                    if (!resolved) {
                        LOG("Scanner: refresh - no door or worldLocMarker for cross-cell target, distance may be wrong");
                    }
                }
                // Sinon (refCell extérieure différente du joueur) : actualPos = refPos direct (cas normal)
            }

            // Distance 3D complete (style f4access), coherente avec le scan initial
            // et le reste du scanner. Evite les sauts 2D/3D dans les refresh.
            auto diff = actualPos - playerPos;
            float dist3D = diff.Length();

            obj.formID = targetRef->GetFormID();
            obj.lastKnownPos = actualPos;
            obj.distance = dist3D;
            obj.zDiff = actualPos.z - playerPos.z;
            LOG("Scanner: quest target refreshed to FormID={:08X} dist={:.0f} (3D, actualPos=({:.0f},{:.0f},{:.0f})){}",
                obj.formID, dist3D, actualPos.x, actualPos.y, actualPos.z,
                ignoreCTDA ? " [PASS 1 — CTDA ignored]" : "");
            refreshed = true;
            break;  // sort de la boucle des targets
        }
        }  // fin de la boucle des 2 passes
        return;  // objectif trouvé (avec ou sans target valide) → on arrête de chercher
    }
}

static void ScannerAnnounceCurrent() {
    if (g_scannedFiltered.empty() || g_scanIndex < 0) {
        Speak(TR("No object selected"));
        return;
    }

    auto& obj = *g_scannedFiltered[g_scanIndex];

    // Rafraîchir le target de quête si les conditions ont changé (nouveau stage)
    if (obj.category == kCatQuests) {
        RefreshQuestTarget(obj);
    }

    // Vérifier que l'objet est encore valide
    auto* refForm = RE::TESForm::LookupByID(obj.formID);
    auto* ref = (refForm) ? refForm->AsReference() : nullptr;

    if (ref && (ref->IsDisabled() || ref->IsDeleted())) {
        ref = nullptr;  // traiter comme invalide
    }

    // Cas spécial : les objets dynamiques (FormID FF*) disparaissent quand le
    // joueur les ramasse ou quand ils sont détruits. Pour ces refs, !ref =
    // objet parti pour de bon (contrairement à une ref statique dans une cellule
    // déchargée qui pourrait revenir). On invalide l'entrée dans g_scannedAll,
    // on la retire du filtre courant, et on passe automatiquement au suivant.
    // Exclusion : les cibles de quete (kCatQuests) peuvent avoir un FormID FF
    // (instanciees par script Papyrus, ex: Pierre de Dragon FF000DA3) sans que
    // !ref signifie "ramasse" — la cellule cible n'est juste pas streamee. Pour
    // ces cibles, on a deja le mecanisme PASS 1/PASS 2 + boussole qui resout la
    // position via worldLocMarker. Le check "Object gone" n'a de sens que pour
    // les items du monde reellement disparus.
    if (!ref && (obj.formID >> 24) == 0xFF && obj.category != kCatQuests) {
        obj.formID = 0;
        obj.category = kCatAll;
        g_scannedFiltered.erase(g_scannedFiltered.begin() + g_scanIndex);

        if (g_scannedFiltered.empty()) {
            g_scanIndex = -1;
            Speak(TR("Object gone") + L". " + TR("No more objects in this category"));
            return;
        }
        // Rester sur le même index (l'élément suivant a glissé à cette position),
        // sauf si on était sur le dernier — alors wrap au début.
        if (g_scanIndex >= static_cast<int>(g_scannedFiltered.size())) {
            g_scanIndex = 0;
        }
        auto& nextObj = *g_scannedFiltered[g_scanIndex];
        std::wstring posStr = L". " + std::to_wstring(g_scanIndex + 1) + L" of " +
                              std::to_wstring(g_scannedFiltered.size());
        Speak(TR("Object gone") + L". " + FormatObjectAnnounce(nextObj) + posStr);
        return;
    }

    // Si l'objet n'est plus accessible (cellule déchargée, objet dynamique FF*),
    // utiliser la boussole pour l'orientation (comme l'autowalk)
    if (!ref) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player && (obj.lastKnownPos.x != 0 || obj.lastKnownPos.y != 0)) {
            auto playerPos = player->GetPosition();
            auto diff = playerPos - obj.lastKnownPos;
            obj.distance = diff.Length();
            obj.zDiff = obj.lastKnownPos.z - playerPos.z;

            // Pour les quêtes, utiliser le heading de la boussole (pointe vers la bonne porte)
            bool usedCompass = false;
            if (obj.category == kCatQuests) {
                auto* ui = RE::UI::GetSingleton();
                if (ui) {
                    auto hudMenu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
                    if (hudMenu && hudMenu->uiMovie) {
                        RE::GFxValue hudRoot;
                        if (hudMenu->uiMovie->GetVariable(&hudRoot, "_root.HUDMovieBaseInstance") && SafeIsObject(hudRoot)) {
                            RE::GFxValue dataArr;
                            if (hudRoot.GetMember("CompassTargetDataA", &dataArr) && SafeIsArray(dataArr)) {
                                RE::GFxValue questTypeVal, questDoorTypeVal;
                                float questType = -1, questDoorType = -1;
                                if (hudRoot.GetMember("CompassMarkerQuest", &questTypeVal) && SafeIsNumber(questTypeVal))
                                    questType = static_cast<float>(SafeGetNumber(questTypeVal));
                                if (hudRoot.GetMember("CompassMarkerQuestDoor", &questDoorTypeVal) && SafeIsNumber(questDoorTypeVal))
                                    questDoorType = static_cast<float>(SafeGetNumber(questDoorTypeVal));

                                uint32_t arrSize = SafeGetArraySize(dataArr);
                                for (uint32_t i = 0; i + 3 < arrSize; i += 4) {
                                    RE::GFxValue headingVal, typeVal;
                                    dataArr.GetElement(i, &headingVal);
                                    dataArr.GetElement(i + 2, &typeVal);
                                    if (!SafeIsNumber(typeVal)) continue;
                                    float type = static_cast<float>(SafeGetNumber(typeVal));
                                    if ((type == questType || type == questDoorType) && SafeIsNumber(headingVal)) {
                                        float compassHeading = static_cast<float>(SafeGetNumber(headingVal));
                                        float yaw = compassHeading * 3.14159265f / 180.0f;
                                        player->SetRotationZ(yaw);
                                        player->SetRotationX(0);
                                        auto* camera = RE::PlayerCamera::GetSingleton();
                                        if (camera) {
                                            auto* state = camera->cameraStates[RE::CameraState::kThirdPerson].get();
                                            if (state) {
                                                auto* tps = static_cast<RE::ThirdPersonState*>(state);
                                                tps->freeRotation = {0.f, 0.f};
                                            }
                                        }
                                        usedCompass = true;
                                        LOG("Scanner: quest orient via compass heading={:.1f}", compassHeading);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Fallback : orienter vers la position en cache si pas de boussole
            if (!usedCompass) {
                float dx = obj.lastKnownPos.x - playerPos.x;
                float dy = obj.lastKnownPos.y - playerPos.y;
                float dz = obj.lastKnownPos.z - playerPos.z;
                float yaw = std::atan2(dx, dy);
                float hDist = std::sqrt(dx * dx + dy * dy);
                float pitch = -std::atan2(dz, hDist);

                player->SetRotationZ(yaw);
                player->SetRotationX(pitch);

                auto* camera = RE::PlayerCamera::GetSingleton();
                if (camera) {
                    auto* state = camera->cameraStates[RE::CameraState::kThirdPerson].get();
                    if (state) {
                        auto* tps = static_cast<RE::ThirdPersonState*>(state);
                        tps->freeRotation = {0.f, 0.f};
                    }
                }
            }
        }
        Speak(FormatObjectAnnounce(obj));
        return;
    }

    // Recalculer la distance en temps réel + rotation caméra vers l'objet
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        auto playerPos = player->GetPosition();

        // Pour les quêtes avec cible dans une autre cellule, utiliser lastKnownPos
        // (position de la porte redirigée) + boussole pour l'orientation
        if (obj.category == kCatQuests && ref->GetParentCell() != player->GetParentCell()) {
            auto diff = playerPos - obj.lastKnownPos;
            obj.distance = diff.Length();
            obj.zDiff = obj.lastKnownPos.z - playerPos.z;

            // Orientation via la boussole
            bool usedCompass = false;
            auto* ui = RE::UI::GetSingleton();
            if (ui) {
                auto hudMenu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
                if (hudMenu && hudMenu->uiMovie) {
                    RE::GFxValue hudRoot;
                    if (hudMenu->uiMovie->GetVariable(&hudRoot, "_root.HUDMovieBaseInstance") && SafeIsObject(hudRoot)) {
                        RE::GFxValue dataArr;
                        if (hudRoot.GetMember("CompassTargetDataA", &dataArr) && SafeIsArray(dataArr)) {
                            RE::GFxValue questTypeVal;
                            if (hudRoot.GetMember("CompassMarkerQuest", &questTypeVal) && SafeIsNumber(questTypeVal)) {
                                float questType = static_cast<float>(SafeGetNumber(questTypeVal));
                                for (uint32_t i = 0; i < SafeGetArraySize(dataArr); i++) {
                                    RE::GFxValue entry;
                                    if (!dataArr.GetElement(i, &entry) || !SafeIsObject(entry)) continue;
                                    RE::GFxValue typeVal;
                                    if (!entry.GetMember("type", &typeVal) || !SafeIsNumber(typeVal)) continue;
                                    if (static_cast<float>(SafeGetNumber(typeVal)) != questType) continue;
                                    RE::GFxValue headingVal;
                                    if (!entry.GetMember("heading", &headingVal) || !SafeIsNumber(headingVal)) continue;
                                    float heading = static_cast<float>(SafeGetNumber(headingVal));
                                    float yaw = player->GetAngleZ() + heading;
                                    player->SetRotationZ(yaw);
                                    player->SetRotationX(0.0f);
                                    auto* camera = RE::PlayerCamera::GetSingleton();
                                    if (camera) {
                                        auto* state = camera->cameraStates[RE::CameraState::kThirdPerson].get();
                                        if (state) {
                                            auto* tps = static_cast<RE::ThirdPersonState*>(state);
                                            tps->freeRotation = {0.f, 0.f};
                                        }
                                    }
                                    usedCompass = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            if (!usedCompass) {
                float dx = obj.lastKnownPos.x - playerPos.x;
                float dy = obj.lastKnownPos.y - playerPos.y;
                float yaw = std::atan2(dx, dy);
                player->SetRotationZ(yaw);
                player->SetRotationX(0.0f);
            }
            Speak(FormatObjectAnnounce(obj));
            return;
        }

        // Pour les objets proches, viser le centre (meilleur pour le crosshair)
        auto targetPos = ref->GetPosition();
        auto* actor = ref->As<RE::Actor>();
        if (actor) {
            targetPos = GetActorCenter(actor);
        } else {
            // Utiliser le centre 3D réel (worldBound) si disponible — plus précis pour
            // les objets jetés au sol dont la physique Havok a déplacé le mesh
            auto* node = ref->Get3D();
            if (node && node->worldBound.radius > 0.1f) {
                targetPos = node->worldBound.center;
            } else {
                // Fallback : bounds statiques
                auto boundMin = ref->GetBoundMin();
                auto boundMax = ref->GetBoundMax();
                targetPos.z += (boundMin.z + boundMax.z) * 0.5f;
            }
        }

        // Distance calculee via GetStablePosition (centre visuel du mesh si dispo,
        // sinon GetPosition). Coherent avec le scan principal. Pour les objets
        // physiques qui rebondissent, evite les sauts de distance 3000->500.
        auto basePos = GetStablePosition(ref);
        auto diff = playerPos - basePos;
        obj.distance = diff.Length();
        obj.zDiff = basePos.z - playerPos.z;

        // Rotation caméra vers l'objet depuis les yeux (précis pour le crosshair)
        AimAtPosition(player, targetPos, false, nullptr);
    }

    Speak(FormatObjectAnnounce(obj));
}

// --- Cycler les sous-catégories (touche End) ---
static void ScannerCycleSubcategory() {
    if (!HasSubcategories(g_scanCategory)) {
        Speak(TR("No subcategories"));
        return;
    }

    // Cycle uniquement parmi les sous-catégories applicables à la catégorie courante
    const auto& subs = GetSubcategoriesFor(g_scanCategory);
    int curIdx = 0;
    for (size_t i = 0; i < subs.size(); ++i) {
        if (subs[i] == g_scanSubcategory) { curIdx = static_cast<int>(i); break; }
    }
    int nextIdx = (curIdx + 1) % static_cast<int>(subs.size());
    g_scanSubcategory = subs[nextIdx];

    ApplyCategoryFilter();
    g_scanIndex = g_scannedFiltered.empty() ? -1 : 0;

    std::wstring msg = GetSubcategoryName(g_scanSubcategory);
    if (!g_scannedFiltered.empty()) {
        msg += L". " + FormatObjectAnnounce(*g_scannedFiltered[0]);
    }
    Speak(msg);
}

// --- Obtenir le FormID de l'objet courant (pour autowalk) ---
static RE::FormID ScannerGetCurrentFormID() {
    if (g_scannedFiltered.empty() || g_scanIndex < 0)
        return 0;
    return g_scannedFiltered[g_scanIndex]->formID;
}

static std::wstring ScannerGetCurrentName() {
    if (g_scannedFiltered.empty() || g_scanIndex < 0)
        return L"";
    return g_scannedFiltered[g_scanIndex]->name;
}

// --- Téléportation scanner (Alt+Home) ---
// Téléporte le joueur à côté de l'objet sélectionné dans le scanner
static void ScannerTeleport() {
    if (!g_mcmTeleportEnabled.load()) {
        Speak(TR("Teleport disabled"));
        return;
    }
    if (g_scannedFiltered.empty() || g_scanIndex < 0) {
        Speak(TR("No target selected"));
        return;
    }

    auto& obj = *g_scannedFiltered[g_scanIndex];
    RE::FormID targetID = obj.formID;
    std::wstring targetName = obj.name;
    int category = obj.category;

    if (targetID == 0) {
        Speak(TR("No valid target"));
        return;
    }

    auto* task = SKSE::GetTaskInterface();
    if (!task) return;

    RE::NiPoint3 cachedPos = obj.lastKnownPos;  // position en cache pour fallback

    task->AddTask([targetID, targetName, cachedPos, category]() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        // Chercher la référence cible
        auto* form = RE::TESForm::LookupByID(targetID);
        auto* targetRef = form ? form->AsReference() : nullptr;

        if (!targetRef) {
            Speak(TR("Target not found"));
            LOG("ScannerTeleport: FormID {:08X} not found", targetID);
            return;
        }

        // Marqueurs de quête cross-cell : limite stricte de 1000 unités.
        // Raison : quand la ref est dans une autre cellule, sa position est en
        // coordonnees locales de cette cellule, et teleporter directement dessus
        // depuis une position exterieure peut envoyer le joueur dans le vide
        // et casser la quete. Si la cible est dans la MEME cellule que le joueur,
        // on applique la limite normale du MCM (3000 par defaut) — pas de danger.
        if (category == kCatQuests) {
            auto* playerCellCheck = player->GetParentCell();
            auto* targetCellCheck = targetRef->GetParentCell();
            bool crossCell = playerCellCheck && targetCellCheck &&
                             playerCellCheck != targetCellCheck;
            if (crossCell) {
                auto playerPos = player->GetPosition();
                auto targetPos = targetRef->GetPosition();
                float dist = (playerPos - targetPos).Length();
                if (dist > 1000.0f) {
                    Speak(TR("Quest target is too far to teleport"));
                    LOG("ScannerTeleport: blocked - cross-cell quest target distance {:.0f} > 1000", dist);
                    return;
                }
            }
        }

        // Vérification de portée (configurable via MCM)
        float maxTpDist = g_mcmTeleportRange.load();
        auto* playerCell = player->GetParentCell();
        if (playerCell && playerCell->IsInteriorCell()) {
            auto* targetCell = targetRef->GetParentCell();
            if (targetCell != playerCell) {
                Speak(TR("Target is in another area"));
                LOG("ScannerTeleport: blocked - different interior cell");
                return;
            }
        }
        auto playerPos = player->GetPosition();
        auto targetPos = targetRef->GetPosition();
        float dist = (playerPos - targetPos).Length();
        if (dist > maxTpDist) {
            Speak(TR("Target is too far"));
            LOG("ScannerTeleport: blocked - distance {:.0f} > {:.0f}", dist, maxTpDist);
            return;
        }

        LOG("ScannerTeleport: teleporting to '{}' FormID={:08X}",
            WStringToUtf8(targetName), targetID);

        // Téléporter le joueur à côté de la cible
        player->MoveTo(targetRef);

        // Petit décalage pour ne pas être DANS l'objet :
        // avancer de 100 unités dans la direction opposée à la cible
        auto* taskPost = SKSE::GetTaskInterface();
        if (taskPost) {
            taskPost->AddTask([targetID]() {
                auto* p = RE::PlayerCharacter::GetSingleton();
                if (!p) return;

                auto playerPos = p->GetPosition();
                auto* targetForm = RE::TESForm::LookupByID(targetID);
                if (!targetForm) return;
                auto* targetRef = targetForm->AsReference();
                if (!targetRef) return;

                auto targetPos = targetRef->GetPosition();
                float dx = playerPos.x - targetPos.x;
                float dy = playerPos.y - targetPos.y;
                float len = std::sqrt(dx * dx + dy * dy);

                if (len > 1.0f) {
                    // Reculer de 100 unités par rapport à la cible
                    float offsetX = (dx / len) * 100.0f;
                    float offsetY = (dy / len) * 100.0f;
                    RE::NiPoint3 safePos = {targetPos.x + offsetX, targetPos.y + offsetY, playerPos.z};
                    p->SetPosition(safePos, true);
                }

                // Tourner le joueur vers la cible
                float angle = std::atan2(targetPos.x - p->GetPosition().x,
                                          targetPos.y - p->GetPosition().y);
                if (angle < 0) angle += 2.0f * 3.14159265f;
                p->data.angle.z = angle;

                LOG("ScannerTeleport: arrived at ({:.0f},{:.0f},{:.0f})",
                    p->GetPosition().x, p->GetPosition().y, p->GetPosition().z);
            });
        }

        Speak(TR("Teleported to") + L" " + targetName);
    });
}

// --- Verrouillage ennemi (touche C) ---
// --- AUTO-AIM SYSTEM (inspiré de FO4 Access) ---

static RE::ActorHandle g_autoAimTarget;           // cible verrouillée
static std::atomic_bool g_autoAimTracking{false};  // suivi en cours
static std::jthread g_autoAimThread;               // thread de suivi
static int g_autoAimFrameCount{0};                 // compteur pour rate-limit

// Calculer le centre du corps d'un acteur (pas les pieds)
static RE::NiPoint3 GetActorCenter(RE::Actor* actor) {
    // Pour les acteurs morts (ragdoll au sol), utiliser le centre 3D réel
    // car le bounding box statique suppose un corps debout
    if (actor->IsDead()) {
        auto* node = actor->Get3D();
        if (node && node->worldBound.radius > 0.1f) {
            return node->worldBound.center;
        }
    }

    // Acteurs vivants : centre vertical = mi-hauteur du bounding box
    auto pos = actor->GetPosition();
    auto boundMin = actor->GetBoundMin();
    auto boundMax = actor->GetBoundMax();
    float centerZ = pos.z + (boundMax.z - boundMin.z) * 0.5f;
    return {pos.x, pos.y, centerZ};
}

// Tourner le joueur vers une position cible
static void AimAtPosition(RE::PlayerCharacter* player, const RE::NiPoint3& targetPos, bool compensateGravity = false, const RE::NiPoint3* targetVelocity = nullptr) {
    RE::NiPoint3 eyePos, eyeDir;
    player->GetEyeVector(eyePos, eyeDir, true);

    // Lire les données balistiques UNE SEULE FOIS
    float projSpeed = 3600.0f;
    float projGravity = 0.34f;
    float weaponSpeed = 1.0f;

    if (compensateGravity) {
        auto* equippedObj = player->GetEquippedObject(false);
        auto* weapon = equippedObj ? equippedObj->As<RE::TESObjectWEAP>() : nullptr;
        if (weapon && weapon->IsBow()) {
            weaponSpeed = weapon->weaponData.speed;
        }
        auto* ammo = player->GetCurrentAmmo();
        if (ammo) {
            auto& ammoData = ammo->GetRuntimeData();
            auto* proj = ammoData.data.projectile;
            if (proj) {
                projSpeed = proj->data.speed;
                projGravity = proj->data.gravity;
            }
        }
    }

    float v = projSpeed * weaponSpeed;
    float worldScale = RE::bhkWorld::GetWorldScale();  // ~0.0142875
    float g = (compensateGravity && projGravity > 0.0f && worldScale > 0.0f)
              ? projGravity * 9.81f / worldScale : 0.0f;

    // Prédiction de mouvement : viser là où la cible sera au moment de l'impact
    RE::NiPoint3 aimPos = targetPos;
    if (compensateGravity && targetVelocity && v > 0.0f) {
        float dx0 = targetPos.x - eyePos.x;
        float dy0 = targetPos.y - eyePos.y;
        float dist0 = std::sqrt(dx0 * dx0 + dy0 * dy0);
        float flightTime = dist0 / v;

        aimPos.x += targetVelocity->x * flightTime;
        aimPos.y += targetVelocity->y * flightTime;
        aimPos.z += targetVelocity->z * flightTime;
    }

    float dx = aimPos.x - eyePos.x;
    float dy = aimPos.y - eyePos.y;
    float dz = aimPos.z - eyePos.z;

    float yaw = std::atan2(dx, dy);
    float hDist = std::sqrt(dx * dx + dy * dy);
    float pitch;

    // Compensation de gravité : formule balistique EXACTE
    // Résout l'équation de trajectoire parabolique pour trouver l'angle de tir précis.
    // Discriminant = v⁴ - g(g·d² + 2·Δh·v²)
    //   > 0 : deux solutions (on prend la trajectoire basse = tir tendu)
    //   = 0 : distance max, une seule trajectoire possible
    //   < 0 : cible physiquement hors de portée
    if (compensateGravity && g > 0.0f && v > 0.0f && hDist > 100.0f) {
        float v2 = v * v;
        float v4 = v2 * v2;
        float disc = v4 - g * (g * hDist * hDist + 2.0f * dz * v2);

        if (disc >= 0.0f) {
            // Solution exacte : trajectoire basse (tir tendu, plus rapide)
            float sqrtDisc = std::sqrt(disc);
            float theta = std::atan2(v2 - sqrtDisc, g * hDist);
            pitch = -theta;  // négatif car Skyrim : pitch < 0 = regarder vers le haut

            LOG("AutoAim: exact ballistic: v={:.0f} g={:.2f} d={:.0f} dz={:.0f} "
                "theta={:.3f}rad ({:.1f}deg) disc={:.0f}",
                v, g, hDist, dz, theta, theta * 180.0f / 3.14159f, disc);
        } else {
            // Cible physiquement inatteignable — viser directement (meilleur effort)
            pitch = -std::atan2(dz, hDist);
            LOG("AutoAim: target unreachable (disc={:.0f}), aiming directly", disc);
        }
    } else {
        // Pas de compensation (pas d'arc ou trop proche)
        pitch = -std::atan2(dz, hDist);
    }

    player->SetRotationZ(yaw);
    player->SetRotationX(pitch);

    // Réinitialiser le free-look en 3e personne
    auto* camera = RE::PlayerCamera::GetSingleton();
    if (camera) {
        auto* state = camera->cameraStates[RE::CameraState::kThirdPerson].get();
        if (state) {
            auto* tps = static_cast<RE::ThirdPersonState*>(state);
            tps->freeRotation = {0.f, 0.f};
        }
    }
}

// Calculer la portée effective maximale de la flèche équipée
// Prend en compte : vitesse du projectile, gravité, range, lifetime, vitesse de l'arc
// La portée PRATIQUE est basée sur la chute libre de la flèche : à quelle distance
// la gravité fait tomber la flèche de plus que la hauteur d'une cible (~200 unités).
// Au-delà, même avec compensation, les erreurs de physique rendent le tir peu fiable.
// Retourne la distance max en unités Skyrim, ou 0 si pas de données disponibles
static float GetArrowEffectiveRange(RE::PlayerCharacter* player) {
    if (!player) return 0.0f;

    // Récupérer le projectile de la flèche équipée
    float projSpeed    = 3600.0f;
    float projGravity  = 0.34f;
    float projRange    = 0.0f;     // 0 = illimité
    float projLifetime = 0.0f;     // 0 = illimité

    auto* ammo = player->GetCurrentAmmo();
    if (ammo) {
        auto& ammoData = ammo->GetRuntimeData();
        auto* proj = ammoData.data.projectile;
        if (proj) {
            projSpeed    = proj->data.speed;
            projGravity  = proj->data.gravity;
            projRange    = proj->data.range;
            projLifetime = proj->data.lifetime;
        }
    }

    // Multiplicateur de vitesse de l'arc
    float weaponSpeed = 1.0f;
    auto* equippedObj = player->GetEquippedObject(false);
    auto* weapon = equippedObj ? equippedObj->As<RE::TESObjectWEAP>() : nullptr;
    if (weapon && weapon->IsBow()) {
        weaponSpeed = weapon->weaponData.speed;
    }

    // Vitesse réelle du projectile (arc bandé = puissance 1.0)
    float v = projSpeed * weaponSpeed;
    if (v <= 0.0f) return 0.0f;

    float worldScale = RE::bhkWorld::GetWorldScale();  // ~0.0142875
    float g = (projGravity > 0.0f && worldScale > 0.0f) ? projGravity * 9.81f / worldScale : 0.0f;

    // Limite 1 : portée définie dans les données du projectile
    float rangeLimit = (projRange > 0.0f) ? projRange : 999999.0f;

    // Limite 2 : durée de vie × vitesse
    float lifetimeLimit = (projLifetime > 0.0f) ? v * projLifetime : 999999.0f;

    // Limite 3 : portée balistique PRATIQUE
    // = distance horizontale où la chute libre (sans compensation) atteint MAX_DROP
    // Formule : chute = ½·g·t², temps de vol t = d/v → d = v·√(2·MAX_DROP/g)
    // MAX_DROP = 200 unités ≈ hauteur d'un humanoïde (~2.8 mètres)
    // Au-delà, la compensation de gravité exige un arc important et les erreurs
    // de simulation (physique discrète, timing) font rater la cible.
    constexpr float MAX_DROP = 200.0f;
    float practicalLimit = (g > 0.0f) ? v * std::sqrt(2.0f * MAX_DROP / g) : 999999.0f;

    float effectiveRange = std::min({rangeLimit, lifetimeLimit, practicalLimit});

    LOG("AutoAim: range: v={:.0f} g={:.2f} projRange={:.0f} lifetime={:.1f} "
        "rangeLimit={:.0f} lifetimeLimit={:.0f} practical={:.0f} effective={:.0f}",
        v, g, projRange, projLifetime, rangeLimit, lifetimeLimit, practicalLimit, effectiveRange);

    return effectiveRange;
}

// Chercher l'ennemi le plus proche via ProcessLists (trouve les dragons en vol)
static RE::Actor* FindNearestEnemy(RE::PlayerCharacter* player, float& outDist, bool prioritizeDragons = false) {
    auto playerPos = player->GetPosition();
    RE::Actor* best = nullptr;
    float bestScore = 999999.0f;
    float bestDist = 999999.0f;
    bool bestIsDragon = false;

    auto checkActor = [&](RE::Actor* actor) {
        if (!actor) return;
        if (actor == player) return;
        if (actor->IsDead()) return;
        if (actor->IsDisabled()) return;
        if (actor->IsDeleted()) return;
        if (!actor->Is3DLoaded()) return;
        if (actor->IsPlayerTeammate()) return;
        if (!actor->IsHostileToActor(player)) return;

        // Distance vers le centre de la bounding box 3D (meme reference que
        // le scanner d'objets), pas les pieds. Pour un dragon en vol ou un
        // geant en hauteur ca fait une difference notable : la distance
        // annoncee correspond au centre du corps visible, comme dans le
        // scanner — l'utilisateur entend la meme valeur via X et via le
        // scanner pour le meme ennemi.
        auto diff = playerPos - GetStablePosition(actor);
        float dist = diff.Length();

        bool isDragonActor = IsDragon(actor);
        bool dragon = prioritizeDragons && isDragonActor;

        if (prioritizeDragons) {
            const char* n = actor->GetDisplayFullName();
            LOG("FindEnemy: '{}' dist={:.0f} isDragon={} hostile=true",
                n ? n : "?", dist, isDragonActor);
        }

        // Si on priorise les dragons et qu'on en a déjà un, ignorer les non-dragons
        if (prioritizeDragons && bestIsDragon && !dragon) return;
        // Dragon à moins de 5000 unités → priorité absolue
        if (dragon && dist > 5000.0f) dragon = false;

        // Score = distance, pénalisé x3 si pas de ligne de vue
        float score = dist;
        bool losUnused = false;
        bool losOk = actor->HasLineOfSight(player, losUnused);
        if (!losOk) score *= 3.0f;

        if (prioritizeDragons && isDragonActor) {
            LOG("FindEnemy: DRAGON '{}' dist={:.0f} score={:.0f} LOS={} bestIsDragon={}",
                actor->GetDisplayFullName() ? actor->GetDisplayFullName() : "?",
                dist, score, losOk, bestIsDragon);
        }

        // Un dragon bat toujours un non-dragon
        if (dragon && !bestIsDragon) {
            bestScore = score;
            bestDist = dist;
            best = actor;
            bestIsDragon = true;
        } else if (dragon == bestIsDragon && score < bestScore) {
            bestScore = score;
            bestDist = dist;
            best = actor;
            bestIsDragon = dragon;
        }
    };

    // Utiliser ProcessLists pour trouver TOUS les acteurs actifs (y compris dragons en vol)
    auto* procLists = RE::ProcessLists::GetSingleton();
    if (procLists) {
        for (auto& handle : procLists->highActorHandles) {
            auto actorPtr = handle.get();
            if (actorPtr) checkActor(actorPtr.get());
        }
        for (auto& handle : procLists->middleHighActorHandles) {
            auto actorPtr = handle.get();
            if (actorPtr) checkActor(actorPtr.get());
        }
    }

    outDist = bestDist;
    return best;
}

static std::atomic_bool g_aimLoopPlaying{false};

// Arrêter le suivi auto-aim (safe à appeler de n'importe quel thread)
static void StopAutoAim() {
    g_autoAimTracking.store(false);
    g_autoAimTarget.reset();
    if (g_aimLoopPlaying.exchange(false)) {
        StopSoundLoop();  // Arrêter le son de visée en boucle
    }
    // Ne PAS join le thread ici — il s'arrêtera tout seul via stop_token + g_autoAimTracking
    LOG("AutoAim: tracking stopped");
}

// Démarrer le suivi continu
static void StartAutoAimTracking() {
    // Arrêter le thread précédent proprement
    g_autoAimTracking.store(false);
    if (g_autoAimThread.joinable()) {
        g_autoAimThread.request_stop();
        g_autoAimThread.join();
    }
    g_autoAimTracking.store(true);

    g_autoAimThread = std::jthread([](std::stop_token st) {
        LOG("AutoAim: tracking thread started");
        while (!st.stop_requested() && g_autoAimTracking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // re-vise 10x par seconde
            if (!g_autoAimTracking.load() || st.stop_requested()) break;

            // Vérifier que la cible est toujours valide
            auto targetPtr = g_autoAimTarget.get();
            if (!targetPtr) {
                Speak(TR("Target lost"));
                g_autoAimTracking.store(false);
                LOG("AutoAim: target handle invalid");
                break;
            }
            auto* target = targetPtr.get();
            if (!target || target->IsDead()) {
                Speak(TR("Target lost"));
                g_autoAimTracking.store(false);
                LOG("AutoAim: target dead or lost");
                break;
            }

            // Re-viser en silence via game thread + check ligne de vue pour bip
            auto* task = SKSE::GetTaskInterface();
            if (task) {
                auto handle = g_autoAimTarget;  // copie du handle pour le lambda
                task->AddTask([handle]() {
                    if (!g_autoAimTracking.load()) return;
                    auto ptr = handle.get();
                    if (!ptr) return;
                    auto* t = ptr.get();
                    if (!t || t->IsDead()) return;
                    auto* p = RE::PlayerCharacter::GetSingleton();
                    if (!p) return;
                    auto targetCenter = GetActorCenter(t);
                    // Compenser la gravité + prédiction de mouvement si l'arc est bandé
                    bool isBowDrawn = p->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBowDrawn;
                    if (isBowDrawn) {
                        RE::NiPoint3 velocity;
                        t->GetLinearVelocity(velocity);
                        AimAtPosition(p, targetCenter, true, &velocity);
                    } else {
                        AimAtPosition(p, targetCenter);
                    }

                    // Vérifier la portée de la flèche
                    float distToTarget = (p->GetPosition() - t->GetPosition()).Length();
                    float maxRange = GetArrowEffectiveRange(p);
                    bool inRange = (maxRange <= 0.0f) || (distToTarget <= maxRange);

                    // Son en boucle si ligne de vue dégagée ET cible à portée
                    bool losUnused = false;
                    bool hasLOS = p->HasLineOfSight(t, losUnused);
                    bool shouldPlay = hasLOS && inRange;
                    if (shouldPlay && !g_aimLoopPlaying.load()) {
                        PlaySoundLoop(g_soundAimLoopID, g_volumeAim);
                        g_aimLoopPlaying.store(true);
                        LOG("AutoAim: aim loop started (LOS ok, in range {:.0f}/{:.0f})", distToTarget, maxRange);
                    } else if (!shouldPlay && g_aimLoopPlaying.load()) {
                        StopSoundLoop();
                        g_aimLoopPlaying.store(false);
                        if (!hasLOS) LOG("AutoAim: aim loop stopped (LOS blocked)");
                        else LOG("AutoAim: aim loop stopped (out of range {:.0f}/{:.0f})", distToTarget, maxRange);
                    }
                });
            }
        }
        LOG("AutoAim: tracking thread ended");
    });
}

// Chercher la cible de quête HUD la plus proche (même cellule, <2000 unités)
// Retourne true si trouvée, remplit outPos, outName, outDist
// Lit le heading de la boussole pour le marqueur de quête actif depuis le HUD.
// Retourne true si trouvé, et écrit l'angle (en degrés, 0=nord, 90=est, etc.) dans outHeading.
// C'est la direction authentique calculée par Skyrim, qui marche pour TOUTES les distances.
static bool ReadQuestCompassHeading(float& outHeading) {
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto hudMenu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
    if (!hudMenu || !hudMenu->uiMovie) return false;

    RE::GFxValue hudRoot;
    if (!hudMenu->uiMovie->GetVariable(&hudRoot, "_root.HUDMovieBaseInstance")) return false;
    if (!SafeIsObject(hudRoot)) return false;

    RE::GFxValue dataArr;
    if (!hudRoot.GetMember("CompassTargetDataA", &dataArr) || !SafeIsArray(dataArr)) return false;

    RE::GFxValue qtVal, qdVal;
    float qt = -1.0f, qd = -1.0f;
    if (hudRoot.GetMember("CompassMarkerQuest", &qtVal) && SafeIsNumber(qtVal))
        qt = static_cast<float>(SafeGetNumber(qtVal));
    if (hudRoot.GetMember("CompassMarkerQuestDoor", &qdVal) && SafeIsNumber(qdVal))
        qd = static_cast<float>(SafeGetNumber(qdVal));

    uint32_t arrSize = SafeGetArraySize(dataArr);
    for (uint32_t ci = 0; ci + 3 < arrSize; ci += 4) {
        RE::GFxValue hVal, tVal;
        dataArr.GetElement(ci, &hVal);
        dataArr.GetElement(ci + 2, &tVal);
        if (!SafeIsNumber(tVal)) continue;
        float tp = static_cast<float>(SafeGetNumber(tVal));
        if ((tp == qt || tp == qd) && SafeIsNumber(hVal)) {
            outHeading = static_cast<float>(SafeGetNumber(hVal));
            return true;
        }
    }
    return false;
}

// Cherche la position de la première cible de quête active affichée à la boussole.
// Contrairement à FindNearestQuestTarget, cette fonction :
//   - N'a PAS de limite de distance
//   - Gère les cibles cross-cell via worldLocMarker (comme le scanner Quests)
//   - Retourne la position 2D de la "porte d'entrée" si la cible est dans une autre cellule
// Dormant : utilisée par l'ancien module quest_nav.h (supprimé). Conservée pour
// une éventuelle réactivation ultérieure ; sera éliminée par le compilateur tant
// qu'elle n'est référencée nulle part.
static bool GetActiveQuestNavTarget(RE::PlayerCharacter* player, RE::NiPoint3& outPos, std::wstring& outName) {
    if (!player) return false;
    auto* playerCell = player->GetParentCell();
    if (!playerCell) return false;

    auto& objectives = REL::RelocateMemberIfNewer<RE::BSTArray<RE::BGSInstancedQuestObjective>>(
        SKSE::RUNTIME_SSE_1_6_629, player, 0x580, 0x588);

    for (auto& instObj : objectives) {
        if (!instObj.Objective) continue;
        if (instObj.InstanceState != RE::QUEST_OBJECTIVE_STATE::kDisplayed) continue;

        auto* questObj = instObj.Objective;
        auto* quest = questObj->ownerQuest;
        if (!quest || !quest->IsActive()) continue;

        // Seulement les quêtes affichées au HUD (suivies par le joueur)
        auto rawFlags = quest->data.flags.underlying();
        bool displayedInHUD = (rawFlags & 0x20) != 0;
        if (!displayedInHUD) continue;

        for (uint32_t t = 0; t < questObj->numTargets; t++) {
            auto* target = questObj->targets[t];
            if (!target) continue;

            RE::ObjectRefHandle refHandle;
            quest->CreateRefHandleByAliasID(refHandle, target->alias);
            if (!refHandle) continue;
            auto refSmartPtr = refHandle.get();
            if (!refSmartPtr) continue;
            auto* targetRef = refSmartPtr.get();

            // Vérifier les conditions CTDA (boussole utilise la même logique)
            if (target->conditions.head != nullptr) {
                if (!target->conditions.IsTrue(player, targetRef)) continue;
            }

            auto refPos = GetStablePosition(targetRef);
            auto* refCell = targetRef->GetParentCell();
            RE::NiPoint3 actualPos = refPos;

            // Si cellule différente : essayer worldLocMarker (entrée du lieu)
            if (refCell && refCell != playerCell) {
                bool isInterior = refCell->IsInteriorCell();
                if (!isInterior) {
                    auto* targetLocation = refCell->GetLocation();
                    bool resolved = false;
                    for (auto* loc = targetLocation; loc && !resolved; loc = loc->parentLoc) {
                        if (loc->worldLocMarker) {
                            auto markerPtr = loc->worldLocMarker.get();
                            if (markerPtr) {
                                actualPos = markerPtr->GetPosition();
                                resolved = true;
                            }
                        }
                    }
                }
            }

            outPos = actualPos;
            // Nom : objectif si disponible, sinon nom de la quête
            if (questObj->displayText.size() > 0) {
                outName = Utf8ToWString(ResolveQuestAliases(questObj->displayText.c_str(), quest).c_str());
            } else if (quest->GetFullName()) {
                outName = Utf8ToWString(quest->GetFullName());
            } else {
                outName = TR("Quest target");
            }
            return true;
        }
    }
    return false;
}

static bool FindNearestQuestTarget(RE::PlayerCharacter* player, RE::NiPoint3& outPos, std::wstring& outName, float& outDist) {
    auto playerPos = player->GetPosition();
    auto* playerCell = player->GetParentCell();
    if (!playerCell) return false;

    auto& objectives = REL::RelocateMemberIfNewer<RE::BSTArray<RE::BGSInstancedQuestObjective>>(
        SKSE::RUNTIME_SSE_1_6_629, player, 0x580, 0x588);

    float bestDist = 2000.0f;  // max 2000 unités
    bool found = false;

    for (auto& instObj : objectives) {
        if (!instObj.Objective) continue;
        if (instObj.InstanceState != RE::QUEST_OBJECTIVE_STATE::kDisplayed) continue;

        auto* questObj = instObj.Objective;
        auto* quest = questObj->ownerQuest;
        if (!quest || !quest->IsActive()) continue;

        // Seulement les quêtes affichées au HUD
        auto rawFlags = quest->data.flags.underlying();
        bool displayedInHUD = (rawFlags & 0x20) != 0;
        if (!displayedInHUD) continue;

        for (uint32_t t = 0; t < questObj->numTargets; t++) {
            auto* target = questObj->targets[t];
            if (!target) continue;

            RE::ObjectRefHandle refHandle;
            quest->CreateRefHandleByAliasID(refHandle, target->alias);
            if (!refHandle) continue;

            auto refSmartPtr = refHandle.get();
            if (!refSmartPtr) continue;
            auto* targetRef = refSmartPtr.get();

            // Même cellule uniquement
            auto* targetCell = targetRef->GetParentCell();
            if (!targetCell || targetCell != playerCell) continue;

            auto refPos = GetStablePosition(targetRef);
            float dist = (playerPos - refPos).Length();

            if (dist < bestDist) {
                bestDist = dist;
                outPos = refPos;
                outDist = dist;
                found = true;

                // Nom : texte de l'objectif
                if (questObj->displayText.size() > 0) {
                    outName = Utf8ToWString(ResolveQuestAliases(questObj->displayText.c_str(), quest).c_str());
                } else if (quest->GetFullName()) {
                    outName = Utf8ToWString(quest->GetFullName());
                } else {
                    outName = TR("Quest target");
                }
            }
        }
    }
    return found;
}

// Cible "sticky dragon" : si le verrouillage continu Maj+X tombe sur un
// dragon, on reste colle dessus peu importe sa distance (au lieu de basculer
// sur le plus proche a chaque tick). Reset si le dragon meurt, devient
// invalide, ou si on arrete le lock manuellement.
// Declaration AVANT LockNearestEnemy car X seul consulte ce handle pour
// rediriger sa visee sur le dragon locke quand un sticky est actif.
static RE::ActorHandle g_lockedDragon;

// Verrouiller l'ennemi le plus proche (touche X) — tir unique
// Fallback : cible de quête proche si pas d'ennemi
static void LockNearestEnemy() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        Speak(TR("No enemy nearby"));
        return;
    }

    // Si un dragon est actuellement verrouille (mode sticky via Maj+X), on
    // vise CE dragon plutot que l'ennemi le plus proche. Ainsi X seul reste
    // coherent avec le lock sticky : pas de bascule sur un loup qui passe a
    // cote pendant qu'on combat un dragon. Si le dragon est mort/perdu, on
    // ne nettoie pas ici (la boucle StartToggleLockOn s'en occupe) — on
    // retombe juste sur le comportement standard pour cet appui.
    {
        auto dragonPtr = g_lockedDragon.get();
        if (dragonPtr) {
            auto* dragon = dragonPtr.get();
            if (dragon && !dragon->IsDead() && !dragon->IsDeleted() && !dragon->IsDisabled()) {
                auto targetCenter = GetActorCenter(dragon);
                AimAtPosition(player, targetCenter);

                const char* rawName = dragon->GetDisplayFullName();
                std::wstring name = rawName ? Utf8ToWString(rawName) : TR("Enemy");
                // Distance vers le centre de la bounding box (coherent avec le
                // scanner). Pour un dragon en vol c'est important — GetPosition
                // donne les "pieds" du squelette ce qui sous-estime nettement
                // la distance reelle vers le corps du dragon.
                float distD = (player->GetPosition() - GetStablePosition(dragon)).Length();
                float zDiff = targetCenter.z - player->GetPosition().z;
                std::wstring msg = name + L", " + std::to_wstring(static_cast<int>(distD)) +
                                   L" units" + FormatElevationSuffix(zDiff);
                Speak(msg);
                LOG("AutoAim: X redirected to sticky dragon '{}' dist={:.0f}",
                    rawName ? rawName : "?", distD);
                return;
            }
        }
    }

    // Chercher l'ennemi le plus proche
    float dist = 0;
    auto* nearest = FindNearestEnemy(player, dist);

    if (!nearest) {
        // Pas d'ennemi → chercher une cible de quête proche
        RE::NiPoint3 questPos;
        std::wstring questName;
        float questDist = 0;
        if (FindNearestQuestTarget(player, questPos, questName, questDist)) {
            auto playerPos = player->GetPosition();
            LOG("AutoAim: quest target pos=({:.0f},{:.0f},{:.0f}) player pos=({:.0f},{:.0f},{:.0f}) dist={:.0f}",
                questPos.x, questPos.y, questPos.z,
                playerPos.x, playerPos.y, playerPos.z, questDist);
            AimAtPosition(player, questPos);
            float zDiff = questPos.z - playerPos.z;
            std::wstring msg = questName + L", " + std::to_wstring(static_cast<int>(questDist)) + L" units" + FormatElevationSuffix(zDiff);
            Speak(msg);
            return;
        }

        Speak(TR("No enemy nearby"));
        StopAutoAim();
        return;
    }

    // Viser le centre du corps (pas les pieds) — une seule fois, pas de suivi
    auto targetCenter = GetActorCenter(nearest);
    AimAtPosition(player, targetCenter);

    // Annoncer le nom, la distance et l'elevation (coherent avec le scanner)
    const char* rawName = nearest->GetDisplayFullName();
    std::wstring name = rawName ? Utf8ToWString(rawName) : L"Enemy";
    float zDiff = targetCenter.z - player->GetPosition().z;
    std::wstring msg = name + L", " + std::to_wstring(static_cast<int>(dist)) + L" units" + FormatElevationSuffix(zDiff);
    Speak(msg);

    LOG("AutoAim: locked {} at distance {}", rawName ? rawName : "?", dist);
}

// --- Toggle lock-on continu (Shift+X) ---
static std::atomic_bool g_toggleLockOn{false};
static std::jthread g_toggleLockThread;

static void StopToggleLockOn() {
    g_toggleLockOn.store(false);
    g_lockedDragon.reset();
    LOG("ToggleLock: stopped");
}

static void StartToggleLockOn() {
    // Arrêter le thread précédent
    g_toggleLockOn.store(false);
    if (g_toggleLockThread.joinable()) {
        g_toggleLockThread.request_stop();
        g_toggleLockThread.join();
    }
    g_toggleLockOn.store(true);

    g_toggleLockThread = std::jthread([](std::stop_token st) {
        LOG("ToggleLock: tracking thread started");
        while (!st.stop_requested() && g_toggleLockOn.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (!g_toggleLockOn.load() || st.stop_requested()) break;

            auto* task = SKSE::GetTaskInterface();
            if (task) {
                task->AddTask([]() {
                    if (!g_toggleLockOn.load()) return;
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    if (!player) return;

                    // Mode "sticky dragon" : si on a un dragon verrouille,
                    // on reste dessus peu importe la distance, tant qu'il vit
                    // et qu'il existe dans le monde.
                    auto dragonPtr = g_lockedDragon.get();
                    if (dragonPtr) {
                        auto* dragon = dragonPtr.get();
                        if (!dragon || dragon->IsDeleted() || dragon->IsDisabled()) {
                            // Dragon hors zone / supprime → annoncer + retour normal
                            Speak(TR("Target lost"));
                            g_lockedDragon.reset();
                            LOG("ToggleLock: locked dragon lost (deleted/disabled)");
                            // On garde le lock actif → la prochaine iteration
                            // basculera sur l'ennemi le plus proche.
                            return;
                        }
                        if (dragon->IsDead()) {
                            Speak(TR("Dragon dead"));
                            g_lockedDragon.reset();
                            LOG("ToggleLock: locked dragon died");
                            return;
                        }
                        // Dragon vivant et present → on reste colle dessus.
                        auto targetCenter = GetActorCenter(dragon);
                        AimAtPosition(player, targetCenter);
                        return;
                    }

                    // Comportement standard : ennemi le plus proche (bascule auto)
                    float dist = 0;
                    auto* nearest = FindNearestEnemy(player, dist);
                    if (!nearest) {
                        Speak(TR("No enemy nearby"));
                        g_toggleLockOn.store(false);
                        LOG("ToggleLock: no more enemies");
                        return;
                    }

                    auto targetCenter = GetActorCenter(nearest);
                    AimAtPosition(player, targetCenter);
                });
            }
        }
        LOG("ToggleLock: tracking thread ended");
    });
}

static void StartDragonFlightWatch();

static void ToggleLockOnEnemy() {
    if (g_toggleLockOn.load()) {
        // Déjà actif → arrêter
        StopToggleLockOn();
        Speak(TR("Lock off"));
        return;
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        Speak(TR("No enemy nearby"));
        return;
    }

    float dist = 0;
    auto* nearest = FindNearestEnemy(player, dist);
    if (!nearest) {
        Speak(TR("No enemy nearby"));
        return;
    }

    const char* rawName = nearest->GetDisplayFullName();
    std::wstring name = rawName ? Utf8ToWString(rawName) : TR("Enemy");
    Speak(TR("Lock on") + L", " + name);
    LOG("ToggleLock: locked {} at distance {}", rawName ? rawName : "?", dist);

    // Si la cible initiale est un dragon → mode sticky : on memorise son
    // handle pour rester colle dessus quelle que soit la distance dans la
    // boucle StartToggleLockOn (au lieu de basculer sur le plus proche).
    if (IsDragon(nearest)) {
        g_lockedDragon = nearest->GetHandle();
        LOG("ToggleLock: sticky dragon mode ON for '{}'", rawName ? rawName : "?");
        // Démarrer la surveillance vol/sol
        StartDragonFlightWatch();
    } else {
        // Pas de dragon → on s'assure que le mode sticky est OFF
        g_lockedDragon.reset();
    }

    StartToggleLockOn();
}

// --- Annonce de la vie d'un ennemi locke ou le plus proche (Maj+V) ---
// Priorite au dragon sticky (Maj+X sur un dragon = g_lockedDragon memorise).
// Sinon ennemi le plus proche, ce qui couvre les autres cas (lock continu sur
// un non-dragon, ou pas de lock du tout : Maj+V reste utile en combat).
static void AnnounceLockedTargetHealth() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    RE::Actor* target = nullptr;

    // 1) Sticky dragon prioritaire (s'il existe et est valide)
    if (auto dragonPtr = g_lockedDragon.get()) {
        auto* d = dragonPtr.get();
        if (d && !d->IsDead() && !d->IsDeleted() && !d->IsDisabled())
            target = d;
    }

    // 2) Sinon, ennemi le plus proche (couvre lock non-dragon et hors-lock)
    if (!target) {
        float dist = 0;
        target = FindNearestEnemy(player, dist);
    }

    if (!target) {
        Speak(TR("No enemy nearby"));
        return;
    }

    auto* av = target->AsActorValueOwner();
    if (!av) {
        Speak(TR("No enemy nearby"));
        return;
    }

    const float currentHp = av->GetActorValue(RE::ActorValue::kHealth);
    const float maxHp     = av->GetPermanentActorValue(RE::ActorValue::kHealth);
    if (maxHp <= 0.0f) {
        Speak(TR("No enemy nearby"));
        return;
    }

    int pct = static_cast<int>(std::round(currentHp / maxHp * 100.0f));
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;

    const char* rawName = target->GetDisplayFullName();
    std::wstring name = rawName ? Utf8ToWString(rawName) : TR("Enemy");
    std::wstring msg = name + L", " + std::to_wstring(pct) + L"%";
    Speak(msg);
    LOG("LockedHealth: {} = {}% ({}/{})", rawName ? rawName : "?", pct,
        static_cast<int>(currentHp), static_cast<int>(maxHp));
}

// --- Surveillance de l'état de vol des dragons ---
static std::jthread g_dragonWatchThread;
static std::atomic_bool g_dragonWatchActive{false};
static bool g_lastDragonFlying{false};

static void StartDragonFlightWatch() {
    if (g_dragonWatchActive.load()) return;  // déjà actif
    g_dragonWatchActive.store(true);
    g_lastDragonFlying = false;

    g_dragonWatchThread = std::jthread([](std::stop_token st) {
        LOG("DragonWatch: started");
        while (!st.stop_requested() && g_dragonWatchActive.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (st.stop_requested() || !g_dragonWatchActive.load()) break;

            auto* task = SKSE::GetTaskInterface();
            if (task) {
                task->AddTask([]() {
                    if (!g_dragonWatchActive.load()) return;
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    if (!player) return;

                    // Chercher un dragon hostile vivant via ProcessLists
                    auto* procLists = RE::ProcessLists::GetSingleton();
                    if (!procLists) return;

                    RE::Actor* dragon = nullptr;
                    for (auto& handle : procLists->highActorHandles) {
                        auto actorPtr = handle.get();
                        if (!actorPtr) continue;
                        auto* actor = actorPtr.get();
                        if (!actor || actor->IsDead() || !actor->Is3DLoaded()) continue;
                        if (!IsDragon(actor)) continue;
                        if (!actor->IsHostileToActor(player)) continue;
                        dragon = actor;
                        break;
                    }

                    if (!dragon) {
                        // Plus de dragon hostile → arrêter la surveillance
                        g_dragonWatchActive.store(false);
                        g_lastDragonFlying = false;
                        LOG("DragonWatch: no more hostile dragon, stopping");
                        return;
                    }

                    bool flying = dragon->AsActorState()->IsFlying();

                    if (flying && !g_lastDragonFlying) {
                        Speak(TR("Dragon flying"));
                        LOG("DragonWatch: dragon took off");
                    } else if (!flying && g_lastDragonFlying) {
                        Speak(TR("Dragon grounded"));
                        LOG("DragonWatch: dragon landed");
                    }
                    g_lastDragonFlying = flying;
                });
            }
        }
        LOG("DragonWatch: ended");
    });
}

static void StopDragonFlightWatch() {
    g_dragonWatchActive.store(false);
    if (g_dragonWatchThread.joinable()) {
        g_dragonWatchThread.request_stop();
        g_dragonWatchThread.join();
    }
}

// Vérifier si l'arc est bandé (pour auto-aim automatique)
static bool IsBowDrawn() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;
    return player->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBowDrawn;
}

// --- Polling auto-aim arc ---
static std::jthread g_bowAimThread;
static bool g_wasBowDrawn{false};

static void StartBowAutoAimPolling() {
    g_bowAimThread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Désactivé via MCM → ne rien faire
            if (!g_mcmAutoAimEnabled.load()) {
                g_wasBowDrawn = false;
                continue;
            }

            bool bowDrawn = IsBowDrawn();

            if (bowDrawn && !g_wasBowDrawn) {
                // Transition : arc vient d'être bandé → verrouiller l'ennemi le plus proche
                LOG("AutoAim(bow): bow drawn detected, searching enemy...");
                auto* task = SKSE::GetTaskInterface();
                if (task) {
                    task->AddTask([]() {
                        auto* player = RE::PlayerCharacter::GetSingleton();
                        if (!player) return;

                        float dist = 0;
                        auto* nearest = FindNearestEnemy(player, dist, true);
                        if (!nearest) {
                            LOG("AutoAim(bow): no enemy found");
                            return;
                        }

                        auto targetCenter = GetActorCenter(nearest);
                        AimAtPosition(player, targetCenter, true, nullptr);

                        g_autoAimTarget = nearest->GetHandle();
                        StartAutoAimTracking();

                        const char* rawName = nearest->GetDisplayFullName();
                        bool dragon = IsDragon(nearest);
                        std::wstring name = rawName ? Utf8ToWString(rawName) : TR("Enemy");
                        std::wstring msg = name + L", " + std::to_wstring(static_cast<int>(dist));
                        if (dragon) msg += L", " + TR("Dragon");

                        // Vérifier si la cible est à portée de flèche
                        float maxRange = GetArrowEffectiveRange(player);
                        if (maxRange > 0.0f && dist > maxRange) {
                            msg += L", " + TR("out of range");
                        } else {
                            bool losUnused = false;
                            if (!player->HasLineOfSight(nearest, losUnused)) {
                                msg += L", " + TR("obstructed");
                            }
                        }

                        Speak(msg);
                        LOG("AutoAim(bow): locked {} at distance {:.0f}{} range={:.0f} center=({:.0f},{:.0f},{:.0f})",
                            rawName ? rawName : "?", dist, dragon ? " [DRAGON]" : "", maxRange, targetCenter.x, targetCenter.y, targetCenter.z);

                        // Démarrer la surveillance vol/sol si c'est un dragon
                        if (dragon) StartDragonFlightWatch();
                    });
                }
            } else if (!bowDrawn && g_wasBowDrawn) {
                // Arc relâché → arrêter le suivi
                LOG("AutoAim(bow): bow released, stopping tracking");
                StopAutoAim();
            }

            g_wasBowDrawn = bowDrawn;
        }
    });
}

static void StopBowAutoAimPolling() {
    if (g_bowAimThread.joinable()) {
        g_bowAimThread.request_stop();
        g_bowAimThread.join();
    }
}

// --- Assistance cri : auto-valider les cibles de MQ105 (Grises-Barbes) ---

class ShoutAnimListener : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* e,
                                          RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {
        if (!e) return RE::BSEventNotifyControl::kContinue;

        std::string tag = e->tag.c_str();

        // Logger tous les événements d'animation pour trouver le bon
        if (tag.find("Voice") != std::string::npos ||
            tag.find("shout") != std::string::npos ||
            tag.find("Shout") != std::string::npos) {
            LOG("ShoutAnim: tag='{}' payload='{}'", tag, e->payload.c_str());
        }

        // Détecter le cri du joueur (Voice_SpellFire_Event ou shoutStart)
        if (tag != "Voice_SpellFire_Event" && tag != "shoutStart")
            return RE::BSEventNotifyControl::kContinue;

        // Vérifier que c'est bien le joueur
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || e->holder != player) return RE::BSEventNotifyControl::kContinue;

        // Pas besoin de vérifier le cri équipé — si MQ105 obj 40 est actif
        // et que le joueur crie, on valide automatiquement
        // Vérifier que MQ105 est active et que l'objectif 40 est affiché
        auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("MQ105");
        if (!quest || !quest->IsRunning()) return RE::BSEventNotifyControl::kContinue;

        bool obj40Active = false;
        for (auto* obj : quest->objectives) {
            if (obj && obj->index == 40 &&
                obj->state == RE::QUEST_OBJECTIVE_STATE::kDisplayed) {
                obj40Active = true;
                break;
            }
        }
        if (!obj40Active) return RE::BSEventNotifyControl::kContinue;

        // TODO: SetStage ne marche pas depuis le C++, en attente d'un script Papyrus
        // En attendant, le joueur peut taper "setstage MQ105 90" dans la console (accessible avec NVDA)
        LOG("ShoutAssist: MQ105 obj40 active — shout detected, use console: setstage MQ105 90");
        Speak(TR("Use console command: setstage MQ105 90"));

        return RE::BSEventNotifyControl::kContinue;
    }
};

static ShoutAnimListener g_shoutAnimListener;

static void RegisterShoutListener() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        player->AddAnimationGraphEventSink(&g_shoutAnimListener);
        LOG("ShoutAnim listener registered on player");
    } else {
        LOG("WARNING: player not available for ShoutAnim listener");
    }
}

// SCANNER — FIN
