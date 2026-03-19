#pragma once

// MENU MAP — Accessibilité de la carte pour joueurs aveugles
// Navigation par marqueurs avec filtres
//
// Architecture:
//   - Marker list built from persistentCell (C++ data, reliable)
//   - Navigation is speech-only (no GFx visual selection - too fragile)
//   - Tooltip polling reads MarkerDescription when game selects a marker
//   - Fast travel uses the engine's native PlaceMarker + key simulation

// --- Filtres de la carte ---
enum class MapFilter : int {
    All = 0,
    Discovered,
    Undiscovered,
    COUNT
};

static const wchar_t* g_mapFilterNames[] = {
    L"All",
    L"Discovered",
    L"Undiscovered"
};

// --- Marqueur de carte ---
struct MapMarkerInfo {
    RE::FormID    formID{0};
    std::wstring  name;
    std::wstring  typeName;
    float         distance{0.0f};
    std::wstring  direction;
    bool          discovered{false};
    bool          canTravelTo{false};
    RE::NiPoint3  worldPos{0, 0, 0};  // position monde pour recalcul de distance
};

// --- État global ---
static std::vector<MapMarkerInfo>  g_mapMarkers;
static std::vector<int>            g_mapFiltered;   // indices dans g_mapMarkers
static MapFilter                   g_mapFilter{MapFilter::All};
static int                         g_mapIndex{-1};
static std::atomic_bool            g_mapOpen{false};
static std::atomic_bool            g_mapReady{false};   // true quand BuildMapMarkerList terminé
static std::string                 g_mapLastTooltip;    // dernier tooltip lu
static std::atomic_bool            g_mapPolling{false};
static std::atomic_bool            g_mapPendingRead{false};  // flood protection
static bool                        g_mapUseReference{false};  // true = distances depuis le point de référence
static RE::NiPoint3                g_mapReferencePos{0, 0, 0};
static std::wstring                g_mapReferenceName;

// --- Nom du type de marqueur ---
static const wchar_t* GetMarkerTypeName(RE::MARKER_TYPE type) {
    switch (type) {
        case RE::MARKER_TYPE::kCity:            return L"City";
        case RE::MARKER_TYPE::kTown:            return L"Town";
        case RE::MARKER_TYPE::kSettlement:      return L"Settlement";
        case RE::MARKER_TYPE::kCave:            return L"Cave";
        case RE::MARKER_TYPE::kCamp:            return L"Camp";
        case RE::MARKER_TYPE::kFort:            return L"Fort";
        case RE::MARKER_TYPE::kNordicRuins:     return L"Nordic Ruins";
        case RE::MARKER_TYPE::kDwemerRuin:      return L"Dwemer Ruin";
        case RE::MARKER_TYPE::kShipwreck:       return L"Shipwreck";
        case RE::MARKER_TYPE::kGrove:           return L"Grove";
        case RE::MARKER_TYPE::kLandmark:        return L"Landmark";
        case RE::MARKER_TYPE::kDragonLair:      return L"Dragon Lair";
        case RE::MARKER_TYPE::kFarm:            return L"Farm";
        case RE::MARKER_TYPE::kWoodMill:        return L"Wood Mill";
        case RE::MARKER_TYPE::kMine:            return L"Mine";
        case RE::MARKER_TYPE::kImperialCamp:    return L"Imperial Camp";
        case RE::MARKER_TYPE::kStormcloakCamp:  return L"Stormcloak Camp";
        case RE::MARKER_TYPE::kDoomstone:       return L"Standing Stone";
        case RE::MARKER_TYPE::kWheatMill:       return L"Wheat Mill";
        case RE::MARKER_TYPE::kSmelter:         return L"Smelter";
        case RE::MARKER_TYPE::kStable:          return L"Stable";
        case RE::MARKER_TYPE::kImperialTower:   return L"Imperial Tower";
        case RE::MARKER_TYPE::kClearing:        return L"Clearing";
        case RE::MARKER_TYPE::kPass:            return L"Pass";
        case RE::MARKER_TYPE::kAlter:           return L"Altar";
        case RE::MARKER_TYPE::kRock:            return L"Rock";
        case RE::MARKER_TYPE::kLighthouse:      return L"Lighthouse";
        case RE::MARKER_TYPE::kOrcStronghold:   return L"Orc Stronghold";
        case RE::MARKER_TYPE::kGiantCamp:       return L"Giant Camp";
        case RE::MARKER_TYPE::kShack:           return L"Shack";
        case RE::MARKER_TYPE::kNordicTower:     return L"Nordic Tower";
        case RE::MARKER_TYPE::kNordicDwelling:  return L"Nordic Dwelling";
        case RE::MARKER_TYPE::kDocks:           return L"Docks";
        case RE::MARKER_TYPE::kShrine:          return L"Shrine";
        default: {
            int t = static_cast<int>(type);
            if (t >= 35 && t <= 52) return L"Castle";
            if (t >= 53 && t <= 58) return L"Solstheim";
            return L"Location";
        }
    }
}

// --- Direction cardinale (8 directions) ---
static std::wstring GetDirectionString(float dx, float dy) {
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

// Forward declarations
static void ApplyMapFilter();
static void StartMapPolling();
static void StopMapPolling();

// --- Construire la liste de marqueurs depuis la cellule persistante ---
// MUST be called from AddUITask (UI thread) to be safe
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

            auto* mapData = extraMarker->mapData;

            const char* rawName = mapData->locationName.GetFullName();
            if (!rawName || !*rawName) continue;

            bool visible = mapData->flags.any(RE::MapMarkerData::Flag::kVisible);
            bool canTravel = mapData->flags.any(RE::MapMarkerData::Flag::kCanTravelTo);

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
            marker.distance = dist;
            marker.direction = GetDirectionString(dx, dy);
            marker.discovered = canTravel;
            marker.canTravelTo = canTravel;
            marker.worldPos = refPos;

            g_mapMarkers.push_back(std::move(marker));
            markerCount++;
        }
    };

    // Scanner le worldspace principal du joueur
    if (worldSpace->persistentCell) {
        scanPersistentCell(worldSpace->persistentCell);
        LOG("MapMenu: scanned player worldspace '{}', {} markers so far", worldSpace->GetName(), markerCount);
    }

    // Scanner aussi tous les autres worldspaces (pour les villes, DLC, etc.)
    auto& worldSpaces = dataHandler->GetFormArray<RE::TESWorldSpace>();
    for (auto* ws : worldSpaces) {
        if (!ws || ws == worldSpace) continue;  // déjà scanné
        if (!ws->persistentCell) continue;
        int before = markerCount;
        scanPersistentCell(ws->persistentCell);
        if (markerCount > before) {
            LOG("MapMenu: worldspace '{}' added {} markers", ws->GetName(), markerCount - before);
        }
    }

    // Sort by distance
    std::sort(g_mapMarkers.begin(), g_mapMarkers.end(),
        [](const MapMarkerInfo& a, const MapMarkerInfo& b) {
            return a.distance < b.distance;
        });

    LOG("MapMenu: {} refs scanned, {} markers found across all worldspaces", totalRefs, markerCount);

    ApplyMapFilter();
    g_mapReady.store(true);
}

// --- Appliquer le filtre ---
static void ApplyMapFilter() {
    g_mapFiltered.clear();

    for (int i = 0; i < static_cast<int>(g_mapMarkers.size()); i++) {
        auto& m = g_mapMarkers[i];
        switch (g_mapFilter) {
            case MapFilter::All:
                g_mapFiltered.push_back(i);
                break;
            case MapFilter::Discovered:
                if (m.discovered) g_mapFiltered.push_back(i);
                break;
            case MapFilter::Undiscovered:
                if (!m.discovered) g_mapFiltered.push_back(i);
                break;
            default: break;
        }
    }

    g_mapIndex = g_mapFiltered.empty() ? -1 : 0;
    LOG("MapMenu: filter '{}' -> {} markers",
        WStringToUtf8(g_mapFilterNames[static_cast<int>(g_mapFilter)]),
        g_mapFiltered.size());
}

// --- Formater l'annonce d'un marqueur ---
static std::wstring FormatMapMarkerAnnounce(const MapMarkerInfo& m, bool fullDetails = false) {
    std::wstring msg = m.name;
    msg += L", " + std::wstring(m.typeName);

    // Convert distance to a more meaningful unit (Skyrim units / 70 ~ feet, / 21 ~ meters)
    int distUnits = static_cast<int>(m.distance);
    msg += L", " + std::to_wstring(distUnits) + L" units " + m.direction;

    if (fullDetails) {
        if (!m.discovered) msg += L", undiscovered";
        if (m.canTravelTo) msg += L", can fast travel";
        else msg += L", cannot fast travel";
    }

    return msg;
}

// --- Centrer la carte sur un marqueur ---
// Déplace la caméra de la carte vers la position monde du marqueur
// et sélectionne le marqueur GFx s'il existe
static void TryCenterMapOnMarker(const MapMarkerInfo& marker) {
    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;

    auto menuPtr = ui->GetMenu(RE::MapMenu::MENU_NAME);
    if (!menuPtr) return;

    auto* mapMenu = static_cast<RE::MapMenu*>(menuPtr.get());
    if (!mapMenu || !mapMenu->uiMovie) return;

    auto& rd2 = mapMenu->GetRuntimeData2();

    // Sélectionner le marqueur GFx (si trouvé dans le tableau Markers)
    auto& mapMenuGfx = rd2.unk30540;
    if (!mapMenuGfx.IsObject()) return;

    RE::GFxValue markersArr;
    if (!mapMenuGfx.GetMember("Markers", &markersArr)) return;

    uint32_t numMarkers = markersArr.GetArraySize();
    std::string nameUtf8 = WStringToUtf8(marker.name);

    for (uint32_t i = 0; i < numMarkers; i++) {
        RE::GFxValue gfxMarker;
        if (!markersArr.GetElement(i, &gfxMarker) || !gfxMarker.IsObject()) continue;

        RE::GFxValue labelVal;
        if (!gfxMarker.GetMember("_label", &labelVal) || !labelVal.IsString()) continue;

        if (nameUtf8 == labelVal.GetString()) {
            RE::GFxValue args[1];
            args[0].SetNumber(static_cast<double>(i));
            mapMenuGfx.Invoke("SetSelectedMarker", nullptr, args, 1);
            LOG("MapMenu: selected '{}' gfxIdx={}", nameUtf8, i);
            return;
        }
    }

    LOG("MapMenu: camera moved but '{}' not in {} visible GFx markers", nameUtf8, numMarkers);
}

// --- Navigation : marqueur suivant ---
static void MapNextMarker() {
    if (!g_mapReady.load()) {
        Speak(L"Loading markers");
        return;
    }
    if (g_mapFiltered.empty()) {
        Speak(L"No markers in this filter");
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
    std::wstring pos = L". " + std::to_wstring(g_mapIndex + 1) + L" of " + std::to_wstring(g_mapFiltered.size());
    Speak(FormatMapMarkerAnnounce(m) + pos);
    TryCenterMapOnMarker(m);
}

// --- Navigation : marqueur précédent ---
static void MapPrevMarker() {
    if (!g_mapReady.load()) {
        Speak(L"Loading markers");
        return;
    }
    if (g_mapFiltered.empty()) {
        Speak(L"No markers in this filter");
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
    std::wstring pos = L". " + std::to_wstring(g_mapIndex + 1) + L" of " + std::to_wstring(g_mapFiltered.size());
    Speak(FormatMapMarkerAnnounce(m) + pos);
    TryCenterMapOnMarker(m);
}

// --- Annoncer les détails complets ---
static void MapAnnounceDetails() {
    if (!g_mapReady.load() || g_mapFiltered.empty() || g_mapIndex < 0) {
        Speak(L"No marker selected");
        return;
    }

    int filteredIdx = g_mapFiltered[g_mapIndex];
    if (filteredIdx < 0 || filteredIdx >= static_cast<int>(g_mapMarkers.size())) return;

    auto& m = g_mapMarkers[filteredIdx];
    std::wstring pos = L". " + std::to_wstring(g_mapIndex + 1) + L" of " + std::to_wstring(g_mapFiltered.size());
    Speak(FormatMapMarkerAnnounce(m, true) + pos);
}

// --- Point de référence (Shift+Home) ---
static void MapSetReference() {
    if (!g_mapReady.load() || g_mapFiltered.empty() || g_mapIndex < 0) {
        Speak(L"No marker selected");
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

        Speak(L"Reference cleared, distances from player");
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

    Speak(L"Reference: " + g_mapReferenceName + L". Distances from this marker");
}

// --- Cycler les filtres ---
static void MapCycleFilter() {
    if (!g_mapReady.load()) {
        Speak(L"Loading markers");
        return;
    }

    int next = (static_cast<int>(g_mapFilter) + 1) % static_cast<int>(MapFilter::COUNT);
    g_mapFilter = static_cast<MapFilter>(next);

    ApplyMapFilter();

    std::wstring msg = g_mapFilterNames[static_cast<int>(g_mapFilter)];
    msg += L", " + std::to_wstring(g_mapFiltered.size()) + L" markers";
    if (!g_mapFiltered.empty()) {
        int idx = g_mapFiltered[0];
        if (idx >= 0 && idx < static_cast<int>(g_mapMarkers.size())) {
            msg += L". " + FormatMapMarkerAnnounce(g_mapMarkers[idx]);
        }
    }
    Speak(msg);
}

// --- Ouverture de la carte ---
static void OnMapOpen() {
    g_mapOpen.store(true);
    g_mapReady.store(false);
    g_mapFilter = MapFilter::All;
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
            SpeakQueue(std::to_wstring(count) + L" map markers");

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
    // Don't clear vectors here -- polling thread may still have a queued task.
    // The vectors will be cleared on next OnMapOpen().
}

// --- Polling du tooltip (survol souris) ---
static void PollMapTooltip() {
    // Double-check map is still open (task may have been queued before close)
    if (!g_mapOpen.load() || !g_mapPolling.load()) return;

    auto* ui = RE::UI::GetSingleton();
    if (!ui) return;

    auto menuPtr = ui->GetMenu(RE::MapMenu::MENU_NAME);
    if (!menuPtr) return;

    auto* mapMenu = static_cast<RE::MapMenu*>(menuPtr.get());
    if (!mapMenu || !mapMenu->uiMovie) return;

    // Read the tooltip title from MarkerDescription
    RE::GFxValue titleVal;
    if (mapMenu->uiMovie->GetVariable(&titleVal,
            "_root.MarkerDescriptionHolder.Description.Title.text") &&
        titleVal.IsString()) {
        std::string title = titleVal.GetString();
        // Ignore placeholder and empty
        if (!title.empty() && title != "Marker Name" && title != g_mapLastTooltip) {
            g_mapLastTooltip = title;
            LOG("MapMenu: tooltip changed -> '{}'", title);
            Speak(Utf8ToWString(title.c_str()));
        }
    }
}

static void StartMapPolling() {
    g_mapPolling.store(true);
    g_mapLastTooltip.clear();

    std::thread([]() {
        while (g_mapPolling.load()) {
            // Check map is still open
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
