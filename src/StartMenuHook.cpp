#include "StartMenuHook.h"
#include "LocalizationHelper.h"
#include "SpeechManager.h"

StartMenuHook* StartMenuHook::GetSingleton()
{
    static StartMenuHook singleton;
    return &singleton;
}

void StartMenuHook::Install()
{
    if (m_installed) {
        logs::debug("StartMenuHook: Already installed");
        return;
    }

    // We need to hook the StartMenu's AdvanceMovie at runtime
    // Get the menu instance to find its vtable
    auto ui = RE::UI::GetSingleton();
    if (!ui) {
        logs::error("StartMenuHook: Failed to get UI singleton");
        return;
    }

    auto menu = ui->GetMenu("Main Menu");
    if (!menu) {
        logs::error("StartMenuHook: Failed to get Main Menu");
        return;
    }

    // Get the vtable pointer from the menu instance (first pointer in the object)
    auto vtable = *reinterpret_cast<std::uintptr_t**>(menu.get());

    // Save original AdvanceMovie (vtable index 5 in Skyrim's IMenu)
    s_originalAdvanceMovie = reinterpret_cast<AdvanceMovieFn>(vtable[5]);

    // Patch the vtable entry to point to our hook
    DWORD oldProtect;
    VirtualProtect(&vtable[5], sizeof(std::uintptr_t), PAGE_EXECUTE_READWRITE, &oldProtect);
    vtable[5] = reinterpret_cast<std::uintptr_t>(&HookedAdvanceMovie);
    VirtualProtect(&vtable[5], sizeof(std::uintptr_t), oldProtect, &oldProtect);

    m_installed = true;
    logs::info("StartMenuHook: AdvanceMovie hook installed successfully");
}

void StartMenuHook::Uninstall()
{
    if (!m_installed || !s_originalAdvanceMovie) {
        return;
    }

    auto ui = RE::UI::GetSingleton();
    if (!ui) return;

    auto menu = ui->GetMenu("Main Menu");
    if (!menu) {
        // Menu already destroyed, vtable is gone - just reset state
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
    logs::info("StartMenuHook: Hook uninstalled");
}

void StartMenuHook::SetMenuOpen(bool a_open)
{
    m_menuOpen = a_open;
}

void StartMenuHook::ResetState()
{
    m_lastState.clear();
    m_lastSelection = -1;
    m_lastConfirmText.clear();
    m_menuNameAnnounced = false;
}

void StartMenuHook::HookedAdvanceMovie(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime)
{
    // Call original first to let the game update state
    if (s_originalAdvanceMovie) {
        s_originalAdvanceMovie(a_this, a_interval, a_currentTime);
    }

    // Then check for state changes
    auto* hook = GetSingleton();
    if (hook->m_menuOpen) {
        hook->CheckStateChanges(a_this);
    }
}

void StartMenuHook::CheckStateChanges(RE::IMenu* a_menu)
{
    if (!a_menu) return;

    // Read the current state from Scaleform's strCurrentState property
    std::string currentState = GetCurrentState(a_menu);

    // Check for state change
    if (currentState != m_lastState) {
        logs::info("StartMenuHook: State changed '{}' -> '{}'", m_lastState, currentState);
        m_lastState = currentState;
        m_lastSelection = -1;  // Reset selection on state change
        m_lastConfirmText.clear();
        OnStateChanged(a_menu, currentState);
        return;
    }

    // Within a state, check for selection changes
    if (currentState == "Main") {
        CheckMainListSelection(a_menu);
    } else if (currentState == "MainConfirm") {
        // Check if confirm text changed
        std::string confirmText = GetConfirmText(a_menu);
        if (!confirmText.empty() && confirmText != m_lastConfirmText) {
            m_lastConfirmText = confirmText;
            OnMainConfirmState(a_menu);
        }
    }
}

std::string StartMenuHook::GetCurrentState(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) {
        return "";
    }

    RE::GFxValue stateValue;
    if (a_menu->uiMovie->GetVariable(&stateValue, STATE_PATH) && stateValue.IsString()) {
        return stateValue.GetString();
    }

    return "";
}

void StartMenuHook::OnStateChanged(RE::IMenu* a_menu, const std::string& a_newState)
{
    if (a_newState == "PressStart") {
        OnPressStartState(a_menu);
    } else if (a_newState == "Main") {
        OnMainState(a_menu);
    } else if (a_newState == "MainConfirm") {
        OnMainConfirmState(a_menu);
    }
    // Future states: SaveLoad, CharacterLoad, etc.
}

void StartMenuHook::OnMainState(RE::IMenu* a_menu)
{
    // Announce "Main Menu" when first entering main state
    if (!m_menuNameAnnounced) {
        m_menuNameAnnounced = true;
        std::string menuName = LocalizationHelper::Translate("$MAIN MENU");
        if (menuName == "MAIN MENU") {
            menuName = "Main Menu";  // Use mixed case for better speech
        }
        SpeakText(menuName);
    }

    // Announce current selection
    CheckMainListSelection(a_menu);
}

void StartMenuHook::OnMainConfirmState(RE::IMenu* a_menu)
{
    std::string confirmText = GetConfirmText(a_menu);
    if (confirmText.empty()) return;

    // Trim whitespace
    confirmText.erase(0, confirmText.find_first_not_of(" \t\n\r"));
    if (!confirmText.empty()) {
        confirmText.erase(confirmText.find_last_not_of(" \t\n\r") + 1);
    }

    if (!confirmText.empty()) {
        m_lastConfirmText = confirmText;
        SpeakText(confirmText);
    }
}

void StartMenuHook::OnPressStartState(RE::IMenu* a_menu)
{
    (void)a_menu;
    // The "Press Start" state shows the initial splash screen
    // Could announce "Press any key to start" but the game shows this visually
    // For now, just log it
    logs::debug("StartMenuHook: PressStart state entered");
}

void StartMenuHook::CheckMainListSelection(RE::IMenu* a_menu)
{
    auto currentSelection = GetSelectedIndex(a_menu);
    if (currentSelection >= 0 && currentSelection != m_lastSelection) {
        m_lastSelection = currentSelection;
        auto text = GetSelectedItemText(a_menu, currentSelection);
        if (!text.empty()) {
            SpeakText(LocalizationHelper::Translate(text));
        }
    }
}

std::int32_t StartMenuHook::GetSelectedIndex(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) {
        return -1;
    }

    // Read selectedIndex from the CenteredScrollingList
    std::string indexPath = std::string(MAIN_LIST) + ".selectedIndex";
    RE::GFxValue indexValue;
    if (a_menu->uiMovie->GetVariable(&indexValue, indexPath.c_str())) {
        if (indexValue.IsNumber()) {
            return static_cast<std::int32_t>(indexValue.GetNumber());
        }
    }

    return -1;
}

std::string StartMenuHook::GetSelectedItemText(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) {
        return "";
    }

    // Read from the entryList array
    std::string entryListPath = std::string(MAIN_LIST) + ".entryList";
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

    // Entry objects have {text, index, disabled, showIcon}
    RE::GFxValue textValue;
    if (entry.GetMember("text", &textValue) && textValue.IsString()) {
        return textValue.GetString();
    }

    return "";
}

std::string StartMenuHook::GetConfirmText(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) {
        return "";
    }

    RE::GFxValue confirmText;
    if (a_menu->uiMovie->GetVariable(&confirmText, CONFIRM_TEXT) && confirmText.IsString()) {
        return confirmText.GetString();
    }

    return "";
}

void StartMenuHook::SpeakText(const std::string& a_text)
{
    SpeechManager::GetSingleton()->Speak(a_text);
}
