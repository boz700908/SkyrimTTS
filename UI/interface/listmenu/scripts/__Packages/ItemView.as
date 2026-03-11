class ItemView extends MovieClip
{
   var _sortFilter;
   var background;
   var dispatchEvent;
   var itemList;
   var paddingBottom = 2;
   var paddingTop = 0;
   var _sortEnabled = false;
   function ItemView()
   {
      super();
      gfx.events.EventDispatcher.initialize(this);
      this._sortFilter = new skyui.filter.SortFilter();
   }
   function onLoad()
   {
      this.itemList.listEnumeration = new skyui.components.list.BasicEnumeration(this.itemList.entryList);
      this.itemList.addEventListener("invalidateHeight",this,"onInvalidateHeight");
      this.dispatchEvent({type:"onLoad",view:this});
   }
   function get entryList()
   {
      return this.itemList.entryList;
   }
   function get sortEnabled()
   {
      return this._sortEnabled;
   }
   function set sortEnabled(a_sort)
   {
      this._sortEnabled = a_sort;
      var _loc0_;
      var _loc2_;
      if(a_sort)
      {
         _loc2_ = this.itemList.listEnumeration = new skyui.components.list.FilteredEnumeration(this.itemList.entryList);
         _loc2_.addFilter(this._sortFilter);
         this.itemList.listEnumeration = _loc2_;
      }
      else
      {
         this.itemList.listEnumeration = new skyui.components.list.BasicEnumeration(this.itemList.entryList);
      }
   }
   function set entryList(a_newArray)
   {
      this.itemList.entryList = a_newArray;
      this.sortEnabled = this.sortEnabled;
   }
   function handleInput(details, pathToFocus)
   {
      return this.itemList.handleInput(details,pathToFocus);
   }
   function get listHeight()
   {
      return this.itemList.listHeight;
   }
   function setMinViewport(a_minEntries, a_update)
   {
      this.itemList.minViewport = a_minEntries;
      if(a_update)
      {
         this.itemList.requestInvalidate();
      }
   }
   function setMaxViewport(a_maxEntries, a_update)
   {
      this.itemList.maxViewport = a_maxEntries;
      if(a_update)
      {
         this.itemList.requestInvalidate();
      }
   }
   function set listHeight(a_height)
   {
      this.itemList.listHeight = a_height;
      this.background._y = - this.paddingTop;
      this.background._height = a_height + this.paddingTop + this.paddingBottom;
   }
   function onInvalidateHeight(event)
   {
      this.listHeight = event.height;
   }
}
