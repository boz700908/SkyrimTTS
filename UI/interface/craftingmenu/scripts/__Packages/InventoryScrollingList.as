class InventoryScrollingList extends Shared.CenteredScrollingList
{
   var iMaxTextLength;
   function InventoryScrollingList()
   {
      super();
   }
   function SetEntryText(aEntryClip, aEntryObject)
   {
      var _loc5_ = ["None","Equipped","LeftEquip","RightEquip","LeftAndRightEquip"];
      var _loc3_;
      if(aEntryObject.text != undefined)
      {
         _loc3_ = aEntryObject.text;
         if(aEntryObject.soulLVL != undefined)
         {
            _loc3_ = _loc3_ + " (" + aEntryObject.soulLVL + ")";
         }
         if(aEntryObject.count > 1)
         {
            _loc3_ = _loc3_ + " (" + aEntryObject.count.toString() + ")";
         }
         if(_loc3_.length > this.iMaxTextLength)
         {
            _loc3_ = _loc3_.substr(0,this.iMaxTextLength - 3) + "...";
         }
         if(aEntryObject.bestInClass == true)
         {
            _loc3_ += "<img src=\'BestIcon.png\' vspace=\'2\'>";
         }
         aEntryClip.textField.textAutoSize = "shrink";
         aEntryClip.textField.SetText(_loc3_,true);
         if(aEntryObject.negativeEffect == true || aEntryObject.isStealing == true)
         {
            aEntryClip.textField.textColor = aEntryObject.enabled != false ? 16711680 : 8388608;
         }
         else
         {
            aEntryClip.textField.textColor = aEntryObject.enabled != false ? 16777215 : 5000268;
         }
      }
      if(aEntryObject != undefined && aEntryObject.equipState != undefined)
      {
         aEntryClip.EquipIcon.gotoAndStop(_loc5_[aEntryObject.equipState]);
      }
      else
      {
         aEntryClip.EquipIcon.gotoAndStop("None");
      }
      if(aEntryObject.favorite == true && (aEntryObject.equipState == 0 || aEntryObject.equipState == 1))
      {
         aEntryClip.EquipIcon.FavoriteIconInstance.gotoAndStop("On");
      }
      else
      {
         aEntryClip.EquipIcon.FavoriteIconInstance.gotoAndStop("Off");
      }
   }
}
