#include "MenuAccessibility.h"
#include "StartMenuHook.h"
#include "JournalMenuHook.h"
#include "SpeechManager.h"

MenuAccessibility* MenuAccessibility::GetSingleton()
{
    static MenuAccessibility singleton;
    return &singleton;
}

void MenuAccessibility::Register()
{
    if (auto ui = RE::UI::GetSingleton()) {
        ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
        logs::info("MenuAccessibility registered for menu events");
    } else {
        logs::error("Failed to get UI singleton for MenuAccessibility registration");
    }
}

RE::BSEventNotifyControl MenuAccessibility::ProcessEvent(
    const RE::MenuOpenCloseEvent* a_event,
    RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
    if (!a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    logs::info("Menu event: {} {}", a_event->menuName.c_str(), a_event->opening ? "opened" : "closed");

    if (a_event->opening) {
        OnMenuOpened(a_event->menuName);
    } else {
        OnMenuClosed(a_event->menuName);
    }

    return RE::BSEventNotifyControl::kContinue;
}

bool MenuAccessibility::CanProcess(RE::InputEvent* a_event)
{
    if (!a_event) return false;

    // Only handle events when a tracked menu is open
    return m_startMenuOpen || m_journalMenuOpen;
}

bool MenuAccessibility::ProcessButton(RE::ButtonEvent* a_event)
{
    if (!a_event || !a_event->IsDown()) {
        return false;
    }

    auto idCode = a_event->GetIDCode();
    auto device = a_event->GetDevice();

    bool isNavigationKey = false;

    // Keyboard navigation keys (DirectInput scan codes)
    if (device == RE::INPUT_DEVICE::kKeyboard) {
        if (idCode == 0xC8 || idCode == 0xD0) {  // DIK_UP, DIK_DOWN
            isNavigationKey = true;
        }
        if (idCode == 0x11 || idCode == 0x1F) {  // DIK_W, DIK_S
            isNavigationKey = true;
        }
    }
    // Gamepad: DPad Up/Down
    else if (device == RE::INPUT_DEVICE::kGamepad) {
        if (idCode == 0x0001 || idCode == 0x0002) {  // DPad Up, Down
            isNavigationKey = true;
        }
    }

    if (isNavigationKey && m_startMenuOpen) {
        // Defer check to next frame so the game updates the selection first
        if (auto* taskInterface = SKSE::GetTaskInterface()) {
            taskInterface->AddTask([]() {
                // The AdvanceMovie hook handles selection tracking per-frame
                // but deferring a check here ensures we catch it on the next frame
                // after the game processes the navigation input
            });
        }
    }

    // Accept keys (Enter, E)
    bool isAcceptKey = false;
    if (device == RE::INPUT_DEVICE::kKeyboard) {
        isAcceptKey = (idCode == 0x1C || idCode == 0x12);  // DIK_RETURN, DIK_E
    } else if (device == RE::INPUT_DEVICE::kGamepad) {
        isAcceptKey = (idCode == 0x1000);  // A button
    }

    // Back keys (Escape, Tab)
    bool isBackKey = false;
    if (device == RE::INPUT_DEVICE::kKeyboard) {
        isBackKey = (idCode == 0x01 || idCode == 0x0F);  // DIK_ESCAPE, DIK_TAB
    } else if (device == RE::INPUT_DEVICE::kGamepad) {
        isBackKey = (idCode == 0x2000);  // B button
    }

    // Note: Accept and Back key handling will be needed for future menus
    // StartMenu's state changes are detected per-frame via strCurrentState in the AdvanceMovie hook
    (void)isAcceptKey;
    (void)isBackKey;

    return false;  // Don't consume the event, let the game process it too
}

void MenuAccessibility::OnMenuOpened(const RE::BSFixedString& a_menuName)
{
    // Register input handler on first menu open (MenuControls not available at plugin load)
    if (!m_inputHandlerRegistered) {
        if (auto* menuControls = RE::MenuControls::GetSingleton()) {
            menuControls->AddHandler(this);
            m_inputHandlerRegistered = true;
            logs::info("MenuAccessibility registered for input events (deferred)");
        }
    }

    if (a_menuName == "Main Menu"sv) {
        m_startMenuOpen = true;
        logs::info("Main Menu opened");
        StartMenuHook::GetSingleton()->Install();
        StartMenuHook::GetSingleton()->ResetState();
        StartMenuHook::GetSingleton()->SetMenuOpen(true);
    } else if (a_menuName == "Journal Menu"sv) {
        m_journalMenuOpen = true;
        logs::info("Journal Menu opened");
        JournalMenuHook::GetSingleton()->Install();
        JournalMenuHook::GetSingleton()->ResetState();
        JournalMenuHook::GetSingleton()->SetMenuOpen(true);
    }
}

void MenuAccessibility::OnMenuClosed(const RE::BSFixedString& a_menuName)
{
    if (a_menuName == "Main Menu"sv) {
        m_startMenuOpen = false;
        logs::info("Main Menu closed");
        StartMenuHook::GetSingleton()->SetMenuOpen(false);
    } else if (a_menuName == "Journal Menu"sv) {
        m_journalMenuOpen = false;
        logs::info("Journal Menu closed");
        JournalMenuHook::GetSingleton()->SetMenuOpen(false);
    }
}
