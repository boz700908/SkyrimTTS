class ListMenu extends MovieClip
{
   var _childView;
   var _linearChildren;
   var itemList;
   var itemView;
   var _viewPosX = 0;
   var _viewPosY = 0;
   var _viewPortMin = 5;
   var _viewPortMax = 15;
   function ListMenu()
   {
      super();
      Shared.GlobalFunc.MaintainTextFormat();
      Shared.GlobalFunc.SetLockFunction();
      this.itemList = this.itemView.itemList;
      this._linearChildren = new Array();
      this._childView = new Array();
      this._childView.push(this.itemView);
      this._viewPosX = this.itemView._x;
      this._viewPosY = this.itemView._y;
      this._visible = false;
      Mouse.addListener(this);
      gfx.events.EventDispatcher.initialize(this);
   }
   function onLoad()
   {
      super.onLoad();
      this.itemView.setMinViewport(this._viewPortMin);
      this.itemView.setMaxViewport(this._viewPortMax);
      this.itemList.addEventListener("selectionChange",this,"onSelectionChange");
      this.itemList.addEventListener("itemPress",this,"onItemPressed");
      gfx.managers.FocusHandler.instance.setFocus(this.itemList,0);
   }
   function InitExtensions()
   {
      skse.SendModEvent("UIListMenu_LoadMenu");
   }
   function SetPlatform(aiPlatform, abPS3Switch)
   {
   }
   function closeMenu()
   {
      skse.SendModEvent("UIListMenu_CloseMenu");
      skse.CloseMenu("CustomMenu");
   }
   function handleInput(details, pathToFocus)
   {
      var _loc2_ = this._childView[this._childView.length - 1];
      if(_loc2_ && _loc2_.handleInput(details,pathToFocus))
      {
         return true;
      }
      var _loc5_ = pathToFocus.shift();
      if(_loc5_.handleInput(details,pathToFocus))
      {
         return true;
      }
      if(Shared.GlobalFunc.IsKeyPressed(details))
      {
         if(details.navEquivalent == gfx.ui.NavigationCode.TAB)
         {
            this.closeChildView();
         }
      }
      return true;
   }
   function onSelectionChange(event)
   {
      var _loc2_ = this._childView[this._childView.length - 1];
      var _loc3_;
      if(_loc2_)
      {
         _loc3_ = _loc2_.entryList[event.index];
         _loc2_.itemList.listState.selectedEntry = _loc3_;
      }
   }
   function onItemPressed(event)
   {
      var _loc4_ = this._childView[this._childView.length - 1];
      var _loc3_;
      var _loc2_;
      if(_loc4_)
      {
         _loc3_ = _loc4_.entryList[event.index];
         _loc3_.children.splice(0);
         _loc2_ = 0;
         while(_loc2_ < this._linearChildren.length)
         {
            if(this._linearChildren[_loc2_].parent == _loc3_.id)
            {
               _loc3_.children.push(this._linearChildren[_loc2_]);
            }
            _loc2_ = _loc2_ + 1;
         }
         if(_loc3_.children.length > 0)
         {
            this.openChildView(_loc3_,_loc3_.children);
         }
         else
         {
            skse.SendModEvent("UIListMenu_SelectItemText",_loc3_.text,_loc3_.callback);
            skse.SendModEvent("UIListMenu_SelectItem",Number(_loc3_.id).toString(),_loc3_.callback);
            this.closeMenu();
         }
      }
   }
   function openChildView(parentEntry, childEntries)
   {
      var _loc2_ = this._childView[this._childView.length - 1];
      var _loc3_;
      if(_loc2_)
      {
         _loc2_.itemList.disableSelection = _loc2_.itemList.disableInput = true;
         this.attachMovie("ItemView","childView_" + this._childView.length,this.getNextHighestDepth(),{_alpha:0,_x:this._viewPosX,_y:this._viewPosY,objectList:childEntries,parentEntry:parentEntry});
         _loc3_ = this["childView_" + this._childView.length];
         _loc3_.addEventListener("onLoad",this,"onChildViewLoad");
         this._childView.push(_loc3_);
         com.greensock.TweenLite.to(_loc2_,0.5,{_alpha:0,_x:this._viewPosX - _loc2_._width,overwrite:com.greensock.OverwriteManager.NONE,easing:com.greensock.easing.Linear.easeNone});
         com.greensock.TweenLite.to(_loc3_,0.5,{_alpha:100,overwrite:com.greensock.OverwriteManager.NONE,easing:com.greensock.easing.Linear.easeNone});
      }
   }
   function closeChildView()
   {
      var _loc2_ = this._childView[this._childView.length - 1];
      if(_loc2_ == this.itemView)
      {
         skse.SendModEvent("UIListMenu_SelectItemText","",-1);
         skse.SendModEvent("UIListMenu_SelectItem",Number(-1).toString(),-1);
         this.closeMenu();
         return undefined;
      }
      _loc2_.itemList.disableSelection = _loc2_.itemList.disableInput = true;
      com.greensock.TweenLite.to(_loc2_,0.5,{_alpha:0,onCompleteScope:this,onComplete:this.onChildViewRemoved,onCompleteParams:[_loc2_],overwrite:com.greensock.OverwriteManager.NONE,easing:com.greensock.easing.Linear.easeNone});
      this._childView.splice(this._childView.length - 1,1);
      _loc2_ = this._childView[this._childView.length - 1];
      if(_loc2_)
      {
         _loc2_.children.splice(0);
         com.greensock.TweenLite.to(_loc2_,0.5,{_alpha:100,_x:this._viewPosX,onCompleteScope:this,onComplete:this.onChildViewRestored,onCompleteParams:[_loc2_],overwrite:com.greensock.OverwriteManager.NONE,easing:com.greensock.easing.Linear.easeNone});
      }
   }
   function onChildViewRestored(lastChild)
   {
      lastChild.itemList.disableSelection = lastChild.itemList.disableInput = false;
      gfx.managers.FocusHandler.instance.setFocus(lastChild.itemList,0);
   }
   function onChildViewRemoved(lastChild)
   {
      if(lastChild)
      {
         lastChild.removeMovieClip();
      }
   }
   function onChildViewLoad(event)
   {
      if(event.view)
      {
         event.view.setMinViewport(this._viewPortMin);
         event.view.setMaxViewport(this._viewPortMax);
         event.view.itemList.addEventListener("itemPress",this,"onItemPressed");
         event.view.itemList.addEventListener("selectionChange",this,"onSelectionChange");
         event.view.entryList = event.view.objectList;
         event.view.itemList.requestInvalidate();
      }
      gfx.managers.FocusHandler.instance.setFocus(event.view.itemList,0);
   }
   function LM_AddTreeEntries()
   {
      var _loc12_ = false;
      var _loc5_ = 0;
      var _loc3_;
      var _loc4_;
      while(_loc5_ < arguments.length)
      {
         _loc3_ = arguments[_loc5_].split(";;");
         if(_loc3_[0] != "")
         {
            _loc4_ = {text:_loc3_[0],parent:Number(_loc3_[1]),id:Number(_loc3_[2]),callback:Number(_loc3_[3]),hasChildren:Number(_loc3_[4]),children:new Array()};
            this._linearChildren.push(_loc4_);
            if(_loc4_.parent == -1)
            {
               this.itemList.entryList.push(_loc4_);
               _loc12_ = true;
            }
         }
         _loc5_ = _loc5_ + 1;
      }
      if(_loc12_)
      {
         this.itemList.InvalidateData();
         this._visible = true;
      }
   }
   function LM_SetSortingEnabled(a_sort)
   {
      var _loc2_ = 0;
      while(_loc2_ < this._childView.length)
      {
         this._childView[_loc2_].sortEnabled = a_sort;
         _loc2_ = _loc2_ + 1;
      }
   }
}
