class gfx.controls.ButtonBar extends gfx.core.UIComponent
{
   var _dataProvider;
   var _labelFunction;
   var _name;
   var attachMovie;
   var focusEnabled;
   var getNextHighestDepth;
   var renderers;
   var tabChildren;
   var tabEnabled;
   var _itemRenderer = "Button";
   var _spacing = 0;
   var _direction = "horizontal";
   var _selectedIndex = -1;
   var _autoSize = "none";
   var _buttonWidth = 0;
   var _labelField = "label";
   var reflowing = false;
   function ButtonBar()
   {
      super();
      this.renderers = [];
      this.focusEnabled = this.tabEnabled = !this._disabled;
      this.tabChildren = false;
   }
   function get disabled()
   {
      return this._disabled;
   }
   function set disabled(value)
   {
      super.disabled = value;
      this.focusEnabled = this.tabEnabled = !this._disabled;
      if(!this.initialized)
      {
         return;
      }
      var _loc3_ = 0;
      while(_loc3_ < this.renderers.length)
      {
         this.renderers[_loc3_].disabled = this._disabled;
         _loc3_ = _loc3_ + 1;
      }
   }
   function get dataProvider()
   {
      return this._dataProvider;
   }
   function set dataProvider(value)
   {
      if(this._dataProvider == value)
      {
         return;
      }
      if(this._dataProvider != null)
      {
         this._dataProvider.removeEventListener("change",this,"onDataChange");
      }
      this._dataProvider = value;
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
      this.selectedIndex = 0;
      this.tabEnabled = this.focusEnabled = !this._disabled && this._dataProvider.length > 0;
      this.reflowing = false;
      this.invalidate();
   }
   function invalidateData()
   {
      this.selectedIndex = Math.min(this._dataProvider.length - 1,this._selectedIndex);
      this.populateData();
      this.invalidate();
   }
   function get itemRenderer()
   {
      return this._itemRenderer;
   }
   function set itemRenderer(value)
   {
      this._itemRenderer = value;
      while(this.renderers.length > 0)
      {
         this.renderers.pop().removeMovieClip();
      }
      this.invalidate();
   }
   function get spacing()
   {
      return this._spacing;
   }
   function set spacing(value)
   {
      this._spacing = value;
      this.invalidate();
   }
   function get direction()
   {
      return this._direction;
   }
   function set direction(value)
   {
      this._direction = value;
      this.invalidate();
   }
   function get autoSize()
   {
      return this._autoSize;
   }
   function set autoSize(value)
   {
      if(value == this._autoSize)
      {
         return;
      }
      this._autoSize = value;
      var _loc2_ = 0;
      while(_loc2_ < this.renderers.length)
      {
         this.renderers[_loc2_].autoSize = this._autoSize;
         _loc2_ = _loc2_ + 1;
      }
      this.invalidate();
   }
   function get buttonWidth()
   {
      return this._buttonWidth;
   }
   function set buttonWidth(value)
   {
      this._buttonWidth = value;
      this.invalidate();
   }
   function get selectedIndex()
   {
      return this._selectedIndex;
   }
   function set selectedIndex(value)
   {
      if(this._selectedIndex == value)
      {
         return;
      }
      this._selectedIndex = value;
      this.selectItem(this._selectedIndex);
      this.dispatchEventAndSound({type:"change",index:this._selectedIndex,renderer:this.renderers[this._selectedIndex],item:this.selectedItem,data:this.selectedItem.data});
   }
   function get selectedItem()
   {
      return this._dataProvider.requestItemAt(this._selectedIndex);
   }
   function get data()
   {
      return this.selectedItem.data;
   }
   function get labelField()
   {
      return this._labelField;
   }
   function set labelField(value)
   {
      this._labelField = value;
      this.invalidate();
   }
   function get labelFunction()
   {
      return this._labelFunction;
   }
   function set labelFunction(value)
   {
      this._labelFunction = value;
      this.invalidate();
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
   function handleInput(details, pathToFocus)
   {
      var _loc4_ = details.value == "keyDown" || details.value == "keyHold";
      var _loc2_;
      switch(details.navEquivalent)
      {
         case gfx.ui.NavigationCode.LEFT:
            if(this._direction == "horizontal")
            {
               _loc2_ = this._selectedIndex - 1;
            }
            break;
         case gfx.ui.NavigationCode.RIGHT:
            if(this._direction == "horizontal")
            {
               _loc2_ = this._selectedIndex + 1;
            }
            break;
         case gfx.ui.NavigationCode.UP:
            if(this._direction == "vertical")
            {
               _loc2_ = this._selectedIndex - 1;
            }
            break;
         case gfx.ui.NavigationCode.DOWN:
            if(this._direction == "vertical")
            {
               _loc2_ = this._selectedIndex + 1;
            }
      }
      if(_loc2_ != null)
      {
         _loc2_ = Math.max(0,Math.min(this._dataProvider.length - 1,_loc2_));
         if(_loc2_ != this._selectedIndex)
         {
            if(!_loc4_)
            {
               return true;
            }
            this.selectedIndex = _loc2_;
            return true;
         }
      }
      return false;
   }
   function toString()
   {
      return "[Scaleform ButtonBar " + this._name + "]";
   }
   function draw()
   {
      var _loc3_;
      var _loc2_;
      if(!this.reflowing)
      {
         _loc3_ = this._dataProvider.length;
         while(this.renderers.length > _loc3_)
         {
            _loc2_ = MovieClip(this.renderers.pop());
            _loc2_.group.removeButton(_loc2_);
            _loc2_.removeMovieClip();
         }
         while(this.renderers.length < _loc3_)
         {
            this.renderers.push(this.createRenderer(this.renderers.length));
         }
         this.populateData();
         this.reflowing = true;
         this.invalidate();
         return undefined;
      }
      if(this.drawLayout() && this._selectedIndex != -1)
      {
         this.selectItem(this._selectedIndex);
      }
   }
   function drawLayout()
   {
      if(this.renderers.length > 0 && !this.renderers[this.renderers.length - 1].initialized)
      {
         this.reflowing = true;
         this.invalidate();
         return false;
      }
      this.reflowing = false;
      var _loc5_ = 0;
      var _loc4_ = 0;
      var _loc3_ = 0;
      var _loc2_;
      while(_loc3_ < this.renderers.length)
      {
         _loc2_ = this.renderers[_loc3_];
         if(this._autoSize == "none" && this._buttonWidth > 0)
         {
            _loc2_.width = this._buttonWidth;
         }
         if(this._direction == "horizontal")
         {
            _loc2_._y = 0;
            _loc2_._x = _loc5_;
            _loc5_ += _loc2_.width + this._spacing;
         }
         else
         {
            _loc2_._x = 0;
            _loc2_._y = _loc4_;
            _loc4_ += _loc2_.height + this._spacing;
         }
         _loc3_ = _loc3_ + 1;
      }
      return true;
   }
   function createRenderer(index)
   {
      var _loc2_ = this.attachMovie(this.itemRenderer,"clip" + index,this.getNextHighestDepth(),{toggle:true,focusTarget:this,tabEnabled:false,autoSize:this._autoSize});
      if(_loc2_ == null)
      {
         return null;
      }
      _loc2_.addEventListener("click",this,"handleItemClick");
      _loc2_.index = index;
      _loc2_.groupName = this._name + "ButtonGroup";
      return _loc2_;
   }
   function handleItemClick(event)
   {
      var _loc2_ = event.target;
      var _loc5_ = _loc2_.index;
      this.selectedIndex = _loc5_;
      this.dispatchEventAndSound({type:"itemClick",data:this.selectedItem.data,item:this.selectedItem,index:_loc5_,controllerIdx:event.controllerIdx});
   }
   function selectItem(index)
   {
      if(this.renderers.length < 1)
      {
         return undefined;
      }
      var _loc6_ = this.renderers[index];
      if(!_loc6_.selected)
      {
         _loc6_.selected = true;
      }
      var _loc4_ = this.renderers.length;
      var _loc2_ = 0;
      var _loc3_;
      while(_loc2_ < _loc4_)
      {
         if(_loc2_ != index)
         {
            _loc3_ = 100 + _loc4_ - _loc2_;
            this.renderers[_loc2_].swapDepths(_loc3_);
            this.renderers[_loc2_].displayFocus = false;
         }
         _loc2_ = _loc2_ + 1;
      }
      _loc6_.swapDepths(1000);
      _loc6_.displayFocus = this._focused;
   }
   function changeFocus()
   {
      var _loc2_ = this.renderers[this._selectedIndex];
      if(_loc2_ == null)
      {
         return undefined;
      }
      _loc2_.displayFocus = this._focused;
   }
   function onDataChange(event)
   {
      this.invalidateData();
   }
   function populateData()
   {
      var _loc2_ = 0;
      var _loc3_;
      while(_loc2_ < this.renderers.length)
      {
         _loc3_ = this.renderers[_loc2_];
         _loc3_.label = this.itemToLabel(this._dataProvider.requestItemAt(_loc2_));
         _loc3_.data = this._dataProvider.requestItemAt(_loc2_);
         _loc3_.disabled = this._disabled;
         _loc2_ = _loc2_ + 1;
      }
   }
}
