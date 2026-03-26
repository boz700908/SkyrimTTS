class gfx.controls.ScrollBar extends gfx.controls.ScrollIndicator
{
   var __height;
   var __width;
   var _disabled;
   var _name;
   var _rotation;
   var _scrollTarget;
   var _ymouse;
   var constraints;
   var downArrow;
   var dragOffset;
   var gotoAndPlay;
   var initialized;
   var invalidate;
   var onMouseMove;
   var onMouseUp;
   var onRelease;
   var pageSize;
   var thumb;
   var track;
   var trackDragMouseIndex;
   var upArrow;
   var trackScrollPageSize = 1;
   var _trackMode = "scrollPage";
   var trackScrollPosition = -1;
   function ScrollBar()
   {
      super();
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
      this.gotoAndPlay(!this._disabled ? "default" : "disabled");
      if(this.initialized)
      {
         this.upArrow.disabled = this._disabled;
         this.downArrow.disabled = this._disabled;
         this.track.disabled = this._disabled;
      }
   }
   function get position()
   {
      return super.position;
   }
   function set position(value)
   {
      value = Math.round(value);
      if(value == this.position)
      {
         return;
      }
      super.position = value;
      this.updateScrollTarget();
   }
   function get trackMode()
   {
      return this._trackMode;
   }
   function set trackMode(value)
   {
      if(value == this._trackMode)
      {
         return;
      }
      this._trackMode = value;
      if(this.initialized)
      {
         this.track.autoRepeat = this.trackMode == "scrollPage";
      }
   }
   function get availableHeight()
   {
      return this.track.height - this.thumb.height + this.offsetBottom + this.offsetTop;
   }
   function toString()
   {
      return "[Scaleform ScrollBar " + this._name + "]";
   }
   function configUI()
   {
      super.configUI();
      delete this.onRelease;
      if(this.upArrow)
      {
         this.upArrow.addEventListener("click",this,"scrollUp");
         this.upArrow.useHandCursor = !this._disabled;
         this.upArrow.disabled = this._disabled;
         this.upArrow.focusTarget = this;
         this.upArrow.autoRepeat = true;
      }
      if(this.downArrow)
      {
         this.downArrow.addEventListener("click",this,"scrollDown");
         this.downArrow.useHandCursor = !this._disabled;
         this.downArrow.disabled = this._disabled;
         this.downArrow.focusTarget = this;
         this.downArrow.autoRepeat = true;
      }
      this.thumb.addEventListener("press",this,"beginDrag");
      this.thumb.useHandCursor = !this._disabled;
      this.thumb.lockDragStateChange = true;
      this.track.addEventListener("press",this,"beginTrackScroll");
      this.track.addEventListener("click",this,"trackScroll");
      this.track.disabled = this._disabled;
      this.track.autoRepeat = this.trackMode == "scrollPage";
      Mouse.addListener(this);
      var _loc3_ = this._rotation;
      this._rotation = 0;
      this.constraints = new gfx.utils.Constraints(this);
      if(this.downArrow)
      {
         this.constraints.addElement(this.downArrow,gfx.utils.Constraints.BOTTOM);
      }
      this.constraints.addElement(this.track,gfx.utils.Constraints.TOP | gfx.utils.Constraints.BOTTOM);
      this._rotation = _loc3_;
   }
   function draw()
   {
      if(this.direction == "horizontal")
      {
         this.constraints.update(this.__height,this.__width);
      }
      else
      {
         this.constraints.update(this.__width,this.__height);
      }
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
      if(!this.initialized)
      {
         this.invalidate();
         return undefined;
      }
      if(this._disabled)
      {
         return undefined;
      }
      var _loc5_ = Math.max(1,this.maxPosition - this.minPosition + this.pageSize);
      var _loc4_ = this.track.height + this.offsetTop + this.offsetBottom;
      var _loc6_ = _loc4_;
      this.thumb.height = Math.max(10,Math.min(_loc4_,this.pageSize / _loc5_ * _loc6_));
      var _loc2_ = (this._position - this.minPosition) / (this.maxPosition - this.minPosition);
      var _loc3_ = this.track._y - this.offsetTop;
      var _loc7_ = _loc2_ * this.availableHeight + _loc3_;
      this.thumb._y = Math.max(_loc3_,Math.min(this.track._y + this.track.height - this.thumb.height + this.offsetBottom,_loc7_));
      this.thumb.visible = !(isNaN(_loc2_) || this.maxPosition <= 0 || this.maxPosition == Infinity);
      if(this.thumb.visible)
      {
         this.track.disabled = false;
         if(this.upArrow)
         {
            if(this._position == this.minPosition)
            {
               this.upArrow.disabled = true;
            }
            else
            {
               this.upArrow.disabled = false;
            }
         }
         if(this.downArrow)
         {
            if(this._position == this.maxPosition)
            {
               this.downArrow.disabled = true;
            }
            else
            {
               this.downArrow.disabled = false;
            }
         }
      }
      else
      {
         if(this.upArrow)
         {
            this.upArrow.disabled = true;
         }
         if(this.downArrow)
         {
            this.downArrow.disabled = true;
         }
         this.track.disabled = true;
      }
   }
   function scrollUp()
   {
      this.position -= this.pageScrollSize;
   }
   function scrollDown()
   {
      this.position += this.pageScrollSize;
   }
   function beginDrag()
   {
      if(this.isDragging == true)
      {
         return undefined;
      }
      this.isDragging = true;
      this.onMouseMove = this.doDrag;
      this.onMouseUp = this.endDrag;
      this.dragOffset = {y:this._ymouse - this.thumb._y};
   }
   function doDrag()
   {
      var _loc2_ = (this._ymouse - this.dragOffset.y - this.track._y) / this.availableHeight;
      this.position = this.minPosition + _loc2_ * (this.maxPosition - this.minPosition);
   }
   function endDrag()
   {
      delete this.onMouseUp;
      delete this.onMouseMove;
      this.isDragging = false;
      if(this.trackDragMouseIndex != undefined)
      {
         if(!this.thumb.hitTest(_root._xmouse,_root._ymouse))
         {
            this.thumb.onReleaseOutside(this.trackDragMouseIndex);
         }
         else
         {
            this.thumb.onRelease(this.trackDragMouseIndex);
         }
      }
      delete this.trackDragMouseIndex;
   }
   function beginTrackScroll(e)
   {
      var _loc2_ = (this._ymouse - this.thumb.height / 2 - this.track._y) / this.availableHeight;
      this.trackScrollPosition = Math.round(_loc2_ * (this.maxPosition - this.minPosition) + this.minPosition);
      if(Key.isDown(16) || this.trackMode == "scrollToCursor")
      {
         this.position = this.trackScrollPosition;
         this.trackDragMouseIndex = e.controllerIdx;
         this.thumb.onPress(this.trackDragMouseIndex);
         this.dragOffset = {y:this.thumb.height / 2};
      }
   }
   function trackScroll()
   {
      if(this.isDragging || this.position == this.trackScrollPosition)
      {
         return undefined;
      }
      var _loc3_ = this.position >= this.trackScrollPosition ? - this.trackScrollPageSize : this.trackScrollPageSize;
      var _loc2_ = this.position + _loc3_;
      this.position = _loc3_ >= 0 ? Math.min(_loc2_,this.trackScrollPosition) : Math.max(_loc2_,this.trackScrollPosition);
   }
   function updateScrollTarget()
   {
      if(this._scrollTarget == null)
      {
         return undefined;
      }
      if(this._scrollTarget && !this._disabled)
      {
         this._scrollTarget.scroll = this._position;
      }
   }
   function scrollWheel(delta)
   {
      this.position -= delta * this.pageScrollSize;
   }
}
