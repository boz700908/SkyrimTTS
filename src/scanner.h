#pragma once

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
    RE::FormType formType{RE::FormType::None};  // type Bethesda du base form (Weapon, Armor, AlchemyItem, etc.)
    std::wstring doorDestination;   // destination d'une porte (nom de la cellule)
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
    RingHallOfStories           // HallofStoriesDiskScript (anneaux de la Gorge du Monde)
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
    return cat == kCatActivators || cat == kCatContainers ||
           cat == kCatDoors || cat == kCatCorpses || cat == kCatItems;
}

// --- Liste ordonnée des sous-catégories applicables à une catégorie ---
// Le cycle (touche +/- pour avancer dans les sous-cats) utilise cette liste.
static const std::vector<ScanSubcategory>& GetSubcategoriesFor(ScanCategory cat) {
    static const std::vector<ScanSubcategory> empty = {ScanSubcategory::All};
    static const std::vector<ScanSubcategory> twoTypes = {
        ScanSubcategory::All, ScanSubcategory::TypeA, ScanSubcategory::TypeB
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
    if (cat == kCatItems) return items;
    if (cat == kCatContainers || cat == kCatDoors || cat == kCatCorpses || cat == kCatActivators)
        return twoTypes;
    return empty;
}

// --- Nom de la sous-catégorie (dépend de la catégorie courante) ---
static const wchar_t* GetSubcategoryName(ScanSubcategory sub) {
    if (g_scanCategory == kCatContainers) {
        switch (sub) {
            case ScanSubcategory::All: return L"All";
            case ScanSubcategory::TypeA: return L"Non-empty";
            case ScanSubcategory::TypeB: return L"Empty";
            default: return L"?";
        }
    } else if (g_scanCategory == kCatDoors) {
        switch (sub) {
            case ScanSubcategory::All: return L"All";
            case ScanSubcategory::TypeA: return L"Locked";
            case ScanSubcategory::TypeB: return L"Cell doors";
            default: return L"?";
        }
    } else if (g_scanCategory == kCatCorpses) {
        switch (sub) {
            case ScanSubcategory::All: return L"All";
            case ScanSubcategory::TypeA: return L"Unlooted";
            case ScanSubcategory::TypeB: return L"Looted";
            default: return L"?";
        }
    } else if (g_scanCategory == kCatActivators) {
        switch (sub) {
            case ScanSubcategory::All: return L"All";
            case ScanSubcategory::TypeA: return L"Furniture";
            case ScanSubcategory::TypeB: return L"Other";
            default: return L"?";
        }
    } else if (g_scanCategory == kCatItems) {
        switch (sub) {
            case ScanSubcategory::All:             return L"All";
            case ScanSubcategory::ItemWeapons:     return L"Weapons";
            case ScanSubcategory::ItemArmor:       return L"Armor";
            case ScanSubcategory::ItemPotions:     return L"Potions";
            case ScanSubcategory::ItemFood:        return L"Food";
            case ScanSubcategory::ItemIngredients: return L"Ingredients";
            case ScanSubcategory::ItemScrolls:     return L"Scrolls";
            case ScanSubcategory::ItemBooks:       return L"Books";
            case ScanSubcategory::ItemSoulGems:    return L"Soul Gems";
            case ScanSubcategory::ItemMisc:        return L"Miscellaneous";
            default: return L"?";
        }
    }
    return L"All";
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
        if (g_scanSubcategory == ScanSubcategory::TypeA) return !obj.empty;  // unlooted = non-vide
        if (g_scanSubcategory == ScanSubcategory::TypeB) return obj.empty;   // looted = vide
    } else if (g_scanCategory == kCatActivators) {
        if (g_scanSubcategory == ScanSubcategory::TypeA) return obj.isFurniture;
        if (g_scanSubcategory == ScanSubcategory::TypeB) return !obj.isFurniture;
    } else if (g_scanCategory == kCatItems) {
        // AlchemyItem couvre potions ET nourriture — on distingue via la structure interne :
        // on traite tout AlchemyItem comme Potions par défaut, sauf si c'est explicitement Food.
        // Pour simplicité : Potions = AlchemyItem hors food, Food = test isFood plus bas.
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
        if (g_scanCategory == kCatAll || obj.category == g_scanCategory) {
            if (g_scanCategory == kCatAll || MatchesSubcategory(obj)) {
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
static std::string ResolveQuestAliases(const std::string& text, RE::TESQuest* quest) {
    std::string result = text;
    size_t pos = 0;
    while ((pos = result.find("<Alias=", pos)) != std::string::npos) {
        size_t end = result.find('>', pos);
        if (end == std::string::npos) break;

        // Extraire le nom de l'alias (ex: "RiverwoodFriend")
        std::string aliasName = result.substr(pos + 7, end - pos - 7);

        // Chercher l'alias dans la quête
        std::string replacement = aliasName;  // fallback : le nom de l'alias brut
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

        result.replace(pos, end - pos + 1, replacement);
        pos += replacement.size();
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

    if (angle >= 337.5f || angle < 22.5f)   return L"north";
    if (angle >= 22.5f  && angle < 67.5f)   return L"northeast";
    if (angle >= 67.5f  && angle < 112.5f)  return L"east";
    if (angle >= 112.5f && angle < 157.5f)  return L"southeast";
    if (angle >= 157.5f && angle < 202.5f)  return L"south";
    if (angle >= 202.5f && angle < 247.5f)  return L"southwest";
    if (angle >= 247.5f && angle < 292.5f)  return L"west";
    return L"northwest";
}

// --- Formater l'annonce d'un objet ---
static std::wstring FormatObjectAnnounce(const ScannedObject& obj) {
    std::wstring msg = obj.name;

    if (!obj.doorDestination.empty()) msg += L", to " + obj.doorDestination;
    if (obj.locked) msg += L", locked";
    if (obj.empty) msg += L", empty";

    msg += L", " + std::to_wstring(static_cast<int>(obj.distance)) + L" units";

    // Direction seulement pour les piliers puzzle
    std::wstring nameCheck = obj.name;
    if (nameCheck.find(L"Pilier") != std::wstring::npos || nameCheck.find(L"Pillar") != std::wstring::npos) {
        std::wstring dir = GetObjectDirection(obj);
        if (!dir.empty()) msg += L" " + dir;
    }

    if (obj.zDiff > 256.0f) msg += L", above";
    else if (obj.zDiff < -256.0f) msg += L", below";

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
        }
    }

    g_scriptTypeCache[fid] = result;
    return result;
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
            if (ref.IsDisabled() || ref.IsDeleted()) continue;

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

            // Distance
            auto refPos = ref.GetPosition();
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
                obj.name = L"Word Wall";
                obj.distance = dist;
                obj.zDiff = refPos.z - playerPos.z;
                obj.lastKnownPos = refPos;
                obj.category = kCatActivators;
                g_scannedAll.push_back(std::move(obj));
            }

            // Nom
            const char* rawName = ref.GetDisplayFullName();
            if (!rawName || !*rawName) continue;

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

            // Détection conteneur/cadavre vide
            bool isEmpty = false;
            if (cat == kCatContainers || cat == kCatCorpses) {
                try {
                    isEmpty = (ref.GetInventoryCount() == 0);
                } catch (...) {}
            }

            // Détection porte de cellule (avec teleport)
            bool isCellDoor = !doorDest.empty();

            // Détection meuble (Furniture)
            bool isFurniture = (base->GetFormType() == RE::FormType::Furniture);

            ScannedObject obj;
            obj.formID = ref.GetFormID();
            obj.name = rawName ? Utf8ToWString(rawName) : Utf8ToWString(nameStr.c_str());
            obj.distance = dist;
            obj.zDiff = zDiff;
            obj.lastKnownPos = refPos;
            obj.category = cat;
            obj.locked = locked;
            obj.empty = isEmpty;
            obj.dead = (cat == kCatCorpses);
            obj.isCellDoor = isCellDoor;
            obj.isFurniture = isFurniture;
            obj.formType = base->GetFormType();
            obj.doorDestination = std::move(doorDest);

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

    // Scanner les objectifs de quête actifs
    try {
        auto& objectives = REL::RelocateMemberIfNewer<RE::BSTArray<RE::BGSInstancedQuestObjective>>(
            SKSE::RUNTIME_SSE_1_6_629, player, 0x580, 0x588);
        LOG("Scanner: checking {} quest objectives", objectives.size());
        for (auto& instObj : objectives) {
            if (!instObj.Objective) {
                LOG("Scanner: quest obj - skipped (null Objective)");
                continue;
            }
            LOG("Scanner: quest obj index={} state={}", instObj.Objective->index, static_cast<int>(instObj.InstanceState));
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
                // Le log des aliases est verbeux — on ne le fait qu'au PASS 0 pour éviter les doublons
                if (!ignoreCTDA) {
                    LOG("Scanner: target[{}] alias field={} (quest has {} aliases)", t, aliasIdx, quest->aliases.size());
                    for (uint32_t a = 0; a < quest->aliases.size(); a++) {
                        auto* dbgAlias = quest->aliases[a];
                        if (dbgAlias) {
                            LOG("Scanner:   aliases[{}] name='{}' id={}", a, dbgAlias->aliasName.c_str(), dbgAlias->aliasID);
                        }
                    }
                }

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

                auto refPos = targetRef->GetPosition();
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
                    // On skip les markers interior (ex: marker de Fort-Dragon interne) qui ne
                    // servent à rien pour naviguer depuis l'extérieur.
                    if (!resolvedPos && !isInterior) {
                        auto* targetLocation = refCell->GetLocation();
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
                                    actualPos = markerPtr->GetPosition();
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
                            LOG("Scanner: quest compass fallback heading={:.1f}°", compassHeading);
                        }
                    }
                }

                // Distance 2D (comme la carte) pour les quêtes — cohérent avec les lieux
                float dx = playerPos.x - actualPos.x;
                float dy = playerPos.y - actualPos.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                float zDiff = actualPos.z - playerPos.z;

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
                obj.lastKnownPos = actualPos;
                obj.category = kCatQuests;
                obj.locked = false;
                obj.empty = false;
                obj.dead = false;

                // Éviter les doublons (même texte d'objectif)
                bool duplicate = false;
                for (auto& existing : g_scannedAll) {
                    if (existing.category == kCatQuests && existing.name == obj.name) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    LOG("Scanner: skipping duplicate quest objective '{}'", objText);
                    continue;
                }

                g_scannedAll.push_back(std::move(obj));
                questTargetResolved = true;  // au moins un target a abouti → pas de PASS 1 nécessaire
                LOG("Scanner: quest objective '{}' at distance {} (target FormID={:08X})",
                    objText, dist, targetRef->GetFormID());
            }
            }  // fin de la boucle des 2 passes (CTDA puis fallback sans CTDA)
        }
    } catch (...) {
        LOG("Scanner: exception while scanning quest objectives");
    }

    // --- Marqueur personnalisé de la carte (touche P) ---
    if (g_customMarkerActive && (g_customMarkerPos.x != 0 || g_customMarkerPos.y != 0)) {
        float dx = g_customMarkerPos.x - playerPos.x;
        float dy = g_customMarkerPos.y - playerPos.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        ScannedObject obj;
        obj.formID = g_customMarkerFormID;  // FormID réel du marqueur de carte
        obj.name = L"Marker: " + g_customMarkerName;
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

                        auto* mapData = extraMarker->mapData;
                        // On affiche les lieux même non découverts pour l'accessibilité :
                        // les joueurs aveugles ne peuvent pas explorer visuellement, donc le
                        // scanner doit leur révéler ce qu'il y a autour.
                        bool visible = mapData->flags.any(RE::MapMarkerData::Flag::kVisible);

                        const char* rawName = mapData->locationName.GetFullName();
                        if (!rawName || !*rawName) continue;

                        auto refPos = ref->GetPosition();
                        float dx = refPos.x - playerPos.x;
                        float dy = refPos.y - playerPos.y;
                        float dist = std::sqrt(dx * dx + dy * dy);

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
                        const wchar_t* typeName = GetMarkerTypeName(markerType);

                        ScannedObject obj;
                        obj.formID = ref->GetFormID();
                        obj.name = Utf8ToWString(rawName);
                        if (typeName && std::wstring(typeName) != L"Location") {
                            obj.name += L" (";
                            obj.name += typeName;
                            obj.name += L")";
                        }
                        if (!visible) {
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

                // Scanner les autres worldspaces (villes, DLC)
                auto& worldSpaces = dataHandler->GetFormArray<RE::TESWorldSpace>();
                for (auto* ws : worldSpaces) {
                    if (!ws || ws == worldSpace) continue;
                    if (!ws->persistentCell) continue;
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
                std::wstring msg = std::to_wstring(count) + L" " + g_categoryNames[g_scanCategory];
                if (count > 0) {
                    msg += L". " + FormatObjectAnnounce(*g_scannedFiltered[0]);
                } else {
                    msg += L" nearby";
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
    for (auto& obj : g_scannedAll) {
        auto* form = RE::TESForm::LookupByID(obj.formID);
        if (!form) {
            // Objet inaccessible (FF* dynamique, cellule déchargée)
            // Recalculer la distance depuis la position en cache
            if (player && (obj.lastKnownPos.x != 0 || obj.lastKnownPos.y != 0)) {
                auto diff = playerPos - obj.lastKnownPos;
                obj.distance = diff.Length();
                obj.zDiff = obj.lastKnownPos.z - playerPos.z;
            }
            continue;
        }
        auto* ref = form->AsReference();
        if (!ref) continue;

        // Objet ramassé, supprimé ou désactivé → retirer de la liste
        // Exception : les quêtes ne sont jamais invalidées ici (leur cible peut être
        // dans une cellule non chargée, désactivée, ou pas encore créée)
        if (obj.category != kCatQuests) {
            if (ref->IsDisabled() || ref->IsDeleted() || !ref->Is3DLoaded()) {
                obj.category = kCatAll;
                obj.formID = 0;
                continue;
            }
        }

        // Recalculer la distance en temps réel
        if (player) {
            if (obj.category == kCatQuests && (obj.lastKnownPos.x != 0 || obj.lastKnownPos.y != 0)) {
                // Pour les quêtes redirigées vers une porte, utiliser lastKnownPos
                // (sinon on recalculerait vers le PNJ intérieur à 26000 unités)
                auto diff = playerPos - obj.lastKnownPos;
                obj.distance = diff.Length();
                obj.zDiff = obj.lastKnownPos.z - playerPos.z;
            } else {
                auto refPos = ref->GetPosition();
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

        // Re-vérifier empty
        if (obj.category == kCatContainers || obj.category == kCatCorpses) {
            try { obj.empty = (ref->GetInventoryCount() == 0); } catch (...) {}
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
        Speak(L"No objects in this category");
        return;
    }
    g_scanIndex++;
    if (g_scanIndex >= static_cast<int>(g_scannedFiltered.size()))
        g_scanIndex = 0;
    // Recalculer la distance en temps réel
    auto& nextObj = *g_scannedFiltered[g_scanIndex];
    auto* p = RE::PlayerCharacter::GetSingleton();
    if (p) {
        auto* refForm = RE::TESForm::LookupByID(nextObj.formID);
        auto* ref = refForm ? refForm->AsReference() : nullptr;
        if (ref && ref->Is3DLoaded()) {
            auto diff = p->GetPosition() - ref->GetPosition();
            nextObj.distance = diff.Length();
            nextObj.zDiff = ref->GetPosition().z - p->GetPosition().z;
        } else if (nextObj.lastKnownPos.x != 0 || nextObj.lastKnownPos.y != 0) {
            auto diff = p->GetPosition() - nextObj.lastKnownPos;
            nextObj.distance = diff.Length();
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
        Speak(L"No objects in this category");
        return;
    }
    g_scanIndex--;
    if (g_scanIndex < 0)
        g_scanIndex = static_cast<int>(g_scannedFiltered.size()) - 1;
    // Recalculer la distance en temps réel
    auto& prevObj = *g_scannedFiltered[g_scanIndex];
    auto* p = RE::PlayerCharacter::GetSingleton();
    if (p) {
        auto* refForm = RE::TESForm::LookupByID(prevObj.formID);
        auto* ref = refForm ? refForm->AsReference() : nullptr;
        if (ref && ref->Is3DLoaded()) {
            auto diff = p->GetPosition() - ref->GetPosition();
            prevObj.distance = diff.Length();
            prevObj.zDiff = ref->GetPosition().z - p->GetPosition().z;
        } else if (prevObj.lastKnownPos.x != 0 || prevObj.lastKnownPos.y != 0) {
            auto diff = p->GetPosition() - prevObj.lastKnownPos;
            prevObj.distance = diff.Length();
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

    std::wstring msg = g_categoryNames[g_scanCategory];

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

    std::wstring msg = g_categoryNames[g_scanCategory];

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
            auto refPos = targetRef->GetPosition();
            auto* refCell = targetRef->GetParentCell();
            RE::NiPoint3 actualPos = refPos;

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

            // Distance 2D (X/Y), cohérent avec la carte et le scan principal
            float dx = playerPos.x - actualPos.x;
            float dy = playerPos.y - actualPos.y;
            float dist2D = std::sqrt(dx * dx + dy * dy);

            obj.formID = targetRef->GetFormID();
            obj.lastKnownPos = actualPos;
            obj.distance = dist2D;
            obj.zDiff = actualPos.z - playerPos.z;
            LOG("Scanner: quest target refreshed to FormID={:08X} dist={:.0f} (2D, actualPos=({:.0f},{:.0f},{:.0f})){}",
                obj.formID, dist2D, actualPos.x, actualPos.y, actualPos.z,
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
        Speak(L"No object selected");
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

        // Distance calculée depuis GetPosition() (base de l'objet, cohérent avec le scan)
        // targetPos (centre 3D) est utilisé uniquement pour l'orientation caméra
        auto basePos = ref->GetPosition();
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
        Speak(L"No subcategories");
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
        Speak(L"Teleport disabled");
        return;
    }
    if (g_scannedFiltered.empty() || g_scanIndex < 0) {
        Speak(L"No target selected");
        return;
    }

    auto& obj = *g_scannedFiltered[g_scanIndex];
    RE::FormID targetID = obj.formID;
    std::wstring targetName = obj.name;
    int category = obj.category;

    if (targetID == 0) {
        Speak(L"No valid target");
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
            Speak(L"Target not found");
            LOG("ScannerTeleport: FormID {:08X} not found", targetID);
            return;
        }

        // Marqueurs de quête : limite stricte de 1000 unités
        // (souvent dans une autre cellule, téléporter peut casser la quête)
        if (category == kCatQuests) {
            auto playerPos = player->GetPosition();
            auto targetPos = targetRef->GetPosition();
            float dist = (playerPos - targetPos).Length();
            if (dist > 1000.0f) {
                Speak(L"Quest target is too far to teleport");
                LOG("ScannerTeleport: blocked - quest target distance {:.0f} > 1000", dist);
                return;
            }
        }

        // Vérification de portée (configurable via MCM)
        float maxTpDist = g_mcmTeleportRange.load();
        auto* playerCell = player->GetParentCell();
        if (playerCell && playerCell->IsInteriorCell()) {
            auto* targetCell = targetRef->GetParentCell();
            if (targetCell != playerCell) {
                Speak(L"Target is in another area");
                LOG("ScannerTeleport: blocked - different interior cell");
                return;
            }
        }
        auto playerPos = player->GetPosition();
        auto targetPos = targetRef->GetPosition();
        float dist = (playerPos - targetPos).Length();
        if (dist > maxTpDist) {
            Speak(L"Target is too far");
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

        Speak(L"Teleported to " + targetName);
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

        auto diff = playerPos - actor->GetPosition();
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
                Speak(L"Target lost");
                g_autoAimTracking.store(false);
                LOG("AutoAim: target handle invalid");
                break;
            }
            auto* target = targetPtr.get();
            if (!target || target->IsDead()) {
                Speak(L"Target lost");
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

            auto refPos = targetRef->GetPosition();
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
                outName = L"Quest target";
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

            auto refPos = targetRef->GetPosition();
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
                    outName = L"Quest target";
                }
            }
        }
    }
    return found;
}

// Verrouiller l'ennemi le plus proche (touche X) — tir unique
// Fallback : cible de quête proche si pas d'ennemi
static void LockNearestEnemy() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        Speak(L"No enemy nearby");
        return;
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
            std::wstring msg = questName + L", " + std::to_wstring(static_cast<int>(questDist)) + L" units";
            Speak(msg);
            return;
        }

        Speak(L"No enemy nearby");
        StopAutoAim();
        return;
    }

    // Viser le centre du corps (pas les pieds) — une seule fois, pas de suivi
    auto targetCenter = GetActorCenter(nearest);
    AimAtPosition(player, targetCenter);

    // Annoncer le nom et la distance
    const char* rawName = nearest->GetDisplayFullName();
    std::wstring name = rawName ? Utf8ToWString(rawName) : L"Enemy";
    std::wstring msg = name + L", " + std::to_wstring(static_cast<int>(dist)) + L" units";
    Speak(msg);

    LOG("AutoAim: locked {} at distance {}", rawName ? rawName : "?", dist);
}

// --- Toggle lock-on continu (Shift+X) ---
static std::atomic_bool g_toggleLockOn{false};
static std::jthread g_toggleLockThread;

static void StopToggleLockOn() {
    g_toggleLockOn.store(false);
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

                    // Toujours chercher l'ennemi le plus proche (bascule auto)
                    float dist = 0;
                    auto* nearest = FindNearestEnemy(player, dist);
                    if (!nearest) {
                        Speak(L"No enemy nearby");
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
        Speak(L"Lock off");
        return;
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        Speak(L"No enemy nearby");
        return;
    }

    float dist = 0;
    auto* nearest = FindNearestEnemy(player, dist);
    if (!nearest) {
        Speak(L"No enemy nearby");
        return;
    }

    const char* rawName = nearest->GetDisplayFullName();
    std::wstring name = rawName ? Utf8ToWString(rawName) : L"Enemy";
    Speak(L"Lock on, " + name);
    LOG("ToggleLock: locked {} at distance {}", rawName ? rawName : "?", dist);

    // Démarrer la surveillance vol/sol si c'est un dragon
    if (IsDragon(nearest)) StartDragonFlightWatch();

    StartToggleLockOn();
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
                        Speak(L"Dragon en vol");
                        LOG("DragonWatch: dragon took off");
                    } else if (!flying && g_lastDragonFlying) {
                        Speak(L"Dragon au sol");
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
                        std::wstring name = rawName ? Utf8ToWString(rawName) : L"Enemy";
                        std::wstring msg = name + L", " + std::to_wstring(static_cast<int>(dist));
                        if (dragon) msg += L", Dragon";

                        // Vérifier si la cible est à portée de flèche
                        float maxRange = GetArrowEffectiveRange(player);
                        if (maxRange > 0.0f && dist > maxRange) {
                            msg += L", out of range";
                        } else {
                            bool losUnused = false;
                            if (!player->HasLineOfSight(nearest, losUnused)) {
                                msg += L", obstructed";
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
        Speak(L"Use console command: setstage MQ105 90");

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
