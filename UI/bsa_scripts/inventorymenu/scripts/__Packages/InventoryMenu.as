class InventoryMenu extends ItemMenu
{
   var AltButtonArt;
   var BottomBar_mc;
   var ChargeButtonArt;
   var EquipButtonArt;
   var InventoryLists_mc;
   var ItemCardListButtonArt;
   var ItemCard_mc;
   var PrevButtonArt;
   var SaveIndices;
   var ShouldProcessItemsListInput;
   var ToggleMenuFade;
   var bFadedIn;
   var bMenuClosing;
   var bPCControlsReady = true;
   function InventoryMenu()
   {
      super();
      this.bMenuClosing = false;
      this.EquipButtonArt = {PCArt:"M1M2",XBoxArt:"360_LTRT",PS3Art:"PS3_LTRT"};
      this.AltButtonArt = {PCArt:"E",XBoxArt:"360_A",PS3Art:"PS3_A"};
      this.ChargeButtonArt = {PCArt:"T",XBoxArt:"360_RB",PS3Art:"PS3_RB"};
      this.ItemCardListButtonArt = [{PCArt:"Enter",XBoxArt:"360_A",PS3Art:"PS3_A"},{PCArt:"Tab",XBoxArt:"360_B",PS3Art:"PS3_B"}];
      this.PrevButtonArt = undefined;
   }
   function InitExtensions()
   {
      super.InitExtensions();
      Shared.GlobalFunc.AddReverseFunctions();
      this.InventoryLists_mc.ZoomButtonHolderInstance.gotoAndStop(1);
      this.BottomBar_mc.SetButtonArt(this.ChargeButtonArt,3);
      gfx.io.GameDelegate.addCallBack("AttemptEquip",this,"AttemptEquip");
      gfx.io.GameDelegate.addCallBack("DropItem",this,"DropItem");
      gfx.io.GameDelegate.addCallBack("AttemptChargeItem",this,"AttemptChargeItem");
      gfx.io.GameDelegate.addCallBack("ItemRotating",this,"ItemRotating");
      this.ItemCard_mc.addEventListener("itemPress",this,"onItemCardListPress");
   }
   function handleInput(details, pathToFocus)
   {
      if(this.bFadedIn && !pathToFocus[0].handleInput(details,pathToFocus.slice(1)))
      {
         if(Shared.GlobalFunc.IsKeyPressed(details))
         {
            if(this.InventoryLists_mc.currentState == InventoryLists.ONE_PANEL && details.navEquivalent == gfx.ui.NavigationCode.LEFT)
            {
               this.StartMenuFade();
               gfx.io.GameDelegate.call("ShowTweenMenu",[]);
            }
            else if(details.navEquivalent == gfx.ui.NavigationCode.TAB)
            {
               this.StartMenuFade();
               gfx.io.GameDelegate.call("CloseTweenMenu",[]);
            }
         }
      }
      return true;
   }
   function onExitMenuRectClick()
   {
      this.StartMenuFade();
      gfx.io.GameDelegate.call("ShowTweenMenu",[]);
   }
   function StartMenuFade()
   {
      this.InventoryLists_mc.HideCategoriesList();
      this.ToggleMenuFade();
      this.SaveIndices();
      this.bMenuClosing = true;
   }
   function onFadeCompletion()
   {
      if(this.bMenuClosing)
      {
         gfx.io.GameDelegate.call("CloseMenu",[]);
      }
   }
   function onShowItemsList(event)
   {
      super.onShowItemsList(event);
      if(event.index != -1)
      {
         this.UpdateBottomBarButtons();
      }
   }
   function onItemHighlightChange(event)
   {
      super.onItemHighlightChange(event);
      if(event.index != -1)
      {
         this.UpdateBottomBarButtons();
      }
   }
   function UpdateBottomBarButtons()
   {
      this.BottomBar_mc.SetButtonArt(this.AltButtonArt,0);
      switch(this.ItemCard_mc.itemInfo.type)
      {
         case InventoryDefines.ICT_ARMOR:
            this.BottomBar_mc.SetButtonText("$Equip",0);
            break;
         case InventoryDefines.ICT_BOOK:
            this.BottomBar_mc.SetButtonText("$Read",0);
            break;
         case InventoryDefines.ICT_POTION:
            this.BottomBar_mc.SetButtonText("$Use",0);
            break;
         case InventoryDefines.ICT_FOOD:
         case InventoryDefines.ICT_INGREDIENT:
            this.BottomBar_mc.SetButtonText("$Eat",0);
            break;
         default:
            this.BottomBar_mc.SetButtonArt(this.EquipButtonArt,0);
            this.BottomBar_mc.SetButtonText("$Equip",0);
      }
      this.BottomBar_mc.SetButtonText("$Drop",1);
      if((this.InventoryLists_mc.ItemsList.selectedEntry.filterFlag & this.InventoryLists_mc.CategoriesList.entryList[0].flag) != 0)
      {
         this.BottomBar_mc.SetButtonText("$Unfavorite",2);
      }
      else
      {
         this.BottomBar_mc.SetButtonText("$Favorite",2);
      }
      if(this.ItemCard_mc.itemInfo.charge != undefined && this.ItemCard_mc.itemInfo.charge < 100)
      {
         this.BottomBar_mc.SetButtonText("$Charge",3);
      }
      else
      {
         this.BottomBar_mc.SetButtonText("",3);
      }
   }
   function onHideItemsList(event)
   {
      super.onHideItemsList(event);
      this.BottomBar_mc.UpdatePerItemInfo({type:InventoryDefines.ICT_NONE});
   }
   function onItemSelect(event)
   {
      if(event.entry.enabled && event.keyboardOrMouse != 0)
      {
         gfx.io.GameDelegate.call("ItemSelect",[]);
      }
   }
   function AttemptEquip(aiSlot, abCheckOverList)
   {
      var _loc2_ = abCheckOverList == undefined ? true : abCheckOverList;
      if(this.ShouldProcessItemsListInput(_loc2_))
      {
         gfx.io.GameDelegate.call("ItemSelect",[aiSlot]);
      }
   }
   function DropItem()
   {
      if(this.ShouldProcessItemsListInput(false) && this.InventoryLists_mc.ItemsList.selectedEntry != undefined)
      {
         if(this.InventoryLists_mc.ItemsList.selectedEntry.count <= InventoryDefines.QUANTITY_MENU_COUNT_LIMIT)
         {
            this.onQuantityMenuSelect({amount:1});
         }
         else
         {
            this.ItemCard_mc.ShowQuantityMenu(this.InventoryLists_mc.ItemsList.selectedEntry.count);
         }
      }
   }
   function AttemptChargeItem()
   {
      if(this.ShouldProcessItemsListInput(false) && this.ItemCard_mc.itemInfo.charge != undefined && this.ItemCard_mc.itemInfo.charge < 100)
      {
         gfx.io.GameDelegate.call("ShowSoulGemList",[]);
      }
   }
   function onQuantityMenuSelect(event)
   {
      gfx.io.GameDelegate.call("ItemDrop",[event.amount]);
   }
   function onMouseRotationFastClick(aiMouseButton)
   {
      gfx.io.GameDelegate.call("CheckForMouseEquip",[aiMouseButton],this,"AttemptEquip");
   }
   function onItemCardListPress(event)
   {
      gfx.io.GameDelegate.call("ItemCardListCallback",[event.index]);
   }
   function onItemCardSubMenuAction(event)
   {
      super.onItemCardSubMenuAction(event);
      gfx.io.GameDelegate.call("QuantitySliderOpen",[event.opening]);
      if(event.menu == "list")
      {
         if(event.opening == true)
         {
            this.PrevButtonArt = this.BottomBar_mc.GetButtonsArt();
            this.BottomBar_mc.SetButtonsText("$Select","$Cancel");
            this.BottomBar_mc.SetButtonsArt(this.ItemCardListButtonArt);
         }
         else
         {
            this.BottomBar_mc.SetButtonsArt(this.PrevButtonArt);
            this.PrevButtonArt = undefined;
            gfx.io.GameDelegate.call("RequestItemCardInfo",[],this,"UpdateItemCardInfo");
            this.UpdateBottomBarButtons();
         }
      }
   }
   function SetPlatform(aiPlatform, abPS3Switch)
   {
      this.InventoryLists_mc.ZoomButtonHolderInstance.gotoAndStop(1);
      this.InventoryLists_mc.ZoomButtonHolderInstance.ZoomButton._visible = aiPlatform != 0;
      this.InventoryLists_mc.ZoomButtonHolderInstance.ZoomButton.SetPlatform(aiPlatform,abPS3Switch);
      super.SetPlatform(aiPlatform,abPS3Switch);
   }
   function ItemRotating()
   {
      this.InventoryLists_mc.ZoomButtonHolderInstance.PlayForward(this.InventoryLists_mc.ZoomButtonHolderInstance._currentframe);
   }
}
