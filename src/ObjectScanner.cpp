#include "ObjectScanner.h"
#include "AutoWalk.h"
#include "Settings.h"
#include "SpeechManager.h"
#include <algorithm>
#include <cmath>

ObjectScanner* ObjectScanner::GetSingleton()
{
    static ObjectScanner singleton;
    return &singleton;
}

void ObjectScanner::Register()
{
    auto* inputDeviceManager = RE::BSInputDeviceManager::GetSingleton();
    if (!inputDeviceManager) {
        logs::error("ObjectScanner: BSInputDeviceManager not available");
        return;
    }

    inputDeviceManager->AddEventSink(this);
    m_registered = true;
    logs::info("ObjectScanner: Registered as input event sink (non-consuming)");
}

RE::BSEventNotifyControl ObjectScanner::ProcessEvent(RE::InputEvent* const* a_event,
                                                      [[maybe_unused]] RE::BSTEventSource<RE::InputEvent*>* a_source)
{
    // Always update AutoWalk each frame we get input events
    AutoWalk::GetSingleton()->Update();

    if (!a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Auto-rescan if player moved enough
    if (!m_scannedObjects.empty() && NeedsRescan()) {
        ScanObjects();
    }

    // Walk the linked list of input events
    for (auto* event = *a_event; event; event = event->next) {
        if (event->eventType.get() != RE::INPUT_EVENT_TYPE::kButton) {
            continue;
        }

        auto* buttonEvent = static_cast<RE::ButtonEvent*>(event);
        if (buttonEvent->IsDown() && buttonEvent->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
            // Only process when game is not paused
            auto* ui = RE::UI::GetSingleton();
            if (!ui || !ui->GameIsPaused()) {
                HandleButtonEvent(buttonEvent);
            }
        }
    }

    // Always return kContinue so we never consume events
    return RE::BSEventNotifyControl::kContinue;
}

void ObjectScanner::HandleButtonEvent(RE::ButtonEvent* a_event)
{
    auto* settings = Settings::GetSingleton();
    auto scanCode = static_cast<std::uint32_t>(a_event->GetIDCode());

    if (scanCode == settings->GetKey("scan")) {
        ScanObjects();
        if (!m_scannedObjects.empty()) {
            m_currentIndex = 0;
            AnnounceCurrentObject();
        } else {
            SpeechManager::GetSingleton()->Speak("No objects found", true);
        }
    } else if (scanCode == settings->GetKey("nextObject")) {
        if (!m_scannedObjects.empty()) {
            SelectNext();
        }
    } else if (scanCode == settings->GetKey("prevObject")) {
        if (!m_scannedObjects.empty()) {
            SelectPrevious();
        }
    } else if (scanCode == settings->GetKey("nextCategory")) {
        NextCategory();
    } else if (scanCode == settings->GetKey("prevCategory")) {
        PreviousCategory();
    } else if (scanCode == settings->GetKey("lookAt")) {
        LookAtCurrentObject();
    } else if (scanCode == settings->GetKey("walkTo")) {
        if (m_currentIndex >= 0 && m_currentIndex < static_cast<std::int32_t>(m_scannedObjects.size())) {
            auto& obj = m_scannedObjects[m_currentIndex];
            if (obj.ref) {
                AutoWalk::GetSingleton()->WalkTo(obj.ref, 100.0f, [name = obj.name]() {
                    SpeechManager::GetSingleton()->Speak("Arrived at " + name, true);
                });
            }
        }
    } else if (scanCode == settings->GetKey("subcategory")) {
        CycleSubcategory();
    }
}

// --- Scanning ---

void ObjectScanner::ScanObjects()
{
    m_scannedObjects.clear();
    m_currentIndex = -1;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    auto playerPos = player->GetPosition();
    m_lastScanPosition = playerPos;

    float radius = Settings::GetSingleton()->GetScanRadius();

    auto* tes = RE::TES::GetSingleton();
    if (!tes) {
        return;
    }

    tes->ForEachReferenceInRange(player, radius, [&](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
        if (!ref || ref == player) {
            return RE::BSContainer::ForEachResult::kContinue;
        }

        if (!IsValidReference(ref)) {
            return RE::BSContainer::ForEachResult::kContinue;
        }

        if (!MatchesCategory(ref)) {
            return RE::BSContainer::ForEachResult::kContinue;
        }

        if (!MatchesSubcategory(ref)) {
            return RE::BSContainer::ForEachResult::kContinue;
        }

        auto refPos = ref->GetPosition();
        auto name = ref->GetName();
        if (!name || name[0] == '\0') {
            return RE::BSContainer::ForEachResult::kContinue;
        }

        ScannedObject obj;
        obj.ref = ref;
        obj.name = name;
        obj.distance = playerPos.GetDistance(refPos);
        obj.zDifference = refPos.z - playerPos.z;

        m_scannedObjects.push_back(std::move(obj));
        return RE::BSContainer::ForEachResult::kContinue;
    });

    // Sort results
    auto* playerCell = player->GetParentCell();
    bool isInterior = playerCell && playerCell->IsInteriorCell();
    float elevThreshold = Settings::GetSingleton()->GetElevationThreshold();

    std::sort(m_scannedObjects.begin(), m_scannedObjects.end(),
        [isInterior, elevThreshold](const ScannedObject& a, const ScannedObject& b) {
            if (isInterior) {
                bool aSameLevel = std::abs(a.zDifference) <= elevThreshold;
                bool bSameLevel = std::abs(b.zDifference) <= elevThreshold;
                if (aSameLevel != bSameLevel) {
                    return aSameLevel;
                }
            }
            return a.distance < b.distance;
        });

    if (!m_scannedObjects.empty()) {
        m_currentIndex = 0;
    }

    logs::info("ObjectScanner: Found {} objects in category {}", m_scannedObjects.size(),
        GetCategoryName(m_currentCategory));
}

bool ObjectScanner::IsValidReference(RE::TESObjectREFR* a_ref) const
{
    if (a_ref->IsDisabled() || a_ref->IsMarkedForDeletion()) {
        return false;
    }

    auto* baseObj = a_ref->GetBaseObject();
    if (!baseObj) {
        return false;
    }

    return true;
}

bool ObjectScanner::MatchesCategory(RE::TESObjectREFR* a_ref) const
{
    if (m_currentCategory == ScanCategory::All) {
        auto* actor = a_ref->As<RE::Actor>();
        if (actor && !actor->IsDead()) {
            return true;
        }

        auto* baseObj = a_ref->GetBaseObject();
        if (!baseObj) return false;

        auto formType = baseObj->GetFormType();
        switch (formType) {
        case RE::FormType::Door:
        case RE::FormType::Container:
        case RE::FormType::Weapon:
        case RE::FormType::Armor:
        case RE::FormType::Ammo:
        case RE::FormType::Book:
        case RE::FormType::Misc:
        case RE::FormType::KeyMaster:
        case RE::FormType::AlchemyItem:
        case RE::FormType::Ingredient:
        case RE::FormType::SoulGem:
        case RE::FormType::Scroll:
            return true;
        default:
            return false;
        }
    }

    auto* baseObj = a_ref->GetBaseObject();

    switch (m_currentCategory) {
    case ScanCategory::NPCs: {
        auto* actor = a_ref->As<RE::Actor>();
        return actor && !actor->IsDead();
    }
    case ScanCategory::Doors: {
        return baseObj && baseObj->GetFormType() == RE::FormType::Door;
    }
    case ScanCategory::Containers: {
        return baseObj && baseObj->GetFormType() == RE::FormType::Container;
    }
    case ScanCategory::Items: {
        if (!baseObj) return false;
        auto formType = baseObj->GetFormType();
        return formType == RE::FormType::Weapon ||
               formType == RE::FormType::Armor ||
               formType == RE::FormType::Ammo ||
               formType == RE::FormType::Book ||
               formType == RE::FormType::Misc ||
               formType == RE::FormType::KeyMaster ||
               formType == RE::FormType::AlchemyItem ||
               formType == RE::FormType::Ingredient ||
               formType == RE::FormType::SoulGem ||
               formType == RE::FormType::Scroll;
    }
    default:
        return false;
    }
}

bool ObjectScanner::MatchesSubcategory(RE::TESObjectREFR* a_ref) const
{
    if (m_currentSubcategory == ScanSubcategory::All) {
        return true;
    }

    switch (m_currentCategory) {
    case ScanCategory::Doors: {
        if (m_currentSubcategory == ScanSubcategory::TypeA) {
            auto* lock = a_ref->GetLock();
            return lock && lock->IsLocked();
        } else if (m_currentSubcategory == ScanSubcategory::TypeB) {
            auto* extraTeleport = a_ref->extraList.GetByType<RE::ExtraTeleport>();
            return extraTeleport && extraTeleport->teleportData;
        }
        break;
    }
    default:
        break;
    }

    return true;
}

// --- Navigation ---

void ObjectScanner::SelectNext()
{
    if (m_scannedObjects.empty()) return;

    m_currentIndex++;
    if (m_currentIndex >= static_cast<std::int32_t>(m_scannedObjects.size())) {
        m_currentIndex = 0;
    }
    AnnounceCurrentObject();
}

void ObjectScanner::SelectPrevious()
{
    if (m_scannedObjects.empty()) return;

    m_currentIndex--;
    if (m_currentIndex < 0) {
        m_currentIndex = static_cast<std::int32_t>(m_scannedObjects.size()) - 1;
    }
    AnnounceCurrentObject();
}

void ObjectScanner::NextCategory()
{
    auto cat = static_cast<int>(m_currentCategory) + 1;
    if (cat >= static_cast<int>(ScanCategory::COUNT)) {
        cat = 0;
    }
    m_currentCategory = static_cast<ScanCategory>(cat);
    m_currentSubcategory = ScanSubcategory::All;
    AnnounceCategoryChange();
    ScanObjects();
    if (!m_scannedObjects.empty()) {
        AnnounceCurrentObject(false);
    } else {
        SpeechManager::GetSingleton()->Speak("No objects found", false);
    }
}

void ObjectScanner::PreviousCategory()
{
    auto cat = static_cast<int>(m_currentCategory) - 1;
    if (cat < 0) {
        cat = static_cast<int>(ScanCategory::COUNT) - 1;
    }
    m_currentCategory = static_cast<ScanCategory>(cat);
    m_currentSubcategory = ScanSubcategory::All;
    AnnounceCategoryChange();
    ScanObjects();
    if (!m_scannedObjects.empty()) {
        AnnounceCurrentObject(false);
    } else {
        SpeechManager::GetSingleton()->Speak("No objects found", false);
    }
}

void ObjectScanner::CycleSubcategory()
{
    auto sub = static_cast<int>(m_currentSubcategory) + 1;
    if (sub >= static_cast<int>(ScanSubcategory::COUNT)) {
        sub = 0;
    }
    m_currentSubcategory = static_cast<ScanSubcategory>(sub);

    auto subName = GetSubcategoryName();
    SpeechManager::GetSingleton()->Speak(subName, true);

    ScanObjects();
    if (!m_scannedObjects.empty()) {
        AnnounceCurrentObject(false);
    } else {
        SpeechManager::GetSingleton()->Speak("No objects found", false);
    }
}

// --- Announcements ---

void ObjectScanner::AnnounceCurrentObject(bool a_interrupt)
{
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<std::int32_t>(m_scannedObjects.size())) {
        return;
    }

    auto& obj = m_scannedObjects[m_currentIndex];

    // Refresh distance from current player position
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player && obj.ref) {
        auto playerPos = player->GetPosition();
        auto refPos = obj.ref->GetPosition();
        obj.distance = playerPos.GetDistance(refPos);
        obj.zDifference = refPos.z - playerPos.z;
    }

    auto announcement = GetObjectAnnouncement(obj);
    announcement += ". " + std::to_string(m_currentIndex + 1) + " of " + std::to_string(m_scannedObjects.size());

    SpeechManager::GetSingleton()->Speak(announcement, a_interrupt);
}

void ObjectScanner::AnnounceCategoryChange()
{
    auto name = GetCategoryName(m_currentCategory);
    SpeechManager::GetSingleton()->Speak(name, true);
}

std::string ObjectScanner::GetObjectAnnouncement(const ScannedObject& a_obj) const
{
    std::string result = a_obj.name;

    // Add door details
    if (a_obj.ref) {
        auto* baseObj = a_obj.ref->GetBaseObject();
        if (baseObj && baseObj->GetFormType() == RE::FormType::Door) {
            auto details = GetDoorDetails(a_obj.ref);
            if (!details.empty()) {
                result += ", " + details;
            }
        }
    }

    // Add distance
    result += ", " + std::to_string(static_cast<int>(a_obj.distance)) + " units";

    // Add elevation
    auto elevStr = GetElevationString(a_obj.zDifference);
    if (!elevStr.empty()) {
        result += ", " + elevStr;
    }

    return result;
}

std::string ObjectScanner::GetDoorDetails(RE::TESObjectREFR* a_ref) const
{
    std::string details;

    // Lock state
    auto* lock = a_ref->GetLock();
    if (lock && lock->IsLocked()) {
        auto level = lock->GetLockLevel(a_ref);
        switch (level) {
        case RE::LOCK_LEVEL::kVeryEasy:
            details = "novice lock";
            break;
        case RE::LOCK_LEVEL::kEasy:
            details = "apprentice lock";
            break;
        case RE::LOCK_LEVEL::kAverage:
            details = "adept lock";
            break;
        case RE::LOCK_LEVEL::kHard:
            details = "expert lock";
            break;
        case RE::LOCK_LEVEL::kVeryHard:
            details = "master lock";
            break;
        case RE::LOCK_LEVEL::kRequiresKey:
            details = "requires key";
            break;
        default:
            details = "locked";
            break;
        }
    }

    // Destination for cell doors
    auto* extraTeleport = a_ref->extraList.GetByType<RE::ExtraTeleport>();
    if (extraTeleport && extraTeleport->teleportData) {
        auto linkedDoorHandle = extraTeleport->teleportData->linkedDoor;
        auto linkedDoorPtr = linkedDoorHandle.get();
        if (linkedDoorPtr) {
            auto* destCell = linkedDoorPtr->GetParentCell();
            if (destCell) {
                auto* destName = destCell->GetName();
                if (destName && destName[0] != '\0') {
                    if (!details.empty()) details += ", ";
                    details += "to " + std::string(destName);
                }
            }
        }
    }

    return details;
}

std::string ObjectScanner::GetElevationString(float a_zDiff) const
{
    float threshold = Settings::GetSingleton()->GetElevationThreshold();
    if (a_zDiff > threshold) {
        return "above";
    } else if (a_zDiff < -threshold) {
        return "below";
    }
    return "";
}

// --- Actions ---

void ObjectScanner::LookAtCurrentObject()
{
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<std::int32_t>(m_scannedObjects.size())) {
        return;
    }

    auto& obj = m_scannedObjects[m_currentIndex];
    if (!obj.ref) return;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    auto playerPos = player->GetPosition();
    auto targetPos = obj.ref->GetPosition();
    auto dir = targetPos - playerPos;

    float yaw = std::atan2(dir.x, dir.y);
    float hDist = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    float pitch = -std::atan2(dir.z, hDist);

    player->SetAngle(RE::NiPoint3(pitch, 0.0f, yaw));

    SpeechManager::GetSingleton()->Speak("Looking at " + obj.name, true);
}

// --- Utility ---

std::string ObjectScanner::GetCategoryName(ScanCategory a_cat) const
{
    switch (a_cat) {
    case ScanCategory::All: return "All";
    case ScanCategory::NPCs: return "NPCs";
    case ScanCategory::Doors: return "Doors";
    case ScanCategory::Containers: return "Containers";
    case ScanCategory::Items: return "Items";
    default: return "Unknown";
    }
}

std::string ObjectScanner::GetSubcategoryName() const
{
    if (m_currentSubcategory == ScanSubcategory::All) {
        return "All";
    }

    switch (m_currentCategory) {
    case ScanCategory::Doors:
        return m_currentSubcategory == ScanSubcategory::TypeA ? "Locked" : "Cell Doors";
    default:
        return "All";
    }
}

bool ObjectScanner::NeedsRescan() const
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;

    float dist = player->GetPosition().GetDistance(m_lastScanPosition);
    return dist > Settings::GetSingleton()->GetRescanDistance();
}
