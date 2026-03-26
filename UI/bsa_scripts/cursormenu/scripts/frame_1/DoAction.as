var mouseListener = new Object();
mouseListener.onMouseMove = function()
{
   _root.mc_Cursor._x = _xmouse;
   _root.mc_Cursor._y = _ymouse;
};
Mouse.addListener(mouseListener);
