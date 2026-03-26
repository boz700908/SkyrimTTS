class FavoritesCenteredList extends Shared.CenteredScrollingList
{
   var EntriesA;
   var __get__selectedEntry;
   var iNumTopHalfEntries;
   var iPlatform;
   var moveSelectionDown;
   var moveSelectionUp;
   function FavoritesCenteredList()
   {
      super();
   }
   function SetEntryText(aEntryClip, aEntryObject)
   {
      var _loc6_ = ["None","Equipped","LeftEquip","RightEquip","LeftAndRightEquip"];
      var _loc4_;
      if(aEntryObject.text != undefined)
      {
         if(aEntryObject.hotkey != undefined && aEntryObject.hotkey != -1)
         {
            this.AppendHotkeyText(aEntryClip.textField,aEntryObject.hotkey,aEntryObject.text);
         }
         else
         {
            aEntryClip.textField.SetText(aEntryObject.text);
         }
         _loc4_ = 35;
         if(aEntryClip.textField.text.length > _loc4_)
         {
            aEntryClip.textField.SetText(aEntryClip.textField.text.substr(0,_loc4_ - 3) + "...");
         }
      }
      else
      {
         aEntryClip.textField.SetText(" ");
      }
      aEntryClip.textField.textAutoSize = "shrink";
      if(aEntryObject != undefined)
      {
         aEntryClip.EquipIcon.gotoAndStop(_loc6_[aEntryObject.equipState]);
      }
      else
      {
         aEntryClip.EquipIcon.gotoAndStop("None");
      }
      var _loc5_;
      if(this.iPlatform == 0)
      {
         aEntryClip._alpha = aEntryObject != this.selectedEntry ? 60 : 100;
      }
      else
      {
         _loc5_ = 8;
         if(aEntryClip.clipIndex < this.iNumTopHalfEntries)
         {
            aEntryClip._alpha = 60 - _loc5_ * (this.iNumTopHalfEntries - aEntryClip.clipIndex);
         }
         else if(aEntryClip.clipIndex > this.iNumTopHalfEntries)
         {
            aEntryClip._alpha = 60 - _loc5_ * (aEntryClip.clipIndex - this.iNumTopHalfEntries);
         }
         else
         {
            aEntryClip._alpha = 100;
         }
      }
   }
   function AppendHotkeyText(atfText, aiHotkey, astrItemName)
   {
      if(aiHotkey >= 0 && aiHotkey <= 7)
      {
         atfText.SetText(aiHotkey + 1 + ". " + astrItemName);
      }
      else
      {
         atfText.SetText("$HK" + aiHotkey);
         atfText.SetText(atfText.text + ". " + astrItemName);
      }
   }
   function InvalidateData()
   {
      this.EntriesA.sort(this.doABCSort);
      super.InvalidateData();
   }
   function doABCSort(aObj1, aObj2)
   {
      if(aObj1.text < aObj2.text)
      {
         return -1;
      }
      if(aObj1.text > aObj2.text)
      {
         return 1;
      }
      return 0;
   }
   function onMouseWheel(delta)
   {
      if(delta == 1)
      {
         this.moveSelectionUp();
      }
      else if(delta == -1)
      {
         this.moveSelectionDown();
      }
   }
}
