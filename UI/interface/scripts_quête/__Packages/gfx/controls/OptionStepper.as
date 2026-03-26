class gfx.controls.OptionStepper extends gfx.core.UIComponent
{
   var _dataProvider;
   var _height;
   var _labelFunction;
   var _name;
   var _width;
   var constraints;
   var dispatchEvent;
   var focusEnabled;
   var gotoAndPlay;
   var nextBtn;
   var prevBtn;
   var selectedItem;
   var tabChildren;
   var tabEnabled;
   var textField;
   var soundMap = {theme:"default",focusIn:"focusIn",focusOut:"focusOut",change:"change"};
   var _selectedIndex = 0;
   var _labelField = "label";
   function OptionStepper()
   {
      super();
      this.tabChildren = false;
      this.focusEnabled = this.tabEnabled = !this._disabled;
      this.dataProvider = [];
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
      this.gotoAndPlay(!this._disabled ? (!this._focused ? "default" : "focused") : "disabled");
      if(!this.initialized)
      {
         return;
      }
      this.updateAfterStateChange();
      this.prevBtn.disabled = this.nextBtn.disabled = this._disabled;
   }
   function get dataProvider()
   {
      return this._dataProvider;
   }
   function set dataProvider(value)
   {
      if(value == this._dataProvider)
      {
         return;
      }
      if(this._dataProvider != null)
      {
         this._dataProvider.removeEventListener("change",this,"onDataChange");
      }
      this._dataProvider = value;
      this.selectedItem = null;
      if(this._dataProvider == null)
      {
         return;
      }
      if(value instanceof Array && !value.isDataProvider)
      {
         gfx.data.DataProvider.initialize(this._dataProvider);
      }
      else if(this._dataProvider.initialize != null)
      {
         this._dataProvider.initialize(this);
      }
      this._dataProvider.addEventListener("change",this,"onDataChange");
      this.updateSelectedItem();
   }
   function get selectedIndex()
   {
      return this._selectedIndex;
   }
   function set selectedIndex(value)
   {
      var _loc2_ = Math.max(0,Math.min(this._dataProvider.length - 1,value));
      if(_loc2_ == this._selectedIndex)
      {
         return;
      }
      this._selectedIndex = _loc2_;
      this.updateSelectedItem();
   }
   function get labelField()
   {
      return this._labelField;
   }
   function set labelField(value)
   {
      this._labelField = value;
      this.updateLabel();
   }
   function get labelFunction()
   {
      return this._labelFunction;
   }
   function set labelFunction(value)
   {
      this._labelFunction = value;
      this.updateLabel();
   }
   function itemToLabel(item)
   {
      if(item == null)
      {
         return "";
      }
      if(this._labelFunction != null)
      {
         return this._labelFunction(item);
      }
      if(this._labelField != null && item[this._labelField] != null)
      {
         return item[this._labelField];
      }
      return item.toString();
   }
   function invalidateData()
   {
      this._dataProvider.requestItemAt(this._selectedIndex,this,"populateText");
   }
   function handleInput(details, pathToFocus)
   {
      var _loc2_ = details.value == "keyDown" || details.value == "keyHold";
      switch(details.navEquivalent)
      {
         case gfx.ui.NavigationCode.RIGHT:
            if(this._selectedIndex < this._dataProvider.length - 1)
            {
               if(_loc2_)
               {
                  this.onNext();
               }
               return true;
            }
            break;
         case gfx.ui.NavigationCode.LEFT:
            if(this._selectedIndex > 0)
            {
               if(_loc2_)
               {
                  this.onPrev();
               }
               return true;
            }
            break;
         case gfx.ui.NavigationCode.HOME:
            if(!_loc2_)
            {
               this.selectedIndex = 0;
            }
            return true;
         case gfx.ui.NavigationCode.END:
            if(!_loc2_)
            {
               this.selectedIndex = this._dataProvider.length - 1;
            }
            return true;
      }
      return false;
   }
   function toString()
   {
      return "[Scaleform OptionStepper " + this._name + "]";
   }
   function configUI()
   {
      super.configUI();
      this.nextBtn.addEventListener("click",this,"onNext");
      this.prevBtn.addEventListener("click",this,"onPrev");
      this.prevBtn.focusTarget = this.nextBtn.focusTarget = this;
      this.prevBtn.tabEnabled = this.nextBtn.tabEnabled = false;
      this.prevBtn.autoRepeat = this.nextBtn.autoRepeat = true;
      this.prevBtn.disabled = this.nextBtn.disabled = this._disabled;
      this.constraints = new gfx.utils.Constraints(this,true);
      this.constraints.addElement(this.textField,gfx.utils.Constraints.ALL);
      this.updateAfterStateChange();
   }
   function draw()
   {
      if(this.sizeIsInvalid)
      {
         this._width = this.__width;
         this._height = this.__height;
      }
      super.draw();
      if(this.constraints != null)
      {
         this.constraints.update(this.__width,this.__height);
      }
   }
   function changeFocus()
   {
      this.gotoAndPlay(!this._disabled ? (!this._focused ? "default" : "focused") : "disabled");
      this.updateAfterStateChange();
      this.prevBtn.displayFocus = this.nextBtn.displayFocus = this._focused != 0;
   }
   function updateAfterStateChange()
   {
      this.validateNow();
      this.updateLabel();
      if(this.constraints != null)
      {
         this.constraints.update(this.__width,this.__height);
      }
      this.dispatchEvent({type:"stateChange",state:(!this._disabled ? (!this._focused ? "default" : "focused") : "disabled")});
   }
   function updateLabel()
   {
      if(this.selectedItem == null)
      {
         return undefined;
      }
      if(this.textField != null)
      {
         this.textField.text = this.itemToLabel(this.selectedItem);
      }
   }
   function updateSelectedItem()
   {
      this.invalidateData();
   }
   function populateText(item)
   {
      this.selectedItem = item;
      this.updateLabel();
      this.dispatchEvent({type:"change"});
   }
   function onNext(evtObj)
   {
      this.selectedIndex += 1;
   }
   function onPrev(evtObj)
   {
      this.selectedIndex -= 1;
   }
   function onDataChange(event)
   {
      this.invalidateData();
   }
}
