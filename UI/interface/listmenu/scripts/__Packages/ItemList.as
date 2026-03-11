class ItemList extends skyui.components.list.ScrollingList
{
   var __get__entryList;
   var _entryList;
   var _listHeight;
   var _maxListIndex;
   var background;
   var dispatchEvent;
   var scrollbar;
   var entryHeight = 25;
   var minViewport = 5;
   var maxViewport = 15;
   function ItemList()
   {
      super();
   }
   function set entryList(a_newArray)
   {
      this._entryList = a_newArray;
   }
   function set listHeight(a_height)
   {
      this._listHeight = this.background._height = a_height;
      if(this.scrollbar != undefined)
      {
         this.scrollbar.height = this._listHeight;
      }
      this._maxListIndex = Math.floor(this._listHeight / this.entryHeight);
   }
   function InvalidateData()
   {
      var _loc3_ = Math.min(Math.max(this.entryHeight * this.minViewport,this.entryHeight * this.entryList.length),this.entryHeight * this.maxViewport);
      this.dispatchEvent({type:"invalidateHeight",height:_loc3_});
      super.InvalidateData();
   }
}
