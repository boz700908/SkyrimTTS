#pragma once

#include "pch.h"
#include <string>

// Centralized utility for localization string handling
class LocalizationHelper
{
public:
    // Translate a $KEY string using game's translation system
    // Falls back to CleanString() if translation not found
    static std::string Translate(const std::string& a_key);

    // Clean a localization string by removing $ prefix and replacing _/- with spaces
    static std::string CleanString(const std::string& a_text);

private:
    static std::string WideToNarrow(const std::wstring& a_wide);
};
