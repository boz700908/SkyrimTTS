#pragma once

#include "pch.h"

// Hooks Journal Menu (quest_journal) AdvanceMovie for per-frame state tracking
// The Journal Menu has 3 tabs: Quests, Stats, System
// System tab has many sub-states (save/load, settings, controls, etc.)
class JournalMenuHook
{
public:
    static JournalMenuHook* GetSingleton();

    void Install();
    void Uninstall();

    bool IsInstalled() const { return m_installed; }

    void SetMenuOpen(bool a_open);
    void ResetState();

private:
    JournalMenuHook() = default;
    ~JournalMenuHook() = default;
    JournalMenuHook(const JournalMenuHook&) = delete;
    JournalMenuHook(JournalMenuHook&&) = delete;
    JournalMenuHook& operator=(const JournalMenuHook&) = delete;
    JournalMenuHook& operator=(JournalMenuHook&&) = delete;

    // The hooked AdvanceMovie function
    static void HookedAdvanceMovie(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime);

    // State checking
    void CheckStateChanges(RE::IMenu* a_menu);

    // Tab tracking
    std::int32_t GetCurrentTab(RE::IMenu* a_menu);
    void OnTabChanged(RE::IMenu* a_menu, std::int32_t a_newTab);

    // Quests page
    void CheckQuestsPage(RE::IMenu* a_menu);
    std::int32_t GetQuestSelectedIndex(RE::IMenu* a_menu);
    std::string GetQuestSelectedText(RE::IMenu* a_menu);

    // Stats page
    void CheckStatsPage(RE::IMenu* a_menu);
    std::int32_t GetStatsCategoryIndex(RE::IMenu* a_menu);
    std::string GetStatsCategoryText(RE::IMenu* a_menu, std::int32_t a_index);

    // System page
    void CheckSystemPage(RE::IMenu* a_menu);
    std::int32_t GetSystemState(RE::IMenu* a_menu);
    std::int32_t GetSystemCategoryIndex(RE::IMenu* a_menu);
    std::string GetSystemCategoryText(RE::IMenu* a_menu, std::int32_t a_index);
    std::int32_t GetSystemCategoryCount(RE::IMenu* a_menu);
    std::string GetSystemConfirmText(RE::IMenu* a_menu);

    // Settings sub-states
    std::int32_t GetSettingsCategoryIndex(RE::IMenu* a_menu);
    std::string GetSettingsCategoryText(RE::IMenu* a_menu, std::int32_t a_index);
    std::int32_t GetOptionsSelectedIndex(RE::IMenu* a_menu);
    std::int32_t GetOptionsCount(RE::IMenu* a_menu);
    std::string GetOptionsSelectedText(RE::IMenu* a_menu, std::int32_t a_index);
    std::string GetOptionValueText(RE::IMenu* a_menu, std::int32_t a_index);

    // Speech output
    // a_interrupt = true for navigation (interrupts current speech)
    // a_interrupt = false for queued speech (e.g. initial selection after tab/state name)
    // m_queueNextAnnouncement overrides to false for the first selection after a menu/tab/state announcement
    void SpeakText(const std::string& a_text, bool a_interrupt = true);

    // State
    bool m_installed = false;
    bool m_menuOpen = false;
    bool m_menuNameAnnounced = false;
    std::int32_t m_lastTab = -1;
    std::int32_t m_lastQuestSelection = -1;
    std::int32_t m_lastStatsCategorySelection = -1;
    std::int32_t m_lastSystemState = -1;
    std::int32_t m_lastSystemCategorySelection = -1;
    std::int32_t m_lastSettingsCategorySelection = -1;
    std::int32_t m_lastOptionsSelection = -1;
    std::string m_lastOptionsValue;
    std::string m_lastConfirmText;
    bool m_queueNextAnnouncement = false;

    // Tab names
    static constexpr const char* TAB_NAMES[] = { "Quests", "Stats", "System" };

    // Scaleform paths (from extracted quest_journal SWF)
    static constexpr const char* MENU_ROOT = "_root.QuestJournalFader.Menu_mc";

    // Tab tracking
    static constexpr const char* CURRENT_TAB = "_root.QuestJournalFader.Menu_mc.iCurrentTab";

    // Quests page
    static constexpr const char* QUEST_TITLE_LIST = "_root.QuestJournalFader.Menu_mc.QuestsFader.Page_mc.TitleList_mc.List_mc";

    // Stats page
    static constexpr const char* STATS_CATEGORY_LIST = "_root.QuestJournalFader.Menu_mc.StatsFader.Page_mc.CategoryList_mc.List_mc";

    // System page
    static constexpr const char* SYSTEM_PAGE = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc";
    static constexpr const char* SYSTEM_CATEGORY_LIST = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc.CategoryList_mc.List_mc";
    static constexpr const char* SYSTEM_STATE = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc.iCurrentState";
    static constexpr const char* SYSTEM_CONFIRM_TEXT = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc.ConfirmPanel.ConfirmText.textField.text";
    static constexpr const char* SETTINGS_LIST = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc.SettingsPanel.List_mc";
    static constexpr const char* OPTIONS_LIST = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc.OptionsListsPanel.OptionsLists.List_mc";

    // System page state constants (matching SystemPage.as)
    static constexpr std::int32_t SYSTEM_MAIN_STATE = 0;
    static constexpr std::int32_t SYSTEM_SAVE_LOAD_STATE = 1;
    static constexpr std::int32_t SYSTEM_SAVE_LOAD_CONFIRM_STATE = 2;
    static constexpr std::int32_t SYSTEM_SETTINGS_CATEGORY_STATE = 3;
    static constexpr std::int32_t SYSTEM_OPTIONS_LISTS_STATE = 4;
    static constexpr std::int32_t SYSTEM_DEFAULT_SETTINGS_CONFIRM_STATE = 5;
    static constexpr std::int32_t SYSTEM_INPUT_MAPPING_STATE = 6;
    static constexpr std::int32_t SYSTEM_QUIT_CONFIRM_STATE = 7;
    static constexpr std::int32_t SYSTEM_PC_QUIT_LIST_STATE = 8;
    static constexpr std::int32_t SYSTEM_PC_QUIT_CONFIRM_STATE = 9;
    static constexpr std::int32_t SYSTEM_DELETE_SAVE_CONFIRM_STATE = 10;
    static constexpr std::int32_t SYSTEM_TRANSITIONING = 15;

    // Original function pointer
    using AdvanceMovieFn = void(*)(RE::IMenu*, float, std::uint32_t);
    static inline AdvanceMovieFn s_originalAdvanceMovie = nullptr;
};
