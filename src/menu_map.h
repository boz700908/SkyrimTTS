#pragma once

// MENU MAP — Accessibilité de la carte pour joueurs aveugles
// Navigation par marqueurs avec filtres
//
// Architecture:
//   - Marker list built from persistentCell (C++ data, reliable)
//   - Navigation is speech-only, camera moves for visual only
//   - Fast travel via Papyrus Game.FastTravel() (bypasses fragile GFx selection)
//   - Tooltip polling only for mouse hover (disabled during scanner navigation)

// --- Filtres de la carte ---
enum class MapFilter : int {
    All = 0,
    Discovered,
    Undiscovered,
    QuestTargets,
    COUNT
};

// Retourne le libelle traduit du filtre principal.
static std::wstring GetMapFilterName(MapFilter f) {
    switch (f) {
        case MapFilter::All:          return TR("All");
        case MapFilter::Discovered:   return TR("Discovered");
        case MapFilter::Undiscovered: return TR("Undiscovered");
        case MapFilter::QuestTargets: return TR("Quest Targets");
        default:                      return L"";
    }
}

// Sous-catégories par type de lieu (Alt+End)
enum class MapSubFilter : int {
    AllTypes = 0,
    Cities,
    Castles,
    Towns,
    Dungeons,
    Forts,
    Camps,
    COUNT
};

// Retourne le libelle traduit du sous-filtre par type.
static std::wstring GetMapSubFilterName(MapSubFilter f) {
    switch (f) {
        case MapSubFilter::AllTypes: return TR("All types");
        case MapSubFilter::Cities:   return TR("Cities");
        case MapSubFilter::Castles:  return TR("Castles");
        case MapSubFilter::Towns:    return TR("Towns");
        case MapSubFilter::Dungeons: return TR("Dungeons");
        case MapSubFilter::Forts:    return TR("Forts");
        case MapSubFilter::Camps:    return TR("Camps");
        default:                     return L"";
    }
}

// --- Marqueur de carte ---
struct MapMarkerInfo {
    RE::FormID    formID{0};
    std::wstring  name;
    std::wstring  typeName;
    RE::MARKER_TYPE markerType{RE::MARKER_TYPE::kNone};  // pour le sous-filtre par type
    float         distance{0.0f};
    std::wstring  direction;
    bool          discovered{false};
    bool          canTravelTo{false};
    bool          isQuestTarget{false};
    RE::NiPoint3  worldPos{0, 0, 0};  // position monde pour recalcul de distance
};

// --- État global ---
static std::vector<MapMarkerInfo>  g_mapMarkers;
static std::vector<int>            g_mapFiltered;   // indices dans g_mapMarkers
static MapFilter                   g_mapFilter{MapFilter::All};
static MapSubFilter                g_mapSubFilter{MapSubFilter::AllTypes};
static int                         g_mapIndex{-1};
static std::atomic_bool            g_mapOpen{false};
static std::atomic_bool            g_mapReady{false};   // true quand BuildMapMarkerList terminé
static std::string                 g_mapLastTooltip;    // dernier tooltip lu
static std::atomic_bool            g_mapPolling{false};
static std::atomic_bool            g_mapScannerActive{false};  // true = on navigue au scanner, supprime le polling tooltip
static std::atomic_bool            g_mapPendingRead{false};  // flood protection
static bool                        g_mapFastTravelConfirm{false};  // true = en attente de confirmation voyage rapide
static bool                        g_mapUseReference{false};  // true = distances depuis le point de référence
static RE::NiPoint3                g_mapReferencePos{0, 0, 0};
static std::wstring                g_mapReferenceName;

// --- Marqueur personnalisé (P) pour le scanner/autowalk ---
static RE::NiPoint3                g_customMarkerPos{0, 0, 0};
static std::wstring                g_customMarkerName;
static RE::FormID                  g_customMarkerFormID{0};
static bool                        g_customMarkerActive{false};

// --- Nom du type de marqueur (traduit) ---
static std::wstring GetMarkerTypeName(RE::MARKER_TYPE type) {
    switch (type) {
        case RE::MARKER_TYPE::kCity:            return TR("City");
        case RE::MARKER_TYPE::kTown:            return TR("Town");
        case RE::MARKER_TYPE::kSettlement:      return TR("Settlement");
        case RE::MARKER_TYPE::kCave:            return TR("Cave");
        case RE::MARKER_TYPE::kCamp:            return TR("Camp");
        case RE::MARKER_TYPE::kFort:            return TR("Fort");
        case RE::MARKER_TYPE::kNordicRuins:     return TR("Nordic Ruins");
        case RE::MARKER_TYPE::kDwemerRuin:      return TR("Dwemer Ruin");
        case RE::MARKER_TYPE::kShipwreck:       return TR("Shipwreck");
        case RE::MARKER_TYPE::kGrove:           return TR("Grove");
        case RE::MARKER_TYPE::kLandmark:        return TR("Landmark");
        case RE::MARKER_TYPE::kDragonLair:      return TR("Dragon Lair");
        case RE::MARKER_TYPE::kFarm:            return TR("Farm");
        case RE::MARKER_TYPE::kWoodMill:        return TR("Wood Mill");
        case RE::MARKER_TYPE::kMine:            return TR("Mine");
        case RE::MARKER_TYPE::kImperialCamp:    return TR("Imperial Camp");
        case RE::MARKER_TYPE::kStormcloakCamp:  return TR("Stormcloak Camp");
        case RE::MARKER_TYPE::kDoomstone:       return TR("Standing Stone");
        case RE::MARKER_TYPE::kWheatMill:       return TR("Wheat Mill");
        case RE::MARKER_TYPE::kSmelter:         return TR("Smelter");
        case RE::MARKER_TYPE::kStable:          return TR("Stable");
        case RE::MARKER_TYPE::kImperialTower:   return TR("Imperial Tower");
        case RE::MARKER_TYPE::kClearing:        return TR("Clearing");
        case RE::MARKER_TYPE::kPass:            return TR("Pass");
        case RE::MARKER_TYPE::kAlter:           return TR("Altar");
        case RE::MARKER_TYPE::kRock:            return TR("Rock");
        case RE::MARKER_TYPE::kLighthouse:      return TR("Lighthouse");
        case RE::MARKER_TYPE::kOrcStronghold:   return TR("Orc Stronghold");
        case RE::MARKER_TYPE::kGiantCamp:       return TR("Giant Camp");
        case RE::MARKER_TYPE::kShack:           return TR("Shack");
        case RE::MARKER_TYPE::kNordicTower:     return TR("Nordic Tower");
        case RE::MARKER_TYPE::kNordicDwelling:  return TR("Nordic Dwelling");
        case RE::MARKER_TYPE::kDocks:           return TR("Docks");
        case RE::MARKER_TYPE::kShrine:          return TR("Shrine");
        default: {
            int t = static_cast<int>(type);
            // Grandes villes de Bordeciel (35-52) : chaque ville a deux markers
            // distincts dans les donnees vanilla — un "Castle" (chateau du Jarl,
            // numero impair) et un "Capitol" (la ville elle-meme, numero pair).
            // Exemple : Whiterun = Fort Dragon (39, Castle) + Blanche-Rive (40, Capitol).
            if (t >= 35 && t <= 52) {
                return (t % 2 == 0) ? TR("Capitol") : TR("Castle");
            }
            // Lieux uniques de Solstheim (DLC Dragonborn)
            switch (type) {
                case RE::MARKER_TYPE::kDLC02_TempleOfMiraak: return TR("Temple");
                case RE::MARKER_TYPE::kDLC02_RavenRock:      return TR("Town");
                case RE::MARKER_TYPE::kDLC02_BeastStone:     return TR("Standing Stone");
                case RE::MARKER_TYPE::kDLC02_TelMithryn:     return TR("Settlement");
                case RE::MARKER_TYPE::kDLC02_ToSkyrim:       return TR("Docks");
                case RE::MARKER_TYPE::kDLC02_ToSolstheim:    return TR("Docks");
                default: break;
            }
            return TR("Location");
        }
    }
}

// --- Direction cardinale (8 directions) ---
static std::wstring GetDirectionString(float dx, float dy) {
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

// Forward declarations
static void ApplyMapFilter();
static void StartMapPolling();
static void StopMapPolling();

// --- Construire la liste de marqueurs depuis la cellule persistante ---
// MUST be called from AddUITask (UI thread) to be safe
static void AddQuestTargetsToMap(RE::PlayerCharacter* player, const RE::NiPoint3& playerPos, int& questCount) {
    if (!player) return;

    auto& objectives = REL::RelocateMemberIfNewer<RE::BSTArray<RE::BGSInstancedQuestObjective>>(
        SKSE::RUNTIME_SSE_1_6_629, player, 0x580, 0x588);

    std::set<std::string> seenObjectives;  // éviter les doublons par texte d'objectif

    LOG("MapMenu: quest scan - {} objectives in array", objectives.size());
    for (uint32_t i = 0; i < objectives.size(); i++) {
        auto& inst = objectives[i];
        if (!inst.Objective) continue;
        if (!inst.Objective->ownerQuest) continue;

        auto* quest = inst.Objective->ownerQuest;
        const char* qname = quest->GetFullName();
        const char* objText = inst.Objective->displayText.c_str();

        // Seulement les objectifs actuellement affichés (pas complétés/échoués)
        if (inst.InstanceState != RE::QUEST_OBJECTIVE_STATE::kDisplayed) {
            LOG("MapMenu: quest '{}' obj='{}' SKIPPED state={}", qname ? qname : "?", objText ? objText : "?", static_cast<int>(inst.InstanceState));
            continue;
        }

        if (!quest->IsActive()) {
            LOG("MapMenu: quest '{}' obj='{}' SKIPPED not active", qname ? qname : "?", objText ? objText : "?");
            continue;
        }

        LOG("MapMenu: quest '{}' obj='{}' PASSED filters", qname ? qname : "?", objText ? objText : "?");

        const char* objTextRaw = inst.Objective->displayText.c_str();
        std::string objTextStr = objTextRaw ? objTextRaw : "";

        // Éviter les doublons par texte d'objectif
        if (seenObjectives.count(objTextStr)) continue;
        seenObjectives.insert(objTextStr);

        LOG("MapMenu: quest '{}' has {} targets", qname ? qname : "?", inst.Objective->numTargets);

        // Double passage : d'abord avec CTDA, puis sans si rien ne passe
        // (même logique que le scanner — certaines quêtes ont des CTDA qui échouent
        //  alors que la quête est bien active et affichée sur la boussole)
        for (int pass = 0; pass < 2; pass++) {
        bool foundAnyTarget = false;
        for (uint32_t t = 0; t < inst.Objective->numTargets; t++) {
            auto* tgt = inst.Objective->targets[t];
            if (!tgt) continue;

            RE::ObjectRefHandle rh;
            quest->CreateRefHandleByAliasID(rh, tgt->alias);
            if (!rh) continue;
            auto sp = rh.get();
            if (!sp) continue;
            auto* ref = sp.get();
            if (!ref) continue;

            // Pass 0 : vérifier les conditions CTDA. Pass 1 : ignorer les CTDA.
            if (pass == 0 && tgt->conditions.head != nullptr) {
                if (!tgt->conditions.IsTrue(player, ref)) continue;
            }

            RE::FormID fid = ref->GetFormID();

            auto rp = ref->GetPosition();

            // Si la cible est dans un intérieur, trouver la position extérieure
            auto* refCell = ref->GetParentCell();
            if (refCell && refCell->IsInteriorCell()) {
                bool foundExit = false;

                // 1. worldLocMarker : position exacte de l'entrée du lieu sur la carte
                auto* targetLocation = refCell->GetLocation();
                for (auto* loc = targetLocation; loc && !foundExit; loc = loc->parentLoc) {
                    if (loc->worldLocMarker) {
                        auto markerPtr = loc->worldLocMarker.get();
                        if (markerPtr) {
                            // Vérifier que le marqueur est dans un worldspace extérieur
                            // sinon GetPosition() retourne des coordonnées intérieures inutilisables
                            auto* markerCell = markerPtr->GetParentCell();
                            if (markerCell && markerCell->IsInteriorCell()) {
                                LOG("MapMenu: quest '{}' worldLocMarker for '{}' is in interior cell, skipping",
                                    objText, loc->GetFullName() ? loc->GetFullName() : "?");
                                continue;
                            }
                            rp = markerPtr->GetPosition();
                            foundExit = true;
                            LOG("MapMenu: quest '{}' resolved via worldLocMarker loc='{}' pos=({:.0f},{:.0f},{:.0f})",
                                objText, loc->GetFullName() ? loc->GetFullName() : "?", rp.x, rp.y, rp.z);
                        }
                    }
                }

                // 2. Fallback : chercher une porte de sortie dans la cellule
                if (!foundExit) {
                    for (auto& doorHandle : refCell->GetRuntimeData().references) {
                        auto doorPtr = doorHandle.get();
                        if (!doorPtr) continue;
                        auto* doorBase = doorPtr->GetBaseObject();
                        if (!doorBase || doorBase->GetFormType() != RE::FormType::Door) continue;
                        auto* extraTele = doorPtr->extraList.GetByType<RE::ExtraTeleport>();
                        if (!extraTele || !extraTele->teleportData) continue;
                        auto linkedDoor = extraTele->teleportData->linkedDoor.get();
                        if (!linkedDoor) continue;
                        auto* destCell = linkedDoor->GetParentCell();
                        if (destCell && !destCell->IsInteriorCell()) {
                            rp = linkedDoor->GetPosition();
                            foundExit = true;
                            LOG("MapMenu: quest '{}' redirected to exit door at ({:.0f},{:.0f},{:.0f})", objText, rp.x, rp.y, rp.z);
                            break;
                        }
                        if (linkedDoor->GetWorldspace()) {
                            rp = linkedDoor->GetPosition();
                            foundExit = true;
                            LOG("MapMenu: quest '{}' redirected to exit door at ({:.0f},{:.0f},{:.0f})", objText, rp.x, rp.y, rp.z);
                            break;
                        }
                    }
                }
            }

            // Dédoublonnage : ignorer si un marqueur de quête existe déjà à la même position
            bool duplicate = false;
            for (auto& existing : g_mapMarkers) {
                if (!existing.isQuestTarget) continue;
                float ddx = existing.worldPos.x - rp.x;
                float ddy = existing.worldPos.y - rp.y;
                if (ddx * ddx + ddy * ddy < 100.0f * 100.0f) {  // < 100 unités = même endroit
                    duplicate = true;
                    LOG("MapMenu: quest '{}' ref={:08X} skipped (duplicate position near {:08X})", objText, fid, existing.formID);
                    break;
                }
            }
            if (duplicate) continue;

            float dx = rp.x - playerPos.x;
            float dy = rp.y - playerPos.y;

            MapMarkerInfo m;
            m.formID = fid;
            m.name = Utf8ToWString(objText);
            m.typeName = TR("Quest Target");
            m.distance = std::sqrt(dx * dx + dy * dy);
            m.direction = GetDirectionString(dx, dy);
            m.isQuestTarget = true;
            m.worldPos = rp;

            LOG("MapMenu: quest '{}' ref={:08X} pos=({:.0f},{:.0f},{:.0f}) dist={:.0f} pass={}",
                objText, fid, rp.x, rp.y, rp.z, m.distance, pass);
            g_mapMarkers.push_back(std::move(m));
            questCount++;
            foundAnyTarget = true;
            break;  // un seul marqueur par objectif (le premier valide)
        }
        if (foundAnyTarget) break;  // pass 0 a trouvé un target, pas besoin du pass 1
        if (pass == 0) {
            LOG("MapMenu: quest '{}' pass 0 failed for all targets, retrying without CTDA", qname ? qname : "?");
        }
        }  // fin boucle pass
    }
}

static void BuildMapMarkerList() {
    g_mapReady.store(false);
    g_mapMarkers.clear();
    g_mapFiltered.clear();
    g_mapIndex = -1;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        LOG("MapMenu: no player");
        return;
    }
    auto playerPos = player->GetPosition();

    // Get worldspace - try player first (most reliable)
    auto* worldSpace = player->GetWorldspace();
    if (!worldSpace) {
        LOG("MapMenu: no worldspace (interior?)");
        return;
    }

    LOG("MapMenu: worldspace = '{}' ({:08X})", worldSpace->GetName(), worldSpace->GetFormID());

    // Scanner les marqueurs depuis TOUS les worldspaces via TESDataHandler
    // (pas juste la cellule persistante du joueur)
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        LOG("MapMenu: no TESDataHandler");
        return;
    }

    int totalRefs = 0;
    int markerCount = 0;

    // Lambda pour scanner une cellule persistante
    auto scanPersistentCell = [&](RE::TESObjectCELL* persistentCell) {
        if (!persistentCell) return;
        for (auto& refPtr : persistentCell->GetRuntimeData().references) {
            totalRefs++;
            auto* ref = refPtr.get();
            if (!ref) continue;

            auto* extraMarker = ref->extraList.GetByType<RE::ExtraMapMarker>();
            if (!extraMarker) continue;
            if (!extraMarker->mapData) continue;

            // Ignorer les markers desactives (kInitiallyDisabled + ExtraEnableStateParent
            // controle par quest alias). Camps de guerre civile, Fort Garde-l'Aube, etc.
            // ont kCanTravelTo=1 dans l'ESM mais restent disabled tant que leur quete
            // n'a pas appele Enable() — le moteur ne les affiche pas sur la carte vanilla.
            if (ref->IsDisabled() || ref->IsMarkedForDeletion()) continue;

            auto* mapData = extraMarker->mapData;

            const char* rawName = mapData->locationName.GetFullName();
            if (!rawName || !*rawName) continue;

            bool canTravel = mapData->flags.any(RE::MapMarkerData::Flag::kCanTravelTo);
            // kCanTravelTo bascule a 1 UNIQUEMENT quand le joueur entre
            // physiquement dans le rayon de decouverte du marker, et c'est
            // le bit qui conditionne le fast travel cote moteur.
            // kVisible est pre-defini a 1 dans les ESM pour les grandes
            // villes (Solitude, Blancherive...) et les camps militaires,
            // donc ne convient PAS comme indicateur "decouvert".
            // kShowAllHidden exclu pour gerer le cas de la commande console
            // "tmm 1" qui active tous les marqueurs.
            bool discovered = canTravel &&
                              !mapData->flags.any(RE::MapMarkerData::Flag::kShowAllHidden);

            RE::MARKER_TYPE markerType = mapData->type.get();

            auto refPos = ref->GetPosition();
            float dx = refPos.x - playerPos.x;
            float dy = refPos.y - playerPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            // Éviter les doublons par FormID
            bool dup = false;
            for (auto& existing : g_mapMarkers) {
                if (existing.formID == ref->GetFormID()) { dup = true; break; }
            }
            if (dup) continue;

            MapMarkerInfo marker;
            marker.formID = ref->GetFormID();
            marker.name = Utf8ToWString(rawName);
            marker.typeName = GetMarkerTypeName(markerType);
            marker.markerType = markerType;
            marker.distance = dist;
            marker.direction = GetDirectionString(dx, dy);
            marker.discovered = discovered;
            marker.canTravelTo = canTravel;
            marker.worldPos = refPos;

            g_mapMarkers.push_back(std::move(marker));
            markerCount++;
        }
    };

    // Trouver le worldspace racine (ex: si on est dans Blancherive, remonter à Tamriel)
    auto* rootWorld = worldSpace;
    while (rootWorld->parentWorld) {
        rootWorld = rootWorld->parentWorld;
    }
    LOG("MapMenu: root worldspace = '{}' ({:08X})", rootWorld->GetName(), rootWorld->GetFormID());

    // Scanner le worldspace racine
    if (rootWorld->persistentCell) {
        scanPersistentCell(rootWorld->persistentCell);
        LOG("MapMenu: scanned root worldspace '{}', {} markers so far", rootWorld->GetName(), markerCount);
    }

    // Scanner les worldspaces enfants de la racine (villes, etc.)
    // Exclure les worldspaces indépendants (Solstheim quand on est à Bordeciel et inversement)
    auto& worldSpaces = dataHandler->GetFormArray<RE::TESWorldSpace>();
    for (auto* ws : worldSpaces) {
        if (!ws || ws == rootWorld) continue;  // déjà scanné
        if (!ws->persistentCell) continue;

        // N'inclure que les worldspaces qui descendent de la même racine
        bool sameRoot = false;
        for (auto* parent = ws->parentWorld; parent; parent = parent->parentWorld) {
            if (parent == rootWorld) { sameRoot = true; break; }
        }
        if (!sameRoot) continue;

        int before = markerCount;
        scanPersistentCell(ws->persistentCell);
        if (markerCount > before) {
            LOG("MapMenu: worldspace '{}' added {} markers", ws->GetName(), markerCount - before);
        }
    }

    // --- Ajouter les cibles de quête actives ---
    int questCount = 0;
    AddQuestTargetsToMap(player, playerPos, questCount);
    LOG("MapMenu: {} quest targets added", questCount);

    // Sort by distance
    std::sort(g_mapMarkers.begin(), g_mapMarkers.end(),
        [](const MapMarkerInfo& a, const MapMarkerInfo& b) {
            return a.distance < b.distance;
        });

    LOG("MapMenu: {} refs scanned, {} markers + {} quests found", totalRefs, markerCount, questCount);

    ApplyMapFilter();
    g_mapReady.store(true);
}

// --- Appliquer le filtre ---
static bool MatchesSubFilter(const MapMarkerInfo& m) {
    if (g_mapSubFilter == MapSubFilter::AllTypes) return true;
    // Les cibles de quete n'ont pas de markerType (kNone), donc elles ne
    // correspondent a aucun sous-filtre par type — elles ont leur propre
    // categorie principale "QuestTargets" pour etre listees.
    if (m.isQuestTarget) return false;

    int t = static_cast<int>(m.markerType);
    switch (g_mapSubFilter) {
        case MapSubFilter::Cities:
            // kCity (1) + Capitols pairs (36, 38, 40, 42, 44, 46, 48, 50, 52)
            // + Raven Rock (54, ville de Solstheim).
            return m.markerType == RE::MARKER_TYPE::kCity ||
                   (t >= 35 && t <= 52 && t % 2 == 0) ||
                   m.markerType == RE::MARKER_TYPE::kDLC02_RavenRock;
        case MapSubFilter::Castles:
            // Chateaux des Jarls (impairs 35, 37, 39, 41, 43, 45, 47, 49, 51).
            return t >= 35 && t <= 52 && t % 2 == 1;
        case MapSubFilter::Towns:
            return m.markerType == RE::MARKER_TYPE::kTown ||
                   m.markerType == RE::MARKER_TYPE::kSettlement ||
                   m.markerType == RE::MARKER_TYPE::kDLC02_TelMithryn;
        case MapSubFilter::Dungeons:
            return m.markerType == RE::MARKER_TYPE::kCave ||
                   m.markerType == RE::MARKER_TYPE::kNordicRuins ||
                   m.markerType == RE::MARKER_TYPE::kDwemerRuin ||
                   m.markerType == RE::MARKER_TYPE::kDragonLair;
        case MapSubFilter::Forts:
            return m.markerType == RE::MARKER_TYPE::kFort ||
                   m.markerType == RE::MARKER_TYPE::kImperialTower ||
                   m.markerType == RE::MARKER_TYPE::kNordicTower;
        case MapSubFilter::Camps:
            return m.markerType == RE::MARKER_TYPE::kCamp ||
                   m.markerType == RE::MARKER_TYPE::kImperialCamp ||
                   m.markerType == RE::MARKER_TYPE::kStormcloakCamp ||
                   m.markerType == RE::MARKER_TYPE::kGiantCamp ||
                   m.markerType == RE::MARKER_TYPE::kOrcStronghold;
        default: return true;
    }
}

static void ApplyMapFilter() {
    g_mapFiltered.clear();

    for (int i = 0; i < static_cast<int>(g_mapMarkers.size()); i++) {
        auto& m = g_mapMarkers[i];

        // Filtre principal
        bool passMain = false;
        switch (g_mapFilter) {
            case MapFilter::All:          passMain = true; break;
            case MapFilter::Discovered:   passMain = m.discovered; break;
            case MapFilter::Undiscovered: passMain = !m.discovered && !m.isQuestTarget; break;
            case MapFilter::QuestTargets: passMain = m.isQuestTarget; break;
            default: break;
        }
        if (!passMain) continue;

        // Sous-filtre par type
        if (!MatchesSubFilter(m)) continue;

        g_mapFiltered.push_back(i);
    }

    g_mapIndex = g_mapFiltered.empty() ? -1 : 0;
    LOG("MapMenu: filter '{}' sub '{}' -> {} markers",
        WStringToUtf8(GetMapFilterName(g_mapFilter)),
        WStringToUtf8(GetMapSubFilterName(g_mapSubFilter)),
        g_mapFiltered.size());
}

// --- Formater l'annonce d'un marqueur ---
static std::wstring FormatMapMarkerAnnounce(const MapMarkerInfo& m, bool fullDetails = false) {
    std::wstring msg = m.name;
    msg += L", " + m.typeName;

    // Convert distance to a more meaningful unit (Skyrim units / 70 ~ feet, / 21 ~ meters)
    int distUnits = static_cast<int>(m.distance);
    msg += L", " + std::to_wstring(distUnits) + L" " + TR("units") + L" " + m.direction;

    if (fullDetails) {
        if (!m.discovered) msg += L", " + TR("undiscovered");
        if (m.canTravelTo) msg += L", " + TR("can fast travel");
        else msg += L", " + TR("cannot fast travel");
    }

    return msg;
}

// --- Loguer l'état de la caméra ---
static void LogMapCameraState(const char* context) {
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menuPtr = ui->GetMenu(RE::MapMenu::MENU_NAME);
    if (!menuPtr) return;
    auto* mapMenu = static_cast<RE::MapMenu*>(menuPtr.get());
    if (!mapMenu) return;

    auto& rd2 = mapMenu->GetRuntimeData2();
    auto& cam = rd2.camera;
    auto* camState = cam.currentState.get();

    if (!camState) {
        LOG("MapMenu[{}]: camera state is null", context);
        return;
    }

    auto* worldState = skyrim_cast<RE::MapCameraStates::World*>(camState);
    if (worldState) {
        LOG("MapMenu[{}]: cam=World pos=({:.0f},{:.0f},{:.0f}) scroll=({:.0f},{:.0f},{:.0f})",
            context,
            worldState->currentPosition.x, worldState->currentPosition.y, worldState->currentPosition.z,
            worldState->currentPositionScrollOffset.x, worldState->currentPositionScrollOffset.y, worldState->currentPositionScrollOffset.z);
        if (worldState->mapData) {
            LOG("MapMenu[{}]: mapBounds min=({:.0f},{:.0f}) max=({:.0f},{:.0f})",
                context,
                worldState->mapData->minimumCoordinates.x, worldState->mapData->minimumCoordinates.y,
                worldState->mapData->maximumCoordinates.x, worldState->mapData->maximumCoordinates.y);
        }
    } else {
        auto* transState = skyrim_cast<RE::MapCameraStates::Transition*>(camState);
        if (transState) {
            LOG("MapMenu[{}]: cam=Transition pos=({:.0f},{:.0f},{:.0f}) dest=({:.0f},{:.0f},{:.0f}) origin=({:.0f},{:.0f},{:.0f})",
                context,
                transState->currentPosition.x, transState->currentPosition.y, transState->currentPosition.z,
                transState->zoomDestination.x, transState->zoomDestination.y, transState->zoomDestination.z,
                transState->zoomOrigin.x, transState->zoomOrigin.y, transState->zoomOrigin.z);
        } else {
            LOG("MapMenu[{}]: cam=unknown state type", context);
        }
    }

    // Log le worldspace affiché
    if (rd2.worldSpace) {
        LOG("MapMenu[{}]: worldSpace='{}'", context, rd2.worldSpace->GetName());
    }

    // Log le marqueur GFx sélectionné
    auto& mapMenuGfx = rd2.unk30540;
    if (SafeIsObject(mapMenuGfx)) {
        RE::GFxValue selMarker;
        if (mapMenuGfx.GetMember("SelectedMarker", &selMarker) && SafeIsObject(selMarker)) {
            RE::GFxValue lbl, xv, yv, vis;
            selMarker.GetMember("_label", &lbl);
            selMarker.GetMember("_x", &xv);
            selMarker.GetMember("_y", &yv);
            selMarker.GetMember("_visible", &vis);
            LOG("MapMenu[{}]: GFx selected='{}' x={:.1f} y={:.1f} visible={}",
                context,
                (SafeIsString(lbl) ? SafeGetString(lbl) : "?"),
                (SafeIsNumber(xv) ? SafeGetNumber(xv) : -1),
                (SafeIsNumber(yv) ? SafeGetNumber(yv) : -1),
                (SafeIsNumber(vis) || SafeIsBool(vis)) ? (SafeIsNumber(vis) ? (SafeGetNumber(vis) != 0) : SafeGetBool(vis)) : false);
        } else {
            LOG("MapMenu[{}]: GFx no marker selected", context);
        }
    }
}

// --- Centrer la carte sur un marqueur ---
// Déplace la caméra C++ vers les coordonnées monde du marqueur (visuel uniquement)
// Le voyage rapide est géré par MapFastTravel() via Papyrus, sans passer par GFx
static void TryCenterMapOnMarker(const MapMarkerInfo& marker) {
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;

    auto menuPtr = ui->GetMenu(RE::MapMenu::MENU_NAME);
    if (!menuPtr) return;

    auto* mapMenu = static_cast<RE::MapMenu*>(menuPtr.get());
    if (!mapMenu || !mapMenu->uiMovie) return;

    auto& rd2 = mapMenu->GetRuntimeData2();

    LOG("MapMenu: >>> TryCenterMapOnMarker '{}' type='{}' worldPos=({:.0f},{:.0f},{:.0f}) quest={}",
        WStringToUtf8(marker.name), WStringToUtf8(marker.typeName),
        marker.worldPos.x, marker.worldPos.y, marker.worldPos.z,
        marker.isQuestTarget);

    // Désactiver le polling tooltip — le scanner gère la lecture
    g_mapScannerActive.store(true);

    // Déplacer la caméra vers la position monde du marqueur (visuel uniquement)
    auto& cam = rd2.camera;
    auto* camState = cam.currentState.get();
    if (camState) {
        auto* worldState = skyrim_cast<RE::MapCameraStates::World*>(camState);
        if (worldState) {
            worldState->currentPosition.x = marker.worldPos.x;
            worldState->currentPosition.y = marker.worldPos.y;
        }
    }
}

// --- Navigation : marqueur suivant ---
static void MapNextMarker() {
    g_mapFastTravelConfirm = false;  // annuler toute confirmation en cours
    if (!g_mapReady.load()) {
        Speak(TR("Loading markers"));
        return;
    }
    if (g_mapFiltered.empty()) {
        Speak(TR("No markers in this filter"));
        return;
    }

    g_mapIndex++;
    if (g_mapIndex >= static_cast<int>(g_mapFiltered.size()))
        g_mapIndex = 0;

    int filteredIdx = g_mapFiltered[g_mapIndex];
    if (filteredIdx < 0 || filteredIdx >= static_cast<int>(g_mapMarkers.size())) {
        LOG("MapMenu: bounds error idx={} markers={}", filteredIdx, g_mapMarkers.size());
        return;
    }

    auto& m = g_mapMarkers[filteredIdx];
    std::wstring pos = L". " + std::to_wstring(g_mapIndex + 1) + L" " + TR("of") + L" " + std::to_wstring(g_mapFiltered.size());
    Speak(FormatMapMarkerAnnounce(m) + pos);
    TryCenterMapOnMarker(m);
}

// --- Navigation : marqueur précédent ---
static void MapPrevMarker() {
    g_mapFastTravelConfirm = false;  // annuler toute confirmation en cours
    if (!g_mapReady.load()) {
        Speak(TR("Loading markers"));
        return;
    }
    if (g_mapFiltered.empty()) {
        Speak(TR("No markers in this filter"));
        return;
    }

    g_mapIndex--;
    if (g_mapIndex < 0)
        g_mapIndex = static_cast<int>(g_mapFiltered.size()) - 1;

    int filteredIdx = g_mapFiltered[g_mapIndex];
    if (filteredIdx < 0 || filteredIdx >= static_cast<int>(g_mapMarkers.size())) {
        LOG("MapMenu: bounds error idx={} markers={}", filteredIdx, g_mapMarkers.size());
        return;
    }

    auto& m = g_mapMarkers[filteredIdx];
    std::wstring pos = L". " + std::to_wstring(g_mapIndex + 1) + L" " + TR("of") + L" " + std::to_wstring(g_mapFiltered.size());
    Speak(FormatMapMarkerAnnounce(m) + pos);
    TryCenterMapOnMarker(m);
}

// --- Annoncer les détails complets ---
static void MapAnnounceDetails() {
    if (!g_mapReady.load() || g_mapFiltered.empty() || g_mapIndex < 0) {
        Speak(TR("No marker selected"));
        return;
    }

    int filteredIdx = g_mapFiltered[g_mapIndex];
    if (filteredIdx < 0 || filteredIdx >= static_cast<int>(g_mapMarkers.size())) return;

    auto& m = g_mapMarkers[filteredIdx];
    std::wstring pos = L". " + std::to_wstring(g_mapIndex + 1) + L" " + TR("of") + L" " + std::to_wstring(g_mapFiltered.size());
    Speak(FormatMapMarkerAnnounce(m, true) + pos);
}

// --- Point de référence (Shift+Home) ---
static void MapSetReference() {
    if (!g_mapReady.load() || g_mapFiltered.empty() || g_mapIndex < 0) {
        Speak(TR("No marker selected"));
        return;
    }

    // Si déjà en mode référence, revenir au mode joueur
    if (g_mapUseReference) {
        g_mapUseReference = false;

        // Recalculer les distances depuis le joueur
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            auto playerPos = player->GetPosition();
            for (auto& m : g_mapMarkers) {
                float dx = m.worldPos.x - playerPos.x;
                float dy = m.worldPos.y - playerPos.y;
                m.distance = std::sqrt(dx * dx + dy * dy);
                m.direction = GetDirectionString(dx, dy);
            }
        }

        // Re-trier et re-filtrer
        std::sort(g_mapMarkers.begin(), g_mapMarkers.end(),
            [](const MapMarkerInfo& a, const MapMarkerInfo& b) { return a.distance < b.distance; });
        ApplyMapFilter();

        Speak(TR("Reference cleared, distances from player"));
        return;
    }

    // Définir le point de référence
    int filteredIdx = g_mapFiltered[g_mapIndex];
    if (filteredIdx < 0 || filteredIdx >= static_cast<int>(g_mapMarkers.size())) return;

    auto& refMarker = g_mapMarkers[filteredIdx];
    g_mapReferencePos = refMarker.worldPos;
    g_mapReferenceName = refMarker.name;
    g_mapUseReference = true;

    // Recalculer les distances depuis le point de référence
    for (auto& m : g_mapMarkers) {
        float dx = m.worldPos.x - g_mapReferencePos.x;
        float dy = m.worldPos.y - g_mapReferencePos.y;
        m.distance = std::sqrt(dx * dx + dy * dy);
        m.direction = GetDirectionString(dx, dy);
    }

    // Re-trier et re-filtrer
    std::sort(g_mapMarkers.begin(), g_mapMarkers.end(),
        [](const MapMarkerInfo& a, const MapMarkerInfo& b) { return a.distance < b.distance; });
    ApplyMapFilter();
    g_mapIndex = 0;

    Speak(TR("Reference") + L": " + g_mapReferenceName + L". " + TR("Distances from this marker"));
}

// --- Sous-filtre par type de lieu (Alt+End) ---
static void MapCycleSubFilter() {
    if (!g_mapReady.load()) {
        Speak(TR("Loading markers"));
        return;
    }

    int next = (static_cast<int>(g_mapSubFilter) + 1) % static_cast<int>(MapSubFilter::COUNT);
    g_mapSubFilter = static_cast<MapSubFilter>(next);

    ApplyMapFilter();

    std::wstring msg = GetMapSubFilterName(g_mapSubFilter);
    msg += L", " + std::to_wstring(g_mapFiltered.size()) + L" " + TR("markers");
    if (!g_mapFiltered.empty()) {
        int idx = g_mapFiltered[0];
        if (idx >= 0 && idx < static_cast<int>(g_mapMarkers.size())) {
            msg += L". " + FormatMapMarkerAnnounce(g_mapMarkers[idx]);
        }
    }
    Speak(msg);
}

// --- Placer un marqueur personnalisé (P) ---
static void MapPlaceCustomMarker() {
    if (!g_mapReady.load() || g_mapFiltered.empty() || g_mapIndex < 0) {
        Speak(TR("No marker selected"));
        return;
    }

    int filteredIdx = g_mapFiltered[g_mapIndex];
    if (filteredIdx < 0 || filteredIdx >= static_cast<int>(g_mapMarkers.size())) return;

    auto& m = g_mapMarkers[filteredIdx];

    // Si on appuie P sur le même marqueur déjà actif, le retirer
    if (g_customMarkerActive && g_customMarkerName == m.name) {
        g_customMarkerActive = false;
        g_customMarkerName.clear();
        g_customMarkerPos = {0, 0, 0};
        Speak(TR("Marker removed"));
        return;
    }

    g_customMarkerPos = m.worldPos;
    g_customMarkerName = m.name;
    g_customMarkerFormID = m.formID;
    g_customMarkerActive = true;

    Speak(TR("Marker placed on") + L" " + m.name);
}

// --- Cycler les filtres ---
static void MapCycleFilter() {
    if (!g_mapReady.load()) {
        Speak(TR("Loading markers"));
        return;
    }

    int next = (static_cast<int>(g_mapFilter) + 1) % static_cast<int>(MapFilter::COUNT);
    g_mapFilter = static_cast<MapFilter>(next);
    g_mapSubFilter = MapSubFilter::AllTypes;  // reset sous-filtre

    ApplyMapFilter();

    std::wstring msg = GetMapFilterName(g_mapFilter);
    msg += L", " + std::to_wstring(g_mapFiltered.size()) + L" " + TR("markers");
    if (!g_mapFiltered.empty()) {
        int idx = g_mapFiltered[0];
        if (idx >= 0 && idx < static_cast<int>(g_mapMarkers.size())) {
            msg += L". " + FormatMapMarkerAnnounce(g_mapMarkers[idx]);
        }
    }
    Speak(msg);
}

// --- Cycler les filtres dans l'autre sens (pour la manette) ---
static void MapCyclePrevFilter() {
    if (!g_mapReady.load()) {
        Speak(TR("Loading markers"));
        return;
    }

    int count = static_cast<int>(MapFilter::COUNT);
    int prev = (static_cast<int>(g_mapFilter) - 1 + count) % count;
    g_mapFilter = static_cast<MapFilter>(prev);
    g_mapSubFilter = MapSubFilter::AllTypes;  // reset sous-filtre

    ApplyMapFilter();

    std::wstring msg = GetMapFilterName(g_mapFilter);
    msg += L", " + std::to_wstring(g_mapFiltered.size()) + L" " + TR("markers");
    if (!g_mapFiltered.empty()) {
        int idx = g_mapFiltered[0];
        if (idx >= 0 && idx < static_cast<int>(g_mapMarkers.size())) {
            msg += L". " + FormatMapMarkerAnnounce(g_mapMarkers[idx]);
        }
    }
    Speak(msg);
}

// --- Cycler les sous-filtres dans l'autre sens (pour la manette) ---
static void MapCyclePrevSubFilter() {
    if (!g_mapReady.load()) {
        Speak(TR("Loading markers"));
        return;
    }

    int count = static_cast<int>(MapSubFilter::COUNT);
    int prev = (static_cast<int>(g_mapSubFilter) - 1 + count) % count;
    g_mapSubFilter = static_cast<MapSubFilter>(prev);

    ApplyMapFilter();

    std::wstring msg = GetMapSubFilterName(g_mapSubFilter);
    msg += L", " + std::to_wstring(g_mapFiltered.size()) + L" " + TR("markers");
    if (!g_mapFiltered.empty()) {
        int idx = g_mapFiltered[0];
        if (idx >= 0 && idx < static_cast<int>(g_mapMarkers.size())) {
            msg += L". " + FormatMapMarkerAnnounce(g_mapMarkers[idx]);
        }
    }
    Speak(msg);
}

// --- Voyage rapide via Papyrus (bypass GFx) ---
// Premier Entrée = demande confirmation, deuxième Entrée = confirme
static void MapFastTravel() {
    if (!g_mapReady.load() || g_mapFiltered.empty() || g_mapIndex < 0) {
        Speak(TR("No marker selected"));
        return;
    }

    int filteredIdx = g_mapFiltered[g_mapIndex];
    if (filteredIdx < 0 || filteredIdx >= static_cast<int>(g_mapMarkers.size())) return;

    auto& m = g_mapMarkers[filteredIdx];

    if (!m.canTravelTo) {
        Speak(TR("Cannot fast travel here"));
        return;
    }

    if (m.formID == 0) {
        Speak(TR("No valid destination"));
        return;
    }

    // Premier appui : demander confirmation
    if (!g_mapFastTravelConfirm) {
        g_mapFastTravelConfirm = true;
        Speak(TR("Fast travel to") + L" " + m.name + L"? " + TR("Press Enter to confirm"));
        return;
    }

    // Deuxième appui : confirmer et voyager
    g_mapFastTravelConfirm = false;

    LOG("MapMenu: fast travel to '{}' formID={:08X}", WStringToUtf8(m.name), m.formID);

    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        LOG("MapMenu: VM not available");
        Speak(TR("Error: VM not available"));
        return;
    }

    auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SkyrimTTS_AutoWalkQuest");
    if (!quest) {
        LOG("MapMenu: SkyrimTTS_AutoWalkQuest quest not found");
        Speak(TR("Error: autowalk quest not found"));
        return;
    }

    auto* policy = vm->GetObjectHandlePolicy();
    if (!policy) {
        LOG("MapMenu: no handle policy");
        return;
    }

    auto handle = policy->GetHandleForObject(RE::FormType::Quest, quest);
    if (handle == policy->EmptyHandle()) {
        LOG("MapMenu: failed to get quest handle");
        Speak(TR("Error: quest handle failed"));
        return;
    }

    auto* args = RE::MakeFunctionArguments(static_cast<std::int32_t>(m.formID));
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    vm->DispatchMethodCall(
        handle,
        RE::BSFixedString("SkyrimTTS_AutoWalk"),
        RE::BSFixedString("OnFastTravel"),
        args,
        callback);

    Speak(TR("Traveling to") + L" " + m.name);
}

// --- Ouverture de la carte ---
static void OnMapOpen() {
    g_mapOpen.store(true);
    g_mapReady.store(false);
    g_mapFilter = MapFilter::All;
    g_mapSubFilter = MapSubFilter::AllTypes;
    g_mapMarkers.clear();
    g_mapFiltered.clear();
    g_mapIndex = -1;
    g_mapLastTooltip.clear();
    g_mapUseReference = false;
    g_mapReferenceName.clear();

    // Build marker list on UI thread (safe for GFx access)
    auto* task = SKSE::GetTaskInterface();
    if (task) {
        task->AddUITask([]() {
            // Guard: map may have closed before this runs
            if (!g_mapOpen.load()) return;

            BuildMapMarkerList();
            int count = static_cast<int>(g_mapFiltered.size());
            SpeakQueue(std::to_wstring(count) + L" " + TR("map markers"));

            // Auto-select first marker
            if (!g_mapFiltered.empty()) {
                g_mapIndex = 0;
                int idx = g_mapFiltered[0];
                if (idx >= 0 && idx < static_cast<int>(g_mapMarkers.size())) {
                    SpeakQueue(FormatMapMarkerAnnounce(g_mapMarkers[idx]));
                }
            }
        });
    }

    // Start tooltip polling (for mouse hover detection)
    StartMapPolling();
}

// --- Fermeture de la carte ---
static void OnMapClose() {
    StopMapPolling();
    g_mapOpen.store(false);
    g_mapReady.store(false);
    g_mapScannerActive.store(false);
    // Don't clear vectors here -- polling thread may still have a queued task.
    // The vectors will be cleared on next OnMapOpen().
}

// --- Polling du tooltip (survol souris) ---
static void PollMapTooltip() {
    if (!g_mapOpen.load() || !g_mapPolling.load()) return;

    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;

    auto menuPtr = ui->GetMenu(RE::MapMenu::MENU_NAME);
    if (!menuPtr) return;

    auto* mapMenu = static_cast<RE::MapMenu*>(menuPtr.get());
    if (!mapMenu || !mapMenu->uiMovie) return;

    // Pas de polling tooltip quand on navigue au scanner
    if (g_mapScannerActive.load()) return;

    // Mode souris : lire le tooltip normalement
    RE::GFxValue titleVal;
    if (mapMenu->uiMovie->GetVariable(&titleVal,
            "_root.MarkerDescriptionHolder.Description.Title.text") &&
        SafeIsString(titleVal)) {
        std::string title = SafeGetString(titleVal);
        if (!title.empty() && title != "Marker Name" && title != g_mapLastTooltip) {
            g_mapLastTooltip = title;
            Speak(Utf8ToWString(title.c_str()));
        }
    }
}

static void StartMapPolling() {
    g_mapPolling.store(true);
    g_mapLastTooltip.clear();

    std::thread([]() {
        while (g_mapPolling.load()) {
            if (!g_mapOpen.load()) break;

            auto* task = SKSE::GetTaskInterface();
            if (task) {
                task->AddUITask([]() {
                    PollMapTooltip();
                });
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }).detach();
}

static void StopMapPolling() {
    g_mapPolling.store(false);
}

// MAP MENU — FIN
