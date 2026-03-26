function InitExtensions()
{
   SleepWaitMenu_mc.InitExtensions();
}
function SetPlatform(aPlatform, abPS3Switch)
{
   SleepWaitMenu_mc.SetPlatform(aPlatform,abPS3Switch);
}
_global.gfxExtensions = true;
Selection.alwaysEnableArrowKeys = true;
Selection.alwaysEnableKeyboardPress = true;
Selection.disableFocusAutoRelease = true;
Shared.GlobalFunc.MaintainTextFormat();
