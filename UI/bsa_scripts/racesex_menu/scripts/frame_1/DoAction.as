function InitExtensions()
{
   RaceSexMenuBaseInstance.RaceSexPanelsInstance.InitExtensions();
}
function SetPlatform(aiPlatform, abPS3Switch)
{
   RaceSexMenuBaseInstance.RaceSexPanelsInstance.SetPlatform(aiPlatform,abPS3Switch);
}
function FaceChanges()
{
   gfx.io.GameDelegate.call("ChangeHeadPreset",[1,0]);
   gfx.io.GameDelegate.call("ChangeTintingMask",[0,1]);
   gfx.io.GameDelegate.call("ChangeWeight",[0.3,2]);
   gfx.io.GameDelegate.call("ChangeFaceDetails",[0,3]);
   gfx.io.GameDelegate.call("ChangeHeadPart",[0,6]);
   gfx.io.GameDelegate.call("ChangePreset",[1,24]);
   gfx.io.GameDelegate.call("ChangeDoubleMorph",[0.9,26]);
   gfx.io.GameDelegate.call("ChangeDoubleMorph",[0.18,27]);
   gfx.io.GameDelegate.call("ChangeDoubleMorph",[0.8,31]);
   gfx.io.GameDelegate.call("ChangeTintingMask",[0,32]);
   gfx.io.GameDelegate.call("ChangePreset",[0,12]);
   gfx.io.GameDelegate.call("ChangeHeadPart",[3,13]);
   gfx.io.GameDelegate.call("ChangeDoubleMorph",[-0.8,15]);
   gfx.io.GameDelegate.call("ChangeDoubleMorph",[0.6,14]);
   gfx.io.GameDelegate.call("ChangeDoubleMorph",[-0.26,42]);
   gfx.io.GameDelegate.call("ChangeHeadPart",[41,10]);
   gfx.io.GameDelegate.call("ChangeHairColorPreset",[3,11]);
   gfx.io.GameDelegate.call("ChangePreset",[1,39]);
   gfx.io.GameDelegate.call("ChangeDoubleMorph",[-0.6,31]);
   gfx.io.GameDelegate.call("ChangeHeadPart",[0,20]);
   gfx.io.GameDelegate.call("ChangeName",["Big E"]);
}
function handleInput(details, pathToFocus)
{
   pathToFocus[0].handleInput(details,pathToFocus.slice(1));
   Buttons[details.navEquivalent] = Shared.GlobalFunc.IsKeyPressed(details,true);
   if(Buttons[gfx.ui.NavigationCode.TAB] && Buttons[gfx.ui.NavigationCode.PAGE_DOWN] && Buttons[gfx.ui.NavigationCode.PAGE_UP] || Buttons[gfx.ui.NavigationCode.GAMEPAD_R1] && Buttons[gfx.ui.NavigationCode.GAMEPAD_L1] && Buttons[gfx.ui.NavigationCode.GAMEPAD_B] || Buttons[gfx.ui.NavigationCode.GAMEPAD_R1] && Buttons[gfx.ui.NavigationCode.GAMEPAD_L1] && Buttons[gfx.ui.NavigationCode.TAB])
   {
      count++;
   }
   if(count == 4)
   {
      gfx.io.GameDelegate.call("ChangeRace",[6,0]);
      setInterval(_root.FaceChanges,5000);
   }
   return true;
}
_global.gfxExtensions = true;
var count = 0;
var Buttons = new Object();
stop();
