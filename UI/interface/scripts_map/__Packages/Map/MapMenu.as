class Map.MapMenu
{
   var BottomBar;
   var LocalMapMenu;
   var MapHeight;
   var MapMovie;
   var MapWidth;
   var MarkerContainer;
   var MarkerData;
   var MarkerDescriptionHolder;
   var MarkerDescriptionObj;
   var Markers;
   var NextCreateIndex;
   var PlayerLocationMarkerType;
   var SelectedMarker;
   var YouAreHereMarker;
   var iPlatform;
   static var REFRESH_SHOW = 0;
   static var REFRESH_X = 1;
   static var REFRESH_Y = 2;
   static var REFRESH_ROTATION = 3;
   static var REFRESH_STRIDE = 4;
   static var CREATE_NAME = 0;
   static var CREATE_ICONTYPE = 1;
   static var CREATE_UNDISCOVERED = 2;
   static var CREATE_STRIDE = 3;
   static var MARKER_CREATE_PER_FRAME = 10;
   function MapMenu(aMapMovie)
   {
      this.MapMovie = aMapMovie == undefined ? _root : aMapMovie;
      this.MarkerContainer = this.MapMovie.createEmptyMovieClip("MarkerClips",1);
      this.BottomBar = _root.Bottom;
      this.Markers = new Array();
      this.NextCreateIndex = -1;
      this.MapWidth = 0;
      this.MapHeight = 0;
      this.LocalMapMenu = this.MapMovie.LocalMapFader.MapClip;
      if(this.LocalMapMenu != undefined)
      {
         this.LocalMapMenu.SetBottomBar(this.BottomBar);
         Mouse.addListener(this);
      }
      this.MarkerDescriptionHolder = this.MapMovie.attachMovie("DescriptionHolder","MarkerDescriptionHolder",this.MapMovie.getNextHighestDepth());
      this.MarkerDescriptionHolder._visible = false;
      this.MarkerDescriptionHolder.hitTestDisable = true;
      this.MarkerDescriptionObj = this.MarkerDescriptionHolder.Description;
      Stage.addListener(this);
      this.Init();
   }
   function onResize()
   {
      this.MapWidth = Stage.visibleRect.right - Stage.visibleRect.left;
      this.MapHeight = Stage.visibleRect.bottom - Stage.visibleRect.top;
      var _loc3_;
      if(this.MapMovie == _root)
      {
         this.MarkerContainer._x = Stage.visibleRect.left;
         this.MarkerContainer._y = Stage.visibleRect.top;
      }
      else
      {
         _loc3_ = Map.LocalMap(this.MapMovie);
         if(_loc3_ != undefined)
         {
            this.MapWidth = _loc3_.TextureWidth;
            this.MapHeight = _loc3_.TextureHeight;
         }
      }
      Shared.GlobalFunc.SetLockFunction();
      MovieClip(_root.Bottom).Lock("B");
   }
   function onMouseDown()
   {
      if(!this.BottomBar.hitTest(_root._xmouse,_root._ymouse))
      {
         gfx.io.GameDelegate.call("ClickCallback",[]);
      }
   }
   function SetNumMarkers(aNumMarkers)
   {
      if(undefined != this.MarkerContainer)
      {
         this.MarkerContainer.removeMovieClip();
         this.MarkerContainer = this.MapMovie.createEmptyMovieClip("MarkerClips",1);
         this.onResize();
      }
      delete this.Markers;
      this.Markers = new Array(aNumMarkers);
      Map.MapMarker.TopDepth = aNumMarkers;
      this.NextCreateIndex = 0;
      this.SetSelectedMarker(-1);
   }
   function GetCreatingMarkers()
   {
      return this.NextCreateIndex != -1;
   }
   function CreateMarkers()
   {
      var _loc5_;
      var _loc3_;
      var _loc6_;
      var _loc7_;
      var _loc2_;
      var _loc4_;
      if(-1 != this.NextCreateIndex && this.MarkerContainer != undefined)
      {
         _loc5_ = 0;
         _loc3_ = this.NextCreateIndex * Map.MapMenu.CREATE_STRIDE;
         _loc6_ = this.Markers.length;
         _loc7_ = this.MarkerData.length;
         while(this.NextCreateIndex < _loc6_ && _loc3_ < _loc7_ && _loc5_ < Map.MapMenu.MARKER_CREATE_PER_FRAME)
         {
            _loc2_ = this.MarkerContainer.attachMovie(Map.MapMarker.IconTypes[this.MarkerData[_loc3_ + Map.MapMenu.CREATE_ICONTYPE]],"Marker" + this.NextCreateIndex,this.NextCreateIndex);
            this.Markers[this.NextCreateIndex] = _loc2_;
            if(this.MarkerData[_loc3_ + Map.MapMenu.CREATE_ICONTYPE] == this.PlayerLocationMarkerType)
            {
               this.YouAreHereMarker = _loc2_.Icon;
            }
            _loc2_.Index = this.NextCreateIndex;
            _loc2_.label = this.MarkerData[_loc3_ + Map.MapMenu.CREATE_NAME];
            _loc2_.textField._visible = false;
            _loc2_.visible = false;
            if(this.MarkerData[_loc3_ + Map.MapMenu.CREATE_UNDISCOVERED] && _loc2_.IconClip != undefined)
            {
               _loc4_ = _loc2_.IconClip.getNextHighestDepth();
               _loc2_.IconClip.attachMovie(Map.MapMarker.IconTypes[this.MarkerData[_loc3_ + Map.MapMenu.CREATE_ICONTYPE]] + "Undiscovered","UndiscoveredIcon",_loc4_);
            }
            _loc5_ = _loc5_ + 1;
            this.NextCreateIndex = this.NextCreateIndex + 1;
            _loc3_ += Map.MapMenu.CREATE_STRIDE;
         }
         if(this.NextCreateIndex >= _loc6_)
         {
            this.NextCreateIndex = -1;
         }
      }
   }
   function RefreshMarkers()
   {
      var _loc4_ = 0;
      var _loc3_ = 0;
      var _loc6_ = this.Markers.length;
      var _loc5_ = this.MarkerData.length;
      var _loc2_;
      while(_loc4_ < _loc6_ && _loc3_ < _loc5_)
      {
         _loc2_ = this.Markers[_loc4_];
         _loc2_._visible = this.MarkerData[_loc3_ + Map.MapMenu.REFRESH_SHOW];
         if(_loc2_._visible)
         {
            _loc2_._x = this.MarkerData[_loc3_ + Map.MapMenu.REFRESH_X] * this.MapWidth;
            _loc2_._y = this.MarkerData[_loc3_ + Map.MapMenu.REFRESH_Y] * this.MapHeight;
            _loc2_._rotation = this.MarkerData[_loc3_ + Map.MapMenu.REFRESH_ROTATION];
         }
         _loc4_ = _loc4_ + 1;
         _loc3_ += Map.MapMenu.REFRESH_STRIDE;
      }
      if(this.SelectedMarker != undefined)
      {
         this.MarkerDescriptionHolder._x = this.SelectedMarker._x + this.MarkerContainer._x;
         this.MarkerDescriptionHolder._y = this.SelectedMarker._y + this.MarkerContainer._y;
      }
   }
   function SetSelectedMarker(aiSelectedMarkerIndex)
   {
      var _loc3_ = aiSelectedMarkerIndex < 0 ? undefined : this.Markers[aiSelectedMarkerIndex];
      if(_loc3_ != this.SelectedMarker)
      {
         if(this.SelectedMarker != undefined)
         {
            this.SelectedMarker.MarkerRollOut();
            this.SelectedMarker = undefined;
            this.MarkerDescriptionHolder.gotoAndPlay("Hide");
         }
         if(_loc3_ != undefined && !this.BottomBar.hitTest(_root._xmouse,_root._ymouse) && _loc3_.visible && _loc3_.MarkerRollOver())
         {
            this.SelectedMarker = _loc3_;
            this.MarkerDescriptionHolder._visible = true;
            this.MarkerDescriptionHolder.gotoAndPlay("Show");
         }
         else
         {
            this.SelectedMarker = undefined;
         }
      }
   }
   function ClickSelectedMarker()
   {
      if(this.SelectedMarker != undefined)
      {
         this.SelectedMarker.MarkerClick();
      }
   }
   function Init()
   {
      this.onResize();
      if(this.BottomBar != undefined)
      {
         this.BottomBar.swapDepths(4);
      }
      if(this.MapMovie.LocalMapFader != undefined)
      {
         this.MapMovie.LocalMapFader.swapDepths(3);
         this.MapMovie.LocalMapFader.gotoAndStop("hide");
         this.BottomBar.LocalMapButton.addEventListener("click",this,"OnLocalButtonClick");
         this.BottomBar.JournalButton.addEventListener("click",this,"OnJournalButtonClick");
         this.BottomBar.PlayerLocButton.addEventListener("click",this,"OnPlayerLocButtonClick");
      }
      gfx.io.GameDelegate.addCallBack("RefreshMarkers",this,"RefreshMarkers");
      gfx.io.GameDelegate.addCallBack("SetSelectedMarker",this,"SetSelectedMarker");
      gfx.io.GameDelegate.addCallBack("ClickSelectedMarker",this,"ClickSelectedMarker");
      gfx.io.GameDelegate.addCallBack("SetDateString",this,"SetDateString");
      gfx.io.GameDelegate.addCallBack("ShowJournal",this,"ShowJournal");
   }
   function OnLocalButtonClick()
   {
      gfx.io.GameDelegate.call("ToggleMapCallback",[]);
   }
   function OnJournalButtonClick()
   {
      gfx.io.GameDelegate.call("OpenJournalCallback",[]);
   }
   function OnPlayerLocButtonClick()
   {
      gfx.io.GameDelegate.call("CurrentLocationCallback",[]);
   }
   function SetPlatform(aPlatformNum, abPS3Switch)
   {
      if(this.BottomBar != undefined)
      {
         this.BottomBar.LeftButton.SetPlatform(aPlatformNum,abPS3Switch);
         this.BottomBar.RightButton.SetPlatform(aPlatformNum,abPS3Switch);
         this.BottomBar.JournalButton.SetPlatform(aPlatformNum,abPS3Switch);
         this.BottomBar.PlayerLocButton.SetPlatform(aPlatformNum,abPS3Switch);
         this.BottomBar.LocalMapButton.SetPlatform(aPlatformNum,abPS3Switch);
         this.BottomBar.JournalButton.disabled = aPlatformNum != Shared.ButtonChange.PLATFORM_PC;
         this.BottomBar.PlayerLocButton.disabled = aPlatformNum != Shared.ButtonChange.PLATFORM_PC;
         this.BottomBar.LocalMapButton.disabled = aPlatformNum != Shared.ButtonChange.PLATFORM_PC;
      }
      this.iPlatform = aPlatformNum;
   }
   function SetDateString(astrDate)
   {
      this.BottomBar.DateText.SetText(astrDate);
   }
   function ShowJournal(abShow)
   {
      if(this.BottomBar != undefined)
      {
         this.BottomBar._visible = !abShow;
      }
   }
   function SetCurrentLocationEnabled(abEnabled)
   {
      if(this.BottomBar != undefined && this.iPlatform == Shared.ButtonChange.PLATFORM_PC)
      {
         this.BottomBar.PlayerLocButton.disabled = !abEnabled;
      }
   }
   function SetJournalInput(buttonName)
   {
      this.BottomBar.JournalButton.PCArt = buttonName;
      this.BottomBar.JournalButton.XBoxArt = buttonName;
      this.BottomBar.JournalButton.PS3Art = buttonName;
      this.BottomBar.JournalButton.RefreshArt();
   }
}
