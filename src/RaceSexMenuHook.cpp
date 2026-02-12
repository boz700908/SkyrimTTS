#include "RaceSexMenuHook.h"
#include "LocalizationHelper.h"
#include "SpeechManager.h"

RaceSexMenuHook* RaceSexMenuHook::GetSingleton()
{
    static RaceSexMenuHook singleton;
    return &singleton;
}

void RaceSexMenuHook::Install()
{
    if (m_installed) {
        logs::debug("RaceSexMenuHook: Already installed");
        return;
    }

    auto ui = RE::UI::GetSingleton();
    if (!ui) {
        logs::error("RaceSexMenuHook: Failed to get UI singleton");
        return;
    }

    auto menu = ui->GetMenu("RaceSex Menu");
    if (!menu) {
        logs::error("RaceSexMenuHook: Failed to get RaceSex Menu");
        return;
    }

    auto vtable = *reinterpret_cast<std::uintptr_t**>(menu.get());

    s_originalAdvanceMovie = reinterpret_cast<AdvanceMovieFn>(vtable[5]);

    DWORD oldProtect;
    VirtualProtect(&vtable[5], sizeof(std::uintptr_t), PAGE_EXECUTE_READWRITE, &oldProtect);
    vtable[5] = reinterpret_cast<std::uintptr_t>(&HookedAdvanceMovie);
    VirtualProtect(&vtable[5], sizeof(std::uintptr_t), oldProtect, &oldProtect);

    m_installed = true;
    logs::info("RaceSexMenuHook: AdvanceMovie hook installed successfully");
}

void RaceSexMenuHook::Uninstall()
{
    if (!m_installed || !s_originalAdvanceMovie) {
        return;
    }

    auto ui = RE::UI::GetSingleton();
    if (!ui) return;

    auto menu = ui->GetMenu("RaceSex Menu");
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
    logs::info("RaceSexMenuHook: Hook uninstalled");
}

void RaceSexMenuHook::SetMenuOpen(bool a_open)
{
    m_menuOpen = a_open;
}

void RaceSexMenuHook::ResetState()
{
    m_menuNameAnnounced = false;
    m_queueNextAnnouncement = false;
    m_lastCategoryIndex = -1;
    m_lastRaceIndex = -1;
    m_lastSliderIndex = -1;
    m_lastSliderPosition = -999.0;
    m_lastNameEntryActive = false;
    m_lastNameText.clear();
}

void RaceSexMenuHook::HookedAdvanceMovie(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime)
{
    if (s_originalAdvanceMovie) {
        s_originalAdvanceMovie(a_this, a_interval, a_currentTime);
    }

    auto* hook = GetSingleton();
    if (hook->m_menuOpen) {
        hook->CheckStateChanges(a_this);
    }
}

// --- Main State Check ---

void RaceSexMenuHook::CheckStateChanges(RE::IMenu* a_menu)
{
    if (!a_menu) return;

    // Announce menu name on first frame
    if (!m_menuNameAnnounced) {
        m_menuNameAnnounced = true;
        SpeakText("Character Creation", true);
        m_queueNextAnnouncement = true;
    }

    // Check for name entry state (highest priority - it's an overlay)
    CheckNameEntry(a_menu);
    if (m_lastNameEntryActive) return;

    // Check category changes
    CheckCategorySelection(a_menu);

    // Check sub-panel based on current category
    std::int32_t category = GetCategorySelectedIndex(a_menu);
    if (category == RACE_CATEGORY) {
        CheckRaceSelection(a_menu);
    } else if (category >= 0) {
        // Handle Body, Head, and any other categories the game may add
        CheckSliderSelection(a_menu);
        CheckSliderValue(a_menu);
    }
}

// --- Category Selection ---

void RaceSexMenuHook::CheckCategorySelection(RE::IMenu* a_menu)
{
    std::int32_t categoryIndex = GetCategorySelectedIndex(a_menu);
    if (categoryIndex < 0 || categoryIndex == m_lastCategoryIndex) return;

    m_lastCategoryIndex = categoryIndex;

    // Reset sub-panel tracking on category change
    m_lastRaceIndex = -1;
    m_lastSliderIndex = -1;
    m_lastSliderPosition = -999.0;

    std::int32_t totalCount = GetCategoryCount(a_menu);
    std::string text = GetCategoryText(a_menu, categoryIndex);
    if (!text.empty()) {
        std::string announcement = LocalizationHelper::Translate(text);
        if (totalCount > 0) {
            announcement += ", " + std::to_string(categoryIndex + 1) + " of " + std::to_string(totalCount);
        }
        SpeakText(announcement);
        m_queueNextAnnouncement = true;
    }
}

// --- Race Selection ---

void RaceSexMenuHook::CheckRaceSelection(RE::IMenu* a_menu)
{
    std::int32_t raceIndex = GetRaceSelectedIndex(a_menu);
    if (raceIndex < 0 || raceIndex == m_lastRaceIndex) return;

    m_lastRaceIndex = raceIndex;

    std::int32_t totalCount = GetRaceCount(a_menu);
    std::string raceName = GetRaceText(a_menu, raceIndex);
    std::string raceDesc = GetRaceDescription(a_menu, raceIndex);

    if (!raceName.empty()) {
        std::string announcement = LocalizationHelper::Translate(raceName);
        if (totalCount > 0) {
            announcement += ", " + std::to_string(raceIndex + 1) + " of " + std::to_string(totalCount);
        }
        SpeakText(announcement);

        // Queue the description so it follows the race name without interrupting
        if (!raceDesc.empty()) {
            SpeechManager::GetSingleton()->Speak(raceDesc, false);
        }
    }
}

// --- Slider Selection ---

void RaceSexMenuHook::CheckSliderSelection(RE::IMenu* a_menu)
{
    std::int32_t sliderIndex = GetSliderSelectedIndex(a_menu);
    if (sliderIndex < 0 || sliderIndex == m_lastSliderIndex) return;

    m_lastSliderIndex = sliderIndex;

    std::string sliderName = GetSliderText(a_menu, sliderIndex);
    std::string callbackName = GetSliderCallbackName(a_menu, sliderIndex);
    double position = GetSliderPosition(a_menu, sliderIndex);
    double min = GetSliderMin(a_menu, sliderIndex);
    double max = GetSliderMax(a_menu, sliderIndex);
    double interval = GetSliderInterval(a_menu, sliderIndex);
    m_lastSliderPosition = position;

    if (!sliderName.empty()) {
        std::string valueStr = FormatSliderValue(callbackName, position, min, max, interval);

        // Get filtered position for "N of M"
        std::int32_t filteredPos = -1;
        std::int32_t filteredTotal = 0;
        GetFilteredSliderInfo(a_menu, sliderIndex, filteredPos, filteredTotal);

        std::string announcement = LocalizationHelper::Translate(sliderName);
        if (!valueStr.empty()) {
            announcement += ": " + valueStr;
        }
        if (filteredPos >= 0 && filteredTotal > 0) {
            announcement += ", " + std::to_string(filteredPos + 1) + " of " + std::to_string(filteredTotal);
        }
        SpeakText(announcement);
    } else {
        logs::warn("RaceSexMenuHook: Slider at index {} has empty text", sliderIndex);
    }
}

void RaceSexMenuHook::CheckSliderValue(RE::IMenu* a_menu)
{
    std::int32_t sliderIndex = GetSliderSelectedIndex(a_menu);
    if (sliderIndex < 0 || sliderIndex != m_lastSliderIndex) return;

    double position = GetSliderPosition(a_menu, sliderIndex);
    if (position == m_lastSliderPosition) return;

    m_lastSliderPosition = position;

    std::string callbackName = GetSliderCallbackName(a_menu, sliderIndex);
    double min = GetSliderMin(a_menu, sliderIndex);
    double max = GetSliderMax(a_menu, sliderIndex);
    double interval = GetSliderInterval(a_menu, sliderIndex);

    std::string valueStr = FormatSliderValue(callbackName, position, min, max, interval);
    if (!valueStr.empty()) {
        SpeakText(valueStr, true);
    }
}

// --- Name Entry ---

void RaceSexMenuHook::CheckNameEntry(RE::IMenu* a_menu)
{
    bool nameEntryActive = IsNameEntryActive(a_menu);

    if (nameEntryActive && !m_lastNameEntryActive) {
        m_lastNameEntryActive = true;
        m_lastNameText.clear();
        SpeakText("Enter character name", true);
    } else if (!nameEntryActive && m_lastNameEntryActive) {
        m_lastNameEntryActive = false;
        m_lastNameText.clear();
    }

    // Track typed characters while name entry is active
    if (m_lastNameEntryActive) {
        std::string currentText = GetNameText(a_menu);
        if (currentText != m_lastNameText) {
            if (currentText.length() > m_lastNameText.length()) {
                // Character(s) added - speak the new character(s)
                std::string newChars = currentText.substr(m_lastNameText.length());
                SpeakText(newChars, true);
            } else if (currentText.length() < m_lastNameText.length()) {
                // Character deleted - speak the deleted character
                std::string deleted = m_lastNameText.substr(currentText.length());
                SpeakText(deleted, true);
            }
            m_lastNameText = currentText;
        }
    }
}

bool RaceSexMenuHook::IsNameEntryActive(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return false;

    std::string alphaPath = std::string(NAME_ENTRY_PATH) + "._alpha";
    RE::GFxValue alphaVal;
    if (a_menu->uiMovie->GetVariable(&alphaVal, alphaPath.c_str()) && alphaVal.IsNumber()) {
        return alphaVal.GetNumber() > 50.0;
    }

    return false;
}

std::string RaceSexMenuHook::GetNameText(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return "";

    std::string path = std::string(NAME_ENTRY_PATH) + ".TextInputInstance.textField.text";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsString()) {
        return val.GetString();
    }

    return "";
}

// --- Slider Value Formatting ---

std::string RaceSexMenuHook::FormatSliderValue(
    const std::string& a_callbackName, double a_position,
    double a_min, double a_max, double a_interval)
{
    // Special case: Sex slider (0 = Male, 1 = Female)
    if (a_callbackName == "ChangeSex") {
        return (a_position <= 0.0) ? "Male" : "Female";
    }

    // Determine discrete vs continuous based on interval value:
    // - Discrete presets have interval >= 1.0 (integer steps)
    // - Continuous morphs have fractional intervals (e.g. 0.1)
    if (a_interval >= 1.0) {
        // Discrete preset: show "N of M"
        int total = static_cast<int>((a_max - a_min) / a_interval) + 1;
        int current = static_cast<int>((a_position - a_min) / a_interval) + 1;
        if (current < 1) current = 1;
        if (current > total) current = total;
        return std::to_string(current) + " of " + std::to_string(total);
    } else if (a_interval > 0.0) {
        // Continuous slider: show percentage
        double range = a_max - a_min;
        if (range <= 0.0) return "";
        int percent = static_cast<int>(((a_position - a_min) / range) * 100.0);
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        return std::to_string(percent) + "%";
    }

    return "";
}

// --- Filtered Slider Info ---

void RaceSexMenuHook::GetFilteredSliderInfo(RE::IMenu* a_menu, std::int32_t a_rawIndex,
    std::int32_t& a_filteredPosition, std::int32_t& a_filteredTotal)
{
    a_filteredPosition = -1;
    a_filteredTotal = 0;

    if (!a_menu || !a_menu->uiMovie) return;

    // Read the current item filter
    std::string filterPath = std::string(SLIDER_LIST) + ".iItemFilter";
    RE::GFxValue filterVal;
    std::uint32_t itemFilter = 0xFFFFFFFF;
    if (a_menu->uiMovie->GetVariable(&filterVal, filterPath.c_str()) && filterVal.IsNumber()) {
        itemFilter = static_cast<std::uint32_t>(filterVal.GetNumber());
    }

    std::string entryListPath = std::string(SLIDER_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return;
    }

    std::int32_t filteredCount = 0;
    for (std::uint32_t i = 0; i < entryList.GetArraySize(); ++i) {
        RE::GFxValue entry;
        if (!entryList.GetElement(i, &entry) || !entry.IsObject()) continue;

        RE::GFxValue flagVal;
        if (!entry.GetMember("filterFlag", &flagVal) || !flagVal.IsNumber()) continue;

        std::uint32_t filterFlag = static_cast<std::uint32_t>(flagVal.GetNumber());
        if ((filterFlag & itemFilter) != 0) {
            if (static_cast<std::int32_t>(i) == a_rawIndex) {
                a_filteredPosition = filteredCount;
            }
            filteredCount++;
        }
    }

    a_filteredTotal = filteredCount;
}

// --- Scaleform Value Readers: Categories ---

std::int32_t RaceSexMenuHook::GetCategorySelectedIndex(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string path = std::string(CATEGORIES_LIST) + ".selectedIndex";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsNumber()) {
        return static_cast<std::int32_t>(val.GetNumber());
    }

    return -1;
}

std::int32_t RaceSexMenuHook::GetCategoryCount(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string path = std::string(CATEGORIES_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (a_menu->uiMovie->GetVariable(&entryList, path.c_str()) && entryList.IsArray()) {
        return static_cast<std::int32_t>(entryList.GetArraySize());
    }

    return -1;
}

std::string RaceSexMenuHook::GetCategoryText(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string entryListPath = std::string(CATEGORIES_LIST) + ".entryList";
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

// --- Scaleform Value Readers: Race List ---

std::int32_t RaceSexMenuHook::GetRaceSelectedIndex(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string path = std::string(RACE_LIST) + ".selectedIndex";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsNumber()) {
        return static_cast<std::int32_t>(val.GetNumber());
    }

    return -1;
}

std::int32_t RaceSexMenuHook::GetRaceCount(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string path = std::string(RACE_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (a_menu->uiMovie->GetVariable(&entryList, path.c_str()) && entryList.IsArray()) {
        return static_cast<std::int32_t>(entryList.GetArraySize());
    }

    return -1;
}

std::string RaceSexMenuHook::GetRaceText(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string entryListPath = std::string(RACE_LIST) + ".entryList";
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

std::string RaceSexMenuHook::GetRaceDescription(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string entryListPath = std::string(RACE_LIST) + ".entryList";
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

    RE::GFxValue descValue;
    if (entry.GetMember("raceDescription", &descValue) && descValue.IsString()) {
        return descValue.GetString();
    }

    return "";
}

// --- Scaleform Value Readers: Slider List ---

std::int32_t RaceSexMenuHook::GetSliderSelectedIndex(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    std::string path = std::string(SLIDER_LIST) + ".selectedIndex";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsNumber()) {
        return static_cast<std::int32_t>(val.GetNumber());
    }

    return -1;
}

std::string RaceSexMenuHook::GetSliderText(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string entryListPath = std::string(SLIDER_LIST) + ".entryList";
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

std::string RaceSexMenuHook::GetSliderCallbackName(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string entryListPath = std::string(SLIDER_LIST) + ".entryList";
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

    RE::GFxValue val;
    if (entry.GetMember("callbackName", &val) && val.IsString()) {
        return val.GetString();
    }

    return "";
}

double RaceSexMenuHook::GetSliderPosition(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return -999.0;

    std::string entryListPath = std::string(SLIDER_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return -999.0;
    }

    if (static_cast<std::uint32_t>(a_index) >= entryList.GetArraySize()) {
        return -999.0;
    }

    RE::GFxValue entry;
    if (!entryList.GetElement(static_cast<std::uint32_t>(a_index), &entry) || !entry.IsObject()) {
        return -999.0;
    }

    RE::GFxValue val;
    if (entry.GetMember("position", &val) && val.IsNumber()) {
        return val.GetNumber();
    }

    return -999.0;
}

double RaceSexMenuHook::GetSliderMin(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return 0.0;

    std::string entryListPath = std::string(SLIDER_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return 0.0;
    }

    if (static_cast<std::uint32_t>(a_index) >= entryList.GetArraySize()) {
        return 0.0;
    }

    RE::GFxValue entry;
    if (!entryList.GetElement(static_cast<std::uint32_t>(a_index), &entry) || !entry.IsObject()) {
        return 0.0;
    }

    RE::GFxValue val;
    if (entry.GetMember("sliderMin", &val) && val.IsNumber()) {
        return val.GetNumber();
    }

    return 0.0;
}

double RaceSexMenuHook::GetSliderMax(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return 0.0;

    std::string entryListPath = std::string(SLIDER_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return 0.0;
    }

    if (static_cast<std::uint32_t>(a_index) >= entryList.GetArraySize()) {
        return 0.0;
    }

    RE::GFxValue entry;
    if (!entryList.GetElement(static_cast<std::uint32_t>(a_index), &entry) || !entry.IsObject()) {
        return 0.0;
    }

    RE::GFxValue val;
    if (entry.GetMember("sliderMax", &val) && val.IsNumber()) {
        return val.GetNumber();
    }

    return 0.0;
}

double RaceSexMenuHook::GetSliderInterval(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return 1.0;

    std::string entryListPath = std::string(SLIDER_LIST) + ".entryList";
    RE::GFxValue entryList;
    if (!a_menu->uiMovie->GetVariable(&entryList, entryListPath.c_str()) || !entryList.IsArray()) {
        return 1.0;
    }

    if (static_cast<std::uint32_t>(a_index) >= entryList.GetArraySize()) {
        return 1.0;
    }

    RE::GFxValue entry;
    if (!entryList.GetElement(static_cast<std::uint32_t>(a_index), &entry) || !entry.IsObject()) {
        return 1.0;
    }

    RE::GFxValue val;
    if (entry.GetMember("interval", &val) && val.IsNumber()) {
        return val.GetNumber();
    }

    return 1.0;
}

// --- Speech Output ---

void RaceSexMenuHook::SpeakText(const std::string& a_text, bool a_interrupt)
{
    if (m_queueNextAnnouncement) {
        m_queueNextAnnouncement = false;
        SpeechManager::GetSingleton()->Speak(a_text, false);
    } else {
        SpeechManager::GetSingleton()->Speak(a_text, a_interrupt);
    }
}
