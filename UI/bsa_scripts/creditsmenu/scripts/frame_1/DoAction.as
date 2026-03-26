function onCodeObjectInit()
{
   Menu_mc.onCodeObjectInit();
}
function ReleaseCodeObject()
{
   delete CodeObj;
}
function SetPlatform(aiPlatform, abPS3Switch)
{
   BackMouseButton.SetPlatform(aiPlatform,abPS3Switch);
   BackGamepadButton.SetPlatform(aiPlatform,abPS3Switch);
   BackMouseButton._visible = aiPlatform == 0;
   BackGamepadButton._visible = aiPlatform != 0;
}
var CodeObj = new Object();
Shared.GlobalFunc.MaintainTextFormat();
