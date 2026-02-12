#include "JournalMenuHook.h"
#include "LocalizationHelper.h"
#include "SpeechManager.h"

JournalMenuHook* JournalMenuHook::GetSingleton()
{
    static JournalMenuHook singleton;
    return &singleton;
}

void JournalMenuHook::Install()
{
    if (m_installed) {
        logs::debug("JournalMenuHook: Already installed");
        return;
    }

    auto ui = RE::UI::GetSingleton();
    if (!ui) {
        logs::error("JournalMenuHook: Failed to get UI singleton");
        return;
    }

    auto menu = ui->GetMenu("Journal Menu");
    if (!menu) {
        logs::error("JournalMenuHook: Failed to get Journal Menu");
        return;
    }

    // Get the vtable pointer from the menu instance
    auto vtable = *reinterpret_cast<std::uintptr_t**>(menu.get());

    // Save original AdvanceMovie (vtable index 5 in Skyrim's IMenu)
    s_originalAdvanceMovie = reinterpret_cast<AdvanceMovieFn>(vtable[5]);

    // Patch the vtable entry to point to our hook
    DWORD oldProtect;
    VirtualProtect(&vtable[5], sizeof(std::uintptr_t), PAGE_EXECUTE_READWRITE, &oldProtect);
    vtable[5] = reinterpret_cast<std::uintptr_t>(&HookedAdvanceMovie);
    VirtualProtect(&vtable[5], sizeof(std::uintptr_t), oldProtect, &oldProtect);

    m_installed = true;
    logs::info("JournalMenuHook: AdvanceMovie hook installed successfully");
}

void JournalMenuHook::Uninstall()
{
    if (!m_installed || !s_originalAdvanceMovie) {
        return;
    }

    auto ui = RE::UI::GetSingleton();
    if (!ui) return;

    auto menu = ui->GetMenu("Journal Menu");
    if (!menu) {
        s_originalAdvanceMovie = nullptr;
        m_installed = false;
        return;
    }

    auto vtable = *reinterpret_cast<std::uintptr_t**>(menu.get());

    DWORD oldProtect;
    VirtualProtect(&vtable[5], sizeof(std::uintptr_t), PAGE_EXECUTE_READWRITE, &oldProtect);
    vtable[5] = reinterpret_cast<std::uintptr_t>(s_originalAdvanceMovie);
    VirtualProtect(&vtable[5], sizeof(std::uintptr_t), oldProtect, &oldProtect);

    s_originalAdvanceMovie = nullptr;
    m_installed = false;
    logs::info("JournalMenuHook: Hook uninstalled");
}

void JournalMenuHook::SetMenuOpen(bool a_open)
{
    m_menuOpen = a_open;
}

void JournalMenuHook::ResetState()
{
    m_menuNameAnnounced = false;
    m_lastTab = -1;
    m_lastQuestSelection = -1;
    m_lastStatsCategorySelection = -1;
    m_lastSystemState = -1;
    m_lastSystemCategorySelection = -1;
    m_lastSettingsCategorySelection = -1;
    m_lastOptionsSelection = -1;
    m_lastOptionsValue.clear();
    m_lastConfirmText.clear();
    m_queueNextAnnouncement = false;
}

void JournalMenuHook::HookedAdvanceMovie(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime)
{
    if (s_originalAdvanceMovie) {
        s_originalAdvanceMovie(a_this, a_interval, a_currentTime);
    }

    auto* hook = GetSingleton();
    if (hook->m_menuOpen) {
        hook->CheckStateChanges(a_this);
    }
}

void JournalMenuHook::CheckStateChanges(RE::IMenu* a_menu)
{
    if (!a_menu) return;

    // Check tab changes first
    std::int32_t currentTab = GetCurrentTab(a_menu);
    if (currentTab >= 0 && currentTab != m_lastTab) {
        std::int32_t previousTab = m_lastTab;
        m_lastTab = currentTab;
        OnTabChanged(a_menu, currentTab);

        // If this is the first tab detection, don't return - also check the page
        if (previousTab >= 0) {
            return;  // Tab just changed, wait for next frame for page content
        }
    }

    // Check page-specific state based on current tab
    if (currentTab == 0) {
        CheckQuestsPage(a_menu);
    } else if (currentTab == 1) {
        CheckStatsPage(a_menu);
    } else if (currentTab == 2) {
        CheckSystemPage(a_menu);
    }
}

std::int32_t JournalMenuHook::GetCurrentTab(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    RE::GFxValue tabValue;
    if (a_menu->uiMovie->GetVariable(&tabValue, CURRENT_TAB) && tabValue.IsNumber()) {
        return static_cast<std::int32_t>(tabValue.GetNumber());
    }

    return -1;
}

void JournalMenuHook::OnTabChanged(RE::IMenu* a_menu, std::int32_t a_newTab)
{
    (void)a_menu;

    // Reset page-specific state when switching tabs
    m_lastQuestSelection = -1;
    m_lastStatsCategorySelection = -1;
    m_lastSystemState = -1;
    m_lastSystemCategorySelection = -1;
    m_lastSettingsCategorySelection = -1;
    m_lastOptionsSelection = -1;
    m_lastOptionsValue.clear();
    m_lastConfirmText.clear();

    if (a_newTab >= 0 && a_newTab < 3) {
        std::string announcement = TAB_NAMES[a_newTab];
        announcement += " tab";
        SpeakText(announcement, true);
        // Queue the next announcement (initial selection) so it follows the tab name
        m_queueNextAnnouncement = true;
        logs::info("JournalMenuHook: Tab changed to {} ({})", a_newTab, TAB_NAMES[a_newTab]);
    }
}

// --- Quests Page ---

void JournalMenuHook::CheckQuestsPage(RE::IMenu* a_menu)
{
    std::int32_t selectedIndex = GetQuestSelectedIndex(a_menu);
    if (selectedIndex >= 0 && selectedIndex != m_lastQuestSelection) {
        m_lastQuestSelection = selectedIndex;
        std::string text = GetQuestSelectedText(a_menu);
        if (!text.empty()) {
            std::string announcement = LocalizationHelper::Translate(text);
            SpeakText(announcement);
        }
    }
}

std::int32_t JournalMenuHook::GetQuestSelectedIndex(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string path = std::string(QUEST_TITLE_LIST) + ".selectedIndex";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsNumber()) {
        return static_cast<std::int32_t>(val.GetNumber());
    }

    return -1;
}

std::string JournalMenuHook::GetQuestSelectedText(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return "";

    // Use selectedEntry shortcut to get the currently selected entry object
    std::string entryListPath = std::string(QUEST_TITLE_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return "";
    }

    std::int32_t index = GetQuestSelectedIndex(a_menu);
    if (index < 0 || static_cast<std::uint32_t>(index) >= entryList.GetArraySize()) {
        return "";
    }

    RE::GFxValue entry;
    if (!entryList.GetElement(static_cast<std::uint32_t>(index), &entry) || !entry.IsObject()) {
        return "";
    }

    RE::GFxValue textValue;
    if (entry.GetMember("text", &textValue) && textValue.IsString()) {
        return textValue.GetString();
    }

    return "";
}

// --- Stats Page ---

void JournalMenuHook::CheckStatsPage(RE::IMenu* a_menu)
{
    std::int32_t categoryIndex = GetStatsCategoryIndex(a_menu);
    if (categoryIndex >= 0 && categoryIndex != m_lastStatsCategorySelection) {
        m_lastStatsCategorySelection = categoryIndex;
        std::string text = GetStatsCategoryText(a_menu, categoryIndex);
        if (!text.empty()) {
            std::string announcement = LocalizationHelper::Translate(text);
            SpeakText(announcement);
        }
    }
}

std::int32_t JournalMenuHook::GetStatsCategoryIndex(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string path = std::string(STATS_CATEGORY_LIST) + ".selectedIndex";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsNumber()) {
        return static_cast<std::int32_t>(val.GetNumber());
    }

    return -1;
}

std::string JournalMenuHook::GetStatsCategoryText(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string entryListPath = std::string(STATS_CATEGORY_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return "";
    }

    if (static_cast<std::uint32_t>(a_index) >= entryList.GetArraySize()) {
        return "";
    }

    RE::GFxValue entry;
    if (!entryList.GetElement(static_cast<std::uint32_t>(a_index), &entry) || !entry.IsObject()) {
        return "";
    }

    RE::GFxValue textValue;
    if (entry.GetMember("text", &textValue) && textValue.IsString()) {
        return textValue.GetString();
    }

    return "";
}

// --- System Page ---

void JournalMenuHook::CheckSystemPage(RE::IMenu* a_menu)
{
    // Check for system state changes first
    std::int32_t currentState = GetSystemState(a_menu);

    if (currentState != m_lastSystemState) {
        std::int32_t previousState = m_lastSystemState;
        m_lastSystemState = currentState;

        // Reset sub-state tracking on state change
        m_lastSystemCategorySelection = -1;
        m_lastSettingsCategorySelection = -1;
        m_lastOptionsSelection = -1;
        m_lastOptionsValue.clear();
        m_lastConfirmText.clear();

        // Announce state transitions
        if (currentState == SYSTEM_TRANSITIONING) {
            return;  // Don't announce transitional states
        }

        if (currentState == SYSTEM_SAVE_LOAD_STATE) {
            SpeakText("Save and Load", true);
            m_queueNextAnnouncement = true;
        } else if (currentState == SYSTEM_SETTINGS_CATEGORY_STATE) {
            SpeakText("Settings", true);
            m_queueNextAnnouncement = true;
        } else if (currentState == SYSTEM_OPTIONS_LISTS_STATE) {
            // Don't announce state name - the first option will be announced with its value
            m_queueNextAnnouncement = false;
        } else if (currentState == SYSTEM_INPUT_MAPPING_STATE) {
            SpeakText("Controls", true);
            m_queueNextAnnouncement = true;
        } else if (currentState == SYSTEM_QUIT_CONFIRM_STATE ||
                   currentState == SYSTEM_PC_QUIT_CONFIRM_STATE) {
            std::string confirmText = GetSystemConfirmText(a_menu);
            if (!confirmText.empty()) {
                m_lastConfirmText = confirmText;
                SpeakText(confirmText, true);
            }
        } else if (currentState == SYSTEM_SAVE_LOAD_CONFIRM_STATE ||
                   currentState == SYSTEM_DEFAULT_SETTINGS_CONFIRM_STATE ||
                   currentState == SYSTEM_DELETE_SAVE_CONFIRM_STATE) {
            std::string confirmText = GetSystemConfirmText(a_menu);
            if (!confirmText.empty()) {
                m_lastConfirmText = confirmText;
                SpeakText(confirmText, true);
            }
        } else if (currentState == SYSTEM_PC_QUIT_LIST_STATE) {
            SpeakText("Quit", true);
            m_queueNextAnnouncement = true;
        } else if (currentState == SYSTEM_MAIN_STATE && previousState >= 0) {
            // Returning to main system menu from a sub-state
            SpeakText("System", true);
            m_queueNextAnnouncement = true;
        }

        if (previousState >= 0) {
            return;  // State just changed, wait for page content on next frame
        }
    }

    // Within a state, check for selection changes
    if (currentState == SYSTEM_MAIN_STATE || currentState == SYSTEM_PC_QUIT_LIST_STATE) {
        // Main category list or PC quit list
        std::int32_t categoryIndex = GetSystemCategoryIndex(a_menu);
        if (categoryIndex >= 0 && categoryIndex != m_lastSystemCategorySelection) {
            m_lastSystemCategorySelection = categoryIndex;
            std::int32_t totalCount = GetSystemCategoryCount(a_menu);
            std::string text = GetSystemCategoryText(a_menu, categoryIndex);
            if (!text.empty()) {
                std::string announcement = LocalizationHelper::Translate(text);
                if (totalCount > 0) {
                    announcement += ", " + std::to_string(categoryIndex + 1) + " of " + std::to_string(totalCount);
                }
                SpeakText(announcement);
            }
        }
    } else if (currentState == SYSTEM_SETTINGS_CATEGORY_STATE) {
        // Settings category selection (Display, Audio, Gameplay)
        std::int32_t settingsIndex = GetSettingsCategoryIndex(a_menu);
        if (settingsIndex >= 0 && settingsIndex != m_lastSettingsCategorySelection) {
            m_lastSettingsCategorySelection = settingsIndex;
            std::string text = GetSettingsCategoryText(a_menu, settingsIndex);
            if (!text.empty()) {
                std::string announcement = LocalizationHelper::Translate(text);
                SpeakText(announcement);
            }
        }
    } else if (currentState == SYSTEM_OPTIONS_LISTS_STATE) {
        // Individual settings options - announce name and current value/state
        std::int32_t optionsIndex = GetOptionsSelectedIndex(a_menu);
        std::string currentValue = GetOptionValueText(a_menu, optionsIndex);

        if (optionsIndex >= 0 && optionsIndex != m_lastOptionsSelection) {
            // Selection changed - announce option name with its current value and index
            m_lastOptionsSelection = optionsIndex;
            m_lastOptionsValue = currentValue;
            std::string text = GetOptionsSelectedText(a_menu, optionsIndex);
            if (!text.empty()) {
                std::string announcement = LocalizationHelper::Translate(text);
                if (!currentValue.empty()) {
                    announcement += ": " + currentValue;
                }
                std::int32_t totalCount = GetOptionsCount(a_menu);
                if (totalCount > 0) {
                    announcement += ", " + std::to_string(optionsIndex + 1) + " of " + std::to_string(totalCount);
                }
                SpeakText(announcement);
            }
        } else if (optionsIndex >= 0 && currentValue != m_lastOptionsValue) {
            // Same selection but value changed (user adjusted slider/toggled checkbox)
            // Only announce the new value, not the full option name
            m_lastOptionsValue = currentValue;
            if (!currentValue.empty()) {
                SpeakText(currentValue, true);
            }
        }
    } else if (currentState == SYSTEM_SAVE_LOAD_CONFIRM_STATE ||
               currentState == SYSTEM_DEFAULT_SETTINGS_CONFIRM_STATE ||
               currentState == SYSTEM_QUIT_CONFIRM_STATE ||
               currentState == SYSTEM_PC_QUIT_CONFIRM_STATE ||
               currentState == SYSTEM_DELETE_SAVE_CONFIRM_STATE) {
        // Check if confirm text changed
        std::string confirmText = GetSystemConfirmText(a_menu);
        if (!confirmText.empty() && confirmText != m_lastConfirmText) {
            m_lastConfirmText = confirmText;
            SpeakText(confirmText);
        }
    }
}

std::int32_t JournalMenuHook::GetSystemState(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, SYSTEM_STATE) && val.IsNumber()) {
        return static_cast<std::int32_t>(val.GetNumber());
    }

    return -1;
}

std::int32_t JournalMenuHook::GetSystemCategoryIndex(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string path = std::string(SYSTEM_CATEGORY_LIST) + ".selectedIndex";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsNumber()) {
        return static_cast<std::int32_t>(val.GetNumber());
    }

    return -1;
}

std::string JournalMenuHook::GetSystemCategoryText(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string entryListPath = std::string(SYSTEM_CATEGORY_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return "";
    }

    if (static_cast<std::uint32_t>(a_index) >= entryList.GetArraySize()) {
        return "";
    }

    RE::GFxValue entry;
    if (!entryList.GetElement(static_cast<std::uint32_t>(a_index), &entry) || !entry.IsObject()) {
        return "";
    }

    RE::GFxValue textValue;
    if (entry.GetMember("text", &textValue) && textValue.IsString()) {
        return textValue.GetString();
    }

    return "";
}

std::int32_t JournalMenuHook::GetSystemCategoryCount(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string entryListPath = std::string(SYSTEM_CATEGORY_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) && entryList.IsArray()) {
        return static_cast<std::int32_t>(entryList.GetArraySize());
    }

    return -1;
}

std::string JournalMenuHook::GetSystemConfirmText(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return "";

    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, SYSTEM_CONFIRM_TEXT) && val.IsString()) {
        std::string text = val.GetString();
        // Trim whitespace
        text.erase(0, text.find_first_not_of(" \t\n\r"));
        if (!text.empty()) {
            text.erase(text.find_last_not_of(" \t\n\r") + 1);
        }
        return text;
    }

    return "";
}

// --- Settings Sub-states ---

std::int32_t JournalMenuHook::GetSettingsCategoryIndex(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string path = std::string(SETTINGS_LIST) + ".selectedIndex";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsNumber()) {
        return static_cast<std::int32_t>(val.GetNumber());
    }

    return -1;
}

std::string JournalMenuHook::GetSettingsCategoryText(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string entryListPath = std::string(SETTINGS_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return "";
    }

    if (static_cast<std::uint32_t>(a_index) >= entryList.GetArraySize()) {
        return "";
    }

    RE::GFxValue entry;
    if (!entryList.GetElement(static_cast<std::uint32_t>(a_index), &entry) || !entry.IsObject()) {
        return "";
    }

    RE::GFxValue textValue;
    if (entry.GetMember("text", &textValue) && textValue.IsString()) {
        return textValue.GetString();
    }

    return "";
}

std::int32_t JournalMenuHook::GetOptionsSelectedIndex(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string path = std::string(OPTIONS_LIST) + ".selectedIndex";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsNumber()) {
        return static_cast<std::int32_t>(val.GetNumber());
    }

    return -1;
}

std::int32_t JournalMenuHook::GetOptionsCount(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string entryListPath = std::string(OPTIONS_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) && entryList.IsArray()) {
        return static_cast<std::int32_t>(entryList.GetArraySize());
    }

    return -1;
}

std::string JournalMenuHook::GetOptionsSelectedText(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string entryListPath = std::string(OPTIONS_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return "";
    }

    if (static_cast<std::uint32_t>(a_index) >= entryList.GetArraySize()) {
        return "";
    }

    RE::GFxValue entry;
    if (!entryList.GetElement(static_cast<std::uint32_t>(a_index), &entry) || !entry.IsObject()) {
        return "";
    }

    RE::GFxValue textValue;
    if (entry.GetMember("text", &textValue) && textValue.IsString()) {
        return textValue.GetString();
    }

    return "";
}

std::string JournalMenuHook::GetOptionValueText(RE::IMenu* a_menu, std::int32_t a_index)
{
    // Reads the current value of a settings option and formats it based on movieType:
    //   movieType 0 = Scrollbar (slider) -> percentage (e.g. "75%")
    //   movieType 1 = Stepper (dropdown) -> selected option text from "options" array
    //   movieType 2 = Checkbox (toggle)  -> "On" / "Off"
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string entryListPath = std::string(OPTIONS_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return "";
    }

    if (static_cast<std::uint32_t>(a_index) >= entryList.GetArraySize()) {
        return "";
    }

    RE::GFxValue entry;
    if (!entryList.GetElement(static_cast<std::uint32_t>(a_index), &entry) || !entry.IsObject()) {
        return "";
    }

    // Get movieType to determine the control type
    RE::GFxValue movieTypeVal;
    if (!entry.GetMember("movieType", &movieTypeVal) || !movieTypeVal.IsNumber()) {
        return "";
    }
    std::int32_t movieType = static_cast<std::int32_t>(movieTypeVal.GetNumber());

    // Get the current value
    RE::GFxValue valueVal;
    if (!entry.GetMember("value", &valueVal) || !valueVal.IsNumber()) {
        return "";
    }
    double value = valueVal.GetNumber();

    switch (movieType) {
    case 0: {
        // Scrollbar/slider - value is 0.0 to 1.0, display as percentage
        int percent = static_cast<int>(value * 100.0);
        return std::to_string(percent) + "%";
    }
    case 1: {
        // Stepper/dropdown - value is index into "options" array
        RE::GFxValue options;
        if (entry.GetMember("options", &options) && options.IsArray()) {
            int idx = static_cast<int>(value);
            if (idx >= 0 && static_cast<std::uint32_t>(idx) < options.GetArraySize()) {
                RE::GFxValue optionText;
                if (options.GetElement(static_cast<std::uint32_t>(idx), &optionText) && optionText.IsString()) {
                    return LocalizationHelper::Translate(optionText.GetString());
                }
            }
        }
        return "";
    }
    case 2: {
        // Checkbox/toggle - value is 0 or 1
        return (value != 0.0) ? "On" : "Off";
    }
    default:
        return "";
    }
}

void JournalMenuHook::SpeakText(const std::string& a_text, bool a_interrupt)
{
    // When m_queueNextAnnouncement is set, force queue mode (interrupt=false)
    // so the announcement follows the previous one (e.g. initial selection after tab/state name)
    if (m_queueNextAnnouncement) {
        m_queueNextAnnouncement = false;
        SpeechManager::GetSingleton()->Speak(a_text, false);
    } else {
        SpeechManager::GetSingleton()->Speak(a_text, a_interrupt);
    }
}
