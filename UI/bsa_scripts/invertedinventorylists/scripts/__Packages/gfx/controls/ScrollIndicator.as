class gfx.controls.ScrollIndicator extends gfx.core.UIComponent
{
   var __height;
   var __width;
   var _disabled;
   var _name;
   var _parent;
   var _rotation;
   var _scrollTarget;
   var dispatchEventAndSound;
   var focusEnabled;
   var focusTarget;
   var initSize;
   var initialized;
   var inspectableScrollTarget;
   var invalidate;
   var lastVScrollPos;
   var onRelease;
   var pageSize;
   var scrollerIntervalID;
   var tabChildren;
   var tabEnabled;
   var thumb;
   var track;
   var useHandCursor;
   var direction = "vertical";
   var soundMap = {theme:"default",scroll:"scroll"};
   var pageScrollSize = 1;
   var minPosition = 0;
   var maxPosition = 10;
   var _position = 5;
   var offsetTop = 0;
   var offsetBottom = 0;
   var isDragging = false;
   function ScrollIndicator()
   {
      super();
      this.tabChildren = false;
      this.focusEnabled = this.tabEnabled = !this._disabled;
   }
   function get disabled()
   {
      return this._disabled;
   }
   function set disabled(value)
   {
      if(this._disabled == value)
      {
         return;
      }
      super.disabled = value;
      this.focusEnabled = this.tabEnabled = !this._disabled;
      if(this._scrollTarget)
      {
         this.tabEnabled = false;
      }
      if(this.initialized)
      {
         this.thumb.disabled = this._disabled;
      }
   }
   function setScrollProperties(pageSize, minPosition, maxPosition, pageScrollSize)
   {
      this.pageSize = pageSize;
      if(pageScrollSize != undefined)
      {
         this.pageScrollSize = pageScrollSize;
      }
      this.minPosition = minPosition;
      this.maxPosition = maxPosition;
      this.updateThumb();
   }
   function get position()
   {
      return this._position;
   }
   function set position(value)
   {
      if(value == this._position)
      {
         return;
      }
      this._position = Math.max(this.minPosition,Math.min(this.maxPosition,value));
      this.dispatchEventAndSound({type:"scroll",position:this._position});
      this.invalidate();
   }
   function update()
   {
   }
   function get scrollTarget()
   {
      return this._scrollTarget;
   }
   function set scrollTarget(value)
   {
      var _loc2_ = this._scrollTarget;
      this._scrollTarget = value;
      if(_loc2_ && value._parent != _loc2_)
      {
         _loc2_.removeListener(this);
         if(_loc2_.scrollBar != null)
         {
            _loc2_.scrollBar = null;
         }
         this.focusTarget = null;
         _loc2_.noAutoSelection = false;
      }
      if(value instanceof gfx.core.UIComponent && value.scrollBar !== null)
      {
         value.scrollBar = this;
         return;
      }
      if(this._scrollTarget == null)
      {
         this.tabEnabled = true;
         return;
      }
      this._scrollTarget.addListener(this);
      this._scrollTarget.noAutoSelection = true;
      this.focusTarget = this._scrollTarget;
      this.tabEnabled = false;
      this.onScroller();
   }
   function get availableHeight()
   {
      return (this.direction != "horizontal" ? this.__height : this.__width) - this.thumb.height + this.offsetBottom + this.offsetTop;
   }
   function toString()
   {
      return "[Scaleform ScrollIndicator " + this._name + "]";
   }
   function configUI()
   {
      super.configUI();
      if(this.track == null)
      {
         this.track = new gfx.controls.Button();
      }
      this.thumb.focusTarget = this;
      this.track.focusTarget = this;
      this.thumb.disabled = this._disabled;
      this.onRelease = function()
      {
      };
      this.useHandCursor = false;
      this.initSize();
      this.direction = this._rotation == 0 ? "vertical" : "horizontal";
      var _loc3_;
      if(this.inspectableScrollTarget != null)
      {
         _loc3_ = this._parent[this.inspectableScrollTarget];
         if(_loc3_ != null)
         {
            this.scrollTarget = _loc3_;
         }
         this.inspectableScrollTarget = null;
      }
   }
   function draw()
   {
      this.track._height = this.direction != "horizontal" ? this.__height : this.__width;
      if(this._scrollTarget instanceof TextField)
      {
         this.setScrollProperties(this._scrollTarget.bottomScroll - this._scrollTarget.scroll,1,this._scrollTarget.maxscroll);
      }
      else
      {
         this.updateThumb();
      }
   }
   function updateThumb()
   {
      if(!this.thumb.initialized)
      {
         this.invalidate();
         return undefined;
      }
      if(this._disabled)
      {
         return undefined;
      }
      var _loc5_ = Math.max(1,this.maxPosition - this.minPosition + this.pageSize);
      var _loc4_ = (this.direction != "horizontal" ? this.__height : this.__width) + this.offsetTop + this.offsetBottom;
      this.thumb.height = Math.max(10,this.pageSize / _loc5_ * _loc4_);
      var _loc2_ = (this.position - this.minPosition) / (this.maxPosition - this.minPosition);
      var _loc3_ = - this.offsetTop;
      var _loc6_ = _loc2_ * this.availableHeight + _loc3_;
      this.thumb._y = Math.max(_loc3_,Math.min(_loc4_ - this.offsetTop,_loc6_));
      this.thumb.visible = !(isNaN(_loc2_) || this.maxPosition == 0);
   }
   function onScroller()
   {
      if(this.isDragging)
      {
         return undefined;
      }
      if(this.lastVScrollPos == this._scrollTarget.scroll)
      {
         delete this.lastVScrollPos;
         return undefined;
      }
      this.setScrollProperties(this._scrollTarget.bottomScroll - this._scrollTarget.scroll,1,this._scrollTarget.maxscroll);
      this.position = this._scrollTarget.scroll;
      this.lastVScrollPos = this._scrollTarget.scroll;
      if(this.scrollerIntervalID == undefined)
      {
         this.scrollerIntervalID = setInterval(this,"scrollerDelayUpdate",10);
      }
   }
   function scrollerDelayUpdate()
   {
      this.onScroller();
      clearInterval(this.scrollerIntervalID);
      delete this.scrollerIntervalID;
   }
}
