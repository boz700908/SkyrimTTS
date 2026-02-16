Scriptname SA_AutoWalkScript extends Quest

; Called by C++ via DispatchMethodCall when user wants to walk to a target
Function OnWalkToTarget(int aiFormID, float afStopDistance)
    Actor player = Game.GetPlayer()
    Form targetForm = Game.GetForm(aiFormID)

    if targetForm == None
        Debug.Trace("SA_AutoWalk: Target form not found for ID " + aiFormID)
        return
    endif

    ObjectReference targetRef = targetForm as ObjectReference
    if targetRef == None
        Debug.Trace("SA_AutoWalk: Target is not an ObjectReference")
        return
    endif

    ; Stop any existing translation first
    player.StopTranslation()

    ; Smoothly move the player toward the target
    ; Speed ~300 units/sec is roughly walking pace
    ; This does NOT block player input
    player.TranslateToRef(targetRef, 300.0)
EndFunction

; Called by C++ when walk is cancelled (movement keys/gamepad) or arrival detected
Function OnStopWalking()
    Actor player = Game.GetPlayer()
    player.StopTranslation()
EndFunction
