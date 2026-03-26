ScriptName SkyrimTTS_MQ105Fix extends ReferenceAlias

; Script d'accessibilite pour MQ105 (La technique de la voix)
; Detecte les cris du joueur et avance la quete automatiquement
; car le joueur aveugle ne peut pas viser les cibles fantomes.

int ShoutCount = 0

Event OnInit()
    RegisterForAnimationEvent(GetActorReference(), "shoutRelease")
    Debug.Trace("SkyrimTTS:MQ105 - Initialized, listening for shouts")
EndEvent

Event OnPlayerLoadGame()
    RegisterForAnimationEvent(GetActorReference(), "shoutRelease")
    Debug.Trace("SkyrimTTS:MQ105 - Re-registered after load")
EndEvent

Event OnAnimationEvent(ObjectReference akSource, string asEventName)
    if asEventName != "shoutRelease"
        return
    endIf

    ; Verifier que MQ105 est au bon stage
    Quest kMQ105 = Game.GetForm(0x0004E4E6) as Quest
    if kMQ105 == None
        Debug.Trace("SkyrimTTS:MQ105 - Quest not found!")
        return
    endIf

    if !kMQ105.GetStageDone(85) || kMQ105.GetStageDone(90)
        return
    endIf

    ShoutCount += 1
    Debug.Notification("Cri " + ShoutCount + "/3")
    Debug.Trace("SkyrimTTS:MQ105 - Shout " + ShoutCount + "/3")

    if ShoutCount >= 3
        kMQ105.SetStage(90)
        ShoutCount = 0
        Debug.Notification("Demonstration reussie !")
        Debug.Trace("SkyrimTTS:MQ105 - Stage 90 set, quest advanced!")
    endIf
EndEvent
