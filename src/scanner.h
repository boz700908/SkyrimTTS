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

// --- Sous-catégories (comme FO4 Access) ---
enum class ScanSubcategory : int {
    All = 0,   // Pas de filtre
    TypeA,     // Dépend de la catégorie (voir GetSubcategoryName)
    TypeB,
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
        if (movie->GetVariable(&questList, path) && questList.IsArray()) {
            LOG("Scanner: journal quest list found at '{}'  size={}", path, questList.GetArraySize());
            found = true;
            break;
        }
    }
    if (!found) {
        LOG("Scanner: journal GFx quest list not found in any path");
        return;
    }

    uint32_t count = questList.GetArraySize();
    g_activeQuestFormIDs.clear();
    g_miscQuestsActive = false;  // reset, sera mis à true si une entrée Misc est active

    LOG("Scanner: journal list has {} entries, dumping all:", count);
    for (uint32_t i = 0; i < count; i++) {
        RE::GFxValue entry;
        if (!questList.GetElement(i, &entry) || !entry.IsObject()) continue;

        RE::GFxValue activeVal, formIDVal;
        bool isActive = false;
        uint32_t formID = 0;

        if (entry.GetMember("active", &activeVal)) {
            isActive = activeVal.IsBool() ? activeVal.GetBool() : (activeVal.IsNumber() && activeVal.GetNumber() != 0.0);
        }
        if (entry.GetMember("formID", &formIDVal) && formIDVal.IsNumber()) {
            formID = static_cast<uint32_t>(formIDVal.GetNumber());
        }

        // Log chaque entrée
        RE::GFxValue textVal;
        std::string entryText = "?";
        if (entry.GetMember("text", &textVal) && textVal.IsString()) entryText = textVal.GetString();
        LOG("Scanner: journal entry[{}] text='{}' formID={:08X} active={}", i, entryText, formID, isActive);

        if (formID == 0) {
            // Entrée "Divers" — formID=0 (peut y en avoir plusieurs, garder true si vu)
            if (isActive) g_miscQuestsActive = true;
            LOG("Scanner: journal Misc entry active={}", isActive);
        } else if (isActive) {
            g_activeQuestFormIDs.insert(formID);
            LOG("Scanner: journal quest active FormID={:08X}", formID);
        }
    }

    g_questFilterInitialized = true;
    LOG("Scanner: {} active quests read from journal, misc={}", g_activeQuestFormIDs.size(), g_miscQuestsActive);
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
           cat == kCatDoors || cat == kCatCorpses;
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
    }
    return true;
}

// --- Filtrer les résultats par catégorie ---
static void ApplyCategoryFilter() {
    // Retenir l'objet courant pour le retrouver après filtrage
    RE::FormID currentFormID = 0;
    if (g_scanIndex >= 0 && g_scanIndex < static_cast<int>(g_scannedFiltered.size())) {
        currentFormID = g_scannedFiltered[g_scanIndex]->formID;
    }

    g_scannedFiltered.clear();
    for (auto& obj : g_scannedAll) {
        if (g_scanCategory == kCatAll || obj.category == g_scanCategory) {
            if (g_scanCategory == kCatAll || MatchesSubcategory(obj)) {
                g_scannedFiltered.push_back(&obj);
            }
        }
    }

    // Essayer de retrouver l'objet courant
    g_scanIndex = g_scannedFiltered.empty() ? -1 : 0;
    if (currentFormID != 0) {
        for (int i = 0; i < static_cast<int>(g_scannedFiltered.size()); i++) {
            if (g_scannedFiltered[i]->formID == currentFormID) {
                g_scanIndex = i;
                break;
            }
        }
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

// Callback après scan automatique
enum ScanAction { kScanOnly, kScanThenNextCat, kScanThenPrevCat, kScanThenNextObj, kScanThenPrevObj };
static ScanAction g_pendingScanAction{kScanOnly};

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

            // Distance
            auto refPos = ref.GetPosition();
            auto diff = playerPos - refPos;
            float dist = diff.Length();

            // Détecter les Murs des Mots (triggers invisibles sans nom)
            if (base && base->Is(RE::FormType::Activator)) {
                auto* vm = RE::SkyrimVM::GetSingleton();
                if (vm && vm->impl) {
                    auto* policy = vm->impl->GetObjectHandlePolicy();
                    if (policy) {
                        auto vmH = policy->GetHandleForObject(
                            static_cast<RE::VMTypeID>(RE::FormType::Reference), &ref);
                        RE::BSTSmartPointer<RE::BSScript::Object> wwObj;
                        const char* wwScripts[] = {
                            "WordWallTriggerScript", "WordWallTriggerBleakFallsScript",
                            "WordWallTrigger02Script", "DLC2WordWallTriggerScript",
                            "DLC1WordWallTriggerScript", "DBSanctuaryWordWallTriggerScript",
                            "DLC2WordWallTrigger02Script", "DLC2WordWallTriggerBendWillScript",
                            "DLC1WordWallTrigger02Script"
                        };
                        for (auto* ws : wwScripts) {
                            if (vm->impl->FindBoundObject(vmH, ws, wwObj) && wwObj) {
                                // C'est un Mur des Mots !
                                ScannedObject obj;
                                obj.formID = ref.GetFormID();
                                obj.name = L"Word Wall";
                                obj.distance = dist;
                                obj.zDiff = refPos.z - playerPos.z;
                                obj.lastKnownPos = refPos;
                                obj.category = kCatActivators;
                                LOG("Scanner: Word Wall found FormID={:08X} dist={:.0f}", ref.GetFormID(), dist);
                                g_scannedAll.push_back(std::move(obj));
                                break;
                            }
                        }
                    }
                }
            }

            // Nom
            const char* rawName = ref.GetDisplayFullName();
            if (!rawName || !*rawName) continue;

            // Diagnostic pilier puzzle : lire l'état via le script Papyrus
            std::string nameStr = rawName;
            if (nameStr.find("Pilier") != std::string::npos || nameStr.find("Pillar") != std::string::npos || nameStr.find("pilier") != std::string::npos) {
                LOG("Scanner: PILLAR DIAGNOSTIC for '{}' FormID={:08X}", rawName, ref.GetFormID());
                auto* vmSingleton = RE::SkyrimVM::GetSingleton();
                if (vmSingleton && vmSingleton->impl) {
                    auto* handlePolicy = vmSingleton->impl->GetObjectHandlePolicy();
                    if (handlePolicy) {
                        auto vmHandle = handlePolicy->GetHandleForObject(
                            static_cast<RE::VMTypeID>(RE::FormType::Reference), &ref);
                        // Essayer différents noms de scripts pour les piliers
                        const char* scriptNames[] = {
                            "defaultPuzzlePillarScript", "DefaultPuzzlePillarScript",
                            "HallofStoriesDiskScript", "intPuzzlePillarScript"
                        };
                        for (auto* sName : scriptNames) {
                            RE::BSTSmartPointer<RE::BSScript::Object> scriptObj;
                            if (vmSingleton->impl->FindBoundObject(vmHandle, sName, scriptObj) && scriptObj) {
                                // Lire le state Papyrus (position01, position02, position03)
                                std::string state = scriptObj->currentState.c_str();
                                LOG("Scanner: PUZZLE FormID={:08X} script='{}' state='{}'", ref.GetFormID(), sName, state);

                                int posNum = 0;
                                if (state == "position01") posNum = 1;
                                else if (state == "position02") posNum = 2;
                                else if (state == "position03") posNum = 3;

                                const char* symbol = "unknown";
                                if (state == "busy") {
                                    symbol = "turning";
                                } else if (posNum > 0) {
                                    // Pour les piliers standard : Eagle/Snake/Whale
                                    if (std::string(sName) == "defaultPuzzlePillarScript" ||
                                        std::string(sName) == "DefaultPuzzlePillarScript" ||
                                        std::string(sName) == "intPuzzlePillarScript") {
                                        if (posNum == 1) symbol = "Eagle";
                                        else if (posNum == 2) symbol = "Snake";
                                        else if (posNum == 3) symbol = "Whale";
                                    }
                                    // Pour les anneaux : chercher les symboles via le linkedRef (serrure)
                                    else if (std::string(sName) == "HallofStoriesDiskScript") {
                                        // Le linkedRef de l'anneau pointe vers la serrure
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
                                        // Si pas trouvé dans la table, afficher le numéro
                                        if (std::string(symbol) == "unknown") {
                                            if (posNum == 1) symbol = "Position 1";
                                            else if (posNum == 2) symbol = "Position 2";
                                            else if (posNum == 3) symbol = "Position 3";
                                        }
                                    }
                                }

                                nameStr = std::string(rawName) + " (" + symbol + ")";
                                rawName = nullptr;
                                break;
                            }
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

            // Filtrer par quêtes actives dans le journal
            if (g_questFilterInitialized) {
                bool questTracked = g_activeQuestFormIDs.find(quest->GetFormID()) != g_activeQuestFormIDs.end();
                // Les quêtes Divers (type Misc) sont incluses si "Divers" est coché
                bool isMiscQuest = !displayedInHUD;  // les quêtes Divers n'ont pas le flag HUD
                if (!questTracked && !(isMiscQuest && g_miscQuestsActive)) {
                    continue;  // quête non traquée dans le journal
                }
            } else {
                // Avant la première ouverture du journal, afficher toutes les quêtes displayed
                if (!isActive && !displayedInHUD) continue;
            }

            // Parcourir les cibles de l'objectif pour trouver la référence
            for (uint32_t t = 0; t < questObj->numTargets; t++) {
                auto* target = questObj->targets[t];
                if (!target) {
                    LOG("Scanner: target[{}] is null", t);
                    continue;
                }

                // Résoudre l'alias pour obtenir la référence
                uint32_t aliasIdx = target->alias;
                LOG("Scanner: target[{}] alias field={} (quest has {} aliases)", t, aliasIdx, quest->aliases.size());

                // Lister tous les alias pour debug
                for (uint32_t a = 0; a < quest->aliases.size(); a++) {
                    auto* dbgAlias = quest->aliases[a];
                    if (dbgAlias) {
                        LOG("Scanner:   aliases[{}] name='{}' id={}", a, dbgAlias->aliasName.c_str(), dbgAlias->aliasID);
                    }
                }

                // Résoudre l'alias via CreateRefHandleByAliasID (méthode du moteur)
                // Plus fiable que BGSRefAlias::GetReference() pour les refs distantes
                RE::ObjectRefHandle refHandle;
                quest->CreateRefHandleByAliasID(refHandle, aliasIdx);

                if (!refHandle) {
                    LOG("Scanner: alias {} - CreateRefHandleByAliasID returned empty handle", aliasIdx);
                    continue;
                }

                auto refSmartPtr = refHandle.get();
                if (!refSmartPtr) {
                    LOG("Scanner: alias {} - handle.get() returned null", aliasIdx);
                    continue;
                }
                auto* targetRef = refSmartPtr.get();

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
                    LOG("Scanner: quest target in different cell ('{}' vs '{}'), searching door to target cell",
                        refCell->GetName() ? refCell->GetName() : "?",
                        playerCell->GetName() ? playerCell->GetName() : "?");

                    // Chercher la porte qui mène directement à la cellule cible
                    // (plus fiable que la boussole quand il y a plusieurs quêtes)
                    RE::TESObjectREFR* bestDoor = nullptr;
                    float bestDoorDist = 999999.0f;

                    // Lambda pour chercher dans une cellule
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

                            // La porte mène-t-elle à la cellule de la cible ?
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

                    // Chercher dans la cellule du joueur
                    searchDoorsForCell(playerCell);

                    // En extérieur, chercher aussi dans les cellules voisines + persistante
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

                    // Fallback : si pas de porte directe, utiliser la boussole
                    if (!bestDoor) {
                        LOG("Scanner: no direct door to '{}', trying compass", refCell->GetName());
                        float compassHeading = -1.0f;
                        auto* ui = RE::UI::GetSingleton();
                        if (ui) {
                            auto hudMenu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
                            if (hudMenu && hudMenu->uiMovie) {
                                RE::GFxValue hudRoot;
                                if (hudMenu->uiMovie->GetVariable(&hudRoot, "_root.HUDMovieBaseInstance") && hudRoot.IsObject()) {
                                    RE::GFxValue dataArr;
                                    if (hudRoot.GetMember("CompassTargetDataA", &dataArr) && dataArr.IsArray()) {
                                        RE::GFxValue qtVal, qdVal;
                                        float qt = -1, qd = -1;
                                        if (hudRoot.GetMember("CompassMarkerQuest", &qtVal) && qtVal.IsNumber())
                                            qt = static_cast<float>(qtVal.GetNumber());
                                        if (hudRoot.GetMember("CompassMarkerQuestDoor", &qdVal) && qdVal.IsNumber())
                                            qd = static_cast<float>(qdVal.GetNumber());
                                        uint32_t arrSize = dataArr.GetArraySize();
                                        for (uint32_t ci = 0; ci + 3 < arrSize; ci += 4) {
                                            RE::GFxValue hVal, tVal;
                                            dataArr.GetElement(ci, &hVal);
                                            dataArr.GetElement(ci + 2, &tVal);
                                            if (!tVal.IsNumber()) continue;
                                            float tp = static_cast<float>(tVal.GetNumber());
                                            if ((tp == qt || tp == qd) && hVal.IsNumber()) {
                                                compassHeading = static_cast<float>(hVal.GetNumber());
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (compassHeading >= 0) {
                            float compassRad = compassHeading * 3.14159265f / 180.0f;
                            for (auto& doorHandle : playerCell->GetRuntimeData().references) {
                                auto doorPtr = doorHandle.get();
                                if (!doorPtr) continue;
                                auto* doorBase = doorPtr->GetBaseObject();
                                if (!doorBase || doorBase->GetFormType() != RE::FormType::Door) continue;
                                auto* extraTele = doorPtr->extraList.GetByType<RE::ExtraTeleport>();
                                if (!extraTele || !extraTele->teleportData) continue;
                                auto doorPos = doorPtr->GetPosition();
                                float dx = doorPos.x - playerPos.x;
                                float dy = doorPos.y - playerPos.y;
                                float doorAngle = std::atan2(dx, dy);
                                if (doorAngle < 0) doorAngle += 2.0f * 3.14159265f;
                                float angleDiff = std::abs(doorAngle - compassRad);
                                if (angleDiff > 3.14159265f) angleDiff = 2.0f * 3.14159265f - angleDiff;
                                if (angleDiff < bestDoorDist) {
                                    bestDoorDist = angleDiff;
                                    bestDoor = doorPtr;
                                }
                            }
                        }
                    }

                    if (bestDoor) {
                        actualPos = bestDoor->GetPosition();
                        LOG("Scanner: quest redirected to door '{}' FormID={:08X} dist={:.0f}",
                            bestDoor->GetDisplayFullName() ? bestDoor->GetDisplayFullName() : "?",
                            bestDoor->GetFormID(), bestDoorDist);
                    }
                }

                auto diff = playerPos - actualPos;
                float dist = diff.Length();
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
                LOG("Scanner: quest objective '{}' at distance {} (target FormID={:08X})",
                    objText, dist, targetRef->GetFormID());
            }
        }
    } catch (...) {
        LOG("Scanner: exception while scanning quest objectives");
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
                        bool visible = mapData->flags.any(RE::MapMarkerData::Flag::kVisible);
                        if (!visible) continue;  // seulement les lieux visibles sur la carte

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

        // Exécuter l'action demandée après le scan
        switch (g_pendingScanAction) {
            case kScanThenNextCat: ScannerNextCategoryImpl(); break;
            case kScanThenPrevCat: ScannerPrevCategoryImpl(); break;
            case kScanThenNextObj: ScannerNextObject(); break;
            case kScanThenPrevObj: ScannerPrevObject(); break;
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
        // Ne pas écraser kCatQuests — les objectifs de quête restent dans leur catégorie
        if (obj.category != kCatQuests) {
            obj.category = CategorizeRef(*ref);
        }

        // Re-vérifier empty
        if (obj.category == kCatContainers || obj.category == kCatCorpses) {
            try { obj.empty = (ref->GetInventoryCount() == 0); } catch (...) {}
        }

        // Mettre à jour le state des piliers/anneaux puzzle
        std::string objNameUtf8 = WStringToUtf8(obj.name);
        if (objNameUtf8.find("Pilier") != std::string::npos || objNameUtf8.find("Pillar") != std::string::npos ||
            objNameUtf8.find("nneau") != std::string::npos || objNameUtf8.find("Ring") != std::string::npos ||
            objNameUtf8.find("Disk") != std::string::npos) {
            auto* vm = RE::SkyrimVM::GetSingleton();
            if (vm && vm->impl) {
                auto* policy = vm->impl->GetObjectHandlePolicy();
                if (policy) {
                    auto handle = policy->GetHandleForObject(
                        static_cast<RE::VMTypeID>(RE::FormType::Reference), ref);
                    RE::BSTSmartPointer<RE::BSScript::Object> scriptObj;
                    const char* refreshScripts[] = {"defaultPuzzlePillarScript", "HallofStoriesDiskScript"};
                    std::string rfoundScript;
                    for (auto* rs : refreshScripts) {
                        if (vm->impl->FindBoundObject(handle, rs, scriptObj) && scriptObj) {
                            rfoundScript = rs;
                            break;
                        }
                    }
                    if (!rfoundScript.empty()) {
                        std::string state = scriptObj->currentState.c_str();
                        int posNum = 0;
                        if (state == "position01") posNum = 1;
                        else if (state == "position02") posNum = 2;
                        else if (state == "position03") posNum = 3;

                        const char* symbol = "unknown";
                        if (state == "busy") {
                            symbol = "turning";
                        } else if (posNum > 0) {
                            if (rfoundScript == "defaultPuzzlePillarScript") {
                                if (posNum == 1) symbol = "Eagle";
                                else if (posNum == 2) symbol = "Snake";
                                else if (posNum == 3) symbol = "Whale";
                            } else if (rfoundScript == "HallofStoriesDiskScript") {
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
static void ScannerNextObject() {
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
    std::wstring pos = L". " + std::to_wstring(g_scanIndex + 1) + L" of " + std::to_wstring(g_scannedFiltered.size());
    Speak(FormatObjectAnnounce(*g_scannedFiltered[g_scanIndex]) + pos);
}

static void ScannerPrevObject() {
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
    std::wstring pos = L". " + std::to_wstring(g_scanIndex + 1) + L" of " + std::to_wstring(g_scannedFiltered.size());
    Speak(FormatObjectAnnounce(*g_scannedFiltered[g_scanIndex]) + pos);
}

// --- Changer de catégorie (rescan + filtre, saute les catégories vides) ---
static void ScannerNextCategory() {
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
static void ScannerAnnounceCurrent() {
    if (g_scannedFiltered.empty() || g_scanIndex < 0) {
        Speak(L"No object selected");
        return;
    }

    auto& obj = *g_scannedFiltered[g_scanIndex];

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
                        if (hudMenu->uiMovie->GetVariable(&hudRoot, "_root.HUDMovieBaseInstance") && hudRoot.IsObject()) {
                            RE::GFxValue dataArr;
                            if (hudRoot.GetMember("CompassTargetDataA", &dataArr) && dataArr.IsArray()) {
                                RE::GFxValue questTypeVal, questDoorTypeVal;
                                float questType = -1, questDoorType = -1;
                                if (hudRoot.GetMember("CompassMarkerQuest", &questTypeVal) && questTypeVal.IsNumber())
                                    questType = static_cast<float>(questTypeVal.GetNumber());
                                if (hudRoot.GetMember("CompassMarkerQuestDoor", &questDoorTypeVal) && questDoorTypeVal.IsNumber())
                                    questDoorType = static_cast<float>(questDoorTypeVal.GetNumber());

                                uint32_t arrSize = dataArr.GetArraySize();
                                for (uint32_t i = 0; i + 3 < arrSize; i += 4) {
                                    RE::GFxValue headingVal, typeVal;
                                    dataArr.GetElement(i, &headingVal);
                                    dataArr.GetElement(i + 2, &typeVal);
                                    if (!typeVal.IsNumber()) continue;
                                    float type = static_cast<float>(typeVal.GetNumber());
                                    if ((type == questType || type == questDoorType) && headingVal.IsNumber()) {
                                        float compassHeading = static_cast<float>(headingVal.GetNumber());
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
        auto targetPos = ref->GetPosition();
        auto diff = playerPos - targetPos;
        obj.distance = diff.Length();
        obj.zDiff = targetPos.z - playerPos.z;

        // Rotation caméra vers l'objet (Look at)
        float dx = targetPos.x - playerPos.x;
        float dy = targetPos.y - playerPos.y;
        float dz = targetPos.z - playerPos.z;
        float yaw = std::atan2(dx, dy);  // radians
        float hDist = std::sqrt(dx * dx + dy * dy);
        float pitch = -std::atan2(dz, hDist);  // négatif car axe X inversé

        player->SetRotationZ(yaw);
        player->SetRotationX(pitch);

        // En troisième personne, réinitialiser le free-look de la caméra
        auto* camera = RE::PlayerCamera::GetSingleton();
        if (camera) {
            auto* state = camera->cameraStates[RE::CameraState::kThirdPerson].get();
            if (state) {
                auto* tps = static_cast<RE::ThirdPersonState*>(state);
                tps->freeRotation = {0.f, 0.f};
            }
        }
    }

    Speak(FormatObjectAnnounce(obj));
}

// --- Cycler les sous-catégories (touche End) ---
static void ScannerCycleSubcategory() {
    if (!HasSubcategories(g_scanCategory)) {
        Speak(L"No subcategories");
        return;
    }

    int next = (static_cast<int>(g_scanSubcategory) + 1) % static_cast<int>(ScanSubcategory::COUNT);
    g_scanSubcategory = static_cast<ScanSubcategory>(next);

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

// --- Verrouillage ennemi (touche C) ---
// --- AUTO-AIM SYSTEM (inspiré de FO4 Access) ---

static RE::ActorHandle g_autoAimTarget;           // cible verrouillée
static std::atomic_bool g_autoAimTracking{false};  // suivi en cours
static std::jthread g_autoAimThread;               // thread de suivi
static int g_autoAimFrameCount{0};                 // compteur pour rate-limit

// Calculer le centre du corps d'un acteur (pas les pieds)
static RE::NiPoint3 GetActorCenter(RE::Actor* actor) {
    auto pos = actor->GetPosition();
    auto boundMin = actor->GetBoundMin();
    auto boundMax = actor->GetBoundMax();
    // Centre vertical = position + moitié de la hauteur du bounding box
    float centerZ = pos.z + (boundMax.z - boundMin.z) * 0.5f;
    return {pos.x, pos.y, centerZ};
}

// Tourner le joueur vers une position cible
static void AimAtPosition(RE::PlayerCharacter* player, const RE::NiPoint3& targetPos, bool compensateGravity = false, const RE::NiPoint3* targetVelocity = nullptr) {
    RE::NiPoint3 eyePos, eyeDir;
    player->GetEyeVector(eyePos, eyeDir, true);

    // Position de visée (avec prédiction de mouvement si disponible)
    RE::NiPoint3 aimPos = targetPos;
    if (compensateGravity && targetVelocity) {
        float dx0 = targetPos.x - eyePos.x;
        float dy0 = targetPos.y - eyePos.y;
        float dist0 = std::sqrt(dx0 * dx0 + dy0 * dy0);

        // Estimer la vitesse du projectile pour calculer le temps de vol
        float projSpeed = 3600.0f;
        float weaponSpeed = 1.0f;
        auto* equippedObj = player->GetEquippedObject(false);
        auto* weapon = equippedObj ? equippedObj->As<RE::TESObjectWEAP>() : nullptr;
        if (weapon && weapon->IsBow()) {
            weaponSpeed = weapon->weaponData.speed;
        }
        auto* ammo = player->GetCurrentAmmo();
        if (ammo) {
            auto& ammoData = ammo->GetRuntimeData();
            auto* proj = ammoData.data.projectile;
            if (proj) projSpeed = proj->data.speed;
        }
        float v = projSpeed * weaponSpeed;
        float flightTime = dist0 / v;

        // Prédiction : viser où la cible sera dans flightTime secondes
        aimPos.x += targetVelocity->x * flightTime;
        aimPos.y += targetVelocity->y * flightTime;
        aimPos.z += targetVelocity->z * flightTime;
    }

    float dx = aimPos.x - eyePos.x;
    float dy = aimPos.y - eyePos.y;
    float dz = aimPos.z - eyePos.z;

    float yaw = std::atan2(dx, dy);
    float hDist = std::sqrt(dx * dx + dy * dy);
    float pitch = -std::atan2(dz, hDist);

    // Compensation de gravité pour les projectiles (arc)
    if (compensateGravity && hDist > 100.0f) {
        float projSpeed = 3600.0f;   // valeur par défaut
        float projGravity = 0.34f;   // valeur par défaut
        float weaponSpeed = 1.0f;

        // Lire les vraies données de l'arc et des flèches équipés
        auto* equippedObj = player->GetEquippedObject(false);  // main droite
        auto* weapon = equippedObj ? equippedObj->As<RE::TESObjectWEAP>() : nullptr;
        if (weapon && weapon->IsBow()) {
            weaponSpeed = weapon->weaponData.speed;
        }
        auto* ammo = player->GetCurrentAmmo();
        if (ammo) {
            auto& ammoData = ammo->GetRuntimeData();
            auto* projectile = ammoData.data.projectile;
            if (projectile) {
                projSpeed = projectile->data.speed;
                projGravity = projectile->data.gravity;
            }
        }

        // Vitesse réelle du projectile
        float v = projSpeed * weaponSpeed;

        // Gravité réelle en unités Skyrim/s²
        // 1 unité Skyrim = worldScale mètres Havok, donc g_skyrim = 9.81 * projGravity / worldScale
        float worldScale = RE::bhkWorld::GetWorldScale();  // ~0.0142875
        float g = projGravity * 9.81f / worldScale;

        // Formule de compensation : angle = arctan(g*d / (2*v²))
        if (v > 0.0f) {
            float correction = std::atan2(g * hDist, 2.0f * v * v);
            pitch -= correction;  // relever le tir
            LOG("AutoAim: gravity compensation: v={:.0f} g={:.2f} dist={:.0f} correction={:.3f}rad ({:.1f}deg)",
                v, g, hDist, correction, correction * 180.0f / 3.14159f);
        }
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

// Chercher l'ennemi le plus proche avec score (distance + pénalité LOS)
static RE::Actor* FindNearestEnemy(RE::PlayerCharacter* player, float& outDist) {
    auto playerPos = player->GetPosition();
    RE::Actor* best = nullptr;
    float bestScore = 999999.0f;
    float bestDist = 999999.0f;

    auto searchCell = [&](RE::TESObjectCELL* cell) {
        if (!cell) return;
        for (auto& refHandle : cell->GetRuntimeData().references) {
            auto refPtr = refHandle.get();
            if (!refPtr) continue;

            auto* actor = refPtr->As<RE::Actor>();
            if (!actor) continue;
            if (actor == player) continue;
            if (actor->IsDead()) continue;
            if (actor->IsDisabled()) continue;
            if (actor->IsDeleted()) continue;
            if (!actor->Is3DLoaded()) continue;
            if (actor->IsPlayerTeammate()) continue;
            if (!actor->IsHostileToActor(player)) continue;

            auto diff = playerPos - actor->GetPosition();
            float dist = diff.Length();

            // Score = distance, pénalisé x3 si pas de ligne de vue
            float score = dist;
            bool losOk = false;
            (void)actor->HasLineOfSight(player, losOk);
            if (!losOk) score *= 3.0f;

            if (score < bestScore) {
                bestScore = score;
                bestDist = dist;
                best = actor;
            }
        }
    };

    auto* playerCell = player->GetParentCell();
    if (!playerCell) { outDist = 0; return nullptr; }

    if (playerCell->IsInteriorCell()) {
        searchCell(playerCell);
    } else {
        auto* tes = RE::TES::GetSingleton();
        if (tes && tes->gridCells) {
            for (uint32_t x = 0; x < tes->gridCells->length; x++) {
                for (uint32_t y = 0; y < tes->gridCells->length; y++) {
                    auto* cell = tes->gridCells->GetCell(x, y);
                    if (cell && cell->IsAttached()) searchCell(cell);
                }
            }
        } else {
            searchCell(playerCell);
        }
    }

    outDist = bestDist;
    return best;
}

// Arrêter le suivi auto-aim (safe à appeler de n'importe quel thread)
static void StopAutoAim() {
    g_autoAimTracking.store(false);
    g_autoAimTarget.reset();
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
            std::this_thread::sleep_for(std::chrono::milliseconds(500));  // re-vise 2x par seconde
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

                    // Bip seulement si ligne de vue dégagée
                    bool hasLOS = false;
                    p->HasLineOfSight(t, hasLOS);
                    if (hasLOS) {
                        std::thread([]() { Beep(1000, 50); }).detach();
                    }
                });
            }
        }
        LOG("AutoAim: tracking thread ended");
    });
}

// Verrouiller l'ennemi le plus proche (touche X)
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
                        auto* nearest = FindNearestEnemy(player, dist);
                        if (!nearest) {
                            LOG("AutoAim(bow): no enemy found");
                            return;
                        }

                        auto targetCenter = GetActorCenter(nearest);
                        AimAtPosition(player, targetCenter);

                        g_autoAimTarget = nearest->GetHandle();
                        StartAutoAimTracking();

                        const char* rawName = nearest->GetDisplayFullName();
                        std::wstring name = rawName ? Utf8ToWString(rawName) : L"Enemy";
                        std::wstring msg = name + L", " + std::to_wstring(static_cast<int>(dist)) + L" units";
                        Speak(msg);
                        LOG("AutoAim(bow): locked {} at distance {} center=({:.0f},{:.0f},{:.0f})",
                            rawName ? rawName : "?", dist, targetCenter.x, targetCenter.y, targetCenter.z);
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

// SCANNER — FIN
