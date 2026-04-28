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
ObjectReference TempMarkerRef  ; XMarker cree par PlaceAtMe (mode coords), a supprimer au stop
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

        ; Create a temp XMarker at the target position. Tracker dans TempMarkerRef
        ; pour pouvoir le supprimer au stop sans toucher aux markers de carte vanilla
        ; (qui sont AUSSI des XMarker baseForm 0x10 mais ne doivent JAMAIS etre supprimes).
        ObjectReference tempMarker = PlayerRef.PlaceAtMe(Game.GetForm(0x10), 1, true, true)
        tempMarker.SetPosition(afX, afY, afZ)
        TempMarkerRef = tempMarker
        StartWalkToRef(tempMarker, afStopDistance)
    endIf
EndFunction

; === MOUNTED MODE — Called from C++ when player is on a horse ===
; Reproduit le comportement v1.4 qui marchait : meme dispatch qu'a pied
; (SetPlayerAIDriven sur le joueur) avec en plus un EvaluatePackage sur
; le cheval pour reveiller son AI. Le moteur Skyrim gere le couple
; joueur+cheval : l'IA joueur AI-driven sur un cheval pilote le couple.
; Le 6e parametre aiMountFormID permet de recuperer le cheval pour
; appeler EvaluatePackage dessus (pas d'API Papyrus pour PlayerRef.GetMount).
Function OnWalkToTargetMounted(int aiFormID, float afStopDistance, float afX, float afY, float afZ, int aiMountFormID)
    Debug.Trace("SkyrimTTS:AutoWalkMounted - OnWalkToTargetMounted called: aiFormID=" + aiFormID + " stopDist=" + afStopDistance + " coords=(" + afX + "," + afY + "," + afZ + ") mountID=" + aiMountFormID)
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
        TempMarkerRef = tempMarker
        StartWalkToRefMounted(tempMarker, afStopDistance, mountActor)
    endIf
EndFunction

; === MOUNTED MODE start helper ===
; Reproduit le comportement v1.4 qui marchait : on dispatche le Travel package
; sur le joueur via SetPlayerAIDriven(true) comme en mode a pied. Le moteur
; Skyrim gere le couple joueur+cheval : l'IA joueur AI-driven sur un cheval
; pilote indirectement le cheval.
;
; On ajoute juste un EvaluatePackage sur le cheval pour le reveiller,
; et on flagge MountedMode pour que le stop fasse le bon cleanup.
;
; Pas de Traveler.ForceRefTo(mount) (option A jamais validee empiriquement).
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

    ; Remplir DstMarker avec la cible -> le Travel package lit cet alias
    DstMarker.ForceRefTo(target)
    Debug.Trace("SkyrimTTS:AutoWalkMounted - DstMarker set to " + DstMarker.GetReference())

    ; Reset propre de l'AI joueur avant de la reactiver en AIDriven.
    ; Equivalent Papyrus du SetAIDriven(false)+EvaluatePackage que le C++ v1.4
    ; faisait avant le dispatch et qu'on a supprime cote C++ pour eviter le
    ; crash BSShaderAccumulator. Ici en Papyrus c'est safe (le moteur gere le
    ; timing). Sans ce reset, AIDriven semble silencieusement ignore quand le
    ; joueur est sur un cheval (le contrôle reste au joueur, pas a l'IA).
    Game.SetPlayerAIDriven(false)
    PlayerRef.EvaluatePackage()
    Debug.Trace("SkyrimTTS:AutoWalkMounted - pre-reset AIDriven=false done")

    ; Take away player control, let AI run the Travel package.
    ; Sur un cheval, AIDriven sur le joueur fait que l'IA pilote le couple
    ; joueur+cheval (comportement v1.4 confirme).
    Game.SetPlayerAIDriven(true)
    PlayerRef.EvaluatePackage()
    Debug.Trace("SkyrimTTS:AutoWalkMounted - AI driven, EvaluatePackage called")

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
        ; Orienter le joueur face a la cible (equivalent f4access PlayerRef.SetLookAt)
        ; Actor.SetLookAt est natif Skyrim (verifie dans Actor.psc ligne 604).
        if CurrentTarget != None
            PlayerRef.SetLookAt(CurrentTarget, true)
        endIf
        StopWalkingInternal(false)
        ; Signaler l'arrivee au plugin C++ qui annoncera "Arrived at X".
        ; Le cleanup AIDriven/EvaluatePackage est deja fait par StopWalkingInternal
        ; ci-dessus ; le C++ ne mute plus l'acteur (style f4access).
        ; SendModEvent est natif sur Form (Form.psc ligne 214).
        SendModEvent("SkyrimNVDA_AutoWalkArrived")
    else
        RegisterForSingleUpdate(CheckInterval)
    endIf
EndFunction

; === Internal stop ===
Function StopWalkingInternal(bool abNotify)
    UnregisterForUpdate()
    ; Clear DstMarker so the Travel package deactivates
    DstMarker.Clear()

    ; Cleanup identique pour mounted et a pied : on libere SetPlayerAIDriven et on
    ; reevalue le package du joueur. Le cheval revient a son AI normale au prochain
    ; tick automatique du moteur (pas besoin d'EvaluatePackage explicite : on n'a
    ; plus la ref du cheval ici, et de toute facon c'est l'IA du joueur qui pilotait).
    Game.SetPlayerAIDriven(false)
    PlayerRef.EvaluatePackage()
    if MountedMode
        Debug.Trace("SkyrimTTS:AutoWalkMounted - Stopped")
    else
        Debug.Trace("SkyrimTTS:AutoWalk - Stopped")
    endIf

    ; Supprimer UNIQUEMENT le temp marker qu'on a cree nous-memes via PlaceAtMe (mode coords).
    ; ATTENTION : on ne peut PAS se contenter de tester baseForm.GetFormID() == 0x10
    ; sur CurrentTarget car les markers de carte vanilla (Fort Dragon, villes, donjons)
    ; sont AUSSI des XMarker de baseForm 0x10 — les supprimer les ferait disparaitre
    ; definitivement de la carte. On utilise TempMarkerRef qu'on a explicitement
    ; rempli a la creation du tempMarker, donc on est sur de ne supprimer que ca.
    if TempMarkerRef != None
        TempMarkerRef.Disable()
        TempMarkerRef.Delete()
        Debug.Trace("SkyrimTTS:AutoWalk - Deleted temp XMarker")
        TempMarkerRef = None
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

; === Called from C++ at kPostLoadGame (delayed 3s) to hard-reset autowalk state.
; Covers the "save corruption" case where the game crashed during an autowalk:
; the save file then contains IsWalking=true, MountedMode=true, DstMarker filled,
; Traveler redirected to a mount, and Game.SetPlayerAIDriven(true). On reload,
; this residual state would fire a broken Travel package or cause the next autowalk
; attempt to crash. This function forces a clean slate without trying to clean up
; CurrentTarget (which could be a dangling ref from a deleted XMarker).
Function OnLoadGameReset()
    UnregisterForUpdate()
    DstMarker.Clear()
    Traveler.ForceRefTo(PlayerRef)
    Game.SetPlayerAIDriven(false)
    PlayerRef.EvaluatePackage()
    IsWalking = false
    MountedMode = false
    CurrentTarget = None
    ; Nettoyer un eventuel tempMarker orphelin de la save (mais NE PAS le delete :
    ; la save pourrait avoir une ref obsolete pointant vers un marker de carte vanilla
    ; suite a un ancien bug. On reset juste le pointeur sans toucher a la ref.)
    TempMarkerRef = None
    currentLoopInstance = 0
    Debug.Trace("SkyrimTTS:AutoWalk - OnLoadGameReset: full state cleanup")
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

; === REMOTE ACTIVATE — Called from C++ when player presses G in scanner ===
; Activates the target reference as if the player pressed E next to it.
; Works on items (pickup), containers (open), doors (teleport), activators
; (lever/button/etc.). The C++ side enforces a max distance check before dispatch.
Function OnRemoteActivate(int aiTargetFormID)
    Debug.Trace("SkyrimTTS:RemoteActivate - Called with FormID: " + aiTargetFormID)
    if aiTargetFormID == 0
        Debug.Trace("SkyrimTTS:RemoteActivate - FormID is 0, abort")
        return
    endIf
    Form targetForm = Game.GetForm(aiTargetFormID)
    if targetForm == None
        Debug.Trace("SkyrimTTS:RemoteActivate - GetForm returned None for FormID: " + aiTargetFormID)
        return
    endIf
    ObjectReference targetRef = targetForm as ObjectReference
    if targetRef == None
        Debug.Trace("SkyrimTTS:RemoteActivate - Form is not an ObjectReference: " + aiTargetFormID)
        return
    endIf
    if targetRef.IsDeleted()
        Debug.Trace("SkyrimTTS:RemoteActivate - Target is deleted: " + aiTargetFormID)
        return
    endIf
    Debug.Trace("SkyrimTTS:RemoteActivate - Activating ref: " + aiTargetFormID)
    targetRef.Activate(PlayerRef)
EndFunction
