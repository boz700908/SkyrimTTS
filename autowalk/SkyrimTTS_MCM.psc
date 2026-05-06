ScriptName SkyrimTTS_MCM extends SKI_ConfigBase

; ===========================================================================
; Localisation des libellés du MCM
;
; Tous les libellés visibles ($... préfixés) sont résolus automatiquement par
; SkyUI à partir de Data/Interface/Translations/SkyrimNVDA_<LANGUE>.txt en
; fonction de sLanguage:General dans Skyrim.ini. Le fichier _ENGLISH.txt sert
; aussi de fallback si la langue du joueur n'est pas traduite.
;
; IMPORTANT : ne jamais retirer le ModName "SkyrimNVDA" — c'est ce nom qui
; détermine le préfixe du fichier de traduction. Si on le change, SkyUI
; cherchera SkyrimNVDA_<LANG>.txt avec un autre nom et la traduction casse.
; ===========================================================================

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
    Pages[0] = "$MCM_PageGeneral"
    Pages[1] = "$MCM_PageAudio"
    Pages[2] = "$MCM_PageControls"
    Pages[3] = "$MCM_PageGamepad"
endEvent

; === Version — incrémenter à chaque changement de structure du MCM ===
int function GetVersion()
    return 8
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
    if a_version >= 8
        ; Migration vers libellés traduisibles ($cles). Re-affecter les noms
        ; de pages avec le préfixe $... pour que SkyUI les résolve via
        ; SkyrimNVDA_<LANG>.txt. Les noms anglais en dur de v5/7 deviennent
        ; obsolètes.
        Pages = new string[4]
        Pages[0] = "$MCM_PageGeneral"
        Pages[1] = "$MCM_PageAudio"
        Pages[2] = "$MCM_PageControls"
        Pages[3] = "$MCM_PageGamepad"
    endIf
endEvent

event OnGameReload()
    parent.OnGameReload()
    SyncAllToNative()
endEvent

; === Liste des noms de boutons manette ===
; Les noms physiques (D-pad Up, A, B, X, Y, LS click...) ne sont PAS traduits :
; ils correspondent aux gravures du contrôleur. Un joueur français regarde sa
; manette et voit "A", pas "A traduit". On garde donc l'anglais.
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

    ; Comparaison avec les libellés traduits (résolus par SkyUI au runtime).
    ; "page" reçoit déjà la valeur traduite — la comparaison fonctionne car
    ; on a stocké les mêmes $cles dans Pages[].
    if page == "" || page == "$MCM_PageGeneral"
        AddHeaderOption("$MCM_HeaderScanner")
        oidTeleportToggle = AddToggleOption("$MCM_TeleportEnabled", TeleportEnabled)
        oidScanRange = AddSliderOption("$MCM_ScanRange", ScanRange, "{0}")
        oidTeleportRange = AddSliderOption("$MCM_TeleportRange", TeleportRange, "{0}")

        AddEmptyOption()
        AddHeaderOption("$MCM_HeaderCombat")
        oidAutoAimToggle = AddToggleOption("$MCM_BowAutoAim", AutoAimEnabled)

        AddEmptyOption()
        AddHeaderOption("$MCM_HeaderAnnouncements")
        oidStealthToggle = AddToggleOption("$MCM_StealthAnnouncements", StealthAnnounce)

        AddEmptyOption()
        oidResetAll = AddTextOption("$MCM_ResetAll", "")

    elseIf page == "$MCM_PageAudio"
        AddHeaderOption("$MCM_HeaderSoundVolumes")
        oidAimVolume = AddSliderOption("$MCM_AimSound", AimVolume, "{2}")
        oidKillVolume = AddSliderOption("$MCM_KillSound", KillVolume, "{2}")
        oidDragonHitVolume = AddSliderOption("$MCM_DragonHitSound", DragonHitVolume, "{2}")

    elseIf page == "$MCM_PageControls"
        AddHeaderOption("$MCM_HeaderScannerKeys")
        oidKeyScan = AddKeyMapOption("$MCM_KeyScan", KeyScan)
        oidKeyAnnounce = AddKeyMapOption("$MCM_KeyAnnounce", KeyAnnounce)
        oidKeyNextObject = AddKeyMapOption("$MCM_KeyNextObject", KeyNextObject)
        oidKeyPrevObject = AddKeyMapOption("$MCM_KeyPrevObject", KeyPrevObject)
        oidKeySubcategory = AddKeyMapOption("$MCM_KeySubcategory", KeySubcategory)
        oidKeyTeleport = AddKeyMapOption("$MCM_KeyTeleport", KeyTeleport)

    elseIf page == "$MCM_PageGamepad"
        AddHeaderOption("$MCM_HeaderLBCombos")
        oidGpScanNext = AddMenuOption("$MCM_GpScanNext", GetGpButtonName(GpIdxScanNext))
        oidGpScanPrev = AddMenuOption("$MCM_GpScanPrev", GetGpButtonName(GpIdxScanPrev))
        oidGpScanAnnounce = AddMenuOption("$MCM_GpScanAnnounce", GetGpButtonName(GpIdxScanAnnounce))
        oidGpPrimary = AddMenuOption("$MCM_GpPrimary", GetGpButtonName(GpIdxPrimary))
        oidGpRemoteActivate = AddMenuOption("$MCM_GpRemoteActivate", GetGpButtonName(GpIdxRemoteActivate))
        oidGpTeleport = AddMenuOption("$MCM_GpTeleport", GetGpButtonName(GpIdxTeleport))
        oidGpVitals = AddMenuOption("$MCM_GpVitals", GetGpButtonName(GpIdxVitals))
        oidGpMapSetRef = AddMenuOption("$MCM_GpMapSetRef", GetGpButtonName(GpIdxMapSetRef))

        AddEmptyOption()
        AddHeaderOption("$MCM_HeaderOtherLB")
        oidGpSneak = AddMenuOption("$MCM_GpSneak", GetGpButtonName(GpIdxSneak))
        oidGpPOV = AddMenuOption("$MCM_GpLockOn", GetGpButtonName(GpIdxPOV))

        AddEmptyOption()
        AddHeaderOption("$MCM_HeaderStandalone")
        oidGpLockEnemy = AddMenuOption("$MCM_GpLockEnemy", GetGpButtonName(GpIdxLockEnemy))
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
        bool confirm = ShowMessage("$MCM_ResetConfirm")
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
        SetInfoText("$MCM_Info_StealthAnnouncements")
    elseIf option == oidTeleportToggle
        SetInfoText("$MCM_Info_TeleportEnabled")
    elseIf option == oidAutoAimToggle
        SetInfoText("$MCM_Info_BowAutoAim")
    elseIf option == oidAimVolume
        SetInfoText("$MCM_Info_AimSound")
    elseIf option == oidKillVolume
        SetInfoText("$MCM_Info_KillSound")
    elseIf option == oidDragonHitVolume
        SetInfoText("$MCM_Info_DragonHitSound")
    elseIf option == oidScanRange
        SetInfoText("$MCM_Info_ScanRange")
    elseIf option == oidTeleportRange
        SetInfoText("$MCM_Info_TeleportRange")
    elseIf option == oidKeyScan
        SetInfoText("$MCM_Info_KeyScan")
    elseIf option == oidKeyAnnounce
        SetInfoText("$MCM_Info_KeyAnnounce")
    elseIf option == oidKeyNextObject
        SetInfoText("$MCM_Info_KeyNextObject")
    elseIf option == oidKeyPrevObject
        SetInfoText("$MCM_Info_KeyPrevObject")
    elseIf option == oidKeySubcategory
        SetInfoText("$MCM_Info_KeySubcategory")
    elseIf option == oidKeyTeleport
        SetInfoText("$MCM_Info_KeyTeleport")
    elseIf option == oidResetAll
        SetInfoText("$MCM_Info_ResetAll")
    elseIf option == oidGpScanNext
        SetInfoText("$MCM_Info_GpScanNext")
    elseIf option == oidGpScanPrev
        SetInfoText("$MCM_Info_GpScanPrev")
    elseIf option == oidGpScanAnnounce
        SetInfoText("$MCM_Info_GpScanAnnounce")
    elseIf option == oidGpMapSetRef
        SetInfoText("$MCM_Info_GpMapSetRef")
    elseIf option == oidGpPrimary
        SetInfoText("$MCM_Info_GpPrimary")
    elseIf option == oidGpRemoteActivate
        SetInfoText("$MCM_Info_GpRemoteActivate")
    elseIf option == oidGpTeleport
        SetInfoText("$MCM_Info_GpTeleport")
    elseIf option == oidGpVitals
        SetInfoText("$MCM_Info_GpVitals")
    elseIf option == oidGpSneak
        SetInfoText("$MCM_Info_GpSneak")
    elseIf option == oidGpPOV
        SetInfoText("$MCM_Info_GpLockOn")
    elseIf option == oidGpLockEnemy
        SetInfoText("$MCM_Info_GpLockEnemy")
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
