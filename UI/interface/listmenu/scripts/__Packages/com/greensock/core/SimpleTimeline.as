class com.greensock.core.SimpleTimeline extends com.greensock.core.TweenCore
{
   var _firstChild;
   var _lastChild;
   var cachedTime;
   var cachedTotalTime;
   function SimpleTimeline(vars)
   {
      super(0,vars);
   }
   function insert(tween, time)
   {
      if(time == undefined)
      {
         time = 0;
      }
      var _loc3_ = tween.timeline;
      if(!tween.cachedOrphan && _loc3_)
      {
         _loc3_.remove(tween,true);
      }
      tween.timeline = this;
      tween.cachedStartTime = Number(time) + tween.delay;
      if(tween.gc)
      {
         tween.setEnabled(true,true);
      }
      if(tween.cachedPaused && _loc3_ != this)
      {
         tween.cachedPauseTime = tween.cachedStartTime + (this.rawTime - tween.cachedStartTime) / tween.cachedTimeScale;
      }
      if(this._lastChild)
      {
         this._lastChild.nextNode = tween;
      }
      else
      {
         this._firstChild = tween;
      }
      tween.prevNode = this._lastChild;
      this._lastChild = tween;
      tween.nextNode = undefined;
      tween.cachedOrphan = false;
      return tween;
   }
   function remove(tween, skipDisable)
   {
      if(tween.cachedOrphan)
      {
         return undefined;
      }
      if(skipDisable != true)
      {
         tween.setEnabled(false,true);
      }
      if(tween.nextNode)
      {
         tween.nextNode.prevNode = tween.prevNode;
      }
      else if(this._lastChild == tween)
      {
         this._lastChild = tween.prevNode;
      }
      if(tween.prevNode)
      {
         tween.prevNode.nextNode = tween.nextNode;
      }
      else if(this._firstChild == tween)
      {
         this._firstChild = tween.nextNode;
      }
      tween.cachedOrphan = true;
   }
   function renderTime(time, suppressEvents, force)
   {
      var _loc2_ = this._firstChild;
      var _loc4_;
      var _loc5_;
      this.cachedTotalTime = time;
      this.cachedTime = time;
      while(_loc2_)
      {
         _loc5_ = _loc2_.nextNode;
         if(_loc2_.active || time >= _loc2_.cachedStartTime && !_loc2_.cachedPaused && !_loc2_.gc)
         {
            if(!_loc2_.cachedReversed)
            {
               _loc2_.renderTime((time - _loc2_.cachedStartTime) * _loc2_.cachedTimeScale,suppressEvents,false);
            }
            else
            {
               _loc4_ = !_loc2_.cacheIsDirty ? _loc2_.cachedTotalDuration : _loc2_.totalDuration;
               _loc2_.renderTime(_loc4_ - (time - _loc2_.cachedStartTime) * _loc2_.cachedTimeScale,suppressEvents,false);
            }
         }
         _loc2_ = _loc5_;
      }
   }
   function get rawTime()
   {
      return this.cachedTotalTime;
   }
}
