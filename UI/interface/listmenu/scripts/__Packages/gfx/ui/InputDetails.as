class gfx.ui.InputDetails
{
   var code;
   var control;
   var controllerIdx;
   var navEquivalent;
   var skseKeycode;
   var type;
   var value;
   function InputDetails(a_type, a_code, a_value, a_navEquivalent, a_controllerIdx, a_control, a_skseKeycode)
   {
      this.type = a_type;
      this.code = a_code;
      this.value = a_value;
      this.navEquivalent = a_navEquivalent;
      this.controllerIdx = a_controllerIdx;
      this.control = a_control;
      this.skseKeycode = a_skseKeycode;
   }
   function toString()
   {
      return ["[InputDelegate","code=" + this.code,"type=" + this.type,"value=" + this.value,"navEquivalent=" + this.navEquivalent,"controllerIdx=" + this.controllerIdx + ", control=" + this.control + "]"].toString();
   }
}
