ScriptName SkyrimTTS_MCM extends SKI_ConfigBase

; === Réglages sauvegardés ===
bool property StealthAnnounce = true auto
bool property TeleportEnabled = true auto
bool property AutoAimEnabled = true auto
float property AimVolume = 0.2 auto
float property KillVolume = 0.4 auto
float property DragonHitVolume = 1.0 auto
float property ScanRange = 0.0 auto    ; 0 = unlimited
float property TeleportRange = 5000.0 auto

; Touches (DirectX scancodes)
int property KeyScan = 76 auto          ; Numpad 5
int property KeyNextObject = 209 auto   ; Page Down
int property KeyPrevObject = 201 auto   ; Page Up
int property KeyAnnounce = 199 auto     ; Home (+ Shift = autowalk)
int property KeySubcategory = 207 auto  ; End
int property KeyTeleport = 199 auto     ; Home (+ Alt)

; Gamepad button indexes (index dans la liste partagée, voir C++ g_gamepadButtonCodes)
; 0=DpadUp 1=DpadDown 2=DpadLeft 3=DpadRight 4=A 5=B 6=X 7=Y 8=LSclick 9=RSclick 10=RB 11=Start 12=Back 13=None
int property GpIdxScanNext = 1 auto      ; D-pad Down (objet plus loin)
int property GpIdxScanPrev = 0 auto      ; D-pad Up (objet plus proche)
int property GpIdxScanAnnounce = 2 auto  ; D-pad Left
int property GpIdxMapSetRef = 3 auto     ; D-pad Right
int property GpIdxPrimary = 6 auto       ; X (autowalk / fast travel)
int property GpIdxRemoteActivate = 4 auto ; A (remote activate, equivalent G key)
int property GpIdxTeleport = 5 auto      ; B
int property GpIdxVitals = 7 auto        ; Y
int property GpIdxSneak = 8 auto         ; LS click
int property GpIdxPOV = 9 auto           ; RS click
int property GpIdxLockEnemy = 9 auto     ; RS click (sans LB)

; === OIDs ===
int oidStealthToggle
int oidTeleportToggle
int oidAutoAimToggle
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

; Gamepad OIDs
int oidGpScanNext
int oidGpScanPrev
int oidGpScanAnnounce
int oidGpMapSetRef
int oidGpPrimary
int oidGpRemoteActivate
int oidGpTeleport
int oidGpVitals
int oidGpSneak
int oidGpPOV
int oidGpLockEnemy

; === Initialisation ===
event OnConfigInit()
    ModName = "SkyrimNVDA"
    Pages = new string[4]
    Pages[0] = "General"
    Pages[1] = "Audio"
    Pages[2] = "Controls"
    Pages[3] = "Gamepad"
endEvent

; === Version — incrémenter à chaque changement de structure du MCM ===
int function GetVersion()
    return 7
endFunction

event OnVersionUpdate(int a_version)
    if a_version >= 2
        Pages = new string[3]
        Pages[0] = "General"
        Pages[1] = "Audio"
        Pages[2] = "Controls"
    endIf
    if a_version >= 3
        TeleportRange = 5000.0
    endIf
    if a_version >= 4
        AutoAimEnabled = true
    endIf
    if a_version >= 5
        Pages = new string[4]
        Pages[0] = "General"
        Pages[1] = "Audio"
        Pages[2] = "Controls"
        Pages[3] = "Gamepad"
        GpIdxScanNext = 0
        GpIdxScanPrev = 1
        GpIdxScanAnnounce = 2
        GpIdxMapSetRef = 3
        GpIdxPrimary = 4
        GpIdxTeleport = 5
        GpIdxVitals = 7
        GpIdxSneak = 8
        GpIdxPOV = 9
        GpIdxLockEnemy = 9
    endIf
    if a_version >= 6
        ; Inverser Next/Prev sur la croix directionnelle :
        ; D-pad Down = objet suivant (plus loin), D-pad Up = précédent (plus proche)
        GpIdxScanNext = 1
        GpIdxScanPrev = 0
    endIf
    if a_version >= 7
        ; Remap autowalk vers X (LB+X), libère A (LB+A) pour Remote Activate.
        GpIdxPrimary = 6
        GpIdxRemoteActivate = 4
    endIf
endEvent

event OnGameReload()
    parent.OnGameReload()
    SyncAllToNative()
endEvent

; === Liste des noms de boutons manette (l'ordre doit correspondre à g_gamepadButtonCodes côté C++) ===
string function GetGpButtonName(int idx)
    if idx == 0
        return "D-pad Up"
    elseIf idx == 1
        return "D-pad Down"
    elseIf idx == 2
        return "D-pad Left"
    elseIf idx == 3
        return "D-pad Right"
    elseIf idx == 4
        return "A"
    elseIf idx == 5
        return "B"
    elseIf idx == 6
        return "X"
    elseIf idx == 7
        return "Y"
    elseIf idx == 8
        return "LS click"
    elseIf idx == 9
        return "RS click"
    elseIf idx == 10
        return "RB"
    elseIf idx == 11
        return "Start"
    elseIf idx == 12
        return "Back"
    endIf
    return "None"
endFunction

string[] function GetGpButtonList()
    string[] list = new string[14]
    list[0] = "D-pad Up"
    list[1] = "D-pad Down"
    list[2] = "D-pad Left"
    list[3] = "D-pad Right"
    list[4] = "A"
    list[5] = "B"
    list[6] = "X"
    list[7] = "Y"
    list[8] = "LS click"
    list[9] = "RS click"
    list[10] = "RB"
    list[11] = "Start"
    list[12] = "Back"
    list[13] = "None"
    return list
endFunction

; === Affichage des pages ===
event OnPageReset(string page)
    SetCursorFillMode(TOP_TO_BOTTOM)

    if page == "" || page == "General"
        AddHeaderOption("Scanner")
        oidTeleportToggle = AddToggleOption("Teleport enabled", TeleportEnabled)
        oidScanRange = AddSliderOption("Scan range (0 = unlimited)", ScanRange, "{0}")
        oidTeleportRange = AddSliderOption("Teleport range", TeleportRange, "{0}")

        AddEmptyOption()
        AddHeaderOption("Combat")
        oidAutoAimToggle = AddToggleOption("Bow auto aim", AutoAimEnabled)

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

    elseIf page == "Gamepad"
        AddHeaderOption("LB + button combos (scanner and map)")
        oidGpScanNext = AddMenuOption("Next object / marker", GetGpButtonName(GpIdxScanNext))
        oidGpScanPrev = AddMenuOption("Previous object / marker", GetGpButtonName(GpIdxScanPrev))
        oidGpScanAnnounce = AddMenuOption("Announce current", GetGpButtonName(GpIdxScanAnnounce))
        oidGpPrimary = AddMenuOption("Autowalk / Fast travel", GetGpButtonName(GpIdxPrimary))
        oidGpRemoteActivate = AddMenuOption("Remote activate (G)", GetGpButtonName(GpIdxRemoteActivate))
        oidGpTeleport = AddMenuOption("Teleport (gameplay)", GetGpButtonName(GpIdxTeleport))
        oidGpVitals = AddMenuOption("Vitals (gameplay)", GetGpButtonName(GpIdxVitals))
        oidGpMapSetRef = AddMenuOption("Set reference (map)", GetGpButtonName(GpIdxMapSetRef))

        AddEmptyOption()
        AddHeaderOption("Other LB combos")
        oidGpSneak = AddMenuOption("Sneak toggle", GetGpButtonName(GpIdxSneak))
        oidGpPOV = AddMenuOption("POV toggle", GetGpButtonName(GpIdxPOV))

        AddEmptyOption()
        AddHeaderOption("Standalone (no LB)")
        oidGpLockEnemy = AddMenuOption("Lock nearest enemy", GetGpButtonName(GpIdxLockEnemy))
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

    elseIf option == oidAutoAimToggle
        AutoAimEnabled = !AutoAimEnabled
        SetToggleOptionValue(option, AutoAimEnabled)
        SkyrimTTS_MCM_Native.SetAutoAimEnabled(AutoAimEnabled)

    elseIf option == oidResetAll
        bool confirm = ShowMessage("Reset all settings to defaults?")
        if confirm
            StealthAnnounce = true
            TeleportEnabled = true
            AutoAimEnabled = true
            AimVolume = 0.2
            KillVolume = 0.4
            DragonHitVolume = 1.0
            ScanRange = 0.0
            TeleportRange = 5000.0
            KeyScan = 76
            KeyNextObject = 209
            KeyPrevObject = 201
            KeyAnnounce = 199
            KeySubcategory = 207
            KeyTeleport = 199
            GpIdxScanNext = 1
            GpIdxScanPrev = 0
            GpIdxScanAnnounce = 2
            GpIdxMapSetRef = 3
            GpIdxPrimary = 6
            GpIdxRemoteActivate = 4
            GpIdxTeleport = 5
            GpIdxVitals = 7
            GpIdxSneak = 8
            GpIdxPOV = 9
            GpIdxLockEnemy = 9
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
        SetSliderDialogDefaultValue(5000.0)
        SetSliderDialogRange(500.0, 5000.0)
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

; === Ouverture des menus dropdown (page Gamepad) ===
event OnOptionMenuOpen(int option)
    int currentIdx = -1
    if option == oidGpScanNext
        currentIdx = GpIdxScanNext
    elseIf option == oidGpScanPrev
        currentIdx = GpIdxScanPrev
    elseIf option == oidGpScanAnnounce
        currentIdx = GpIdxScanAnnounce
    elseIf option == oidGpMapSetRef
        currentIdx = GpIdxMapSetRef
    elseIf option == oidGpPrimary
        currentIdx = GpIdxPrimary
    elseIf option == oidGpRemoteActivate
        currentIdx = GpIdxRemoteActivate
    elseIf option == oidGpTeleport
        currentIdx = GpIdxTeleport
    elseIf option == oidGpVitals
        currentIdx = GpIdxVitals
    elseIf option == oidGpSneak
        currentIdx = GpIdxSneak
    elseIf option == oidGpPOV
        currentIdx = GpIdxPOV
    elseIf option == oidGpLockEnemy
        currentIdx = GpIdxLockEnemy
    endIf

    if currentIdx >= 0
        SetMenuDialogOptions(GetGpButtonList())
        SetMenuDialogStartIndex(currentIdx)
        SetMenuDialogDefaultIndex(currentIdx)
    endIf
endEvent

; === Validation des menus dropdown (page Gamepad) ===
event OnOptionMenuAccept(int option, int index)
    if option == oidGpScanNext
        GpIdxScanNext = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpScanNext(index)
    elseIf option == oidGpScanPrev
        GpIdxScanPrev = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpScanPrev(index)
    elseIf option == oidGpScanAnnounce
        GpIdxScanAnnounce = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpScanAnnounce(index)
    elseIf option == oidGpMapSetRef
        GpIdxMapSetRef = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpMapSetRef(index)
    elseIf option == oidGpPrimary
        GpIdxPrimary = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpPrimary(index)
    elseIf option == oidGpRemoteActivate
        GpIdxRemoteActivate = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpRemoteActivate(index)
    elseIf option == oidGpTeleport
        GpIdxTeleport = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpTeleport(index)
    elseIf option == oidGpVitals
        GpIdxVitals = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpVitals(index)
    elseIf option == oidGpSneak
        GpIdxSneak = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpSneak(index)
    elseIf option == oidGpPOV
        GpIdxPOV = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpPOV(index)
    elseIf option == oidGpLockEnemy
        GpIdxLockEnemy = index
        SetMenuOptionValue(option, GetGpButtonName(index))
        SkyrimTTS_MCM_Native.SetGpLockEnemy(index)
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
    elseIf option == oidAutoAimToggle
        SetInfoText("Automatic bow aim lock when drawing a bow. Disable to use vanilla bow combat.")
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
    elseIf option == oidGpScanNext
        SetInfoText("Button pressed with LB to scan the next object (or map marker).")
    elseIf option == oidGpScanPrev
        SetInfoText("Button pressed with LB to scan the previous object (or map marker).")
    elseIf option == oidGpScanAnnounce
        SetInfoText("Button pressed with LB to announce the current target (Home equivalent).")
    elseIf option == oidGpMapSetRef
        SetInfoText("Button pressed with LB on the map to set a reference point for distance calculation.")
    elseIf option == oidGpPrimary
        SetInfoText("Button pressed with LB to start autowalk (in game) or fast travel (on the map).")
    elseIf option == oidGpRemoteActivate
        SetInfoText("Button pressed with LB to remotely activate the current scanner target (G key equivalent).")
    elseIf option == oidGpTeleport
        SetInfoText("Button pressed with LB to teleport to the current scanner target.")
    elseIf option == oidGpVitals
        SetInfoText("Button pressed with LB to announce health, magicka, and stamina.")
    elseIf option == oidGpSneak
        SetInfoText("Button pressed with LB to toggle sneaking.")
    elseIf option == oidGpPOV
        SetInfoText("Button pressed with LB to toggle first/third person view.")
    elseIf option == oidGpLockEnemy
        SetInfoText("Button pressed alone (no LB) to lock onto the nearest enemy.")
    endIf
endEvent

; === Resynchronise tous les réglages vers le C++ ===
function SyncAllToNative()
    SkyrimTTS_MCM_Native.SetStealthAnnounce(StealthAnnounce)
    SkyrimTTS_MCM_Native.SetTeleportEnabled(TeleportEnabled)
    SkyrimTTS_MCM_Native.SetAutoAimEnabled(AutoAimEnabled)
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
    ; Gamepad
    SkyrimTTS_MCM_Native.SetGpScanNext(GpIdxScanNext)
    SkyrimTTS_MCM_Native.SetGpScanPrev(GpIdxScanPrev)
    SkyrimTTS_MCM_Native.SetGpScanAnnounce(GpIdxScanAnnounce)
    SkyrimTTS_MCM_Native.SetGpMapSetRef(GpIdxMapSetRef)
    SkyrimTTS_MCM_Native.SetGpPrimary(GpIdxPrimary)
    SkyrimTTS_MCM_Native.SetGpRemoteActivate(GpIdxRemoteActivate)
    SkyrimTTS_MCM_Native.SetGpTeleport(GpIdxTeleport)
    SkyrimTTS_MCM_Native.SetGpVitals(GpIdxVitals)
    SkyrimTTS_MCM_Native.SetGpSneak(GpIdxSneak)
    SkyrimTTS_MCM_Native.SetGpPOV(GpIdxPOV)
    SkyrimTTS_MCM_Native.SetGpLockEnemy(GpIdxLockEnemy)
endFunction
