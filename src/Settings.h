#pragma once

#include "pch.h"
#include <filesystem>
#include <string>
#include <unordered_map>

class Settings
{
public:
    static Settings* GetSingleton();

    void Load();

    std::uint32_t GetKey(const std::string& a_action) const;

    float GetScanRadius() const { return m_scanRadius; }
    float GetRescanDistance() const { return m_rescanDistance; }
    float GetElevationThreshold() const { return m_elevationThreshold; }
    std::uint64_t GetCrosshairCooldownMs() const { return m_crosshairCooldownMs; }
    bool IsCrosshairEnabled() const { return m_crosshairEnabled; }

private:
    Settings() = default;
    ~Settings() = default;
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;

    void CreateDefaultConfig(const std::filesystem::path& a_path);
    void ParseConfig(const std::filesystem::path& a_path);
    std::uint32_t KeyNameToScanCode(const std::string& a_name) const;

    std::unordered_map<std::string, std::uint32_t> m_keyBindings;

    float m_scanRadius = 4096.0f;
    float m_rescanDistance = 100.0f;
    float m_elevationThreshold = 256.0f;
    std::uint64_t m_crosshairCooldownMs = 500;
    bool m_crosshairEnabled = true;
};
