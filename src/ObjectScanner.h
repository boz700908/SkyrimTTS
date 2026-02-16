#pragma once

#include "pch.h"
#include <functional>
#include <string>
#include <vector>

enum class ScanCategory
{
    All,
    NPCs,
    Doors,
    Containers,
    Items,
    COUNT
};

enum class ScanSubcategory
{
    All,
    TypeA,
    TypeB,
    COUNT
};

struct ScannedObject
{
    RE::TESObjectREFR* ref = nullptr;
    std::string name;
    float distance = 0.0f;
    float zDifference = 0.0f;
};

class ObjectScanner : public RE::PlayerInputHandler
{
public:
    static ObjectScanner* GetSingleton();

    void Register();

    // PlayerInputHandler overrides
    bool CanProcess(RE::InputEvent* a_event) override;
    void ProcessButton(RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data) override;
#ifdef ENABLE_SKYRIM_VR
    void Unk_05() override {}
    void Unk_06() override {}
#endif

private:
    ObjectScanner();
    ~ObjectScanner() override = default;

    // Scanning
    void ScanObjects();
    bool MatchesCategory(RE::TESObjectREFR* a_ref) const;
    bool MatchesSubcategory(RE::TESObjectREFR* a_ref) const;
    bool IsValidReference(RE::TESObjectREFR* a_ref) const;

    // Navigation
    void SelectNext();
    void SelectPrevious();
    void NextCategory();
    void PreviousCategory();
    void CycleSubcategory();

    // Announcements
    void AnnounceCurrentObject(bool a_interrupt = true);
    void AnnounceCategoryChange();
    std::string GetObjectAnnouncement(const ScannedObject& a_obj) const;
    std::string GetDoorDetails(RE::TESObjectREFR* a_ref) const;
    std::string GetElevationString(float a_zDiff) const;

    // Actions
    void LookAtCurrentObject();

    // Utility
    std::string GetCategoryName(ScanCategory a_cat) const;
    std::string GetSubcategoryName() const;
    bool NeedsRescan() const;

    // State
    std::vector<ScannedObject> m_scannedObjects;
    std::int32_t m_currentIndex = -1;
    ScanCategory m_currentCategory = ScanCategory::All;
    ScanSubcategory m_currentSubcategory = ScanSubcategory::All;
    RE::NiPoint3 m_lastScanPosition;
    bool m_registered = false;
};
