function InitExtensions()
{
   Menu_mc.InitExtensions();
}
function SetPlatform(aiPlatform, abPS3Switch)
{
   Menu_mc.SetPlatform(aiPlatform,abPS3Switch);
}
function GetInventoryItemList()
{
   return Menu_mc.GetInventoryItemList();
}
_global.gfxExtensions = true;
Shared.GlobalFunc.MaintainTextFormat();
Menu_mc.SetFadedIn();
stop();
