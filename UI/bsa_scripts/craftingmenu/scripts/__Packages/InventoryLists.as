class InventoryLists extends MovieClip
{
   var CategoriesListHolder;
   var ItemsListHolder;
   var _CategoriesList;
   var _ItemsList;
   var dispatchEvent;
   var iCurrCategoryIndex;
   var iCurrentState;
   var iPlatform;
   var strHideItemsCode;
   var strShowItemsCode;
   static var NO_PANELS = 0;
   static var ONE_PANEL = 1;
   static var TWO_PANELS = 2;
   static var TRANSITIONING_TO_NO_PANELS = 3;
   static var TRANSITIONING_TO_ONE_PANEL = 4;
   static var TRANSITIONING_TO_TWO_PANELS = 5;
   function InventoryLists()
   {
      super();
      this._CategoriesList = this.CategoriesListHolder.List_mc;
      this._ItemsList = this.ItemsListHolder.List_mc;
      gfx.events.EventDispatcher.initialize(this);
      this.gotoAndStop("NoPanels");
      gfx.io.GameDelegate.addCallBack("SetCategoriesList",this,"SetCategoriesList");
      gfx.io.GameDelegate.addCallBack("InvalidateListData",this,"InvalidateListData");
      this.strHideItemsCode = gfx.ui.NavigationCode.LEFT;
      this.strShowItemsCode = gfx.ui.NavigationCode.RIGHT;
   }
   function onLoad()
   {
      this._CategoriesList.addEventListener("itemPress",this,"onCategoriesItemPress");
      this._CategoriesList.addEventListener("listPress",this,"onCategoriesListPress");
      this._CategoriesList.addEventListener("listMovedUp",this,"onCategoriesListMoveUp");
      this._CategoriesList.addEventListener("listMovedDown",this,"onCategoriesListMoveDown");
      this._CategoriesList.addEventListener("selectionChange",this,"onCategoriesListMouseSelectionChange");
      this._CategoriesList.numTopHalfEntries = 7;
      this._CategoriesList.filterer.itemFilter = 1;
      this._ItemsList.maxTextLength = 35;
      this._ItemsList.disableInput = true;
      this._ItemsList.addEventListener("listMovedUp",this,"onItemsListMoveUp");
      this._ItemsList.addEventListener("listMovedDown",this,"onItemsListMoveDown");
      this._ItemsList.addEventListener("selectionChange",this,"onItemsListMouseSelectionChange");
   }
   function SetPlatform(aiPlatform, abPS3Switch)
   {
      this.iPlatform = aiPlatform;
      this._CategoriesList.SetPlatform(aiPlatform,abPS3Switch);
      this._ItemsList.SetPlatform(aiPlatform,abPS3Switch);
      if(this.iPlatform == 0)
      {
         this.CategoriesListHolder.ListBackground.gotoAndStop("Mouse");
         this.ItemsListHolder.ListBackground.gotoAndStop("Mouse");
      }
      else
      {
         this.CategoriesListHolder.ListBackground.gotoAndStop("Gamepad");
         this.ItemsListHolder.ListBackground.gotoAndStop("Gamepad");
      }
   }
   function handleInput(details, pathToFocus)
   {
      var _loc2_ = false;
      if(this.iCurrentState == InventoryLists.ONE_PANEL || this.iCurrentState == InventoryLists.TWO_PANELS)
      {
         if(Shared.GlobalFunc.IsKeyPressed(details))
         {
            if(details.navEquivalent == this.strHideItemsCode && this.iCurrentState == InventoryLists.TWO_PANELS)
            {
               this.HideItemsList();
               _loc2_ = true;
            }
            else if(details.navEquivalent == this.strShowItemsCode && this.iCurrentState == InventoryLists.ONE_PANEL)
            {
               this.ShowItemsList();
               _loc2_ = true;
            }
         }
         if(!_loc2_)
         {
            _loc2_ = pathToFocus[0].handleInput(details,pathToFocus.slice(1));
         }
      }
      return _loc2_;
   }
   function get CategoriesList()
   {
      return this._CategoriesList;
   }
   function get ItemsList()
   {
      return this._ItemsList;
   }
   function get currentState()
   {
      return this.iCurrentState;
   }
   function set currentState(aiNewState)
   {
      switch(aiNewState)
      {
         case InventoryLists.NO_PANELS:
            this.iCurrentState = aiNewState;
            break;
         case InventoryLists.ONE_PANEL:
            this.iCurrentState = aiNewState;
            gfx.managers.FocusHandler.instance.setFocus(this._CategoriesList,0);
            break;
         case InventoryLists.TWO_PANELS:
            this.iCurrentState = aiNewState;
            gfx.managers.FocusHandler.instance.setFocus(this._ItemsList,0);
      }
   }
   function RestoreCategoryIndex()
   {
      this._CategoriesList.selectedIndex = this.iCurrCategoryIndex;
   }
   function ShowCategoriesList(abPlayBladeSound)
   {
      this.iCurrentState = InventoryLists.TRANSITIONING_TO_ONE_PANEL;
      this.dispatchEvent({type:"categoryChange",index:this._CategoriesList.selectedIndex});
      this.gotoAndPlay("Panel1Show");
      if(abPlayBladeSound != false)
      {
         gfx.io.GameDelegate.call("PlaySound",["UIMenuBladeOpenSD"]);
      }
   }
   function HideCategoriesList()
   {
      this.iCurrentState = InventoryLists.TRANSITIONING_TO_NO_PANELS;
      this.gotoAndPlay("Panel1Hide");
      gfx.io.GameDelegate.call("PlaySound",["UIMenuBladeCloseSD"]);
   }
   function ShowItemsList(abPlayBladeSound, abPlayAnim)
   {
      if(abPlayAnim != false)
      {
         this.iCurrentState = InventoryLists.TRANSITIONING_TO_TWO_PANELS;
      }
      if(this._CategoriesList.selectedEntry != undefined)
      {
         this.iCurrCategoryIndex = this._CategoriesList.selectedIndex;
         this._ItemsList.filterer.itemFilter = this._CategoriesList.selectedEntry.flag;
         this._ItemsList.RestoreScrollPosition(this._CategoriesList.selectedEntry.savedItemIndex,true);
      }
      this._ItemsList.UpdateList();
      this.dispatchEvent({type:"showItemsList",index:this._ItemsList.selectedIndex});
      if(abPlayAnim != false)
      {
         this.gotoAndPlay("Panel2Show");
      }
      if(abPlayBladeSound != false)
      {
         gfx.io.GameDelegate.call("PlaySound",["UIMenuBladeOpenSD"]);
      }
      this._ItemsList.disableInput = false;
   }
   function HideItemsList()
   {
      this.iCurrentState = InventoryLists.TRANSITIONING_TO_ONE_PANEL;
      this.dispatchEvent({type:"hideItemsList",index:this._ItemsList.selectedIndex});
      this._ItemsList.selectedIndex = -1;
      this.gotoAndPlay("Panel2Hide");
      gfx.io.GameDelegate.call("PlaySound",["UIMenuBladeCloseSD"]);
      this._ItemsList.disableInput = true;
   }
   function onCategoriesItemPress()
   {
      if(this.iCurrentState == InventoryLists.ONE_PANEL)
      {
         this.ShowItemsList();
      }
      else if(this.iCurrentState == InventoryLists.TWO_PANELS)
      {
         this.ShowItemsList(true,false);
      }
   }
   function onCategoriesListPress()
   {
      if(this.iCurrentState == InventoryLists.TWO_PANELS && !this._ItemsList.disableSelection && !this._ItemsList.disableInput)
      {
         this.HideItemsList();
         this._CategoriesList.UpdateList();
      }
   }
   function onCategoriesListMoveUp(event)
   {
      this.doCategorySelectionChange(event);
      if(event.scrollChanged == true)
      {
         this._CategoriesList._parent.gotoAndPlay("moveUp");
      }
   }
   function onCategoriesListMoveDown(event)
   {
      this.doCategorySelectionChange(event);
      if(event.scrollChanged == true)
      {
         this._CategoriesList._parent.gotoAndPlay("moveDown");
      }
   }
   function onCategoriesListMouseSelectionChange(event)
   {
      if(event.keyboardOrMouse == 0)
      {
         this.doCategorySelectionChange(event);
      }
   }
   function onItemsListMoveUp(event)
   {
      this.doItemsSelectionChange(event);
      if(event.scrollChanged == true)
      {
         this._ItemsList._parent.gotoAndPlay("moveUp");
      }
   }
   function onItemsListMoveDown(event)
   {
      this.doItemsSelectionChange(event);
      if(event.scrollChanged == true)
      {
         this._ItemsList._parent.gotoAndPlay("moveDown");
      }
   }
   function onItemsListMouseSelectionChange(event)
   {
      if(event.keyboardOrMouse == 0)
      {
         this.doItemsSelectionChange(event);
      }
   }
   function doCategorySelectionChange(event)
   {
      this.dispatchEvent({type:"categoryChange",index:event.index});
      if(event.index != -1)
      {
         gfx.io.GameDelegate.call("PlaySound",["UIMenuFocus"]);
      }
   }
   function doItemsSelectionChange(event)
   {
      this._CategoriesList.selectedEntry.savedItemIndex = this._ItemsList.scrollPosition;
      this.dispatchEvent({type:"itemHighlightChange",index:event.index});
      if(event.index != -1)
      {
         gfx.io.GameDelegate.call("PlaySound",["UIMenuFocus"]);
      }
   }
   function SetCategoriesList()
   {
      var _loc12_ = 0;
      var _loc13_ = 1;
      var _loc5_ = 2;
      var _loc11_ = 3;
      this._CategoriesList.entryList.splice(0,this._CategoriesList.entryList.length);
      var _loc3_ = 0;
      var _loc4_;
      while(_loc3_ < arguments.length)
      {
         _loc4_ = {text:arguments[_loc3_ + _loc12_],flag:arguments[_loc3_ + _loc13_],bDontHide:arguments[_loc3_ + _loc5_],savedItemIndex:0,filterFlag:(arguments[_loc3_ + _loc5_] != true ? 0 : 1)};
         if(_loc4_.flag == 0)
         {
            _loc4_.divider = true;
         }
         this._CategoriesList.entryList.push(_loc4_);
         _loc3_ += _loc11_;
      }
      this._CategoriesList.InvalidateData();
      this._ItemsList.filterer.itemFilter = this._CategoriesList.selectedEntry.flag;
   }
   function InvalidateListData()
   {
      var _loc6_ = this._CategoriesList.centeredEntry;
      var _loc7_ = this._CategoriesList.selectedEntry.flag;
      var _loc3_ = 0;
      while(_loc3_ < this._CategoriesList.entryList.length)
      {
         this._CategoriesList.entryList[_loc3_].filterFlag = !this._CategoriesList.entryList[_loc3_].bDontHide ? 0 : 1;
         _loc3_ = _loc3_ + 1;
      }
      this._ItemsList.InvalidateData();
      _loc3_ = 0;
      var _loc5_;
      var _loc2_;
      while(_loc3_ < this._ItemsList.entryList.length)
      {
         _loc5_ = this._ItemsList.entryList[_loc3_].filterFlag;
         _loc2_ = 0;
         while(_loc2_ < this._CategoriesList.entryList.length)
         {
            if(this._CategoriesList.entryList[_loc2_].filterFlag == 0)
            {
               if(this._ItemsList.entryList[_loc3_].filterFlag & this._CategoriesList.entryList[_loc2_].flag)
               {
                  this._CategoriesList.entryList[_loc2_].filterFlag = 1;
               }
            }
            _loc2_ = _loc2_ + 1;
         }
         _loc3_ = _loc3_ + 1;
      }
      this._CategoriesList.onFilterChange();
      var _loc4_ = 0;
      _loc3_ = 0;
      while(_loc3_ < this._CategoriesList.entryList.length)
      {
         if(this._CategoriesList.entryList[_loc3_].filterFlag == 1)
         {
            if(_loc6_.flag == this._CategoriesList.entryList[_loc3_].flag)
            {
               this._CategoriesList.RestoreScrollPosition(_loc4_,false);
            }
            _loc4_ = _loc4_ + 1;
         }
         _loc3_ = _loc3_ + 1;
      }
      this._CategoriesList.UpdateList();
      if(this._CategoriesList.centeredEntry == undefined)
      {
         this._CategoriesList.scrollPosition = this._CategoriesList.scrollPosition - 1;
      }
      if(_loc7_ != this._CategoriesList.selectedEntry.flag)
      {
         this._ItemsList.filterer.itemFilter = this._CategoriesList.selectedEntry.flag;
         this._ItemsList.UpdateList();
         this.dispatchEvent({type:"categoryChange",index:this._CategoriesList.selectedIndex});
      }
      if(this.iCurrentState != InventoryLists.TWO_PANELS && this.iCurrentState != InventoryLists.TRANSITIONING_TO_TWO_PANELS)
      {
         this._ItemsList.selectedIndex = -1;
      }
      else if(this.iCurrentState == InventoryLists.TWO_PANELS)
      {
         this.dispatchEvent({type:"itemHighlightChange",index:this._ItemsList.selectedIndex});
      }
      else
      {
         this.dispatchEvent({type:"showItemsList",index:this._ItemsList.selectedIndex});
      }
   }
}
