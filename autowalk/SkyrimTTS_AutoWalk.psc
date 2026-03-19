ScriptName SkyrimTTS_AutoWalk extends Quest

; === Properties (set in Creation Kit / VMAD) ===
ReferenceAlias Property DstMarker Auto
ReferenceAlias Property Traveler Auto
Scene Property WalkScene Auto
Actor Property PlayerRef Auto

; === Debug: trace on load to verify script is attached ===
Event OnInit()
    Debug.Trace("SkyrimTTS:AutoWalk - Script loaded!")
    if !self.IsRunning()
        self.Start()
        Debug.Trace("SkyrimTTS:AutoWalk - Quest force-started")
    endIf
    Debug.Notification("AutoWalk charge! Appuie sur K pour tester")
    RegisterForKey(37)  ; 37 = touche K
EndEvent

; === Internal state ===
ObjectReference CurrentTarget
float fStopDistance = 100.0
bool IsWalking = false
float CheckInterval = 0.25

; === Called from C++ via DispatchMethodCall ===
Function OnWalkToTarget(int aiFormID, float afStopDistance)
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
EndFunction

; === Called from C++ to stop walking ===
Function OnStopWalking()
    if IsWalking
        Debug.Trace("SkyrimTTS:AutoWalk - Stop signal received")
        StopWalkingInternal(true)
    endIf
EndFunction

; === Test: appui sur K ===
Event OnKeyDown(int keyCode)
    if keyCode == 37  ; K
        if IsWalking
            Debug.Notification("AutoWalk: arret!")
            Debug.Trace("SkyrimTTS:AutoWalk - Key stop")
            StopWalkingInternal(true)
        else
            Debug.Notification("AutoWalk: marche!")
            Debug.Trace("SkyrimTTS:AutoWalk - Key start")
            ; Place a marker 500 units ahead
            Form xmarkerForm = Game.GetForm(0x10)  ; XMarkerHeading
            if xmarkerForm != None
                ObjectReference marker = PlayerRef.PlaceAtMe(xmarkerForm)
                if marker != None
                    float angle = PlayerRef.GetAngleZ()
                    float offsetX = 500.0 * Math.Sin(angle)
                    float offsetY = 500.0 * Math.Cos(angle)
                    marker.MoveTo(PlayerRef, offsetX, offsetY, 0.0)
                    Debug.Trace("SkyrimTTS:AutoWalk - Marker pos=" + marker.GetPositionX() + "," + marker.GetPositionY() + "," + marker.GetPositionZ())
                    StartWalkToRef(marker, 100.0)
                endIf
            endIf
        endIf
    endIf
EndEvent

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

    ; Combat check handled by C++ (hostile enemies only, not foxes/rabbits)

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
    ; Restore player control
    Game.SetPlayerAIDriven(false)
    PlayerRef.EvaluatePackage()
    CurrentTarget = None
    IsWalking = false
    Debug.Trace("SkyrimTTS:AutoWalk - Stopped")
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

    ; Fill DstMarker alias with the target -> Travel package reads this
    DstMarker.ForceRefTo(target)
    Debug.Trace("SkyrimTTS:AutoWalk - DstMarker set to " + DstMarker.GetReference())

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
