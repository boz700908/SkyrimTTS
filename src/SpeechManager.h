#pragma once

#include "pch.h"
#include <atomic>
#include <fstream>
#include <mutex>

// Centralized speech output manager for skyrim-access plugin
// Provides a single point of control for all screen reader output with dedicated logging
//
// Usage: SpeechManager::GetSingleton()->Speak("text")
//
// Features:
// - Dedicated speech log: skyrimaccess_speech.log with timestamped entries
// - Thread-safe logging with mutex protection
// - Optional muting support
class SpeechManager
{
public:
    static SpeechManager* GetSingleton();

    // Initialize the speech log file
    // Called once from main.cpp after SRAL_Initialize()
    void Initialize();

    // Main speech output function
    // All speech should go through this method
    void Speak(const std::string& a_text, bool a_interrupt = false);

    // Mute support (thread-safe)
    void SetMuted(bool a_muted) { m_muted.store(a_muted, std::memory_order_relaxed); }
    bool IsMuted() const { return m_muted.load(std::memory_order_relaxed); }

private:
    SpeechManager() = default;
    ~SpeechManager();
    SpeechManager(const SpeechManager&) = delete;
    SpeechManager(SpeechManager&&) = delete;
    SpeechManager& operator=(const SpeechManager&) = delete;
    SpeechManager& operator=(SpeechManager&&) = delete;

    void LogSpeech(const std::string& a_text);
    std::string GetTimestamp();

    std::ofstream m_speechLog;
    std::mutex m_logMutex;
    std::atomic<bool> m_muted{ false };
    bool m_initialized = false;
};
