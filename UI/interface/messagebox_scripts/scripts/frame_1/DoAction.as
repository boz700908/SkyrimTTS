function InitExtensions()
{
}
function SetPlatform(aiPlatform, abPS3Switch)
{
   MessageMenu.SetPlatform(aiPlatform,abPS3Switch);
}
_global.gfxExtensions = true;
Selection.alwaysEnableArrowKeys = true;
Selection.alwaysEnableKeyboardPress = true;
Selection.disableFocusAutoRelease = true;
Shared.GlobalFunc.MaintainTextFormat();
