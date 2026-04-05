#pragma once

#include <Windows.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>
#include <set>
#include <unordered_map>
#include <vector>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "nvdaController.h"

#define LOG(...) SKSE::log::info(__VA_ARGS__)

// ---------------- nvdaController ----------------

// Normalise les caractères typographiques en équivalents ASCII standard
// NVDA gère ensuite nativement les apostrophes, liaisons, etc.
static std::wstring NormalizeForSpeech(const std::wstring& w) {
    // Quick scan: if no special chars, return as-is (avoids copy)
    bool needsNorm = false;
    for (wchar_t c : w) {
        if ((c >= 0x2013 && c <= 0x2015) || (c >= 0x2018 && c <= 0x201F) || c == 0x2026) {
            needsNorm = true;
            break;
        }
    }
    if (!needsNorm) return w;

    std::wstring out;
    out.reserve(w.size());
    for (wchar_t c : w) {
        switch (c) {
            case 0x2018: case 0x2019: case 0x201B: out += L'\''; break; // apostrophes courbes → '
            case 0x201C: case 0x201D: case 0x201F: out += L'"';  break; // guillemets courbes → "
            case 0x2013: case 0x2014: case 0x2015: out += L'-';  break; // tirets longs → -
            case 0x2026:                            out += L"..."; break; // ellipse → ...
            default:                                out += c;     break;
        }
    }
    return out;
}

// Helper pour log wstring → narrow string (avant que WStringToUtf8 soit défini)
static std::string WToNarrow(const std::wstring& w) {
    std::string s;
    s.reserve(w.size());
    for (wchar_t c : w) s += (c < 128) ? static_cast<char>(c) : '?';
    return s;
}

// Interrompt la synthèse en cours puis lit le texte
static void Speak(const wchar_t* w) {
    if (w && *w) {
        std::wstring norm = NormalizeForSpeech(w);
        nvdaController_cancelSpeech();
        nvdaController_speakText(norm.c_str());
    }
}

static void Speak(const std::wstring& w) {
    if (!w.empty()) {
        std::wstring norm = NormalizeForSpeech(w);
        nvdaController_cancelSpeech();
        nvdaController_speakText(norm.c_str());
    }
}

// Ajoute à la file sans interrompre le speech en cours (pour les infos secondaires)
static void SpeakQueue(const std::wstring& w) {
    if (!w.empty()) {
        std::wstring norm = NormalizeForSpeech(w);
        nvdaController_speakText(norm.c_str());
    }
}

static std::wstring Utf8ToWString(const std::string& s) {
    if (s.empty()) return L"";

    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) {
        n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
        if (n <= 0) return L"";
        std::wstring w(n, 0);
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &w[0], n);
        if (!w.empty() && w.back() == L'\0') w.pop_back();
        return w;
    }

    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

static std::string WStringToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

// ---------------- GFx helpers ----------------

// Protection SEH contre les access violations sur des GFxValue corrompus (mods UI)
// __try/__except ne peut pas être dans une fonction avec des objets C++ (destructeurs),
// donc on isole les appels dangereux ici.
static bool SafeGetVariable(RE::GFxMovieView* movie, RE::GFxValue& out, const char* path) {
    __try {
        return movie->GetVariable(&out, path);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SafeGetMember(const RE::GFxValue& obj, const char* name, RE::GFxValue* out) {
    __try {
        return obj.GetMember(name, out);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Validation rapide : le pointeur interne du GFxValue doit être dans un range d'adresse raisonnable
// Un GFxValue corrompu peut avoir des données UTF-16 brutes au lieu d'un pointeur
static bool SafeGFxValueLooksValid(const RE::GFxValue& v) {
    __try {
        // Lire le type brut — si ça crash, la valeur est corrompue
        auto type = v.GetType();
        // Le type doit être dans le range valide (0-15)
        return static_cast<int>(type) >= 0 && static_cast<int>(type) <= 15;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SafeIsString(const RE::GFxValue& v) {
    __try {
        if (!SafeGFxValueLooksValid(v)) return false;
        return v.IsString();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeIsObject(const RE::GFxValue& v) {
    __try {
        if (!SafeGFxValueLooksValid(v)) return false;
        return v.IsObject();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static const char* SafeGetString(const RE::GFxValue& v) {
    __try {
        if (!SafeGFxValueLooksValid(v)) return nullptr;
        return v.GetString();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static bool SafeIsNumber(const RE::GFxValue& v) {
    __try {
        if (!SafeGFxValueLooksValid(v)) return false;
        return v.IsNumber();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static double SafeGetNumber(const RE::GFxValue& v) {
    __try {
        if (!SafeGFxValueLooksValid(v)) return 0.0;
        return v.GetNumber();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 0.0; }
}

static bool SafeIsBool(const RE::GFxValue& v) {
    __try {
        if (!SafeGFxValueLooksValid(v)) return false;
        return v.IsBool();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeGetBool(const RE::GFxValue& v) {
    __try {
        if (!SafeGFxValueLooksValid(v)) return false;
        return v.GetBool();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeIsArray(const RE::GFxValue& v) {
    __try {
        if (!SafeGFxValueLooksValid(v)) return false;
        return v.IsArray();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static uint32_t SafeGetArraySize(const RE::GFxValue& v) {
    __try {
        if (!SafeGFxValueLooksValid(v)) return 0;
        return v.GetArraySize();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static bool SafeGetElement(const RE::GFxValue& arr, uint32_t idx, RE::GFxValue* out) {
    __try {
        return arr.GetElement(idx, out);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool ExtractString(const RE::GFxValue& v, std::string& out) {
    out.clear();

    if (SafeIsString(v)) {
        const char* s = SafeGetString(v);
        if (!s) return false;
        out = s;
        return true;
    }

    if (SafeIsObject(v)) {
        const char* fields[] = {"selectedTextString", "text", "label", "htmlText", "caption", "title"};
        for (auto f : fields) {
            RE::GFxValue mv;
            if (SafeGetMember(v, f, &mv) && SafeIsString(mv)) {
                const char* s = SafeGetString(mv);
                if (s) { out = s; return true; }
            }
        }

        RE::GFxValue se;
        if (SafeGetMember(v, "selectedEntry", &se) && SafeIsObject(se)) {
            RE::GFxValue lab;
            if (SafeGetMember(se, "label", &lab) && SafeIsString(lab)) {
                const char* s = SafeGetString(lab);
                if (s) { out = s; return true; }
            }
            RE::GFxValue tx;
            if (SafeGetMember(se, "text", &tx) && SafeIsString(tx)) {
                const char* s = SafeGetString(tx);
                if (s) { out = s; return true; }
            }
        }
    }

    return false;
}

static bool GetGFxString(RE::GFxMovieView* movie, const char* path, std::string& out) {
    out.clear();
    if (!movie || !path) return false;

    RE::GFxValue v;
    if (!SafeGetVariable(movie, v, path)) return false;
    return ExtractString(v, out);
}

static bool GetGFxNumber(RE::GFxMovieView* movie, const char* path, double& out) {
    out = 0.0;
    if (!movie || !path) return false;
    RE::GFxValue v;
    if (!SafeGetVariable(movie, v, path)) return false;
    if (v.IsNumber()) { out = v.GetNumber(); return true; }
    if (SafeIsString(v)) {
        const char* s = SafeGetString(v);
        if (s) { try { out = std::stod(s); return true; } catch (...) {} }
    }
    return false;
}

// ---------------- Scaleform localisation ----------------

// Fallback : parsing manuel de Translate_LANGUAGE.txt pour les clés absentes du moteur
static std::unordered_map<std::string, std::wstring> g_fileTranslations;

// Traductions intégrées au plugin pour les clés que ni le moteur ni le fichier ne couvrent
// (ex: noms de statistiques dans le Stats Menu)
struct BuiltinTranslationEntry { const char* key; const wchar_t* value; };

static const std::unordered_map<std::string, std::vector<BuiltinTranslationEntry>> g_builtinTranslations = {
    {"FRENCH", {
        // Stats Menu - General
        {"$Locations Discovered",   L"Lieux d\u00e9couverts"},
        {"$Dungeons Cleared",       L"Donjons nettoy\u00e9s"},
        {"$Days Passed",            L"Jours \u00e9coul\u00e9s"},
        {"$Hours Slept",            L"Heures de sommeil"},
        {"$Hours Waiting",          L"Heures d'attente"},
        {"$Standing Stones Found",  L"Pierres dress\u00e9es trouv\u00e9es"},
        {"$Gold Found",             L"Or trouv\u00e9"},
        {"$Most Gold Carried",      L"Maximum d'or transport\u00e9"},
        {"$Chests Looted",          L"Coffres pill\u00e9s"},
        {"$Skill Increases",        L"Augmentations de comp\u00e9tence"},
        {"$Skill Books Read",       L"Manuels de comp\u00e9tence lus"},
        {"$Food Eaten",             L"Nourriture consomm\u00e9e"},
        {"$Training Sessions",      L"S\u00e9ances d'entra\u00eenement"},
        {"$Books Read",             L"Livres lus"},
        {"$Horses Owned",           L"Chevaux poss\u00e9d\u00e9s"},
        {"$Houses Owned",           L"Maisons poss\u00e9d\u00e9es"},
        {"$Stores Invested In",     L"Magasins investis"},
        {"$Diseases Contracted",    L"Maladies contract\u00e9es"},
        // Stats Menu - Quest
        {"$Quests Completed",                           L"Qu\u00eates accomplies"},
        {"$Misc Objectives Completed",                  L"Objectifs divers accomplis"},
        {"$Main Quests Completed",                      L"Qu\u00eates principales accomplies"},
        {"$Side Quests Completed",                      L"Qu\u00eates secondaires accomplies"},
        {"$The Companions Quests Completed",            L"Qu\u00eates des Compagnons accomplies"},
        {"$College of Winterhold Quests Completed",     L"Qu\u00eates de l'Acad\u00e9mie de Fortdhiver accomplies"},
        {"$Thieves' Guild Quests Completed",            L"Qu\u00eates de la Guilde des voleurs accomplies"},
        {"$The Dark Brotherhood Quests Completed",      L"Qu\u00eates de la Confr\u00e9rie noire accomplies"},
        {"$Civil War Quests Completed",                 L"Qu\u00eates de la guerre civile accomplies"},
        {"$Daedric Quests Completed",                   L"Qu\u00eates da\u00e9driques accomplies"},
        {"$Questlines Completed",                       L"Fils de qu\u00eates accomplis"},
        // Stats Menu - Combat
        {"$People Killed",          L"Personnes tu\u00e9es"},
        {"$Animals Killed",         L"Animaux tu\u00e9s"},
        {"$Creatures Killed",       L"Cr\u00e9atures tu\u00e9es"},
        {"$Undead Killed",          L"Morts-vivants tu\u00e9s"},
        {"$Daedra Killed",          L"Daedra tu\u00e9s"},
        {"$Automatons Killed",      L"Automates tu\u00e9s"},
        {"$Favorite Weapon",        L"Arme favorite"},
        {"$Critical Strikes",       L"Coups critiques"},
        {"$Sneak Attacks",          L"Attaques sournoises"},
        {"$Weapons Disarmed",       L"Armes d\u00e9sarm\u00e9es"},
        {"$Brawls Won",             L"Bagarres gagn\u00e9es"},
        {"$Bunnies Slaughtered",    L"Lapins massacr\u00e9s"},
        // Stats Menu - Magic
        {"$Spells Learned",             L"Sorts appris"},
        {"$Favorite Spell",             L"Sort favori"},
        {"$Favorite School",            L"\u00c9cole favorite"},
        {"$Dragon Souls Collected",     L"\u00c2mes de dragon collect\u00e9es"},
        {"$Words Of Power Learned",     L"Mots de pouvoir appris"},
        {"$Words Of Power Unlocked",    L"Mots de pouvoir d\u00e9bloqu\u00e9s"},
        {"$Shouts Learned",             L"Cris appris"},
        {"$Shouts Unlocked",            L"Cris d\u00e9bloqu\u00e9s"},
        {"$Shouts Mastered",            L"Cris ma\u00eetris\u00e9s"},
        {"$Times Shouted",              L"Fois cri\u00e9"},
        {"$Favorite Shout",             L"Cri favori"},
        // Stats Menu - Crafting
        {"$Soul Gems Used",             L"Gemmes spirituelles utilis\u00e9es"},
        {"$Souls Trapped",              L"\u00c2mes pi\u00e9g\u00e9es"},
        {"$Magic Items Made",           L"Objets magiques cr\u00e9\u00e9s"},
        {"$Weapons Improved",           L"Armes am\u00e9lior\u00e9es"},
        {"$Weapons Made",               L"Armes fabriqu\u00e9es"},
        {"$Armor Improved",             L"Armures am\u00e9lior\u00e9es"},
        {"$Armor Made",                 L"Armures fabriqu\u00e9es"},
        {"$Potions Mixed",              L"Potions pr\u00e9par\u00e9es"},
        {"$Potions Used",               L"Potions utilis\u00e9es"},
        {"$Poisons Mixed",              L"Poisons pr\u00e9par\u00e9s"},
        {"$Poisons Used",               L"Poisons utilis\u00e9s"},
        {"$Ingredients Harvested",      L"Ingr\u00e9dients r\u00e9colt\u00e9s"},
        {"$Ingredients Eaten",          L"Ingr\u00e9dients consomm\u00e9s"},
        {"$Nirnroots Found",            L"Nirnes trouv\u00e9es"},
        {"$Wings Plucked",              L"Ailes arrach\u00e9es"},
        // Stats Menu - Crime
        {"$Total Lifetime Bounty",      L"Prime totale cumul\u00e9e"},
        {"$Largest Bounty",             L"Plus grosse prime"},
        {"$Locks Picked",               L"Serrures croch\u00e9t\u00e9es"},
        {"$Pockets Picked",             L"Poches fouill\u00e9es"},
        {"$Items Pickpocketed",         L"Objets vol\u00e9s \u00e0 la tire"},
        {"$Times Jailed",               L"Fois emprisonn\u00e9"},
        {"$Days Jailed",                L"Jours emprisonn\u00e9"},
        {"$Fines Paid",                 L"Amendes pay\u00e9es"},
        {"$Jail Escapes",               L"\u00c9vasions"},
        {"$Items Stolen",               L"Objets vol\u00e9s"},
        {"$Horses Stolen",              L"Chevaux vol\u00e9s"},
        // Stats Menu - Crime (primes par châtellenie)
        {"$Eastmarch Bounty",           L"Prime d'Estemarche"},
        {"$Falkreath Bounty",           L"Prime de Falkreath"},
        {"$Haafingar Bounty",           L"Prime de Haafingar"},
        {"$Hjaalmarch Bounty",          L"Prime de Hjaalmarch"},
        {"$The Pale Bounty",            L"Prime de la Brèche blanche"},
        {"$The Reach Bounty",           L"Prime du Crevasse"},
        {"$The Rift Bounty",            L"Prime de la Brèche"},
        {"$Whiterun Bounty",            L"Prime de Blancherive"},
        {"$Winterhold Bounty",          L"Prime de Fortdhiver"},
    }},
};

static std::string g_currentLanguage = "ENGLISH";

static void LoadTranslationFile() {
    g_fileTranslations.clear();
    std::string lang = "ENGLISH";
    auto* iniCollection = RE::INISettingCollection::GetSingleton();
    if (iniCollection) {
        auto* setting = iniCollection->GetSetting("sLanguage:General");
        if (setting && setting->data.s && *setting->data.s) {
            lang = setting->data.s;
            for (char& c : lang)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    // Chemin relatif à Data/ — BSResourceNiBinaryStream cherche dans les fichiers loose ET les BSA
    std::string bsaPath = "Interface\\Translate_" + lang + ".txt";
    LOG("LoadTranslationFile: language='{}', opening '{}'", lang, bsaPath);

    RE::BSResourceNiBinaryStream stream(bsaPath.c_str());
    if (!stream.good()) { LOG("LoadTranslationFile: not found (loose or BSA)"); return; }

    auto fileSize = static_cast<size_t>(stream.stream->totalSize);
    std::vector<char> buf(fileSize);
    stream.read(buf.data(), static_cast<std::uint32_t>(fileSize));

    std::wstring content;
    if (fileSize >= 2 &&
        static_cast<unsigned char>(buf[0]) == 0xFF &&
        static_cast<unsigned char>(buf[1]) == 0xFE) {
        const wchar_t* data = reinterpret_cast<const wchar_t*>(buf.data() + 2);
        size_t wlen = (fileSize - 2) / sizeof(wchar_t);
        content.assign(data, wlen);
    } else {
        content = Utf8ToWString(std::string(buf.begin(), buf.end()));
    }

    std::wstringstream ss(content);
    std::wstring line;
    int count = 0;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty() || line[0] != L'$') continue;
        size_t sep = line.find_first_of(L" \t");
        if (sep == std::wstring::npos) continue;
        std::wstring key = line.substr(0, sep);
        size_t valStart = line.find_first_not_of(L" \t", sep);
        if (valStart == std::wstring::npos) continue;
        std::wstring value = line.substr(valStart);
        while (!value.empty() && (value.back() == L' ' || value.back() == L'\t')) value.pop_back();
        if (value.empty()) continue;
        g_fileTranslations[WStringToUtf8(key)] = value;
        ++count;
    }
    LOG("LoadTranslationFile: {} entries loaded", count);

    // Charger les traductions intégrées au plugin (ne remplace pas celles du fichier)
    g_currentLanguage = lang;
    auto langIt = g_builtinTranslations.find(lang);
    if (langIt != g_builtinTranslations.end()) {
        int builtinCount = 0;
        for (auto& entry : langIt->second) {
            if (g_fileTranslations.find(entry.key) == g_fileTranslations.end()) {
                g_fileTranslations[entry.key] = entry.value;
                ++builtinCount;
            }
        }
        LOG("LoadTranslationFile: {} builtin entries added for {}", builtinCount, lang);
    }
}

// Priorité : BSScaleformTranslator (inclut mods) → fichier Translate_*.txt → fallback nettoyé
static std::wstring TranslateKey(const std::string& key) {
    if (key.empty() || key[0] != '$') return Utf8ToWString(key);

    // 1. BSScaleformTranslator (traductions du moteur + mods)
    // Cache the translator pointer (it never changes during a session)
    static RE::BSScaleformTranslator* cachedTranslator = nullptr;
    static bool translatorChecked = false;
    if (!translatorChecked) {
        auto* sfManager = RE::BSScaleformManager::GetSingleton();
        if (sfManager && sfManager->loader) {
            auto* state = sfManager->loader->GetStateAddRef(RE::GFxState::StateType::kTranslator);
            if (state)
                cachedTranslator = static_cast<RE::BSScaleformTranslator*>(state);
        }
        translatorChecked = true;
    }
    if (cachedTranslator) {
        std::wstring wideKey = Utf8ToWString(key);
        RE::BSFixedStringW lookupKey(wideKey.c_str());
        auto& translationMap = cachedTranslator->translator.translationMap;
        auto it = translationMap.find(lookupKey);
        if (it != translationMap.end()) {
            std::wstring result(it->second.c_str());
            static std::string lastHitKey;
            if (key != lastHitKey) {
                LOG("Translate hit (engine): {} -> {}", key, WStringToUtf8(result));
                lastHitKey = key;
            }
            return result;
        }
    }

    // 2. Fichier Translate_*.txt (fallback)
    auto it = g_fileTranslations.find(key);
    if (it != g_fileTranslations.end()) {
        static std::string lastHitKey2;
        if (key != lastHitKey2) {
            LOG("Translate hit (file): {} -> {}", key, WStringToUtf8(it->second));
            lastHitKey2 = key;
        }
        return it->second;
    }

    static std::set<std::string> loggedUnresolved;
    if (loggedUnresolved.insert(key).second) {
        LOG("TranslateKey: unresolved key '{}'", key);
    }
    std::string cleaned = key.substr(1);
    for (char& c : cleaned) if (c == '_') c = ' ';
    return Utf8ToWString(cleaned);
}

static std::wstring ResolveUIString([[maybe_unused]] RE::GFxMovieView* movie, const std::string& raw) {
    if (raw.empty()) return L"";
    if (raw[0] == '$') return TranslateKey(raw);
    return Utf8ToWString(raw);
}

// ---------------- Helpers texte partagés ----------------

// Strips GFx/HTML markup (tags <...>, blocks {...}, &nbsp;) for speech synthesis
static std::wstring StripMarkupForSpeech(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == L'<') {
            while (i < s.size() && s[i] != L'>') ++i;
            if (i < s.size()) ++i;
        }
        else if (s[i] == L'{') {
            while (i < s.size() && s[i] != L'}') ++i;
            if (i < s.size()) ++i;
        }
        else if (s[i] == L'&' && s.compare(i, 6, L"&nbsp;") == 0) {
            out += L' ';
            i += 6;
        }
        else {
            out += s[i++];
        }
    }
    std::wstring result;
    result.reserve(out.size());
    bool prevSpace = true;
    for (wchar_t c : out) {
        if (c == L' ' || c == L'\t') {
            if (!prevSpace) { result += L' '; prevSpace = true; }
        } else {
            result += c;
            prevSpace = false;
        }
    }
    while (!result.empty() && result.back() == L' ') result.pop_back();
    return result;
}

// Replaces <img src='KeyName.png'> tags with readable key names for speech
static std::wstring ReplaceImgTagsWithKeyNames(const std::wstring& html) {
    std::wstring out;
    out.reserve(html.size());
    size_t i = 0;
    while (i < html.size()) {
        if (i + 4 < html.size() && html[i] == L'<' &&
            (html[i+1] == L'i' || html[i+1] == L'I') &&
            (html[i+2] == L'm' || html[i+2] == L'M') &&
            (html[i+3] == L'g' || html[i+3] == L'G')) {
            size_t tagEnd = html.find(L'>', i);
            if (tagEnd == std::wstring::npos) tagEnd = html.size();
            std::wstring tag = html.substr(i, tagEnd - i + 1);
            std::wstring keyName;
            size_t srcPos = tag.find(L"src=");
            if (srcPos == std::wstring::npos) srcPos = tag.find(L"SRC=");
            if (srcPos != std::wstring::npos) {
                srcPos += 4;
                wchar_t quote = (srcPos < tag.size()) ? tag[srcPos] : L'\0';
                if (quote == L'\'' || quote == L'"') {
                    ++srcPos;
                    size_t end = tag.find(quote, srcPos);
                    if (end != std::wstring::npos)
                        keyName = tag.substr(srcPos, end - srcPos);
                }
            }
            if (keyName.size() > 4) {
                std::wstring ext = keyName.substr(keyName.size() - 4);
                if (ext == L".png" || ext == L".PNG")
                    keyName = keyName.substr(0, keyName.size() - 4);
            }
            if (!keyName.empty()) {
                if (keyName.substr(0, 4) == L"360_")
                    keyName = keyName.substr(4) + L" button";
                else if (keyName.substr(0, 4) == L"PS3_")
                    keyName = keyName.substr(4) + L" button";
                out += keyName;
            }
            i = tagEnd + 1;
        } else {
            out += html[i++];
        }
    }
    return out;
}

// --- Shared numeric/text formatters (used by inventory, container, etc.) ---

// Strips non-numeric glyph prefixes (e.g. septim icon "000" + "99" → "99")
static std::wstring SanitizeNumericText(const std::wstring& s) {
    std::wstring out;
    bool seenDigit = false;
    for (wchar_t c : s) {
        if (c == L' ' && !seenDigit) { continue; }
        else if (c >= L'0' && c <= L'9') { seenDigit = true; out += c; }
        else if ((c == L'.' || c == L',') && seenDigit) { out += c; }
        else if ((c == L'-' || c == 0x2212 || c == 0x2013) && !seenDigit) { out += L'-'; }
        else if (seenDigit) break;
    }
    return out;
}

// --- DirectX Scan Code → Key Name ---
static std::wstring DXScanCodeToName(int code) {
    switch (code) {
        case 1:   return L"Escape";
        case 2:   return L"1";
        case 3:   return L"2";
        case 4:   return L"3";
        case 5:   return L"4";
        case 6:   return L"5";
        case 7:   return L"6";
        case 8:   return L"7";
        case 9:   return L"8";
        case 10:  return L"9";
        case 11:  return L"0";
        case 12:  return L"Minus";
        case 13:  return L"Equals";
        case 14:  return L"Backspace";
        case 15:  return L"Tab";
        case 16:  return L"Q";
        case 17:  return L"W";
        case 18:  return L"E";
        case 19:  return L"R";
        case 20:  return L"T";
        case 21:  return L"Y";
        case 22:  return L"U";
        case 23:  return L"I";
        case 24:  return L"O";
        case 25:  return L"P";
        case 26:  return L"Left Bracket";
        case 27:  return L"Right Bracket";
        case 28:  return L"Enter";
        case 29:  return L"Left Ctrl";
        case 30:  return L"A";
        case 31:  return L"S";
        case 32:  return L"D";
        case 33:  return L"F";
        case 34:  return L"G";
        case 35:  return L"H";
        case 36:  return L"J";
        case 37:  return L"K";
        case 38:  return L"L";
        case 39:  return L"Semicolon";
        case 40:  return L"Apostrophe";
        case 41:  return L"Tilde";
        case 42:  return L"Left Shift";
        case 43:  return L"Backslash";
        case 44:  return L"Z";
        case 45:  return L"X";
        case 46:  return L"C";
        case 47:  return L"V";
        case 48:  return L"B";
        case 49:  return L"N";
        case 50:  return L"M";
        case 51:  return L"Comma";
        case 52:  return L"Period";
        case 53:  return L"Slash";
        case 54:  return L"Right Shift";
        case 55:  return L"Numpad *";
        case 56:  return L"Left Alt";
        case 57:  return L"Space";
        case 58:  return L"Caps Lock";
        case 59:  return L"F1";
        case 60:  return L"F2";
        case 61:  return L"F3";
        case 62:  return L"F4";
        case 63:  return L"F5";
        case 64:  return L"F6";
        case 65:  return L"F7";
        case 66:  return L"F8";
        case 67:  return L"F9";
        case 68:  return L"F10";
        case 69:  return L"Num Lock";
        case 70:  return L"Scroll Lock";
        case 71:  return L"Numpad 7";
        case 72:  return L"Numpad 8";
        case 73:  return L"Numpad 9";
        case 74:  return L"Numpad -";
        case 75:  return L"Numpad 4";
        case 76:  return L"Numpad 5";
        case 77:  return L"Numpad 6";
        case 78:  return L"Numpad +";
        case 79:  return L"Numpad 1";
        case 80:  return L"Numpad 2";
        case 81:  return L"Numpad 3";
        case 82:  return L"Numpad 0";
        case 83:  return L"Numpad .";
        case 87:  return L"F11";
        case 88:  return L"F12";
        case 156: return L"Numpad Enter";
        case 157: return L"Right Ctrl";
        case 181: return L"Numpad /";
        case 183: return L"Print Screen";
        case 184: return L"Right Alt";
        case 197: return L"Pause";
        case 199: return L"Home";
        case 200: return L"Up";
        case 201: return L"Page Up";
        case 203: return L"Left";
        case 205: return L"Right";
        case 207: return L"End";
        case 208: return L"Down";
        case 209: return L"Page Down";
        case 210: return L"Insert";
        case 211: return L"Delete";
        case 219: return L"Left Win";
        case 220: return L"Right Win";
        // Mouse buttons (256+)
        case 256: return L"Left Click";
        case 257: return L"Right Click";
        case 258: return L"Middle Click";
        case 259: return L"Mouse 4";
        case 260: return L"Mouse 5";
        case 261: return L"Mouse 6";
        case 262: return L"Mouse 7";
        case 263: return L"Mouse 8";
        case 264: return L"Mouse Wheel Up";
        case 265: return L"Mouse Wheel Down";
        default:  return L"key " + std::to_wstring(code);
    }
}

// --- SkyUI Detection ---
// SkyUI replaces several menus (inventory, container, barter, magic, gift) with different GFx paths.
// Détecté au chargement via le plugin ESP, avec fallback GFx au premier menu.
static std::atomic_bool g_skyuiMode{false};
static std::atomic_bool g_skyuiDetectionDone{false};

static void DetectSkyUIFromPlugin() {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return;
    bool isSkyUI = (dataHandler->LookupModByName("SkyUI_SE.esp") != nullptr);
    g_skyuiMode.store(isSkyUI, std::memory_order_relaxed);
    g_skyuiDetectionDone.store(true, std::memory_order_relaxed);
    LOG("SkyUI detected at startup: {}", isSkyUI ? "yes" : "no");
}

static void DetectSkyUI(RE::GFxMovieView* movie) {
    if (g_skyuiDetectionDone.load(std::memory_order_relaxed)) return;
    RE::GFxValue test;
    bool isSkyUI = SafeGetVariable(movie, test, "_root.Menu_mc.inventoryLists") && !test.IsUndefined();
    g_skyuiMode.store(isSkyUI, std::memory_order_relaxed);
    g_skyuiDetectionDone.store(true, std::memory_order_relaxed);
    LOG("SkyUI detected via GFx fallback: {}", isSkyUI ? "yes" : "no");
}

// Check if a numeric text is zero (handles "0", "0.0", "000", etc.)
static bool isZero(const std::wstring& s) {
    for (auto c : s) {
        if (c != L'0' && c != L'.' && c != L',') return false;
    }
    return true;
}

// Formats a weight value with one decimal, comma as separator
static std::wstring FormatWeight(double w) {
    const double rounded = std::round(w * 10.0) / 10.0;
    const auto iwhole = static_cast<long long>(rounded);
    if (static_cast<double>(iwhole) == rounded) return std::to_wstring(iwhole);
    std::wostringstream ss;
    ss << std::fixed << std::setprecision(1) << rounded;
    std::wstring s = ss.str();
    for (auto& c : s) if (c == L'.') c = L',';
    return s;
}

// ---------------- MCM settings ----------------

static std::atomic_bool g_mcmStealthAnnounce{true};   // annonces furtivité
static std::atomic_bool g_mcmTeleportEnabled{true};    // téléportation scanner

// Touches configurables (DirectX scancodes)
static std::atomic<uint32_t> g_keyScan{76};           // Numpad 5
static std::atomic<uint32_t> g_keyNextObject{209};    // Page Down
static std::atomic<uint32_t> g_keyPrevObject{201};    // Page Up
static std::atomic<uint32_t> g_keyAnnounce{199};      // Home
static std::atomic<uint32_t> g_keySubcategory{207};   // End
static std::atomic<uint32_t> g_keyTeleport{199};      // Home (+ Alt)
static std::atomic<float> g_mcmScanRange{0.0f};       // 0 = illimité
static std::atomic<float> g_mcmTeleportRange{3000.0f}; // distance max de téléportation

// ---------------- INI settings ----------------

static float g_volumeAim       = 1.0f;
static float g_volumeKill      = 1.0f;
static float g_volumeDragonHit = 1.0f;

static void LoadINISettings() {
    // Chemin : Data/SKSE/Plugins/SkyrimNVDA.ini (à côté du DLL)
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string exePath(path);
    auto pos = exePath.find_last_of("\\/");
    std::string iniPath = exePath.substr(0, pos) + "\\Data\\SKSE\\Plugins\\SkyrimNVDA.ini";

    auto readFloat = [&](const char* section, const char* key, float def) -> float {
        char buf[32];
        GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), iniPath.c_str());
        if (buf[0] == '\0') return def;
        try { return std::stof(buf); } catch (...) { return def; }
    };

    g_volumeAim       = readFloat("Sounds", "AimVolume", 1.0f);
    g_volumeKill      = readFloat("Sounds", "KillVolume", 1.0f);
    g_volumeDragonHit = readFloat("Sounds", "DragonHitVolume", 1.0f);

    LOG("INI loaded: aim={:.2f} kill={:.2f} dragonHit={:.2f}", g_volumeAim, g_volumeKill, g_volumeDragonHit);
}

// ---------------- Custom sounds via Papyrus ----------------

// FormIDs locaux SOUN dans SkyrimTTS_AutoWalk.esp (le script Papyrus résout le load order)
static constexpr RE::FormID g_soundAimLoopID   = 0x806;
static constexpr RE::FormID g_soundEnemyDeathID = 0x807;
static constexpr RE::FormID g_soundDragonHitID  = 0x808;

// Appelle une fonction son sur le script Papyrus SkyrimTTS_AutoWalk
static void CallSoundPapyrus(const char* funcName, RE::FormID soundFormID = 0, float volume = 1.0f) {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    std::string func(funcName);
    RE::FormID id = soundFormID;
    task->AddTask([func, id, volume]() {
        auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SkyrimTTS_AutoWalkQuest");
        if (!quest) { LOG("Sound: quest not found"); return; }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return;

        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) return;

        auto handle = policy->GetHandleForObject(RE::FormType::Quest, quest);
        if (handle == policy->EmptyHandle()) return;

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        bool ok = false;
        if (id != 0) {
            auto* args = RE::MakeFunctionArguments(static_cast<std::int32_t>(id), static_cast<float>(volume));
            ok = vm->DispatchMethodCall(handle, RE::BSFixedString("SkyrimTTS_AutoWalk"),
                RE::BSFixedString(func.c_str()), args, callback);
        } else {
            auto* emptyArgs = RE::MakeFunctionArguments();
            ok = vm->DispatchMethodCall(handle, RE::BSFixedString("SkyrimTTS_AutoWalk"),
                RE::BSFixedString(func.c_str()), emptyArgs, callback);
        }
        if (!ok) LOG("Sound: DispatchMethodCall '{}' failed (formID={:08X})", func, id);
    });
}

static void PlaySoundOneShot(RE::FormID soundFormID, float volume = 1.0f) {
    if (soundFormID == 0 || volume <= 0.0f) return;
    CallSoundPapyrus("OnPlaySound", soundFormID, volume);
}

static void PlaySoundLoop(RE::FormID soundFormID, float volume = 1.0f) {
    if (soundFormID == 0 || volume <= 0.0f) return;
    CallSoundPapyrus("OnPlayLoopSound", soundFormID, volume);
}

static void StopSoundLoop() {
    CallSoundPapyrus("OnStopLoopSound");
}

// Converts "250 / 300" or "250/300" to "250 out of 300"
static std::wstring FormatCarryWeight(const std::wstring& raw) {
    const auto pos = raw.find(L'/');
    if (pos == std::wstring::npos) return raw;
    std::wstring cur = raw.substr(0, pos);
    std::wstring max = raw.substr(pos + 1);
    while (!cur.empty() && cur.back() == L' ') cur.pop_back();
    while (!max.empty() && max.front() == L' ') max = max.substr(1);
    return cur + L" out of " + max;
}
