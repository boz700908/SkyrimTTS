this.currentState = StartMenu.CHARACTER_SELECTION_STATE;
stop();
MovieClip.prototype.bringToFront = function(optionsObj)
{
   var _loc6_ = new Array();
   var _loc3_;
   var _loc2_;
   for(var _loc4_ in this._parent)
   {
      if(typeof this._parent[_loc4_] == "movieclip" && this._parent[_loc4_]._parent == this._parent && this._parent[_loc4_].getDepth() > this.getDepth())
      {
         _loc3_ = true;
         _loc2_ = 0;
         while(_loc2_ < optionsObj.protect.length)
         {
            if(this._parent[_loc4_] == optionsObj.protect[_loc2_])
            {
               _loc3_ = false;
               break;
            }
            _loc2_ = _loc2_ + 1;
         }
         if(_loc3_)
         {
            _loc6_.push(this._parent[_loc4_]);
         }
      }
   }
   var _loc4_ = _loc6_.length - 1;
   while(_loc4_ >= 0)
   {
      this.swapDepths(_loc6_[_loc4_]);
      _loc4_ = _loc4_ - 1;
   }
};
CharacterSelectionHint.bringToFront();
