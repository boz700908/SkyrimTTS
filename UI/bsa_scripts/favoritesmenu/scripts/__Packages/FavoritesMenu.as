class FavoritesMenu extends MovieClip
{
   var ItemList;
   var LeftPanel;
   var List_mc;
   var bPCControlsReady = true;
   function FavoritesMenu()
   {
      super();
      this.ItemList = this.List_mc;
   }
   function InitExtensions()
   {
      Shared.GlobalFunc.SetLockFunction();
      this._parent.Lock("BL");
      gfx.io.GameDelegate.addCallBack("PopulateItems",this,"populateItemList");
      gfx.io.GameDelegate.addCallBack("SetSelectedItem",this,"setSelectedItem");
      gfx.io.GameDelegate.addCallBack("StartFadeOut",this,"startFadeOut");
      this.ItemList.addEventListener("listMovedUp",this,"onListMoveUp");
      this.ItemList.addEventListener("listMovedDown",this,"onListMoveDown");
      this.ItemList.addEventListener("itemPress",this,"onItemSelect");
      gfx.managers.FocusHandler.instance.setFocus(this.ItemList,0);
      this._parent.gotoAndPlay("startFadeIn");
   }
   function handleInput(details, pathToFocus)
   {
      if(!pathToFocus[0].handleInput(details,pathToFocus.slice(1)))
      {
         if(Shared.GlobalFunc.IsKeyPressed(details) && details.navEquivalent == gfx.ui.NavigationCode.TAB)
         {
            this.startFadeOut();
         }
      }
      return true;
   }
   function get selectedIndex()
   {
      return this.ItemList.selectedEntry.index;
   }
   function get itemList()
   {
      return this.ItemList;
   }
   function setSelectedItem(aiIndex)
   {
      var _loc2_ = 0;
      while(_loc2_ < this.ItemList.entryList.length)
      {
         if(this.ItemList.entryList[_loc2_].index == aiIndex)
         {
            this.ItemList.selectedIndex = _loc2_;
            this.ItemList.RestoreScrollPosition(_loc2_);
            this.ItemList.UpdateList();
            break;
         }
         _loc2_ = _loc2_ + 1;
      }
   }
   function onListMoveUp(event)
   {
      gfx.io.GameDelegate.call("PlaySound",["UIMenuFocus"]);
      if(event.scrollChanged == true)
      {
         this.gotoAndPlay("moveUp");
      }
   }
   function onListMoveDown(event)
   {
      gfx.io.GameDelegate.call("PlaySound",["UIMenuFocus"]);
      if(event.scrollChanged == true)
      {
         this.gotoAndPlay("moveDown");
      }
   }
   function onItemSelect(event)
   {
      if(event.keyboardOrMouse != 0)
      {
         gfx.io.GameDelegate.call("ItemSelect",[]);
      }
   }
   function startFadeOut()
   {
      this._parent.gotoAndPlay("startFadeOut");
   }
   function onFadeOutCompletion()
   {
      gfx.io.GameDelegate.call("FadeDone",[this.selectedIndex]);
   }
   function SetPlatform(aiPlatform, abPS3Switch)
   {
      this.ItemList.SetPlatform(aiPlatform,abPS3Switch);
      this.LeftPanel._x = aiPlatform != 0 ? -78.2 : -90;
      this.LeftPanel.gotoAndStop(aiPlatform != 0 ? "Gamepad" : "Mouse");
   }
}
