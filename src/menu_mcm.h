#pragma once

// VOCALISATION MCM (SkyUI Mod Configuration Menu) - DEBUT
// Le MCM vit à l'intérieur du JournalMenu.
// Quand on sélectionne "Mod Configuration" dans l'onglet System,
// _root.ConfigPanelFader devient visible et contient le configPanel.

// --- Chemins GFx ---
static constexpr const char* MCM_PANEL          = "_root.ConfigPanelFader.configPanel";
static constexpr const char* MCM_VISIBLE         = "_root.ConfigPanelFader._visible";
// Mod list (left panel — liste des mods)
static constexpr const char* MCM_MODLIST_ENTRY   = "_root.ConfigPanelFader.configPanel.contentHolder.modListPanel.modListFader.list.selectedEntry.text";
// Sub-page list (left panel — pages d'un mod)
static constexpr const char* MCM_SUBLIST_ENTRY   = "_root.ConfigPanelFader.configPanel.contentHolder.modListPanel.subListFader.list.selectedEntry.text";
// Quel panneau est actif : modListPanel._state (0=INIT, 1=LIST_ACTIVE, 2=SUBLIST_ACTIVE)
static constexpr const char* MCM_MODLIST_STATE   = "_root.ConfigPanelFader.configPanel.contentHolder.modListPanel._state";
// Options list (right panel)
static constexpr const char* MCM_OPTIONS_IDX     = "_root.ConfigPanelFader.configPanel.contentHolder.optionsPanel.optionsList.selectedIndex";
static constexpr const char* MCM_OPTIONS_ENTRY   = "_root.ConfigPanelFader.configPanel.contentHolder.optionsPanel.optionsList.selectedEntry";
// Title and info
static constexpr const char* MCM_TITLE           = "_root.ConfigPanelFader.configPanel.titlebar.textField.text";
static constexpr const char* MCM_INFO            = "_root.ConfigPanelFader.configPanel.contentHolder.infoPanel.textField.text";
// Focus : 0 = mod list, 1 = options
static constexpr const char* MCM_FOCUS           = "_root.ConfigPanelFader.configPanel._focus";

// Option types (from OptionsListEntry.as)
static constexpr int MCM_OPT_EMPTY   = 0;
static constexpr int MCM_OPT_HEADER  = 1;
static constexpr int MCM_OPT_TEXT    = 2;
static constexpr int MCM_OPT_TOGGLE  = 3;
static constexpr int MCM_OPT_SLIDER  = 4;
static constexpr int MCM_OPT_MENU    = 5;
static constexpr int MCM_OPT_COLOR   = 6;
static constexpr int MCM_OPT_KEYMAP  = 7;
static constexpr int MCM_OPT_INPUT   = 8;

// Dialog paths (popups for slider, menu, etc.)
static constexpr const char* MCM_DIALOG_MENU_ENTRY  = "_root.ConfigPanelFader.configPanel.dialog.menuList.selectedEntry.text";
static constexpr const char* MCM_DIALOG_SLIDER_VAL  = "_root.ConfigPanelFader.configPanel.dialog.sliderPanel.valueTextField.text";
static constexpr const char* MCM_DIALOG_SLIDER_TEXT = "_root.ConfigPanelFader.configPanel.dialog.sliderPanel.slider.value";
static constexpr const char* MCM_DIALOG_MESSAGE     = "_root.ConfigPanelFader.configPanel.dialog.textField.text";

// --- État ---
static std::atomic_bool g_mcmOpen{false};
static std::atomic_bool g_mcmPendingUIRead{false};
static std::wstring     g_lastMcmModName;
static std::wstring     g_lastMcmPageName;
static std::wstring     g_lastMcmOptionText;
static std::wstring     g_lastMcmInfoText;
static std::wstring     g_lastMcmTitle;
static int              g_lastMcmFocus{-1};
static int              g_lastMcmOptionIdx{-1};
static double           g_lastMcmNumValue{0.0};
static std::wstring     g_lastMcmStrValue;
static std::wstring     g_lastMcmDialogItem;   // current dialog selection (menu list / slider value)

// --- Snapshot ---
struct McmSnapshot {
    bool         visible{false};
    int          focus{-1};      // 0 = mod list, 1 = options
    int          modListState{0}; // 1=LIST_ACTIVE, 2=SUBLIST_ACTIVE
    std::wstring modName;        // selected mod (when list active)
    std::wstring pageName;       // selected sub-page (when sublist active)
    std::wstring title;          // page title bar
    std::wstring infoText;       // description of highlighted option
    // Option courante
    int          optionIdx{-1};
    int          optionType{-1};
    std::wstring optionText;     // label
    std::wstring optionStrValue; // string value (text, slider format, menu label)
    double       optionNumValue{0.0}; // numeric value (toggle on/off, slider value, color, keycode)
    // Dialog popup (menu list selection, slider value text)
    std::wstring dialogItem;
};

static bool ReadMcmSnapshot(McmSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::JournalMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    // Vérifier si le MCM est visible
    RE::GFxValue visVal;
    if (!SafeGetVariable(movie, visVal, MCM_VISIBLE) || !SafeIsBool(visVal) || !SafeGetBool(visVal))
        return false;
    snap.visible = true;

    std::string tmp;

    // Focus (mod list vs options)
    double focusD = 0.0;
    if (GetGFxNumber(movie, MCM_FOCUS, focusD))
        snap.focus = static_cast<int>(focusD);

    // Mod list panel state
    double stateD = 0.0;
    if (GetGFxNumber(movie, MCM_MODLIST_STATE, stateD))
        snap.modListState = static_cast<int>(stateD);

    // Mod name (when browsing mod list)
    if (GetGFxString(movie, MCM_MODLIST_ENTRY, tmp) && !tmp.empty())
        snap.modName = Utf8ToWString(tmp);

    // Sub-page name (when browsing pages of a mod)
    if (GetGFxString(movie, MCM_SUBLIST_ENTRY, tmp) && !tmp.empty())
        snap.pageName = Utf8ToWString(tmp);

    // Title
    if (GetGFxString(movie, MCM_TITLE, tmp) && !tmp.empty())
        snap.title = Utf8ToWString(tmp);

    // Info text (description)
    if (GetGFxString(movie, MCM_INFO, tmp) && !tmp.empty())
        snap.infoText = StripMarkupForSpeech(Utf8ToWString(tmp));

    // Selected option
    double idxD = -1.0;
    if (GetGFxNumber(movie, MCM_OPTIONS_IDX, idxD))
        snap.optionIdx = static_cast<int>(idxD);

    if (snap.optionIdx >= 0) {
        RE::GFxValue entry;
        if (SafeGetVariable(movie, entry, MCM_OPTIONS_ENTRY) && SafeIsObject(entry)) {
            // Option type
            RE::GFxValue typeVal;
            if (entry.GetMember("optionType", &typeVal) && SafeIsNumber(typeVal))
                snap.optionType = static_cast<int>(SafeGetNumber(typeVal));

            // Option text (label)
            RE::GFxValue textVal;
            if (entry.GetMember("text", &textVal) && SafeIsString(textVal)) {
                std::string s = SafeGetString(textVal);
                if (!s.empty()) snap.optionText = Utf8ToWString(s);
            }

            // String value — résoudre les clés de traduction ($Medium, $On, etc.)
            RE::GFxValue strVal;
            if (entry.GetMember("strValue", &strVal) && SafeIsString(strVal)) {
                std::string s = SafeGetString(strVal);
                if (!s.empty()) snap.optionStrValue = ResolveUIString(movie, s);
            }

            // Numeric value
            RE::GFxValue numVal;
            if (entry.GetMember("numValue", &numVal) && SafeIsNumber(numVal))
                snap.optionNumValue = SafeGetNumber(numVal);
        }
    }

    // Check for active dialog (menu dropdown or slider popup)
    {
        std::string dialogStr;
        // Message dialog: read confirmation text (e.g. "Reset all settings to defaults?")
        if (GetGFxString(movie, MCM_DIALOG_MESSAGE, dialogStr) && !dialogStr.empty()) {
            snap.dialogItem = StripMarkupForSpeech(Utf8ToWString(dialogStr));
        }
        // Menu dialog: read selected item text
        else if (GetGFxString(movie, MCM_DIALOG_MENU_ENTRY, dialogStr) && !dialogStr.empty()) {
            snap.dialogItem = Utf8ToWString(dialogStr);
        }
        // Slider dialog: read formatted value text
        else if (GetGFxString(movie, MCM_DIALOG_SLIDER_VAL, dialogStr) && !dialogStr.empty()) {
            snap.dialogItem = StripMarkupForSpeech(Utf8ToWString(dialogStr));
        }
    }

    return snap.visible;
}

// Convertit le type d'option en texte lisible
static std::wstring McmOptionTypeName(int type) {
    switch (type) {
        case MCM_OPT_HEADER:  return TR("header");
        case MCM_OPT_TOGGLE:  return TR("toggle");
        case MCM_OPT_SLIDER:  return TR("slider");
        case MCM_OPT_MENU:    return TR("menu");
        case MCM_OPT_COLOR:   return TR("color");
        case MCM_OPT_KEYMAP:  return TR("key");
        case MCM_OPT_INPUT:   return TR("input");
        default:              return L"";
    }
}

// Formate la valeur d'une option pour la lecture
static std::wstring FormatMcmOptionValue(const McmSnapshot& snap) {
    switch (snap.optionType) {
        case MCM_OPT_TOGGLE:
            return snap.optionNumValue != 0.0 ? TR("on") : TR("off");
        case MCM_OPT_SLIDER:
            // strValue contient le format string, numValue la valeur
            if (!snap.optionStrValue.empty()) {
                // Essayer de formater : "{0}" → remplacer par la valeur
                std::wstring fmt = snap.optionStrValue;
                std::wstring numStr;
                double v = snap.optionNumValue;
                if (v == static_cast<int>(v))
                    numStr = std::to_wstring(static_cast<int>(v));
                else {
                    std::wostringstream ss;
                    ss << std::fixed << std::setprecision(1) << v;
                    numStr = ss.str();
                }
                size_t pos = fmt.find(L"{0}");
                if (pos != std::wstring::npos) {
                    fmt.replace(pos, 3, numStr);
                    return fmt;
                }
                // Pas de placeholder, juste retourner la valeur
                return numStr;
            }
            return std::to_wstring(static_cast<int>(snap.optionNumValue));
        case MCM_OPT_TEXT:
        case MCM_OPT_MENU:
        case MCM_OPT_INPUT:
            return snap.optionStrValue;
        case MCM_OPT_KEYMAP: {
            int keyCode = static_cast<int>(snap.optionNumValue);
            if (keyCode <= 0 || keyCode == 282) return TR("unbound");
            return DXScanCodeToName(keyCode);
        }
        default:
            return L"";
    }
}

static void AnnounceMcmChangeImpl() {
    if (!g_mcmOpen.load()) return;
    McmSnapshot snap;
    if (!ReadMcmSnapshot(snap)) {
        // MCM fermé (retour au journal)
        if (g_mcmOpen.load()) {
            g_mcmOpen.store(false);
            LOG("MCM: closed (ConfigPanelFader hidden)");
        }
        return;
    }

    const bool firstRead = g_lastMcmModName.empty() && g_lastMcmOptionText.empty() && g_lastMcmPageName.empty();

    // Titre de la page (change quand on sélectionne un mod/page)
    if (!snap.title.empty() && snap.title != g_lastMcmTitle) {
        LOG("MCM: title changed to '{}'", WStringToUtf8(snap.title));
        g_lastMcmTitle = snap.title;
    }

    // Focus change (mod list ↔ options)
    if (snap.focus != g_lastMcmFocus) {
        g_lastMcmFocus = snap.focus;
        // Reset pour relire quand on change de panneau
        if (snap.focus == 0) {
            g_lastMcmOptionText.clear();
            g_lastMcmOptionIdx = -1;
        }
    }

    // Navigation dans la liste des mods
    if (snap.focus == 0 && snap.modListState == 1) {
        // On est dans la liste des mods
        if (!snap.modName.empty() && snap.modName != g_lastMcmModName) {
            if (firstRead) SpeakQueue(snap.modName); else Speak(snap.modName);
            g_lastMcmModName = snap.modName;
            g_lastMcmPageName.clear();
            LOG("MCM: mod selected '{}'", WStringToUtf8(snap.modName));
        }
    } else if (snap.focus == 0 && snap.modListState == 2) {
        // On est dans la sous-liste des pages
        if (!snap.pageName.empty() && snap.pageName != g_lastMcmPageName) {
            if (firstRead) SpeakQueue(snap.pageName); else Speak(snap.pageName);
            g_lastMcmPageName = snap.pageName;
            LOG("MCM: page selected '{}'", WStringToUtf8(snap.pageName));
        }
    }

    // Navigation dans les options (panneau droit)
    if (snap.focus == 1 && snap.optionIdx >= 0 && snap.optionType > MCM_OPT_EMPTY) {
        const bool optionChanged = (snap.optionIdx != g_lastMcmOptionIdx) ||
                                   (snap.optionText != g_lastMcmOptionText);
        const bool valueChanged  = !optionChanged &&
                                   (snap.optionNumValue != g_lastMcmNumValue ||
                                    snap.optionStrValue != g_lastMcmStrValue);

        if (optionChanged && !snap.optionText.empty()) {
            // Nouvelle option sélectionnée → lire nom + valeur + type
            std::wstring announce = snap.optionText;

            std::wstring value = FormatMcmOptionValue(snap);
            if (!value.empty())
                announce += L": " + value;

            std::wstring typeName = McmOptionTypeName(snap.optionType);
            if (!typeName.empty() && snap.optionType != MCM_OPT_TEXT && snap.optionType != MCM_OPT_HEADER)
                announce += L", " + typeName;

            if (firstRead) SpeakQueue(announce); else Speak(announce);

            g_lastMcmOptionText = snap.optionText;
            g_lastMcmOptionIdx = snap.optionIdx;
            g_lastMcmNumValue = snap.optionNumValue;
            g_lastMcmStrValue = snap.optionStrValue;
            g_lastMcmInfoText.clear(); // reset — attendre que la bonne description arrive
            LOG("MCM: option '{}' type={} value='{}'",
                WStringToUtf8(snap.optionText), snap.optionType, WStringToUtf8(value));
        } else if (valueChanged) {
            // Même option, valeur changée (toggle Enter, slider gauche/droite)
            std::wstring value = FormatMcmOptionValue(snap);
            if (!value.empty()) {
                Speak(value);
                LOG("MCM: value changed to '{}'", WStringToUtf8(value));
            }
            g_lastMcmNumValue = snap.optionNumValue;
            g_lastMcmStrValue = snap.optionStrValue;
        }
        // Info text désactivé : le jeu met ~200ms pour mettre à jour la description,
        // notre polling la lit trop vite et annonce la description de l'option précédente.
    }

    // Dialog popup (menu dropdown / slider) — lu indépendamment du focus
    if (!snap.dialogItem.empty() && snap.dialogItem != g_lastMcmDialogItem) {
        Speak(snap.dialogItem);
        g_lastMcmDialogItem = snap.dialogItem;
        LOG("MCM: dialog item '{}'", WStringToUtf8(snap.dialogItem));
    } else if (snap.dialogItem.empty() && !g_lastMcmDialogItem.empty()) {
        // Dialog fermé
        g_lastMcmDialogItem.clear();
    }
}

// --- Queue / Polling ---
static void QueueMcmRead() {
    if (!g_mcmOpen.load(std::memory_order_relaxed)) return;
    if (g_mcmPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_mcmPendingUIRead.store(false); return; }
    task->AddUITask([]() {
        g_mcmPendingUIRead.store(false);
        if (g_mcmOpen.load()) AnnounceMcmChangeImpl();
    });
}

// Reset state for fresh MCM open
static void ResetMcmState() {
    g_lastMcmModName.clear();
    g_lastMcmPageName.clear();
    g_lastMcmOptionText.clear();
    g_lastMcmInfoText.clear();
    g_lastMcmTitle.clear();
    g_lastMcmFocus = -1;
    g_lastMcmOptionIdx = -1;
    g_lastMcmNumValue = 0.0;
    g_lastMcmStrValue.clear();
    g_lastMcmDialogItem.clear();
}

// VOCALISATION MCM - FIN
