ScriptName SkyrimTTS_AutoWalk extends Quest

; === Properties (set in Creation Kit / VMAD) ===
ReferenceAlias Property DstMarker Auto
ReferenceAlias Property Traveler Auto
Scene Property WalkScene Auto
Actor Property PlayerRef Auto

Event OnInit()
    Debug.Trace("SkyrimTTS:AutoWalk - Script loaded!")
    if !self.IsRunning()
        self.Start()
        Debug.Trace("SkyrimTTS:AutoWalk - Quest force-started")
    endIf
EndEvent

; === Internal state ===
ObjectReference CurrentTarget
float fStopDistance = 100.0
bool IsWalking = false
bool MountedMode = false   ; true si le walk en cours utilise le mode "mounted"
float CheckInterval = 0.25

; === Called from C++ via DispatchMethodCall ===
; For normal FormIDs (< 0xFF000000), pass formID only
; For dynamic FormIDs (FF*), pass formID=0 + x, y, z coordinates
Function OnWalkToTarget(int aiFormID, float afStopDistance, float afX = 0.0, float afY = 0.0, float afZ = 0.0)
    if aiFormID != 0
        ; Normal case: lookup by FormID
        Form targetForm = Game.GetForm(aiFormID)
        if targetForm == None
            Debug.Trace("SkyrimTTS:AutoWalk - Invalid FormID: " + aiFormID)
            return
        endIf

        ObjectReference targetRef = targetForm as ObjectReference
        if targetRef == None
            Debug.Trace("SkyrimTTS:AutoWalk - Not an ObjectReference: " + aiFormID)
            return
        endIf

        if IsWalking
            StopWalkingInternal(false)
        endIf

        fStopDistance = afStopDistance
        StartWalkToRef(targetRef, afStopDistance)
    else
        ; Dynamic object: create XMarker at coordinates and walk to it
        Debug.Trace("SkyrimTTS:AutoWalk - Walking to coordinates: " + afX + ", " + afY + ", " + afZ)

        if IsWalking
            StopWalkingInternal(false)
        endIf

        fStopDistance = afStopDistance

        ; Create a temp XMarker at the target position
        ObjectReference tempMarker = PlayerRef.PlaceAtMe(Game.GetForm(0x10), 1, true, true)
        tempMarker.SetPosition(afX, afY, afZ)
        StartWalkToRef(tempMarker, afStopDistance)
    endIf
EndFunction

; === MOUNTED MODE — Called from C++ when player is on a horse ===
; OPTION B : on redirige l'alias Traveler vers le CHEVAL au lieu du joueur.
; Notre Travel package est attaché au Traveler alias (via ALPC dans l'ESP), donc
; en changeant l'alias on transfère l'exécution du package au cheval. Le cheval est
; un Actor avec son propre AI complet, il peut exécuter un Travel package nativement.
; Pas besoin de Game.SetPlayerAIDriven : c'est le cheval qui agit, pas le joueur.
Function OnWalkToTargetMounted(int aiFormID, float afStopDistance, float afX, float afY, float afZ, int aiMountFormID)
    ; Récupérer le cheval
    Form mountForm = Game.GetForm(aiMountFormID)
    if mountForm == None
        Debug.Trace("SkyrimTTS:AutoWalkMounted - Invalid mount FormID: " + aiMountFormID)
        return
    endIf
    Actor mountActor = mountForm as Actor
    if mountActor == None
        Debug.Trace("SkyrimTTS:AutoWalkMounted - Mount form is not an Actor: " + aiMountFormID)
        return
    endIf
    Debug.Trace("SkyrimTTS:AutoWalkMounted - Mount actor resolved: " + mountActor)

    if aiFormID != 0
        Form targetForm = Game.GetForm(aiFormID)
        if targetForm == None
            Debug.Trace("SkyrimTTS:AutoWalkMounted - Invalid target FormID: " + aiFormID)
            return
        endIf

        ObjectReference targetRef = targetForm as ObjectReference
        if targetRef == None
            Debug.Trace("SkyrimTTS:AutoWalkMounted - Not an ObjectReference: " + aiFormID)
            return
        endIf

        if IsWalking
            StopWalkingInternal(false)
        endIf

        fStopDistance = afStopDistance
        StartWalkToRefMounted(targetRef, afStopDistance, mountActor)
    else
        Debug.Trace("SkyrimTTS:AutoWalkMounted - Walking to coordinates: " + afX + ", " + afY + ", " + afZ)

        if IsWalking
            StopWalkingInternal(false)
        endIf

        fStopDistance = afStopDistance

        ObjectReference tempMarker = PlayerRef.PlaceAtMe(Game.GetForm(0x10), 1, true, true)
        tempMarker.SetPosition(afX, afY, afZ)
        StartWalkToRefMounted(tempMarker, afStopDistance, mountActor)
    endIf
EndFunction

; === MOUNTED MODE start helper ===
Function StartWalkToRefMounted(ObjectReference target, float stopDist, Actor mountActor)
    if IsWalking
        StopWalkingInternal(false)
    endIf

    fStopDistance = stopDist
    CurrentTarget = target
    MountedMode = true

    float dist = PlayerRef.GetDistance(CurrentTarget)
    Debug.Trace("SkyrimTTS:AutoWalkMounted - StartWalkToRefMounted, distance: " + dist)

    if dist <= fStopDistance
        Debug.Trace("SkyrimTTS:AutoWalkMounted - Already within range: " + dist)
        MountedMode = false
        return
    endIf

    ; Clear sticky combat state on both the player AND the mount. Same
    ; reason as foot mode: stuck IsInCombat flag forces combat stance
    ; which slows movement below run speed.
    PlayerRef.StopCombat()
    PlayerRef.StopCombatAlarm()
    mountActor.StopCombat()
    mountActor.StopCombatAlarm()

    ; OPTION B — Étape clé : rediriger l'alias Traveler vers le CHEVAL.
    ; Notre Travel package est attaché à l'alias Traveler (via ALPC dans l'ESP).
    ; En changeant la ref de l'alias, le package va s'exécuter sur le cheval
    ; au lieu du joueur. Le cheval est un Actor avec son propre AI, il peut
    ; exécuter le package nativement sans avoir besoin d'AIDriven.
    Traveler.ForceRefTo(mountActor)
    Debug.Trace("SkyrimTTS:AutoWalkMounted - Traveler alias redirected to mount: " + Traveler.GetReference())

    ; Remplir DstMarker comme d'habitude (la cible ne change pas)
    DstMarker.ForceRefTo(target)
    Debug.Trace("SkyrimTTS:AutoWalkMounted - DstMarker set to " + DstMarker.GetReference())

    ; Réveiller l'AI du cheval pour qu'il prenne en compte son nouveau package
    mountActor.EvaluatePackage()
    Debug.Trace("SkyrimTTS:AutoWalkMounted - mount.EvaluatePackage called")

    ; PAS de Game.SetPlayerAIDriven : on n'a pas besoin de driver le joueur,
    ; c'est le cheval qui est maintenant l'acteur du package Travel.

    IsWalking = true
    RegisterForSingleUpdate(CheckInterval)
EndFunction

; === Called from C++ to stop walking ===
Function OnStopWalking()
    if IsWalking
        Debug.Trace("SkyrimTTS:AutoWalk - Stop signal received")
        StopWalkingInternal(true)
    endIf
EndFunction

; === Update: check if we arrived ===
Event OnUpdate()
    if IsWalking
        CheckArrival()
    endIf
EndEvent

Function CheckArrival()
    if CurrentTarget == None || CurrentTarget.IsDeleted()
        Debug.Trace("SkyrimTTS:AutoWalk - Target became invalid")
        StopWalkingInternal(true)
        return
    endIf

    ; Note : on ne vérifie PAS IsInCombat(). Le flag de combat Skyrim est parfois
    ; "collé" après un combat (reste à true même quand tous les ennemis sont morts),
    ; ce qui empêchait toute nouvelle autowalk de démarrer. Si un vrai combat
    ; survient, le moteur sort naturellement le joueur de l'AI driven, et le
    ; monitor C++ voit le mouvement input (l'attaque du joueur) → stoppe proprement.

    float dist = PlayerRef.GetDistance(CurrentTarget)
    Debug.Trace("SkyrimTTS:AutoWalk - Check: dist=" + dist)
    if dist <= fStopDistance + 20.0
        Debug.Trace("SkyrimTTS:AutoWalk - Arrived, distance: " + dist)
        StopWalkingInternal(false)
    else
        RegisterForSingleUpdate(CheckInterval)
    endIf
EndFunction

; === Internal stop ===
Function StopWalkingInternal(bool abNotify)
    UnregisterForUpdate()
    ; Clear DstMarker so the Travel package deactivates
    DstMarker.Clear()

    if MountedMode
        ; OPTION B : restaurer l'alias Traveler vers le PlayerRef pour que les
        ; futurs autowalks à pied fonctionnent correctement.
        ; On capture aussi la ref actuelle (le cheval) pour pouvoir EvaluatePackage
        ; dessus afin qu'il revienne à son comportement normal.
        Actor mountActor = Traveler.GetReference() as Actor
        Traveler.ForceRefTo(PlayerRef)
        Debug.Trace("SkyrimTTS:AutoWalkMounted - Traveler restored to PlayerRef")

        if mountActor != None
            mountActor.EvaluatePackage()
            Debug.Trace("SkyrimTTS:AutoWalkMounted - mount.EvaluatePackage called (cleanup)")
        endIf

        ; Pas besoin de SetPlayerAIDriven(false) puisqu'on ne l'a jamais activé en mounted.
        PlayerRef.EvaluatePackage()
        Debug.Trace("SkyrimTTS:AutoWalkMounted - Stopped")
    else
        ; Mode à pied : restaurer le contrôle joueur.
        ; Note : on ne touche PAS à SpeedMult — on ne l'a jamais boosté, donc rien à restaurer.
        Game.SetPlayerAIDriven(false)
        PlayerRef.EvaluatePackage()
        Debug.Trace("SkyrimTTS:AutoWalk - Stopped")
    endIf

    ; Supprimer le temp marker si c'est un XMarker créé par PlaceAtMe (mode coordonnées).
    ; Sans ça, les markers s'accumulent dans la save à chaque autowalk et finissent
    ; par surcharger le scene graph → crash progressif.
    if CurrentTarget != None && CurrentTarget != PlayerRef
        Form baseForm = CurrentTarget.GetBaseObject()
        if baseForm != None && baseForm.GetFormID() == 0x10
            CurrentTarget.Disable()
            CurrentTarget.Delete()
            Debug.Trace("SkyrimTTS:AutoWalk - Deleted temp XMarker")
        endIf
    endIf

    CurrentTarget = None
    IsWalking = false
    MountedMode = false
EndFunction

; === Start walking: fill DstMarker alias, let AI package handle pathfinding ===
Function StartWalkToRef(ObjectReference target, float stopDist)
    if IsWalking
        StopWalkingInternal(false)
    endIf

    fStopDistance = stopDist
    CurrentTarget = target

    float dist = PlayerRef.GetDistance(CurrentTarget)
    Debug.Trace("SkyrimTTS:AutoWalk - StartWalkToRef, distance: " + dist)

    if dist <= fStopDistance
        Debug.Trace("SkyrimTTS:AutoWalk - Already within range: " + dist)
        return
    endIf

    ; Clear any sticky combat state. After a combat ends, Skyrim sometimes
    ; keeps IsInCombat=true for 30+s (until music fully fades, until dead
    ; enemies are "forgotten"). During that time the player is forced into
    ; combat stance which walks slower than running. These two calls clear
    ; the stuck flag. If real enemies are around they re-engage instantly,
    ; so no combat is actually disrupted.
    PlayerRef.StopCombat()
    PlayerRef.StopCombatAlarm()

    ; Fill DstMarker alias with the target -> Travel package reads this
    DstMarker.ForceRefTo(target)
    Debug.Trace("SkyrimTTS:AutoWalk - DstMarker set to " + DstMarker.GetReference())

    ; Note : on ne touche PAS à SpeedMult — l'IA pathfinding gère la vitesse naturellement.
    ; Forcer SpeedMult à 250 causait un mouvement à 2.5x la vitesse normale (bug v1.3.1).

    ; Take away player control, let AI run the Travel package
    Game.SetPlayerAIDriven(true)
    PlayerRef.EvaluatePackage()
    Debug.Trace("SkyrimTTS:AutoWalk - AI driven, EvaluatePackage called")

    IsWalking = true
    RegisterForSingleUpdate(CheckInterval)
EndFunction

; === Utility ===
Function ForceStop()
    if IsWalking
        StopWalkingInternal(true)
    endIf
EndFunction

; === Called from C++ via DispatchMethodCall for fast travel ===
Function OnFastTravel(int aiFormID)
    Form targetForm = Game.GetForm(aiFormID)
    if targetForm == None
        Debug.Trace("SkyrimTTS:FastTravel - Invalid FormID: " + aiFormID)
        return
    endIf

    ObjectReference targetRef = targetForm as ObjectReference
    if targetRef == None
        Debug.Trace("SkyrimTTS:FastTravel - Not an ObjectReference: " + aiFormID)
        return
    endIf

    ; Stop autowalk if active
    if IsWalking
        StopWalkingInternal(false)
    endIf

    Debug.Trace("SkyrimTTS:FastTravel - Traveling to FormID: " + aiFormID)
    Game.FastTravel(targetRef)
EndFunction

; === Sound playback, called from C++ via DispatchMethodCall ===
int currentLoopInstance = 0

; Play a one-shot sound at the player's position
; aiLocalFormID = local FormID without load order (e.g. 0x806, 0x807, 0x808)
; afVolume = volume multiplier (0.0 to 1.0, default 1.0)
Function OnPlaySound(int aiLocalFormID, float afVolume = 1.0)
    Form foundForm = Game.GetFormFromFile(aiLocalFormID, "SkyrimTTS_AutoWalk.esp")
    if foundForm == None
        Debug.Trace("SkyrimTTS:Sound - Form not found for local ID: " + aiLocalFormID)
        return
    endIf
    Sound soundObj = foundForm as Sound
    if soundObj == None
        Debug.Trace("SkyrimTTS:Sound - Form is not a Sound: " + aiLocalFormID)
        return
    endIf
    int instance = soundObj.Play(PlayerRef as ObjectReference)
    if afVolume < 1.0 && instance != 0
        Sound.SetInstanceVolume(instance, afVolume)
    endIf
    Debug.Trace("SkyrimTTS:Sound - Playing local ID: " + aiLocalFormID + " vol: " + afVolume)
EndFunction

; Play a looping sound (e.g. aim feedback), stops any previous loop
; aiLocalFormID = local FormID without load order (e.g. 0x806)
; afVolume = volume multiplier (0.0 to 1.0, default 1.0)
Function OnPlayLoopSound(int aiLocalFormID, float afVolume = 1.0)
    OnStopLoopSound()
    Form foundForm = Game.GetFormFromFile(aiLocalFormID, "SkyrimTTS_AutoWalk.esp")
    if foundForm == None
        Debug.Trace("SkyrimTTS:Sound - Loop form not found for local ID: " + aiLocalFormID)
        return
    endIf
    Sound soundObj = foundForm as Sound
    if soundObj == None
        Debug.Trace("SkyrimTTS:Sound - Loop form is not a Sound: " + aiLocalFormID)
        return
    endIf
    currentLoopInstance = soundObj.Play(PlayerRef as ObjectReference)
    if afVolume < 1.0 && currentLoopInstance != 0
        Sound.SetInstanceVolume(currentLoopInstance, afVolume)
    endIf
    Debug.Trace("SkyrimTTS:Sound - Looping local ID: " + aiLocalFormID + " vol: " + afVolume + " instance: " + currentLoopInstance)
EndFunction

; Stop the current looping sound
Function OnStopLoopSound()
    if currentLoopInstance != 0
        Sound.StopInstance(currentLoopInstance)
        Debug.Trace("SkyrimTTS:Sound - Stopped loop instance: " + currentLoopInstance)
        currentLoopInstance = 0
    endIf
EndFunction
