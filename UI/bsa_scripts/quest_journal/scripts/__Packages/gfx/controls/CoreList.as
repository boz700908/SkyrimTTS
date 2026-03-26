class gfx.controls.CoreList extends gfx.core.UIComponent
{
   var __height;
   var __width;
   var _dataProvider;
   var _labelFunction;
   var _name;
   var _parent;
   var container;
   var createEmptyMovieClip;
   var dispatchEventAndSound;
   var focusEnabled;
   var inspectableRendererInstanceName;
   var invalidate;
   var owner;
   var renderers;
   var tabEnabled;
   var soundMap = {theme:"default",focusIn:"focusIn",focusOut:"focusOut",select:"select",change:"change",rollOver:"rollOver",rollOut:"rollOut",itemClick:"itemClick",itemDoubleClick:"itemDoubleClick",itemPress:"itemPress",itemRollOver:"itemRollOver",itemRollOut:"itemRollOut"};
   var _itemRenderer = "ListItemRenderer";
   var _selectedIndex = -1;
   var _labelField = "label";
   var externalRenderers = false;
   var deferredScrollIndex = -1;
   function CoreList()
   {
      super();
      this.renderers = [];
      this.dataProvider = [];
      if(this.container == undefined)
      {
         this.container = this.createEmptyMovieClip("container",1);
      }
      this.container.scale9Grid = new flash.geom.Rectangle(0,0,1,1);
      this.tabEnabled = this.focusEnabled = true;
   }
   function get itemRenderer()
   {
      return this._itemRenderer;
   }
   function set itemRenderer(value)
   {
      if(value == this._itemRenderer || value == "")
      {
         return;
      }
      this._itemRenderer = value;
      this.resetRenderers();
      this.invalidate();
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
      this.invalidate();
   }
   function get selectedIndex()
   {
      return this._selectedIndex;
   }
   function set selectedIndex(value)
   {
      var _loc3_ = this._selectedIndex;
      this._selectedIndex = value;
      this.dispatchEventAndSound({type:"change",index:this._selectedIndex,lastIndex:_loc3_});
   }
   function scrollToIndex(index)
   {
   }
   function get labelField()
   {
      return this._labelField;
   }
   function set labelField(value)
   {
      this._labelField = value;
      this.invalidateData();
   }
   function get labelFunction()
   {
      return this._labelFunction;
   }
   function set labelFunction(value)
   {
      this._labelFunction = value;
      this.invalidateData();
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
   }
   function get availableWidth()
   {
      return this.__width;
   }
   function get availableHeight()
   {
      return this.__height;
   }
   function setRendererList(value)
   {
      var _loc3_;
      var _loc2_;
      if(this.externalRenderers)
      {
         _loc3_ = 0;
         while(_loc3_ < this.renderers.length)
         {
            _loc2_ = this.renderers[_loc3_];
            _loc2_.owner = null;
            _loc2_.removeEventListener("click",this,"handleItemClick");
            _loc2_.removeEventListener("rollOver",this,"dispatchItemEvent");
            _loc2_.removeEventListener("rollOut",this,"dispatchItemEvent");
            _loc2_.removeEventListener("press",this,"dispatchItemEvent");
            _loc2_.removeEventListener("doubleClick",this,"dispatchItemEvent");
            Mouse.removeListener(_loc2_);
            _loc3_ = _loc3_ + 1;
         }
      }
      else
      {
         this.resetRenderers();
      }
      this.externalRenderers = value != null;
      if(this.externalRenderers)
      {
         this.renderers = value;
      }
      this.invalidate();
   }
   function get rendererInstanceName()
   {
      return null;
   }
   function set rendererInstanceName(value)
   {
      if(value == null || value == "")
      {
         return;
      }
      var _loc3_ = 0;
      var _loc4_ = [];
      var _loc2_;
      while(true)
      {
         _loc3_ = _loc3_ + 1;
         _loc2_ = this._parent[value + _loc3_];
         if(_loc2_ == null && _loc3_ > 0)
         {
            break;
         }
         if(_loc2_ != null)
         {
            this.setUpRenderer(_loc2_);
            Mouse.addListener(_loc2_);
            _loc2_.scrollWheel = function(delta)
            {
               this.owner.scrollWheel(delta);
            };
            _loc4_.push(_loc2_);
         }
      }
      if(_loc4_.length == 0)
      {
         _loc4_ = null;
      }
      this.setRendererList(_loc4_);
   }
   function toString()
   {
      return "[Scaleform CoreList " + this._name + "]";
   }
   function configUI()
   {
      super.configUI();
      if(this._selectedIndex > -1)
      {
         this.deferredScrollIndex = this._selectedIndex;
      }
      if(this.inspectableRendererInstanceName != "")
      {
         this.rendererInstanceName = this.inspectableRendererInstanceName;
      }
      Mouse.addListener(this);
   }
   function createItemRenderer(index)
   {
      var _loc2_ = this.container.attachMovie(this._itemRenderer,"renderer" + index,index);
      if(_loc2_ == null)
      {
         return null;
      }
      this.setUpRenderer(_loc2_);
      return _loc2_;
   }
   function setUpRenderer(clip)
   {
      clip.owner = this;
      clip.tabEnabled = false;
      clip.doubleClickEnabled = true;
      clip.addEventListener("press",this,"dispatchItemEvent");
      clip.addEventListener("click",this,"handleItemClick");
      clip.addEventListener("doubleClick",this,"dispatchItemEvent");
      clip.addEventListener("rollOver",this,"dispatchItemEvent");
      clip.addEventListener("rollOut",this,"dispatchItemEvent");
   }
   function createItemRenderers(startIndex, endIndex)
   {
      var _loc3_ = [];
      var _loc2_ = startIndex;
      while(_loc2_ <= endIndex)
      {
         _loc3_.push(this.createItemRenderer[_loc2_]);
         _loc2_ = _loc2_ + 1;
      }
      return _loc3_;
   }
   function draw()
   {
      if(this.deferredScrollIndex != -1)
      {
         this.scrollToIndex(this.deferredScrollIndex);
         this.deferredScrollIndex = -1;
      }
   }
   function drawRenderers(totalRenderers)
   {
      while(this.renderers.length > totalRenderers)
      {
         this.renderers.pop().removeMovieClip();
      }
      while(this.renderers.length < totalRenderers)
      {
         this.renderers.push(this.createItemRenderer(this.renderers.length));
      }
   }
   function getRendererAt(index)
   {
      return this.renderers[index];
   }
   function resetRenderers()
   {
      while(this.renderers.length > 0)
      {
         this.renderers.pop().removeMovieClip();
      }
   }
   function drawLayout(rendererWidth, rendererHeight)
   {
   }
   function onDataChange(event)
   {
      this.invalidateData();
   }
   function dispatchItemEvent(event)
   {
      var _loc9_;
      switch(event.type)
      {
         case "press":
            _loc9_ = "itemPress";
            break;
         case "click":
            _loc9_ = "itemClick";
            break;
         case "rollOver":
            _loc9_ = "itemRollOver";
            break;
         case "rollOut":
            _loc9_ = "itemRollOut";
            break;
         case "doubleClick":
            _loc9_ = "itemDoubleClick";
            break;
         default:
            return undefined;
      }
      var _loc3_ = {target:this,type:_loc9_,item:event.target.data,renderer:event.target,index:event.target.index,controllerIdx:event.controllerIdx};
      this.dispatchEventAndSound(_loc3_);
   }
   function handleItemClick(event)
   {
      var _loc2_ = event.target.index;
      if(isNaN(_loc2_))
      {
         return undefined;
      }
      this.selectedIndex = _loc2_;
      this.dispatchItemEvent(event);
   }
}
