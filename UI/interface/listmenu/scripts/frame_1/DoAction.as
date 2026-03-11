function InitExtensions()
{
   listMenu.InitExtensions();
}
function SetPlatform(aiPlatform, abPS3Switch)
{
   listMenu.SetPlatform(aiPlatform,abPS3Switch);
}
function handleInput(details, pathToFocus)
{
   return listMenu.handleInput(details,pathToFocus);
}
_global.gfxExtensions = true;
Shared.GlobalFunc.MaintainTextFormat();
stop();
