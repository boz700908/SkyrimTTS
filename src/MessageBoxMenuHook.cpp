#include "MessageBoxMenuHook.h"
#include "SpeechManager.h"

MessageBoxMenuHook* MessageBoxMenuHook::GetSingleton()
{
    static MessageBoxMenuHook singleton;
    return &singleton;
}

void MessageBoxMenuHook::Install()
{
    if (m_installed) {
        logs::debug("MessageBoxMenuHook: Already installed");
        return;
    }

    auto ui = RE::UI::GetSingleton();
    if (!ui) {
        logs::error("MessageBoxMenuHook: Failed to get UI singleton");
        return;
    }

    auto menu = ui->GetMenu("MessageBoxMenu");
    if (!menu) {
        logs::error("MessageBoxMenuHook: Failed to get MessageBoxMenu");
        return;
    }

    auto vtable = *reinterpret_cast<std::uintptr_t**>(menu.get());

    s_originalAdvanceMovie = reinterpret_cast<AdvanceMovieFn>(vtable[5]);

    DWORD oldProtect;
    VirtualProtect(&vtable[5], sizeof(std::uintptr_t), PAGE_EXECUTE_READWRITE, &oldProtect);
    vtable[5] = reinterpret_cast<std::uintptr_t>(&HookedAdvanceMovie);
    VirtualProtect(&vtable[5], sizeof(std::uintptr_t), oldProtect, &oldProtect);

    m_installed = true;
    logs::info("MessageBoxMenuHook: AdvanceMovie hook installed successfully");
}

void MessageBoxMenuHook::Uninstall()
{
    if (!m_installed || !s_originalAdvanceMovie) {
        return;
    }

    auto ui = RE::UI::GetSingleton();
    if (!ui) return;

    auto menu = ui->GetMenu("MessageBoxMenu");
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
    logs::info("MessageBoxMenuHook: Hook uninstalled");
}

void MessageBoxMenuHook::SetMenuOpen(bool a_open)
{
    m_menuOpen = a_open;
}

void MessageBoxMenuHook::ResetState()
{
    m_messageAnnounced = false;
    m_queueNextAnnouncement = false;
    m_lastFocusedButton = -1;
    m_lastButtonCount = 0;
}

void MessageBoxMenuHook::HookedAdvanceMovie(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime)
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

void MessageBoxMenuHook::CheckStateChanges(RE::IMenu* a_menu)
{
    if (!a_menu) return;

    // Wait for message text to be populated before announcing
    if (!m_messageAnnounced) {
        std::string message = GetMessageText(a_menu);
        if (!message.empty()) {
            m_messageAnnounced = true;
            SpeakText(message, true);
            m_queueNextAnnouncement = true;
        }
    }

    // Check button count (buttons may be set up after the message)
    std::int32_t buttonCount = GetButtonCount(a_menu);
    if (buttonCount != m_lastButtonCount) {
        m_lastButtonCount = buttonCount;
        // If buttons just appeared and we haven't tracked focus yet, announce first button
        if (buttonCount > 0 && m_lastFocusedButton < 0) {
            std::int32_t focused = GetFocusedButtonIndex(a_menu);
            if (focused >= 0) {
                m_lastFocusedButton = focused;
                std::string btnText = GetButtonText(a_menu, focused);
                if (!btnText.empty()) {
                    std::string announcement = btnText;
                    if (buttonCount > 1) {
                        announcement += ", " + std::to_string(focused + 1) + " of " + std::to_string(buttonCount);
                    }
                    SpeakText(announcement);
                }
            }
        }
    }

    // Track focused button changes
    if (m_lastButtonCount > 0) {
        std::int32_t focused = GetFocusedButtonIndex(a_menu);
        if (focused >= 0 && focused != m_lastFocusedButton) {
            m_lastFocusedButton = focused;
            std::string btnText = GetButtonText(a_menu, focused);
            if (!btnText.empty()) {
                std::string announcement = btnText;
                if (m_lastButtonCount > 1) {
                    announcement += ", " + std::to_string(focused + 1) + " of " + std::to_string(m_lastButtonCount);
                }
                SpeakText(announcement, true);
            }
        }
    }
}

// --- Scaleform Value Readers ---

std::string MessageBoxMenuHook::GetMessageText(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return "";

    // Try .text first (works for both plain and HTML text)
    std::string textPath = std::string(MESSAGE_TEXT_PATH) + ".text";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, textPath.c_str()) && val.IsString()) {
        std::string text = val.GetString();
        if (!text.empty()) return text;
    }

    // Fallback: try .htmlText
    std::string htmlPath = std::string(MESSAGE_TEXT_PATH) + ".htmlText";
    if (a_menu->uiMovie->GetVariable(&val, htmlPath.c_str()) && val.IsString()) {
        return val.GetString();
    }

    return "";
}

std::int32_t MessageBoxMenuHook::GetButtonCount(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return 0;

    // Iterate through possible buttons until we find one that doesn't exist
    for (std::int32_t i = 0; i < 10; ++i) {
        std::string path = std::string(BUTTONS_PATH) + ".Button" + std::to_string(i);
        RE::GFxValue val;
        if (!a_menu->uiMovie->GetVariable(&val, path.c_str()) || val.IsUndefined()) {
            return i;
        }
    }

    return 10;
}

std::string MessageBoxMenuHook::GetButtonText(RE::IMenu* a_menu, std::int32_t a_index)
{
    if (!a_menu || !a_menu->uiMovie || a_index < 0) return "";

    std::string path = std::string(BUTTONS_PATH) + ".Button" + std::to_string(a_index)
        + ".ButtonText.text";
    RE::GFxValue val;
    if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsString()) {
        return val.GetString();
    }

    return "";
}

std::int32_t MessageBoxMenuHook::GetFocusedButtonIndex(RE::IMenu* a_menu)
{
    if (!a_menu || !a_menu->uiMovie) return -1;

    for (std::int32_t i = 0; i < m_lastButtonCount; ++i) {
        std::string path = std::string(BUTTONS_PATH) + ".Button" + std::to_string(i)
            + "._focused";
        RE::GFxValue val;
        if (a_menu->uiMovie->GetVariable(&val, path.c_str()) && val.IsNumber()
            && val.GetNumber() > 0.0) {
            // _focused is 0 when unfocused, non-zero (controller index + 1) when focused
            return i;
        }
    }

    return -1;
}

// --- Speech Output ---

void MessageBoxMenuHook::SpeakText(const std::string& a_text, bool a_interrupt)
{
    if (m_queueNextAnnouncement) {
        m_queueNextAnnouncement = false;
        SpeechManager::GetSingleton()->Speak(a_text, false);
    } else {
        SpeechManager::GetSingleton()->Speak(a_text, a_interrupt);
    }
}
