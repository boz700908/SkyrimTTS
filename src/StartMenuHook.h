#pragma once

#include "pch.h"

// Hooks StartMenu AdvanceMovie to check for state changes every frame
// Skyrim's StartMenu has a strCurrentState property readable from Scaleform
// which makes state detection much simpler than FO4's approach
class StartMenuHook
{
public:
    static StartMenuHook* GetSingleton();

    void Install();
    void Uninstall();

    bool IsInstalled() const { return m_installed; }

    void SetMenuOpen(bool a_open);
    void ResetState();

private:
    StartMenuHook() = default;
    ~StartMenuHook() = default;
    StartMenuHook(const StartMenuHook&) = delete;
    StartMenuHook(StartMenuHook&&) = delete;
    StartMenuHook& operator=(const StartMenuHook&) = delete;
    StartMenuHook& operator=(StartMenuHook&&) = delete;

    // The hooked AdvanceMovie function
    static void HookedAdvanceMovie(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime);

    // State checking
    void CheckStateChanges(RE::IMenu* a_menu);
    std::string GetCurrentState(RE::IMenu* a_menu);

    // Panel-specific handlers
    void OnStateChanged(RE::IMenu* a_menu, const std::string& a_newState);
    void OnMainState(RE::IMenu* a_menu);
    void OnMainConfirmState(RE::IMenu* a_menu);
    void OnPressStartState(RE::IMenu* a_menu);

    // Selection tracking
    void CheckMainListSelection(RE::IMenu* a_menu);

    // Scaleform helpers
    std::int32_t GetSelectedIndex(RE::IMenu* a_menu);
    std::string GetSelectedItemText(RE::IMenu* a_menu, std::int32_t a_index);
    std::string GetConfirmText(RE::IMenu* a_menu);

    void SpeakText(const std::string& a_text);

    // State
    bool m_installed = false;
    bool m_menuOpen = false;
    std::string m_lastState;
    std::int32_t m_lastSelection = -1;
    std::string m_lastConfirmText;
    bool m_menuNameAnnounced = false;

    // Scaleform paths (derived from extracted StartMenu.as)
    static constexpr const char* MENU_ROOT = "root.MenuHolder.Menu_mc";
    static constexpr const char* MAIN_LIST = "root.MenuHolder.Menu_mc.MainListHolder.List_mc";
    static constexpr const char* CONFIRM_PANEL = "root.MenuHolder.Menu_mc.ConfirmPanel_mc";
    static constexpr const char* CONFIRM_TEXT = "root.MenuHolder.Menu_mc.ConfirmPanel_mc.textField.text";
    static constexpr const char* STATE_PATH = "root.MenuHolder.Menu_mc.strCurrentState";

    // Original function pointer (saved during runtime vtable hook)
    using AdvanceMovieFn = void(*)(RE::IMenu*, float, std::uint32_t);
    static inline AdvanceMovieFn s_originalAdvanceMovie = nullptr;
};
