Scriptname SA_AutoWalkScript extends Quest

; Called by C++ via DispatchMethodCall when user wants to walk to a target
Function OnWalkToTarget(int aiFormID, float afStopDistance)
    Actor player = Game.GetPlayer()
    Form targetForm = Game.GetFormFromID(aiFormID)

    if targetForm == None
        Debug.Trace("SA_AutoWalk: Target form not found for ID " + aiFormID)
        return
    endif

    ObjectReference targetRef = targetForm as ObjectReference
    if targetRef == None
        Debug.Trace("SA_AutoWalk: Target is not an ObjectReference")
        return
    endif

    ; Let the AI take control of player movement
    player.SetPlayerAIDriven(true)

    ; Use the game's navmesh pathfinding to walk to target
    player.PathToReference(targetRef, afStopDistance)
EndFunction

; Called by C++ when walk is cancelled (WASD/Esc) or arrival detected
Function OnStopWalking()
    Actor player = Game.GetPlayer()
    player.SetPlayerAIDriven(false)
EndFunction
