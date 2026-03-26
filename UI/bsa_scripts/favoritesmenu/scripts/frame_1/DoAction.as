function InitExtensions()
{
   MenuHolder.Menu_mc.InitExtensions();
}
function SetPlatform(aiPlatform, abPS3Switch)
{
   MenuHolder.Menu_mc.SetPlatform(aiPlatform,abPS3Switch);
}
function GetSelectedIndex()
{
   return MenuHolder.Menu_mc.selectedIndex;
}
_global.gfxExtensions = true;
Shared.GlobalFunc.MaintainTextFormat();
