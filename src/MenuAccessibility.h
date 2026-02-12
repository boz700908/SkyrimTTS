#pragma once

#include "pch.h"

class MenuAccessibility :
    public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
    public RE::MenuEventHandler
{
public:
    static MenuAccessibility* GetSingleton();

    void Register();

    // Menu events
    RE::BSEventNotifyControl ProcessEvent(
        const RE::MenuOpenCloseEvent* a_event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

    // Input events (MenuEventHandler)
    bool CanProcess(RE::InputEvent* a_event) override;
    bool ProcessButton(RE::ButtonEvent* a_event) override;

private:
    MenuAccessibility() = default;
    ~MenuAccessibility() override = default;
    MenuAccessibility(const MenuAccessibility&) = delete;
    MenuAccessibility(MenuAccessibility&&) = delete;
    MenuAccessibility& operator=(const MenuAccessibility&) = delete;
    MenuAccessibility& operator=(MenuAccessibility&&) = delete;

    void OnMenuOpened(const RE::BSFixedString& a_menuName);
    void OnMenuClosed(const RE::BSFixedString& a_menuName);

    // Menu tracking
    bool m_startMenuOpen = false;
    bool m_journalMenuOpen = false;
    bool m_raceSexMenuOpen = false;
    bool m_messageBoxMenuOpen = false;
    bool m_inputHandlerRegistered = false;
};
