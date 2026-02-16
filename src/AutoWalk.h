#pragma once

#include "pch.h"
#include <functional>

class AutoWalk
{
public:
    static AutoWalk* GetSingleton();

    void Initialize();
    void WalkTo(RE::TESObjectREFR* a_target, float a_stopDistance = 100.0f,
                std::function<void()> a_onArrival = nullptr);
    void Stop();
    bool IsWalking() const;
    void Update();

private:
    AutoWalk() = default;
    ~AutoWalk() = default;
    AutoWalk(const AutoWalk&) = delete;
    AutoWalk(AutoWalk&&) = delete;
    AutoWalk& operator=(const AutoWalk&) = delete;
    AutoWalk& operator=(AutoWalk&&) = delete;

    void CheckArrival();
    bool IsMovementKeyPressed() const;
    bool DispatchPapyrusCall(const char* a_functionName, RE::BSScript::IFunctionArguments* a_args);
    bool DispatchPapyrusStop();

    RE::TESObjectREFR* m_targetRef = nullptr;
    float m_stopDistance = 100.0f;
    std::function<void()> m_onArrival;
    RE::TESQuest* m_quest = nullptr;
    RE::VMHandle m_questHandle = 0;
    bool m_initialized = false;
    bool m_espAvailable = false;
};
