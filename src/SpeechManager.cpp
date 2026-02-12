#include "SpeechManager.h"
#include <SRAL.h>
#include <ShlObj.h>
#include <chrono>
#include <iomanip>
#include <sstream>

SpeechManager* SpeechManager::GetSingleton()
{
    static SpeechManager instance;
    return &instance;
}

SpeechManager::~SpeechManager()
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_speechLog.is_open()) {
        m_speechLog.flush();
        m_speechLog.close();
    }
}

void SpeechManager::Initialize()
{
    if (m_initialized) {
        return;
    }

    // Get Documents folder path
    wchar_t* knownBuffer{ nullptr };
    const auto knownResult = SHGetKnownFolderPath(
        FOLDERID_Documents,
        0,
        nullptr,
        &knownBuffer);

    if (FAILED(knownResult) || !knownBuffer) {
        logs::error("SpeechManager: Failed to get Documents folder path");
        if (knownBuffer) CoTaskMemFree(knownBuffer);
        m_initialized = true;
        return;
    }

    std::filesystem::path speechLogPath = knownBuffer;
    CoTaskMemFree(knownBuffer);

    speechLogPath /= "My Games/Skyrim Special Edition/SKSE/skyrimaccess_speech.log";

    // Open in truncate mode (fresh log each session)
    m_speechLog.open(speechLogPath, std::ios::out | std::ios::trunc);

    if (m_speechLog.is_open()) {
        logs::info("Speech log initialized: {}", speechLogPath.string());
    } else {
        logs::error("SpeechManager: Failed to open speech log: {}", speechLogPath.string());
    }

    m_initialized = true;
}

void SpeechManager::Speak(const std::string& a_text, bool a_interrupt)
{
    if (a_text.empty()) {
        return;
    }

    if (SRAL_GetCurrentEngine() == 0) {
        return;
    }

    if (m_muted.load(std::memory_order_relaxed)) {
        return;
    }

    LogSpeech(a_text);

    SRAL_Speak(a_text.c_str(), a_interrupt);
}

void SpeechManager::LogSpeech(const std::string& a_text)
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (!m_speechLog.is_open()) {
        return;
    }

    m_speechLog << GetTimestamp() << " " << a_text << "\n";
    m_speechLog.flush();
}

std::string SpeechManager::GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm local_tm;
    localtime_s(&local_tm, &now_time_t);

    std::ostringstream oss;
    oss << "[" << std::setfill('0')
        << std::setw(2) << local_tm.tm_hour << ":"
        << std::setw(2) << local_tm.tm_min << ":"
        << std::setw(2) << local_tm.tm_sec << "]";

    return oss.str();
}
