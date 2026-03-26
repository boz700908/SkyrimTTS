function InitExtensions()
{
}
function SetPlatform(aPlatformNum, abPS3Switch)
{
   WorldMap.SetPlatform(aPlatformNum,abPS3Switch);
}
_global.gfxExtensions = true;
Shared.GlobalFunc.MaintainTextFormat();
Shared.GlobalFunc.SetLockFunction();
var WorldMap = new Map.MapMenu();
SetPlatform(0);
