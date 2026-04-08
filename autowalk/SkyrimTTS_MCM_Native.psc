ScriptName SkyrimTTS_MCM_Native
; Fonctions natives implémentées en C++ par le plugin SKSE SkyrimNVDA.dll
; Ce script sert de pont : le MCM Papyrus appelle ces fonctions,
; et le plugin C++ les exécute immédiatement.

Function SetStealthAnnounce(bool enabled) Global Native
Function SetTeleportEnabled(bool enabled) Global Native
Function SetAimVolume(float volume) Global Native
Function SetKillVolume(float volume) Global Native
Function SetDragonHitVolume(float volume) Global Native
Function SetKeyScan(int keyCode) Global Native
Function SetKeyAnnounce(int keyCode) Global Native
Function SetKeyNextObject(int keyCode) Global Native
Function SetKeyPrevObject(int keyCode) Global Native
Function SetKeySubcategory(int keyCode) Global Native
Function SetKeyTeleport(int keyCode) Global Native
Function SetScanRange(float range) Global Native
Function SetTeleportRange(float range) Global Native
Function SetAutoAimEnabled(bool enabled) Global Native

; Gamepad button configuration (index dans la liste partagée)
Function SetGpScanNext(int idx) Global Native
Function SetGpScanPrev(int idx) Global Native
Function SetGpScanAnnounce(int idx) Global Native
Function SetGpMapSetRef(int idx) Global Native
Function SetGpPrimary(int idx) Global Native
Function SetGpTeleport(int idx) Global Native
Function SetGpVitals(int idx) Global Native
Function SetGpSneak(int idx) Global Native
Function SetGpPOV(int idx) Global Native
Function SetGpLockEnemy(int idx) Global Native
