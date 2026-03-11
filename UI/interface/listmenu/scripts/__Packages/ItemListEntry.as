class ItemListEntry extends skyui.components.list.BasicListEntry
{
   var childIndicator;
   var selectIndicator;
   var textField;
   function ItemListEntry()
   {
      super();
   }
   function setEntry(a_entryObject, a_state)
   {
      var _loc4_ = a_entryObject == a_state.list.selectedEntry;
      var _loc3_ = a_entryObject.hasChildren == 1;
      if(this.childIndicator != undefined)
      {
         this.childIndicator._visible = _loc3_;
      }
      if(this.selectIndicator != undefined)
      {
         this.selectIndicator._visible = _loc4_;
      }
      this.textField.text = a_entryObject.text;
   }
}
