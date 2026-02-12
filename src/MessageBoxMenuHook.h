#pragma once

#include "pch.h"

// Hooks MessageBox Menu AdvanceMovie for per-frame accessibility.
// The MessageBox is used throughout the game for confirmations, dialogs, etc.
// It displays a message text and one or more buttons (e.g. "Yes"/"No").
class MessageBoxMenuHook
{
public:
    static MessageBoxMenuHook* GetSingleton();

    void Install();
    void Uninstall();

    bool IsInstalled() const { return m_installed; }

    void SetMenuOpen(bool a_open);
    void ResetState();

private:
    MessageBoxMenuHook() = default;
    ~MessageBoxMenuHook() = default;
    MessageBoxMenuHook(const MessageBoxMenuHook&) = delete;
    MessageBoxMenuHook(MessageBoxMenuHook&&) = delete;
    MessageBoxMenuHook& operator=(const MessageBoxMenuHook&) = delete;
    MessageBoxMenuHook& operator=(MessageBoxMenuHook&&) = delete;

    // The hooked AdvanceMovie function
    static void HookedAdvanceMovie(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime);

    // Main state check (called every frame)
    void CheckStateChanges(RE::IMenu* a_menu);

    // Scaleform value readers
    std::string GetMessageText(RE::IMenu* a_menu);
    std::int32_t GetButtonCount(RE::IMenu* a_menu);
    std::string GetButtonText(RE::IMenu* a_menu, std::int32_t a_index);
    std::int32_t GetFocusedButtonIndex(RE::IMenu* a_menu);

    // Speech output
    void SpeakText(const std::string& a_text, bool a_interrupt = true);

    // State tracking
    bool m_installed = false;
    bool m_menuOpen = false;
    bool m_messageAnnounced = false;
    bool m_queueNextAnnouncement = false;
    std::int32_t m_lastFocusedButton = -1;
    std::int32_t m_lastButtonCount = 0;

    // Scaleform paths
    static constexpr const char* MESSAGE_TEXT_PATH = "_root.MessageMenu.MessageText";
    static constexpr const char* BUTTONS_PATH = "_root.MessageMenu.Buttons";

    // Original function pointer
    using AdvanceMovieFn = void(*)(RE::IMenu*, float, std::uint32_t);
    static inline AdvanceMovieFn s_originalAdvanceMovie = nullptr;
};
