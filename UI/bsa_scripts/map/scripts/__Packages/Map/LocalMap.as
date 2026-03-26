class Map.LocalMap extends MovieClip
{
   var BottomBar;
   var ClearedDescription;
   var ClearedText;
   var IconDisplay;
   var LocalMapHolder_mc;
   var LocationDescription;
   var LocationTextClip;
   var MapImageLoader;
   var TextureHolder;
   var _TextureHeight;
   var _TextureWidth;
   var bUpdated;
   function LocalMap()
   {
      super();
      this.IconDisplay = new Map.MapMenu(this);
      this.MapImageLoader = new MovieClipLoader();
      this.MapImageLoader.addListener(this);
      this._TextureWidth = 800;
      this._TextureHeight = 450;
      this.LocationDescription = this.LocationTextClip.LocationText;
      this.LocationDescription.noTranslate = true;
      this.LocationTextClip.swapDepths(3);
      this.ClearedDescription = this.ClearedText;
      this.ClearedDescription.noTranslate = true;
      this.TextureHolder = this.LocalMapHolder_mc;
   }
   function get TextureWidth()
   {
      return this._TextureWidth;
   }
   function get TextureHeight()
   {
      return this._TextureHeight;
   }
   function onLoadInit(TargetClip)
   {
      TargetClip._width = this._TextureWidth;
      TargetClip._height = this._TextureHeight;
   }
   function InitMap()
   {
      if(!this.bUpdated)
      {
         this.MapImageLoader.loadClip("img://Local_Map",this.TextureHolder);
         this.bUpdated = true;
      }
      var _loc3_ = {x:this._x,y:this._y};
      var _loc2_ = {x:this._x + this._TextureWidth,y:this._y + this._TextureHeight};
      this._parent.localToGlobal(_loc3_);
      this._parent.localToGlobal(_loc2_);
      gfx.io.GameDelegate.call("SetLocalMapExtents",[_loc3_.x,_loc3_.y,_loc2_.x,_loc2_.y]);
   }
   function Show(abShow)
   {
      this._parent.gotoAndPlay(!abShow ? "fadeOut" : "fadeIn");
      this.BottomBar.RightButton.visible = !abShow;
      this.BottomBar.LocalMapButton.label = !abShow ? "$Local Map" : "$World Map";
   }
   function SetBottomBar(aBottomBar)
   {
      this.BottomBar = aBottomBar;
   }
   function SetTitle(aName, aCleared)
   {
      this.LocationDescription.text = aName == undefined ? "" : aName;
      this.ClearedDescription.text = aCleared == undefined ? "" : "(" + aCleared + ")";
   }
}
