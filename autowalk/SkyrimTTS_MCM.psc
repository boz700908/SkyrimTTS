ScriptName SkyrimTTS_MCM extends SKI_ConfigBase

; === Réglages sauvegardés ===
bool property StealthAnnounce = true auto
bool property TeleportEnabled = true auto
float property AimVolume = 0.2 auto
float property KillVolume = 0.4 auto
float property DragonHitVolume = 1.0 auto
float property ScanRange = 0.0 auto    ; 0 = unlimited
float property TeleportRange = 3000.0 auto

; Touches (DirectX scancodes)
int property KeyScan = 76 auto          ; Numpad 5
int property KeyNextObject = 209 auto   ; Page Down
int property KeyPrevObject = 201 auto   ; Page Up
int property KeyAnnounce = 199 auto     ; Home (+ Shift = autowalk)
int property KeySubcategory = 207 auto  ; End
int property KeyTeleport = 199 auto     ; Home (+ Alt)

; === OIDs ===
int oidStealthToggle
int oidTeleportToggle
int oidAimVolume
int oidKillVolume
int oidDragonHitVolume
int oidScanRange
int oidTeleportRange
int oidKeyScan
int oidKeyNextObject
int oidKeyPrevObject
int oidKeyAnnounce
int oidKeySubcategory
int oidKeyTeleport
int oidResetAll

; === Initialisation ===
event OnConfigInit()
    ModName = "SkyrimNVDA"
    Pages = new string[3]
    Pages[0] = "General"
    Pages[1] = "Audio"
    Pages[2] = "Controls"
endEvent

; === Version — incrémenter à chaque changement de structure du MCM ===
int function GetVersion()
    return 3
endFunction

event OnVersionUpdate(int a_version)
    if a_version >= 2
        Pages = new string[3]
        Pages[0] = "General"
        Pages[1] = "Audio"
        Pages[2] = "Controls"
    endIf
    if a_version >= 3
        TeleportRange = 3000.0
    endIf
endEvent

event OnGameReload()
    parent.OnGameReload()
    SyncAllToNative()
endEvent

; === Affichage des pages ===
event OnPageReset(string page)
    SetCursorFillMode(TOP_TO_BOTTOM)

    if page == "" || page == "General"
        AddHeaderOption("Scanner")
        oidTeleportToggle = AddToggleOption("Teleport enabled", TeleportEnabled)
        oidScanRange = AddSliderOption("Scan range (0 = unlimited)", ScanRange, "{0}")
        oidTeleportRange = AddSliderOption("Teleport range", TeleportRange, "{0}")

        AddEmptyOption()
        AddHeaderOption("Announcements")
        oidStealthToggle = AddToggleOption("Stealth announcements", StealthAnnounce)

        AddEmptyOption()
        oidResetAll = AddTextOption("Reset all to defaults", "")

    elseIf page == "Audio"
        AddHeaderOption("Sound volumes")
        oidAimVolume = AddSliderOption("Aim sound", AimVolume, "{2}")
        oidKillVolume = AddSliderOption("Kill sound", KillVolume, "{2}")
        oidDragonHitVolume = AddSliderOption("Dragon hit sound", DragonHitVolume, "{2}")

    elseIf page == "Controls"
        AddHeaderOption("Scanner keys")
        oidKeyScan = AddKeyMapOption("Scan objects", KeyScan)
        oidKeyAnnounce = AddKeyMapOption("Announce / Autowalk", KeyAnnounce)
        oidKeyNextObject = AddKeyMapOption("Next object (Shift = category)", KeyNextObject)
        oidKeyPrevObject = AddKeyMapOption("Previous object (Shift = category)", KeyPrevObject)
        oidKeySubcategory = AddKeyMapOption("Cycle subcategory", KeySubcategory)
        oidKeyTeleport = AddKeyMapOption("Teleport (Alt + key)", KeyTeleport)
    endIf
endEvent

; === Toggles ===
event OnOptionSelect(int option)
    if option == oidStealthToggle
        StealthAnnounce = !StealthAnnounce
        SetToggleOptionValue(option, StealthAnnounce)
        SkyrimTTS_MCM_Native.SetStealthAnnounce(StealthAnnounce)

    elseIf option == oidTeleportToggle
        TeleportEnabled = !TeleportEnabled
        SetToggleOptionValue(option, TeleportEnabled)
        SkyrimTTS_MCM_Native.SetTeleportEnabled(TeleportEnabled)

    elseIf option == oidResetAll
        bool confirm = ShowMessage("Reset all settings to defaults?")
        if confirm
            StealthAnnounce = true
            TeleportEnabled = true
            AimVolume = 0.2
            KillVolume = 0.4
            DragonHitVolume = 1.0
            ScanRange = 0.0
            TeleportRange = 3000.0
            KeyScan = 76
            KeyNextObject = 209
            KeyPrevObject = 201
            KeyAnnounce = 199
            KeySubcategory = 207
            KeyTeleport = 199
            SyncAllToNative()
            ForcePageReset()
        endIf
    endIf
endEvent

; === Ouverture des sliders ===
event OnOptionSliderOpen(int option)
    if option == oidAimVolume
        SetSliderDialogStartValue(AimVolume)
        SetSliderDialogDefaultValue(0.2)
        SetSliderDialogRange(0.0, 2.0)
        SetSliderDialogInterval(0.1)

    elseIf option == oidKillVolume
        SetSliderDialogStartValue(KillVolume)
        SetSliderDialogDefaultValue(0.4)
        SetSliderDialogRange(0.0, 2.0)
        SetSliderDialogInterval(0.1)

    elseIf option == oidDragonHitVolume
        SetSliderDialogStartValue(DragonHitVolume)
        SetSliderDialogDefaultValue(1.0)
        SetSliderDialogRange(0.0, 2.0)
        SetSliderDialogInterval(0.1)

    elseIf option == oidScanRange
        SetSliderDialogStartValue(ScanRange)
        SetSliderDialogDefaultValue(0.0)
        SetSliderDialogRange(0.0, 10000.0)
        SetSliderDialogInterval(500.0)

    elseIf option == oidTeleportRange
        SetSliderDialogStartValue(TeleportRange)
        SetSliderDialogDefaultValue(3000.0)
        SetSliderDialogRange(500.0, 3000.0)
        SetSliderDialogInterval(500.0)
    endIf
endEvent

; === Validation des sliders ===
event OnOptionSliderAccept(int option, float value)
    if option == oidAimVolume
        AimVolume = value
        SetSliderOptionValue(option, value, "{2}")
        SkyrimTTS_MCM_Native.SetAimVolume(value)

    elseIf option == oidKillVolume
        KillVolume = value
        SetSliderOptionValue(option, value, "{2}")
        SkyrimTTS_MCM_Native.SetKillVolume(value)

    elseIf option == oidDragonHitVolume
        DragonHitVolume = value
        SetSliderOptionValue(option, value, "{2}")
        SkyrimTTS_MCM_Native.SetDragonHitVolume(value)

    elseIf option == oidScanRange
        ScanRange = value
        SetSliderOptionValue(option, value, "{0}")
        SkyrimTTS_MCM_Native.SetScanRange(value)

    elseIf option == oidTeleportRange
        TeleportRange = value
        SetSliderOptionValue(option, value, "{0}")
        SkyrimTTS_MCM_Native.SetTeleportRange(value)
    endIf
endEvent

; === Changement de touche ===
event OnOptionKeyMapChange(int option, int keyCode, string conflictControl, string conflictName)
    if option == oidKeyScan
        KeyScan = keyCode
        SetKeyMapOptionValue(option, keyCode)
        SkyrimTTS_MCM_Native.SetKeyScan(keyCode)

    elseIf option == oidKeyAnnounce
        KeyAnnounce = keyCode
        SetKeyMapOptionValue(option, keyCode)
        SkyrimTTS_MCM_Native.SetKeyAnnounce(keyCode)

    elseIf option == oidKeyNextObject
        KeyNextObject = keyCode
        SetKeyMapOptionValue(option, keyCode)
        SkyrimTTS_MCM_Native.SetKeyNextObject(keyCode)

    elseIf option == oidKeyPrevObject
        KeyPrevObject = keyCode
        SetKeyMapOptionValue(option, keyCode)
        SkyrimTTS_MCM_Native.SetKeyPrevObject(keyCode)

    elseIf option == oidKeySubcategory
        KeySubcategory = keyCode
        SetKeyMapOptionValue(option, keyCode)
        SkyrimTTS_MCM_Native.SetKeySubcategory(keyCode)

    elseIf option == oidKeyTeleport
        KeyTeleport = keyCode
        SetKeyMapOptionValue(option, keyCode)
        SkyrimTTS_MCM_Native.SetKeyTeleport(keyCode)
    endIf
endEvent

; === Info bulles ===
event OnOptionHighlight(int option)
    if option == oidStealthToggle
        SetInfoText("Toggle Hidden / Detected / Caution announcements")
    elseIf option == oidTeleportToggle
        SetInfoText("Enable or disable Alt+Announce teleportation")
    elseIf option == oidAimVolume
        SetInfoText("Volume of the aiming feedback loop")
    elseIf option == oidKillVolume
        SetInfoText("Volume of the enemy death sound")
    elseIf option == oidDragonHitVolume
        SetInfoText("Volume of the dragon hit sound")
    elseIf option == oidScanRange
        SetInfoText("Maximum scan distance. 0 = unlimited, 1000 = about 15 meters")
    elseIf option == oidTeleportRange
        SetInfoText("Maximum teleport distance. Does not apply to quest targets")
    elseIf option == oidKeyScan
        SetInfoText("Key to scan nearby objects")
    elseIf option == oidKeyAnnounce
        SetInfoText("Announce selected object. Shift = autowalk")
    elseIf option == oidKeyNextObject
        SetInfoText("Next object. Hold Shift for next category")
    elseIf option == oidKeyPrevObject
        SetInfoText("Previous object. Hold Shift for previous category")
    elseIf option == oidKeySubcategory
        SetInfoText("Cycle through subcategories")
    elseIf option == oidKeyTeleport
        SetInfoText("Teleport to selected object. Requires Alt + this key")
    elseIf option == oidResetAll
        SetInfoText("Reset all settings, volumes, and keys to their default values")
    endIf
endEvent

; === Resynchronise tous les réglages vers le C++ ===
function SyncAllToNative()
    SkyrimTTS_MCM_Native.SetStealthAnnounce(StealthAnnounce)
    SkyrimTTS_MCM_Native.SetTeleportEnabled(TeleportEnabled)
    SkyrimTTS_MCM_Native.SetAimVolume(AimVolume)
    SkyrimTTS_MCM_Native.SetKillVolume(KillVolume)
    SkyrimTTS_MCM_Native.SetDragonHitVolume(DragonHitVolume)
    SkyrimTTS_MCM_Native.SetScanRange(ScanRange)
    SkyrimTTS_MCM_Native.SetKeyScan(KeyScan)
    SkyrimTTS_MCM_Native.SetKeyAnnounce(KeyAnnounce)
    SkyrimTTS_MCM_Native.SetKeyNextObject(KeyNextObject)
    SkyrimTTS_MCM_Native.SetKeyPrevObject(KeyPrevObject)
    SkyrimTTS_MCM_Native.SetKeySubcategory(KeySubcategory)
    SkyrimTTS_MCM_Native.SetKeyTeleport(KeyTeleport)
    SkyrimTTS_MCM_Native.SetScanRange(ScanRange)
    SkyrimTTS_MCM_Native.SetTeleportRange(TeleportRange)
endFunction
