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

    if PlayerRef.IsInCombat()
        Debug.Trace("SkyrimTTS:AutoWalk - Combat detected, stopping")
        StopWalkingInternal(true)
        return
    endIf

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
    ; Restore player control and normal speed
    PlayerRef.SetActorValue("SpeedMult", 100.0)
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

    ; Boost speed during autowalk
    PlayerRef.SetActorValue("SpeedMult", 250.0)

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
