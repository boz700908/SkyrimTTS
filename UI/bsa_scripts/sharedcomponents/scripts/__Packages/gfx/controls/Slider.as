class gfx.controls.Slider extends gfx.core.UIComponent
{
   var _name;
   var _xmouse;
   var constraints;
   var dispatchEvent;
   var dragOffset;
   var focusEnabled;
   var gotoAndPlay;
   var onMouseMove;
   var onMouseUp;
   var tabChildren;
   var tabEnabled;
   var thumb;
   var track;
   var trackDragMouseIndex;
   var liveDragging = false;
   var state = "default";
   var _minimum = 0;
   var _maximum = 10;
   var _value = 0;
   var _snapInterval = 1;
   var _snapping = false;
   var offsetLeft = 0;
   var offsetRight = 0;
   function Slider()
   {
      super();
      this.tabChildren = false;
      this.focusEnabled = this.tabEnabled = !this._disabled;
   }
   function get maximum()
   {
      return this._maximum;
   }
   function set maximum(value)
   {
      this._maximum = value;
      this.invalidate();
   }
   function get minimum()
   {
      return this._minimum;
   }
   function set minimum(value)
   {
      this._minimum = value;
      this.invalidate();
   }
   function get value()
   {
      return this._value;
   }
   function set value(value)
   {
      this._value = this.lockValue(value);
      this.invalidate();
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
      if(!this.initialized)
      {
         return;
      }
      this.thumb.disabled = this.track.disabled = this._disabled;
      this.invalidate();
   }
   function get position()
   {
      return this._value;
   }
   function set position(value)
   {
      this.value = value;
   }
   function get snapping()
   {
      return this._snapping;
   }
   function set snapping(value)
   {
      this._snapping = value;
      this.invalidate();
   }
   function get snapInterval()
   {
      return this._snapInterval;
   }
   function set snapInterval(value)
   {
      this._snapInterval = value;
      this.invalidate();
   }
   function handleInput(details, pathToFocus)
   {
      var _loc2_ = details.value == "keyDown";
      switch(details.navEquivalent)
      {
         case gfx.ui.NavigationCode.RIGHT:
            if(_loc2_)
            {
               this.value += this._snapInterval;
               this.dispatchEvent({type:"change"});
            }
            break;
         case gfx.ui.NavigationCode.LEFT:
            if(_loc2_)
            {
               this.value -= this._snapInterval;
               this.dispatchEvent({type:"change"});
            }
            break;
         case gfx.ui.NavigationCode.HOME:
            if(!_loc2_)
            {
               this.value = this.minimum;
               this.dispatchEvent({type:"change"});
            }
            break;
         case gfx.ui.NavigationCode.END:
            if(!_loc2_)
            {
               this.value = this.maximum;
               this.dispatchEvent({type:"change"});
            }
            break;
         default:
            return false;
      }
      return true;
   }
   function toString()
   {
      return "[Scaleform Slider " + this._name + "]";
   }
   function configUI()
   {
      this.thumb.addEventListener("press",this,"beginDrag");
      this.track.addEventListener("press",this,"trackPress");
      this.thumb.focusTarget = this.track.focusTarget = this;
      this.thumb.disabled = this.track.disabled = this._disabled;
      this.thumb.lockDragStateChange = true;
      this.initSize();
      this.constraints = new gfx.utils.Constraints(this);
      this.constraints.addElement(this.track,gfx.utils.Constraints.LEFT | gfx.utils.Constraints.RIGHT);
      Mouse.addListener(this);
   }
   function draw()
   {
      this.gotoAndPlay(!this._disabled ? (!this._focused ? "default" : "focused") : "disabled");
      if(!this._disabled)
      {
         this.track.displayFocus = this.thumb.displayFocus = false;
         if(this._focused)
         {
            this.track.displayFocus = this.thumb.displayFocus = this._focused;
         }
      }
      this.constraints.update(this.__width,this.__height);
      this.updateThumb();
   }
   function changeFocus()
   {
      this.invalidate();
   }
   function updateThumb()
   {
      if(this._disabled)
      {
         return undefined;
      }
      var _loc2_ = this.__width - this.offsetLeft - this.offsetRight;
      this.thumb._x = (this._value - this._minimum) / (this._maximum - this._minimum) * _loc2_ - this.thumb._width / 2 + this.offsetLeft;
   }
   function beginDrag(event)
   {
      Selection.setFocus(this.thumb);
      this.dragOffset = {x:this._xmouse - this.thumb._x - this.thumb._width / 2};
      this.onMouseMove = this.doDrag;
      this.onMouseUp = this.endDrag;
   }
   function doDrag()
   {
      var _loc3_ = this._xmouse - this.dragOffset.x;
      var _loc4_ = this.__width - this.offsetLeft - this.offsetRight;
      var _loc2_ = this.lockValue((_loc3_ - this.offsetLeft) / _loc4_ * (this._maximum - this._minimum) + this._minimum);
      this.updateThumb();
      if(this.value == _loc2_)
      {
         return undefined;
      }
      this._value = _loc2_;
      if(this.liveDragging)
      {
         this.dispatchEvent({type:"change"});
      }
   }
   function endDrag()
   {
      delete this.onMouseUp;
      delete this.onMouseMove;
      if(!this.liveDragging)
      {
         this.dispatchEvent({type:"change"});
      }
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
   function trackPress(e)
   {
      Selection.setFocus(this.track);
      var _loc3_ = this.__width - this.offsetLeft - this.offsetRight;
      var _loc2_ = this.lockValue((this._xmouse - this.offsetLeft) / _loc3_ * (this._maximum - this._minimum) + this._minimum);
      if(this.value == _loc2_)
      {
         return undefined;
      }
      this.value = _loc2_;
      if(this.liveDragging)
      {
         this.dispatchEvent({type:"change"});
      }
      this.trackDragMouseIndex = e.mouseIndex;
      this.thumb.onPress(this.trackDragMouseIndex);
      this.dragOffset = {x:0};
   }
   function lockValue(value)
   {
      value = Math.max(this._minimum,Math.min(this._maximum,value));
      if(!this.snapping)
      {
         return value;
      }
      return Math.round(value / this.snapInterval) * this.snapInterval;
   }
   function scrollWheel(delta)
   {
      if(this._focused)
      {
         this.value -= delta * this._snapInterval;
         this.dispatchEvent({type:"change"});
      }
   }
}
