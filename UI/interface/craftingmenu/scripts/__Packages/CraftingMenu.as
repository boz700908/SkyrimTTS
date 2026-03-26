class CraftingMenu extends MovieClip
{
   var AdditionalDescription;
   var AdditionalDescriptionHolder;
   var BottomBarInfo;
   var ButtonText;
   var CategoryList;
   var ExitMenuRect;
   var InventoryLists;
   var ItemInfo;
   var ItemInfoHolder;
   var ItemList;
   var ItemListTweener;
   var ItemsListInputCatcher;
   var MenuDescription;
   var MenuDescriptionHolder;
   var MenuName;
   var MenuNameHolder;
   var MenuType;
   var MouseRotationRect;
   var Platform;
   var RestoreCategoryRect;
   var SavedCategoryCenterText;
   var SavedCategoryScrollRatio;
   var SavedCategorySelectedText;
   var _bCanCraft;
   var _bCanFadeItemInfo;
   var _bItemCardAdditionalDescription;
   var bCanExpandPanel;
   var bHideAdditionalDescription;
   static var MT_SINGLE_PANEL = 0;
   static var MT_DOUBLE_PANEL = 1;
   static var LIST_OFFSET = 20;
   static var SELECT_BUTTON = 0;
   static var EXIT_BUTTON = 1;
   static var AUX_BUTTON = 2;
   static var CRAFT_BUTTON = 3;
   function CraftingMenu()
   {
      super();
      this._bCanCraft = false;
      this.bCanExpandPanel = true;
      this._bCanFadeItemInfo = true;
      this.bHideAdditionalDescription = false;
      this._bItemCardAdditionalDescription = false;
      this.ButtonText = new Array("","","","");
      this.Platform = 0;
      this.CategoryList = this.InventoryLists;
      this.ItemInfo = this.ItemInfoHolder.ItemInfo;
      Mouse.addListener(this);
   }
   function Initialize()
   {
      this.ItemInfoHolder = this.ItemInfoHolder;
      this.ItemInfoHolder.gotoAndStop("default");
      this.ItemInfo.addEventListener("endEditItemName",this,"OnEndEditItemName");
      this.ItemInfo.addEventListener("subMenuAction",this,"OnSubMenuAction");
      this.BottomBarInfo = this.BottomBarInfo;
      this.AdditionalDescriptionHolder = this.ItemInfoHolder.AdditionalDescriptionHolder;
      this.AdditionalDescription = this.AdditionalDescriptionHolder.AdditionalDescription;
      this.AdditionalDescription.textAutoSize = "shrink";
      this.MenuName = this.CategoryList.CategoriesList._parent.CategoryLabel;
      this.MenuName.autoSize = "left";
      this.MenuNameHolder._visible = false;
      this.MenuDescription = this.MenuDescriptionHolder.MenuDescription;
      this.MenuDescription.autoSize = "center";
      this.BottomBarInfo.SetButtonsArt([{PCArt:"E",XBoxArt:"360_A",PS3Art:"PS3_A"},{PCArt:"Tab",XBoxArt:"360_B",PS3Art:"PS3_B"},{PCArt:"F",XBoxArt:"360_Y",PS3Art:"PS3_Y"},{PCArt:"R",XBoxArt:"360_X",PS3Art:"PS3_X"}]);
      if(this.ItemListTweener != undefined)
      {
         this.MenuType = CraftingMenu.MT_SINGLE_PANEL;
         this.ItemList = this.ItemListTweener.List_mc;
         this.ItemListTweener.gotoAndPlay("showList");
         gfx.managers.FocusHandler.instance.setFocus(this.ItemList,0);
         this.ItemList.addEventListener("listMovedUp",this,"OnItemListMovedUp");
         this.ItemList.addEventListener("listMovedDown",this,"OnItemListMovedDown");
         this.ItemList.addEventListener("itemPress",this,"OnItemListPressed");
      }
      else if(this.CategoryList != undefined)
      {
         this.MenuType = CraftingMenu.MT_DOUBLE_PANEL;
         gfx.managers.FocusHandler.instance.setFocus(this.CategoryList,0);
         this.CategoryList.ShowCategoriesList();
         this.CategoryList.addEventListener("itemHighlightChange",this,"OnItemHighlightChange");
         this.CategoryList.addEventListener("showItemsList",this,"OnShowItemsList");
         this.CategoryList.addEventListener("hideItemsList",this,"OnHideItemsList");
         this.CategoryList.addEventListener("categoryChange",this,"OnCategoryListChange");
         this.ItemList = this.CategoryList.ItemsList;
         this.ItemList.addEventListener("itemPress",this,"OnItemSelect");
      }
      this.BottomBarInfo["Button" + CraftingMenu.CRAFT_BUTTON].addEventListener("press",this,"onCraftButtonPress");
      this.BottomBarInfo["Button" + CraftingMenu.EXIT_BUTTON].addEventListener("click",this,"onExitButtonPress");
      this.BottomBarInfo["Button" + CraftingMenu.EXIT_BUTTON].disabled = false;
      this.BottomBarInfo["Button" + CraftingMenu.AUX_BUTTON].addEventListener("click",this,"onAuxButtonPress");
      this.BottomBarInfo["Button" + CraftingMenu.AUX_BUTTON].disabled = false;
      this.ItemsListInputCatcher.onMouseDown = function()
      {
         if(Mouse.getTopMostEntity() == this)
         {
            this._parent.onItemsListInputCatcherClick();
         }
      };
      this.RestoreCategoryRect.onRollOver = function()
      {
         if(this._parent.CategoryList.currentState == InventoryLists.TWO_PANELS)
         {
            this._parent.CategoryList.RestoreCategoryIndex();
         }
      };
      this.ExitMenuRect.onPress = function()
      {
         gfx.io.GameDelegate.call("CloseMenu",[]);
      };
      this.bCanCraft = false;
      this.PositionElements();
      this.SetPlatform(this.Platform);
   }
   function get bCanCraft()
   {
      return this._bCanCraft;
   }
   function set bCanCraft(abCanCraft)
   {
      this._bCanCraft = abCanCraft;
      this.UpdateButtonText();
   }
   function onCraftButtonPress()
   {
      if(this.bCanCraft)
      {
         gfx.io.GameDelegate.call("CraftButtonPress",[]);
      }
   }
   function onExitButtonPress()
   {
      gfx.io.GameDelegate.call("CloseMenu",[]);
   }
   function onAuxButtonPress()
   {
      gfx.io.GameDelegate.call("AuxButtonPress",[]);
   }
   function get bCanFadeItemInfo()
   {
      gfx.io.GameDelegate.call("CanFadeItemInfo",[],this,"SetCanFadeItemInfo");
      return this._bCanFadeItemInfo;
   }
   function SetCanFadeItemInfo(abCanFade)
   {
      this._bCanFadeItemInfo = abCanFade;
   }
   function get bItemCardAdditionalDescription()
   {
      return this._bItemCardAdditionalDescription;
   }
   function set bItemCardAdditionalDescription(abItemCardDesc)
   {
      this._bItemCardAdditionalDescription = abItemCardDesc;
      if(abItemCardDesc)
      {
         this.AdditionalDescription.text = "";
      }
   }
   function SetPartitionedFilterMode(abPartitioned)
   {
      this.CategoryList.ItemsList.filterer.SetPartitionedFilterMode(abPartitioned);
   }
   function GetItemShown()
   {
      return this.ItemList.selectedIndex >= 0 && (this.CategoryList == undefined || this.CategoryList.currentState == InventoryLists.TWO_PANELS || this.CategoryList.currentState == InventoryLists.TRANSITIONING_TO_TWO_PANELS);
   }
   function GetNumCategories()
   {
      return !(this.CategoryList != undefined && this.CategoryList.CategoriesList != undefined) ? 0 : this.CategoryList.CategoriesList.entryList.length;
   }
   function onMouseUp()
   {
      if(this.ItemInfo.bEditNameMode && !this.ItemInfo.hitTest(_root._xmouse,_root._ymouse))
      {
         this.OnEndEditItemName({useNewName:false,newName:""});
      }
   }
   function onMouseWheel(delta)
   {
      var _loc2_;
      if(this.CategoryList.currentState == InventoryLists.TWO_PANELS && !this.ItemList.disableSelection && !this.ItemList.disableInput)
      {
         _loc2_ = Mouse.getTopMostEntity();
         while(_loc2_ && _loc2_ != undefined)
         {
            if(_loc2_ == this.ItemsListInputCatcher || _loc2_ == this.MouseRotationRect)
            {
               if(delta == 1)
               {
                  this.ItemList.moveSelectionUp();
               }
               else if(delta == -1)
               {
                  this.ItemList.moveSelectionDown();
               }
            }
            _loc2_ = _loc2_._parent;
         }
      }
   }
   function onMouseRotationStart()
   {
      gfx.io.GameDelegate.call("StartMouseRotation",[]);
      this.CategoryList.CategoriesList.disableSelection = true;
      this.ItemList.disableSelection = true;
   }
   function onMouseRotationStop()
   {
      gfx.io.GameDelegate.call("StopMouseRotation",[]);
      this.CategoryList.CategoriesList.disableSelection = false;
      this.ItemList.disableSelection = false;
   }
   function onItemsListInputCatcherClick()
   {
      if(this.CategoryList.currentState == InventoryLists.TWO_PANELS && !this.ItemList.disableSelection && !this.ItemList.disableInput)
      {
         this.OnItemSelect({index:this.ItemList.selectedIndex});
      }
   }
   function onMouseRotationFastClick(aiMouseButton)
   {
      if(aiMouseButton == 0)
      {
         this.onItemsListInputCatcherClick();
      }
   }
   function UpdateButtonText()
   {
      var _loc2_ = this.ButtonText.concat();
      if(!this.bCanCraft)
      {
         _loc2_[CraftingMenu.CRAFT_BUTTON] = "";
      }
      if(!this.GetItemShown())
      {
         _loc2_[CraftingMenu.SELECT_BUTTON] = "";
      }
      this.BottomBarInfo.SetButtonsText.apply(this.BottomBarInfo,_loc2_);
   }
   function UpdateItemList(abFullRebuild)
   {
      if(abFullRebuild == true)
      {
         this.CategoryList.InvalidateListData();
      }
      else
      {
         this.ItemList.UpdateList();
      }
      if(this.MenuType == CraftingMenu.MT_SINGLE_PANEL)
      {
         this.FadeInfoCard(this.ItemList.entryList.length == 0);
      }
   }
   function UpdateItemDisplay()
   {
      var _loc2_ = this.GetItemShown();
      this.FadeInfoCard(!_loc2_);
      this.SetSelectedItem(this.ItemList.selectedIndex);
      gfx.io.GameDelegate.call("ShowItem3D",[_loc2_]);
   }
   function FadeInfoCard(abFadeOut)
   {
      if(abFadeOut && this.bCanFadeItemInfo)
      {
         this.ItemInfo.FadeOutCard();
         if(this.bHideAdditionalDescription)
         {
            this.AdditionalDescriptionHolder._visible = false;
         }
      }
      else if(!abFadeOut)
      {
         this.ItemInfo.FadeInCard();
         if(this.bHideAdditionalDescription)
         {
            this.AdditionalDescriptionHolder._visible = true;
         }
      }
   }
   function PositionElements()
   {
      Shared.GlobalFunc.SetLockFunction();
      if(this.MenuType == CraftingMenu.MT_SINGLE_PANEL)
      {
         this.ItemListTweener.Lock("L");
         this.ItemListTweener._x -= CraftingMenu.LIST_OFFSET;
      }
      else if(this.MenuType == CraftingMenu.MT_DOUBLE_PANEL)
      {
         MovieClip(this.CategoryList).Lock("L");
         this.CategoryList._x -= CraftingMenu.LIST_OFFSET;
      }
      this.MenuNameHolder.Lock("L");
      this.MenuNameHolder._x -= CraftingMenu.LIST_OFFSET;
      this.MenuDescriptionHolder.Lock("TR");
      var _loc3_ = Stage.visibleRect.x + Stage.safeRect.x;
      var _loc4_ = Stage.visibleRect.x + Stage.visibleRect.width - Stage.safeRect.x;
      this.BottomBarInfo.PositionElements(_loc3_,_loc4_);
      MovieClip(this.ExitMenuRect).Lock("TL");
      this.ExitMenuRect._x -= Stage.safeRect.x + 10;
      this.ExitMenuRect._y -= Stage.safeRect.y;
      this.RestoreCategoryRect._x = this.ExitMenuRect._x + this.CategoryList.CategoriesList._parent._width + 25;
      this.ItemsListInputCatcher._x = this.RestoreCategoryRect._x + this.RestoreCategoryRect._width;
      this.ItemsListInputCatcher._width = _root._width - this.ItemsListInputCatcher._x;
      MovieClip(this.MouseRotationRect).Lock("T");
      this.MouseRotationRect._x = this.ItemInfo._parent._x;
      this.MouseRotationRect._width = this.ItemInfo._parent._width;
      this.MouseRotationRect._height = 0.55 * Stage.visibleRect.height;
   }
   function OnItemListPressed(event)
   {
      gfx.io.GameDelegate.call("CraftSelectedItem",[this.ItemList.selectedIndex]);
      gfx.io.GameDelegate.call("SetSelectedItem",[this.ItemList.selectedIndex]);
   }
   function OnItemSelect(event)
   {
      gfx.io.GameDelegate.call("ChooseItem",[event.index]);
      gfx.io.GameDelegate.call("ShowItem3D",[event.index != -1]);
      this.UpdateButtonText();
   }
   function OnItemHighlightChange(event)
   {
      this.SetSelectedItem(event.index);
      this.FadeInfoCard(event.index == -1);
      this.UpdateButtonText();
      gfx.io.GameDelegate.call("ShowItem3D",[event.index != -1]);
   }
   function OnShowItemsList(event)
   {
      if(this.Platform == 0)
      {
         gfx.io.GameDelegate.call("SetSelectedCategory",[this.CategoryList.CategoriesList.selectedIndex]);
      }
      this.OnItemHighlightChange(event);
   }
   function OnHideItemsList(event)
   {
      this.SetSelectedItem(event.index);
      this.FadeInfoCard(true);
      this.UpdateButtonText();
      gfx.io.GameDelegate.call("ShowItem3D",[false]);
   }
   function OnCategoryListChange(event)
   {
      if(this.Platform != 0)
      {
         gfx.io.GameDelegate.call("SetSelectedCategory",[event.index]);
      }
   }
   function SetSelectedItem(aSelection)
   {
      gfx.io.GameDelegate.call("SetSelectedItem",[aSelection]);
   }
   function handleInput(aInputEvent, aPathToFocus)
   {
      if(this.bCanExpandPanel && aPathToFocus.length > 0)
      {
         aPathToFocus[0].handleInput(aInputEvent,aPathToFocus.slice(1));
      }
      else if(this.MenuType == CraftingMenu.MT_DOUBLE_PANEL && aPathToFocus.length > 1)
      {
         aPathToFocus[1].handleInput(aInputEvent,aPathToFocus.slice(2));
      }
      return true;
   }
   function SetPlatform(aiPlatform, abPS3Switch)
   {
      this.Platform = aiPlatform;
      this.BottomBarInfo.SetPlatform(aiPlatform,abPS3Switch);
      this.ItemInfo.SetPlatform(aiPlatform,abPS3Switch);
      this.CategoryList.SetPlatform(aiPlatform,abPS3Switch);
   }
   function UpdateIngredients(aLineTitle, aIngredients, abShowPlayerCount)
   {
      var _loc4_ = !this.bItemCardAdditionalDescription ? this.AdditionalDescription : this.ItemInfo.GetItemName();
      _loc4_.text = !(aLineTitle != undefined && aLineTitle.length > 0) ? "" : aLineTitle + ": ";
      var _loc11_ = _loc4_.getNewTextFormat();
      var _loc9_ = _loc4_.getNewTextFormat();
      var _loc3_ = 0;
      var _loc2_;
      var _loc6_;
      var _loc5_;
      var _loc8_;
      while(_loc3_ < aIngredients.length)
      {
         _loc2_ = aIngredients[_loc3_];
         _loc9_.color = _loc2_.PlayerCount < _loc2_.RequiredCount ? 7829367 : 16777215;
         _loc4_.setNewTextFormat(_loc9_);
         _loc6_ = "";
         if(_loc2_.RequiredCount > 1)
         {
            _loc6_ = _loc2_.RequiredCount + " ";
         }
         _loc5_ = "";
         if(abShowPlayerCount && _loc2_.PlayerCount >= 1)
         {
            _loc5_ = " (" + _loc2_.PlayerCount + ")";
         }
         _loc8_ = _loc6_ + _loc2_.Name + _loc5_ + (_loc3_ >= aIngredients.length - 1 ? "" : ", ");
         _loc4_.replaceText(_loc4_.length,_loc4_.length + 1,_loc8_);
         _loc3_ = _loc3_ + 1;
      }
      _loc4_.setNewTextFormat(_loc11_);
   }
   function EditItemName(aInitialText, aMaxChars)
   {
      this.ItemInfo.StartEditName(aInitialText,aMaxChars);
   }
   function OnEndEditItemName(event)
   {
      this.ItemInfo.EndEditName();
      gfx.io.GameDelegate.call("EndItemRename",[event.useNewName,event.newName]);
   }
   function ShowSlider(aiMaxValue, aiMinValue, aiCurrentValue, aiSnapInterval)
   {
      this.ItemInfo.ShowEnchantingSlider(aiMaxValue,aiMinValue,aiCurrentValue);
      this.ItemInfo.quantitySlider.snapping = true;
      this.ItemInfo.quantitySlider.snapInterval = aiSnapInterval;
      this.ItemInfo.quantitySlider.addEventListener("change",this,"OnSliderChanged");
      this.OnSliderChanged();
   }
   function OnSliderChanged(event)
   {
      gfx.io.GameDelegate.call("CalculateCharge",[this.ItemInfo.quantitySlider.value],this,"SetChargeValues");
   }
   function SetSliderValue(aValue)
   {
      this.ItemInfo.quantitySlider.value = aValue;
   }
   function OnSubMenuAction(event)
   {
      if(event.opening == true)
      {
         this.ItemList.disableSelection = true;
         this.ItemList.disableInput = true;
         this.CategoryList.CategoriesList.disableSelection = true;
         this.CategoryList.CategoriesList.disableInput = true;
      }
      else if(event.opening == false)
      {
         this.ItemList.disableSelection = false;
         this.ItemList.disableInput = false;
         this.CategoryList.CategoriesList.disableSelection = false;
         this.CategoryList.CategoriesList.disableInput = false;
      }
      if(event.menu == "quantity")
      {
         if(!event.opening)
         {
            gfx.io.GameDelegate.call("SliderClose",[!event.canceled,event.value]);
         }
      }
   }
   function PreRebuildList()
   {
      this.SavedCategoryCenterText = this.CategoryList.CategoriesList.centeredEntry.text;
      this.SavedCategorySelectedText = this.CategoryList.CategoriesList.selectedEntry.text;
      this.SavedCategoryScrollRatio = this.CategoryList.CategoriesList.maxScrollPosition <= 0 ? 0 : this.CategoryList.CategoriesList.scrollPosition / this.CategoryList.CategoriesList.maxScrollPosition;
   }
   function PostRebuildList(abRestoreSelection)
   {
      var _loc3_;
      var _loc4_;
      var _loc5_;
      var _loc2_;
      if(abRestoreSelection)
      {
         _loc3_ = this.CategoryList.CategoriesList.entryList;
         _loc4_ = -1;
         _loc5_ = -1;
         _loc2_ = 0;
         while(_loc2_ < _loc3_.length)
         {
            if(this.SavedCategoryCenterText == _loc3_[_loc2_].text)
            {
               _loc4_ = _loc2_;
            }
            if(this.SavedCategorySelectedText == _loc3_[_loc2_].text)
            {
               _loc5_ = _loc2_;
            }
            _loc2_ = _loc2_ + 1;
         }
         if(_loc4_ == -1)
         {
            _loc4_ = Math.floor(this.SavedCategoryScrollRatio * _loc3_.length);
         }
         _loc4_ = Math.max(0,_loc4_);
         this.CategoryList.CategoriesList.RestoreScrollPosition(_loc4_,false);
         if(_loc5_ != -1)
         {
            this.CategoryList.CategoriesList.selectedIndex = _loc5_;
         }
         this.CategoryList.CategoriesList.UpdateList();
         this.CategoryList.ItemsList.filterer.itemFilter = this.CategoryList.CategoriesList.selectedEntry.flag;
         this.CategoryList.ItemsList.UpdateList();
      }
   }
}
