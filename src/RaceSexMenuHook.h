#pragma once

#include "pch.h"

// Hooks RaceSex Menu (character creation) AdvanceMovie for per-frame accessibility
// The menu has 3 categories (Race/Body/Head), race selection, slider customization,
// and a text entry for character naming.
class RaceSexMenuHook
{
public:
    static RaceSexMenuHook* GetSingleton();

    void Install();
    void Uninstall();

    bool IsInstalled() const { return m_installed; }

    void SetMenuOpen(bool a_open);
    void ResetState();

private:
    RaceSexMenuHook() = default;
    ~RaceSexMenuHook() = default;
    RaceSexMenuHook(const RaceSexMenuHook&) = delete;
    RaceSexMenuHook(RaceSexMenuHook&&) = delete;
    RaceSexMenuHook& operator=(const RaceSexMenuHook&) = delete;
    RaceSexMenuHook& operator=(RaceSexMenuHook&&) = delete;

    // The hooked AdvanceMovie function
    static void HookedAdvanceMovie(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime);

    // Main state check (called every frame)
    void CheckStateChanges(RE::IMenu* a_menu);

    // Sub-checks
    void CheckCategorySelection(RE::IMenu* a_menu);
    void CheckRaceSelection(RE::IMenu* a_menu);
    void CheckSliderSelection(RE::IMenu* a_menu);
    void CheckSliderValue(RE::IMenu* a_menu);
    void CheckNameEntry(RE::IMenu* a_menu);

    // Scaleform value readers - Categories
    std::int32_t GetCategorySelectedIndex(RE::IMenu* a_menu);
    std::int32_t GetCategoryCount(RE::IMenu* a_menu);
    std::string GetCategoryText(RE::IMenu* a_menu, std::int32_t a_index);

    // Scaleform value readers - Race list (narrow panel)
    std::int32_t GetRaceSelectedIndex(RE::IMenu* a_menu);
    std::int32_t GetRaceCount(RE::IMenu* a_menu);
    std::string GetRaceText(RE::IMenu* a_menu, std::int32_t a_index);
    std::string GetRaceDescription(RE::IMenu* a_menu, std::int32_t a_index);

    // Scaleform value readers - Slider list (wide panel)
    std::int32_t GetSliderSelectedIndex(RE::IMenu* a_menu);
    std::string GetSliderText(RE::IMenu* a_menu, std::int32_t a_index);
    std::string GetSliderCallbackName(RE::IMenu* a_menu, std::int32_t a_index);
    double GetSliderPosition(RE::IMenu* a_menu, std::int32_t a_index);
    double GetSliderMin(RE::IMenu* a_menu, std::int32_t a_index);
    double GetSliderMax(RE::IMenu* a_menu, std::int32_t a_index);
    double GetSliderInterval(RE::IMenu* a_menu, std::int32_t a_index);

    // Filtered slider info (accounts for FilteredList filtering)
    void GetFilteredSliderInfo(RE::IMenu* a_menu, std::int32_t a_rawIndex,
        std::int32_t& a_filteredPosition, std::int32_t& a_filteredTotal);

    // Scaleform value readers - Name entry
    bool IsNameEntryActive(RE::IMenu* a_menu);
    std::string GetNameText(RE::IMenu* a_menu);

    // Slider value formatting
    std::string FormatSliderValue(const std::string& a_callbackName, double a_position,
        double a_min, double a_max, double a_interval);

    // Speech output
    void SpeakText(const std::string& a_text, bool a_interrupt = true);

    // State tracking
    bool m_installed = false;
    bool m_menuOpen = false;
    bool m_menuNameAnnounced = false;
    bool m_queueNextAnnouncement = false;

    // Category tracking
    std::int32_t m_lastCategoryIndex = -1;

    // Race list tracking
    std::int32_t m_lastRaceIndex = -1;

    // Slider list tracking
    std::int32_t m_lastSliderIndex = -1;
    double m_lastSliderPosition = -999.0;

    // Name entry tracking
    bool m_lastNameEntryActive = false;
    std::string m_lastNameText;

    // Scaleform paths
    // Note: "Cagetory" is a typo in the original SWF, must match exactly
    static constexpr const char* CATEGORIES_LIST =
        "_root.RaceSexMenuBaseInstance.CagetoryLockBaseInstance.CategoryInstance.List_mc";

    static constexpr const char* RACE_LIST =
        "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.PanelTwoNarrowInstance.List_mc";

    static constexpr const char* SLIDER_LIST =
        "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.PanelTwoWideInstance.List_mc";

    static constexpr const char* NAME_ENTRY_PATH =
        "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.NameEntryInstance";

    // Category constants (from RaceSexPanels.as)
    static constexpr std::int32_t RACE_CATEGORY = 0;
    static constexpr std::int32_t BODY_CATEGORY = 1;
    static constexpr std::int32_t HEAD_CATEGORY = 2;

    // Original function pointer
    using AdvanceMovieFn = void(*)(RE::IMenu*, float, std::uint32_t);
    static inline AdvanceMovieFn s_originalAdvanceMovie = nullptr;
};
