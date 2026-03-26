class gfx.controls.Button extends gfx.core.UIComponent
{
   var _group;
   var _height;
   var _label;
   var _name;
   var _parent;
   var _width;
   var buttonRepeatInterval;
   var constraints;
   var dispatchEvent;
   var doubleClickInterval;
   var focusEnabled;
   var focusIndicator;
   var gotoAndPlay;
   var inspectableGroupName;
   var onDragOut;
   var onDragOver;
   var onPress;
   var onRelease;
   var onReleaseOutside;
   var onRollOut;
   var onRollOver;
   var tabEnabled;
   var textField;
   var state = "up";
   var toggle = false;
   var doubleClickEnabled = false;
   var autoRepeat = false;
   var lockDragStateChange = false;
   var _selected = false;
   var _autoSize = false;
   var _disableFocus = false;
   var _disableConstraints = false;
   var doubleClickDuration = 250;
   var buttonRepeatDuration = 100;
   var buttonRepeatDelay = 100;
   var pressedByKeyboard = false;
   var stateMap = {up:["up"],over:["over"],down:["down"],release:["release","over"],out:["out","up"],disabled:["disabled"],selecting:["selecting","over"],kb_selecting:["kb_selecting","up"],kb_release:["kb_release","out","up"],kb_down:["kb_down","down"]};
   function Button()
   {
      super();
      this.focusEnabled = this.tabEnabled = !this._disableFocus ? !this._disabled : false;
      if(this.inspectableGroupName != null && this.inspectableGroupName != "")
      {
         this.group = this.inspectableGroupName;
      }
   }
   function get labelID()
   {
      return null;
   }
   function set labelID(value)
   {
      if(value != "")
      {
         this.label = gfx.utils.Locale.getTranslatedString(value);
      }
   }
   function get label()
   {
      return this._label;
   }
   function set label(value)
   {
      this._label = value;
      if(this.textField != null)
      {
         this.textField.text = this._label;
      }
      if(this._autoSize && this.textField != null && this.initialized)
      {
         this.__width = this._width = this.calculateWidth();
      }
      this.updateAfterStateChange();
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
      this.clearRepeatInterval();
      this.focusEnabled = this.tabEnabled = !this._disableFocus ? !this._disabled : false;
      this.setState(!this._disabled ? "up" : "disabled");
   }
   function get selected()
   {
      return this._selected;
   }
   function set selected(value)
   {
      if(this._selected == value)
      {
         return;
      }
      this._selected = value;
      if(!this._disabled)
      {
         if(!this._focused)
         {
            this.setState(!(this.displayFocus && this.focusIndicator == null) ? "up" : "over");
         }
         else if(this.pressedByKeyboard && this.focusIndicator != null)
         {
            this.setState("kb_selecting");
         }
         else
         {
            this.setState("selecting");
         }
      }
      if(this.dispatchEvent != null)
      {
         this.dispatchEvent({type:"select",selected:this._selected});
      }
   }
   function get groupName()
   {
      return this._group != null ? this._group.name : null;
   }
   function set groupName(value)
   {
      this.group = value;
   }
   function get group()
   {
      return this._group;
   }
   function set group(value)
   {
      var _loc2_ = gfx.controls.ButtonGroup(value);
      if(typeof value == "string")
      {
         _loc2_ = this._parent["_buttonGroup_" + value];
         if(_loc2_ == null)
         {
            this._parent["_buttonGroup_" + value] = _loc2_ = new gfx.controls.ButtonGroup(value.toString(),this._parent);
         }
      }
      if(this._group == _loc2_)
      {
         return;
      }
      if(this._group != null)
      {
         this._group.removeButton(this);
      }
      this._group = _loc2_;
      if(this._group != null)
      {
         _loc2_.addButton(this);
      }
   }
   function get disableFocus()
   {
      return this._disableFocus;
   }
   function set disableFocus(value)
   {
      this._disableFocus = value;
      this.focusEnabled = this.tabEnabled = !this._disableFocus ? !this._disabled : false;
   }
   function get disableConstraints()
   {
      return this._disableConstraints;
   }
   function set disableConstraints(value)
   {
      this._disableConstraints = value;
   }
   function get autoSize()
   {
      return this._autoSize;
   }
   function set autoSize(value)
   {
      this._autoSize = value;
      if(value && this.initialized)
      {
         this.width = this.calculateWidth();
      }
   }
   function setSize(width, height)
   {
      super.setSize(width,height);
   }
   function handleInput(details, pathToFocus)
   {
      var _loc0_;
      if((_loc0_ = details.navEquivalent) !== gfx.ui.NavigationCode.ENTER)
      {
         return false;
      }
      if(details.value == "keyDown")
      {
         if(!this.pressedByKeyboard)
         {
            this.handlePress();
         }
      }
      else
      {
         this.handleRelease();
      }
      return true;
   }
   function toString()
   {
      return "[Scaleform Button " + this._name + "]";
   }
   function configUI()
   {
      this.constraints = new gfx.utils.Constraints(this,true);
      if(!this._disableConstraints)
      {
         this.constraints.addElement(this.textField,gfx.utils.Constraints.ALL);
      }
      super.configUI();
      if(this._autoSize)
      {
         this.sizeIsInvalid = true;
      }
      this.onRollOver = this.handleMouseRollOver;
      this.onRollOut = this.handleMouseRollOut;
      this.onPress = this.handleMousePress;
      this.onRelease = this.handleMouseRelease;
      this.onDragOver = this.handleDragOver;
      this.onDragOut = this.handleDragOut;
      this.onReleaseOutside = this.handleReleaseOutside;
      if(this.focusIndicator != null && !this._focused && this.focusIndicator._totalFrames == 1)
      {
         this.focusIndicator._visible = false;
      }
      this.updateAfterStateChange();
   }
   function draw()
   {
      if(this.sizeIsInvalid)
      {
         if(this._autoSize && this.textField != null)
         {
            this.__width = this.calculateWidth();
         }
         this._width = this.__width;
         this._height = this.__height;
      }
      this.constraints.update(this.__width,this.__height);
   }
   function updateAfterStateChange()
   {
      if(!this.initialized)
      {
         return undefined;
      }
      this.validateNow();
      if(this.textField != null && this._label != null)
      {
         this.textField.text = this._label;
      }
      if(this.constraints != null)
      {
         this.constraints.update(this.width,this.height);
      }
      this.dispatchEvent({type:"stateChange",state:this.state});
   }
   function calculateWidth()
   {
      if(this.constraints == null)
      {
         this.invalidate();
         return undefined;
      }
      var _loc2_ = this.constraints.getElement(this.textField).metrics;
      var _loc3_ = this.textField.textWidth + _loc2_.left + _loc2_.right + 5;
      return _loc3_;
   }
   function setState(state)
   {
      this.state = state;
      var _loc5_ = this.getStatePrefixes();
      var _loc3_ = this.stateMap[state];
      if(_loc3_ == null || _loc3_.length == 0)
      {
         return undefined;
      }
      var _loc4_;
      var _loc2_;
      do
      {
         _loc4_ = _loc5_.pop().toString();
         _loc2_ = _loc3_.length - 1;
         while(_loc2_ >= 0)
         {
            this.gotoAndPlay(_loc4_ + _loc3_[_loc2_]);
            _loc2_ = _loc2_ - 1;
         }
      }
      while(_loc5_.length > 0);
      this.updateAfterStateChange();
   }
   function getStatePrefixes()
   {
      return !this._selected ? [""] : ["selected_",""];
   }
   function changeFocus()
   {
      if(this._disabled)
      {
         return undefined;
      }
      if(this.focusIndicator == null)
      {
         this.setState(!(this._focused || this._displayFocus) ? "out" : "over");
      }
      if(this.focusIndicator != null)
      {
         if(this.focusIndicator._totalframes == 1)
         {
            this.focusIndicator._visible = this._focused;
         }
         else
         {
            this.focusIndicator.gotoAndPlay(!this._focused ? "hide" : "show");
         }
         if(this.pressedByKeyboard && !this._focused)
         {
            this.setState("kb_release");
            this.pressedByKeyboard = false;
         }
      }
   }
   function handleMouseRollOver(mouseIndex)
   {
      if(this._disabled)
      {
         return undefined;
      }
      if(!this._focused && !this._displayFocus || this.focusIndicator != null)
      {
         this.setState("over");
      }
      this.dispatchEvent({type:"rollOver",mouseIndex:mouseIndex});
   }
   function handleMouseRollOut(mouseIndex)
   {
      if(this._disabled)
      {
         return undefined;
      }
      if(!this._focused && !this._displayFocus || this.focusIndicator != null)
      {
         this.setState("out");
      }
      this.dispatchEvent({type:"rollOut",mouseIndex:mouseIndex});
   }
   function handleMousePress(mouseIndex, button)
   {
      if(this._disabled)
      {
         return undefined;
      }
      if(!this._disableFocus)
      {
         Selection.setFocus(this);
      }
      this.setState("down");
      this.dispatchEvent({type:"press",mouseIndex:mouseIndex,button:button});
      if(this.autoRepeat)
      {
         this.buttonRepeatInterval = setInterval(this,"beginButtonRepeat",this.buttonRepeatDelay,mouseIndex,button);
      }
   }
   function handlePress()
   {
      if(this._disabled)
      {
         return undefined;
      }
      this.pressedByKeyboard = true;
      this.setState(this.focusIndicator != null ? "kb_down" : "down");
      this.dispatchEvent({type:"press"});
   }
   function handleMouseRelease(mouseIndex, button)
   {
      if(this._disabled)
      {
         return undefined;
      }
      clearInterval(this.buttonRepeatInterval);
      delete this.buttonRepeatInterval;
      if(this.doubleClickEnabled)
      {
         if(this.doubleClickInterval != null)
         {
            this.doubleClickExpired();
            this.dispatchEvent({type:"doubleClick",mouseIndex:mouseIndex,button:button});
            this.setState("release");
            return undefined;
         }
         this.doubleClickInterval = setInterval(this,"doubleClickExpired",this.doubleClickDuration);
      }
      this.setState("release");
      this.handleClick(mouseIndex,button);
   }
   function handleRelease()
   {
      if(this._disabled)
      {
         return undefined;
      }
      this.setState(this.focusIndicator != null ? "kb_release" : "release");
      this.handleClick(null);
      this.pressedByKeyboard = false;
   }
   function handleClick(mouseIndex, button)
   {
      if(this.toggle)
      {
         this.selected = !this._selected;
      }
      this.dispatchEvent({type:"click",mouseIndex:mouseIndex,button:button});
   }
   function handleDragOver(mouseIndex)
   {
      if(this._disabled || this.lockDragStateChange)
      {
         return undefined;
      }
      if(this._focused || this._displayFocus)
      {
         this.setState(this.focusIndicator != null ? "kb_down" : "down");
      }
      else
      {
         this.setState("over");
      }
      this.dispatchEvent({type:"dragOver",mouseIndex:mouseIndex});
   }
   function handleDragOut(mouseIndex)
   {
      if(this._disabled || this.lockDragStateChange)
      {
         return undefined;
      }
      if(this._focused || this._displayFocus)
      {
         this.setState(this.focusIndicator != null ? "kb_release" : "release");
      }
      else
      {
         this.setState("out");
      }
      this.dispatchEvent({type:"dragOut",mouseIndex:mouseIndex});
   }
   function handleReleaseOutside(mouseIndex, button)
   {
      this.clearRepeatInterval();
      if(this._disabled)
      {
         return undefined;
      }
      if(this.lockDragStateChange)
      {
         if(this._focused || this._displayFocus)
         {
            this.setState(this.focusIndicator != null ? "kb_release" : "release");
         }
         else
         {
            this.setState("kb_release");
         }
      }
      this.dispatchEvent({type:"releaseOutside",state:this.state,button:button});
   }
   function doubleClickExpired()
   {
      clearInterval(this.doubleClickInterval);
      delete this.doubleClickInterval;
   }
   function beginButtonRepeat(mouseIndex, button)
   {
      this.clearRepeatInterval();
      this.buttonRepeatInterval = setInterval(this,"handleButtonRepeat",this.buttonRepeatDuration,mouseIndex,button);
   }
   function handleButtonRepeat(mouseIndex, button)
   {
      this.dispatchEvent({type:"click",mouseIndex:mouseIndex,button:button});
   }
   function clearRepeatInterval()
   {
      clearInterval(this.buttonRepeatInterval);
      delete this.buttonRepeatInterval;
   }
}
