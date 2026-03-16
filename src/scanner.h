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
    L"Quests"
};

// --- Objet scanné ---
struct ScannedObject {
    RE::FormID  formID{0};
    std::wstring name;
    float       distance{0.0f};
    float       zDiff{0.0f};       // positif = au-dessus, négatif = en-dessous
    ScanCategory category{kCatAll};
    bool        locked{false};
    bool        empty{false};
    bool        dead{false};
    std::wstring doorDestination;   // destination d'une porte (nom de la cellule)
};

// --- État global du scanner ---
static std::vector<ScannedObject>  g_scannedAll;        // tous les objets scannés
static std::vector<ScannedObject*> g_scannedFiltered;   // filtrés par catégorie courante
static ScanCategory                g_scanCategory{kCatAll};
static int                         g_scanIndex{-1};
// Pas de rayon de scan — on scanne toutes les cellules chargées (comme FO4 Access)
static constexpr float             RESCAN_DISTANCE = 100.0f; // auto-rescan si joueur bouge de >100 unités
static RE::NiPoint3                g_lastScanPos{0, 0, 0};

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
            g_scannedFiltered.push_back(&obj);
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

// --- Formater l'annonce d'un objet ---
static std::wstring FormatObjectAnnounce(const ScannedObject& obj) {
    std::wstring msg = obj.name;

    if (!obj.doorDestination.empty()) msg += L", to " + obj.doorDestination;
    if (obj.locked) msg += L", locked";
    if (obj.empty) msg += L", empty";

    msg += L", " + std::to_wstring(static_cast<int>(obj.distance)) + L" units";

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

            // Nom
            const char* rawName = ref.GetDisplayFullName();
            if (!rawName || !*rawName) continue;

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

            ScannedObject obj;
            obj.formID = ref.GetFormID();
            obj.name = Utf8ToWString(rawName);
            obj.distance = dist;
            obj.zDiff = zDiff;
            obj.category = cat;
            obj.locked = locked;
            obj.empty = isEmpty;
            obj.dead = (cat == kCatCorpses);
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
            LOG("Scanner: DISPLAYED objective idx={} quest='{}' text='{}' numTargets={}",
                questObj->index, questName ? questName : "?",
                questObj->displayText.c_str(), questObj->numTargets);

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

                // Chercher par ID d'alias (pas par index dans le tableau)
                RE::BGSBaseAlias* baseAlias = nullptr;
                for (auto* a : quest->aliases) {
                    if (a && a->aliasID == aliasIdx) {
                        baseAlias = a;
                        break;
                    }
                }
                if (!baseAlias) {
                    // Fallback : essayer par index
                    if (aliasIdx < quest->aliases.size()) {
                        baseAlias = quest->aliases[aliasIdx];
                        LOG("Scanner: alias ID {} not found, falling back to index", aliasIdx);
                    }
                }
                if (!baseAlias) continue;
                if (!baseAlias) continue;

                LOG("Scanner: alias '{}' (id={})", baseAlias->aliasName.c_str(), baseAlias->aliasID);

                auto* refAlias = skyrim_cast<RE::BGSRefAlias*>(baseAlias);
                if (!refAlias) {
                    LOG("Scanner: alias is not a RefAlias, skipping");
                    continue;
                }

                auto* targetRef = refAlias->GetReference();
                if (!targetRef) {
                    LOG("Scanner: alias '{}' has no reference (nullptr)", baseAlias->aliasName.c_str());
                    continue;
                }

                auto refPos = targetRef->GetPosition();
                const char* refName = targetRef->GetDisplayFullName();
                auto* refCell = targetRef->GetParentCell();
                LOG("Scanner: target ref='{}' FormID={:08X} pos=({:.0f},{:.0f},{:.0f}) cell='{}' dist={:.0f}",
                    refName ? refName : "?", targetRef->GetFormID(),
                    refPos.x, refPos.y, refPos.z,
                    refCell ? refCell->GetName() : "no cell",
                    (playerPos - refPos).Length());
                auto diff = playerPos - refPos;
                float dist = diff.Length();
                float zDiff = refPos.z - playerPos.z;

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
                obj.formID = targetRef->GetFormID();
                obj.name = Utf8ToWString(objText.c_str());
                obj.distance = dist;
                obj.zDiff = zDiff;
                obj.category = kCatQuests;
                obj.locked = false;
                obj.empty = false;
                obj.dead = false;

                g_scannedAll.push_back(std::move(obj));
                LOG("Scanner: quest objective '{}' at distance {}", objText, dist);
            }
        }
    } catch (...) {
        LOG("Scanner: exception while scanning quest objectives");
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
        if (!form) continue;
        auto* ref = form->AsReference();
        if (!ref) continue;

        // Re-catégoriser (un PNJ vivant peut être mort maintenant)
        // Ne pas écraser kCatQuests — les objectifs de quête restent dans leur catégorie
        if (obj.category != kCatQuests) {
            obj.category = CategorizeRef(*ref);
        }

        // Re-vérifier empty
        if (obj.category == kCatContainers || obj.category == kCatCorpses) {
            try { obj.empty = (ref->GetInventoryCount() == 0); } catch (...) {}
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
    if (!refForm || !refForm->AsReference()) {
        Speak(L"Object no longer available");
        return;
    }
    auto* ref = refForm->AsReference();
    if (ref->IsDisabled() || ref->IsDeleted()) {
        Speak(L"Object no longer available");
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

    std::wstring pos = L". " + std::to_wstring(g_scanIndex + 1) + L" of " + std::to_wstring(g_scannedFiltered.size());
    Speak(FormatObjectAnnounce(obj) + pos);
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
static void LockNearestEnemy() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        Speak(L"No enemy nearby");
        return;
    }

    // Seulement en combat
    if (!player->IsInCombat()) {
        Speak(L"No enemy nearby");
        return;
    }

    auto playerPos = player->GetPosition();

    // Chercher l'ennemi hostile le plus proche dans la cellule
    RE::Actor* nearest = nullptr;
    float nearestDist = 999999.0f;

    auto* cell = player->GetParentCell();
    if (!cell) {
        Speak(L"No enemy nearby");
        return;
    }

    for (auto& refHandle : cell->GetRuntimeData().references) {
        auto refPtr = refHandle.get();
        if (!refPtr) continue;

        auto* actor = refPtr->As<RE::Actor>();
        if (!actor) continue;
        if (actor == player) continue;
        if (actor->IsDead()) continue;
        if (!actor->IsHostileToActor(player)) continue;

        auto diff = playerPos - actor->GetPosition();
        float dist = diff.Length();
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = actor;
        }
    }

    if (!nearest) {
        Speak(L"No enemy nearby");
        return;
    }

    // Tourner le joueur vers l'ennemi
    auto targetPos = nearest->GetPosition();
    float dx = targetPos.x - playerPos.x;
    float dy = targetPos.y - playerPos.y;
    float dz = targetPos.z - playerPos.z;
    float yaw = std::atan2(dx, dy);
    float hDist = std::sqrt(dx * dx + dy * dy);
    float pitch = -std::atan2(dz, hDist);

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

    // Annoncer le nom et la distance
    const char* rawName = nearest->GetDisplayFullName();
    std::wstring name = rawName ? Utf8ToWString(rawName) : L"Enemy";
    std::wstring msg = name + L", " + std::to_wstring(static_cast<int>(nearestDist)) + L" units";
    Speak(msg);

    LOG("LockEnemy: {} at distance {}", rawName ? rawName : "?", nearestDist);
}

// SCANNER — FIN
