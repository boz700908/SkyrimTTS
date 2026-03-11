class skyui.components.list.BasicListEntry extends MovieClip
{
   var itemIndex;
   var onPress;
   var onPressAux;
   var onRollOut;
   var onRollOver;
   function BasicListEntry()
   {
      super();
      this.onRollOver = function()
      {
         var _loc2_ = this._parent;
         if(this.itemIndex != undefined && this.enabled)
         {
            _loc2_.onItemRollOver(this.itemIndex);
         }
      };
      this.onRollOut = function()
      {
         var _loc2_ = this._parent;
         if(this.itemIndex != undefined && this.enabled)
         {
            _loc2_.onItemRollOut(this.itemIndex);
         }
      };
      this.onPress = function(a_mouseIndex, a_keyboardOrMouse)
      {
         var _loc2_ = this._parent;
         if(this.itemIndex != undefined && this.enabled)
         {
            _loc2_.onItemPress(this.itemIndex,a_keyboardOrMouse);
         }
      };
      this.onPressAux = function(a_mouseIndex, a_keyboardOrMouse, a_buttonIndex)
      {
         var _loc2_ = this._parent;
         if(this.itemIndex != undefined && this.enabled)
         {
            _loc2_.onItemPressAux(this.itemIndex,a_keyboardOrMouse,a_buttonIndex);
         }
      };
   }
   function initialize(a_index, a_state)
   {
   }
   function setEntry(a_entryObject, a_state)
   {
   }
}
