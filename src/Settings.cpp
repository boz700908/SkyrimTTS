#include "Settings.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Settings* Settings::GetSingleton()
{
    static Settings singleton;
    return &singleton;
}

void Settings::Load()
{
    auto path = std::filesystem::path("Data/SKSE/Plugins/skyrim-access.json");

    if (!std::filesystem::exists(path)) {
        logs::info("Settings: Config not found, creating defaults at {}", path.string());
        CreateDefaultConfig(path);
    }

    ParseConfig(path);
    logs::info("Settings: Loaded {} key bindings", m_keyBindings.size());
}

void Settings::CreateDefaultConfig(const std::filesystem::path& a_path)
{
    json config = {
        {"keybindings", {
            {"scan", "Numpad5"},
            {"nextObject", "PageDown"},
            {"prevObject", "PageUp"},
            {"nextCategory", "Numpad6"},
            {"prevCategory", "Numpad4"},
            {"lookAt", "Home"},
            {"walkTo", "End"},
            {"subcategory", "Delete"}
        }},
        {"scanner", {
            {"radius", 4096.0},
            {"rescanDistance", 100.0},
            {"elevationThreshold", 256.0}
        }},
        {"crosshair", {
            {"enabled", true},
            {"cooldownMs", 500}
        }}
    };

    std::filesystem::create_directories(a_path.parent_path());
    std::ofstream file(a_path);
    if (file.is_open()) {
        file << config.dump(2);
        logs::info("Settings: Default config written");
    } else {
        logs::error("Settings: Failed to write default config");
    }
}

void Settings::ParseConfig(const std::filesystem::path& a_path)
{
    std::ifstream file(a_path);
    if (!file.is_open()) {
        logs::error("Settings: Failed to open config file");
        return;
    }

    try {
        json config = json::parse(file);

        if (config.contains("keybindings")) {
            auto& keys = config["keybindings"];
            for (auto& [action, keyName] : keys.items()) {
                if (keyName.is_string()) {
                    auto scanCode = KeyNameToScanCode(keyName.get<std::string>());
                    if (scanCode != 0) {
                        m_keyBindings[action] = scanCode;
                    } else {
                        logs::warn("Settings: Unknown key name '{}' for action '{}'", keyName.get<std::string>(), action);
                    }
                }
            }
        }

        if (config.contains("scanner")) {
            auto& scanner = config["scanner"];
            if (scanner.contains("radius")) m_scanRadius = scanner["radius"].get<float>();
            if (scanner.contains("rescanDistance")) m_rescanDistance = scanner["rescanDistance"].get<float>();
            if (scanner.contains("elevationThreshold")) m_elevationThreshold = scanner["elevationThreshold"].get<float>();
        }

        if (config.contains("crosshair")) {
            auto& crosshair = config["crosshair"];
            if (crosshair.contains("enabled")) m_crosshairEnabled = crosshair["enabled"].get<bool>();
            if (crosshair.contains("cooldownMs")) m_crosshairCooldownMs = crosshair["cooldownMs"].get<std::uint64_t>();
        }
    } catch (const json::exception& e) {
        logs::error("Settings: JSON parse error: {}", e.what());
    }
}

std::uint32_t Settings::GetKey(const std::string& a_action) const
{
    auto it = m_keyBindings.find(a_action);
    if (it != m_keyBindings.end()) {
        return it->second;
    }
    return 0;
}

std::uint32_t Settings::KeyNameToScanCode(const std::string& a_name) const
{
    static const std::unordered_map<std::string, std::uint32_t> keyMap = {
        // Row 1
        {"Escape", 0x01}, {"F1", 0x3B}, {"F2", 0x3C}, {"F3", 0x3D}, {"F4", 0x3E},
        {"F5", 0x3F}, {"F6", 0x40}, {"F7", 0x41}, {"F8", 0x42}, {"F9", 0x43},
        {"F10", 0x44}, {"F11", 0x57}, {"F12", 0x58},

        // Row 2
        {"1", 0x02}, {"2", 0x03}, {"3", 0x04}, {"4", 0x05}, {"5", 0x06},
        {"6", 0x07}, {"7", 0x08}, {"8", 0x09}, {"9", 0x0A}, {"0", 0x0B},
        {"Minus", 0x0C}, {"Equals", 0x0D}, {"Backspace", 0x0E},

        // Row 3
        {"Tab", 0x0F}, {"Q", 0x10}, {"W", 0x11}, {"E", 0x12}, {"R", 0x13},
        {"T", 0x14}, {"Y", 0x15}, {"U", 0x16}, {"I", 0x17}, {"O", 0x18},
        {"P", 0x19}, {"LeftBracket", 0x1A}, {"RightBracket", 0x1B}, {"Enter", 0x1C},

        // Row 4
        {"A", 0x1E}, {"S", 0x1F}, {"D", 0x20}, {"F", 0x21}, {"G", 0x22},
        {"H", 0x23}, {"J", 0x24}, {"K", 0x25}, {"L", 0x26}, {"Semicolon", 0x27},
        {"Apostrophe", 0x28}, {"Grave", 0x29},

        // Row 5
        {"LeftShift", 0x2A}, {"Z", 0x2C}, {"X", 0x2D}, {"C", 0x2E}, {"V", 0x2F},
        {"B", 0x30}, {"N", 0x31}, {"M", 0x32}, {"Comma", 0x33}, {"Period", 0x34},
        {"Slash", 0x35}, {"RightShift", 0x36},

        // Modifiers
        {"LeftControl", 0x1D}, {"LeftAlt", 0x38}, {"Space", 0x39},
        {"RightAlt", 0xB8}, {"RightControl", 0x9D}, {"CapsLock", 0x3A},

        // Navigation
        {"Insert", 0xD2}, {"Delete", 0xD3}, {"Home", 0xC7}, {"End", 0xCF},
        {"PageUp", 0xC9}, {"PageDown", 0xD1},
        {"Up", 0xC8}, {"Down", 0xD0}, {"Left", 0xCB}, {"Right", 0xCD},

        // Numpad
        {"Numpad0", 0x52}, {"Numpad1", 0x4F}, {"Numpad2", 0x50}, {"Numpad3", 0x51},
        {"Numpad4", 0x4B}, {"Numpad5", 0x4C}, {"Numpad6", 0x4D}, {"Numpad7", 0x47},
        {"Numpad8", 0x48}, {"Numpad9", 0x49}, {"NumpadPlus", 0x4E}, {"NumpadMinus", 0x4A},
        {"NumpadMultiply", 0x37}, {"NumpadDivide", 0xB5}, {"NumpadEnter", 0x9C},
        {"NumpadDecimal", 0x53}, {"NumLock", 0x45},

        // Extra
        {"Backslash", 0x2B}, {"ScrollLock", 0x46}, {"Pause", 0xC5},
    };

    auto it = keyMap.find(a_name);
    if (it != keyMap.end()) {
        return it->second;
    }
    return 0;
}
