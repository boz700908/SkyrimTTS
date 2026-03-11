class com.greensock.OverwriteManager
{
   static var enabled;
   static var mode;
   static var version = 6.1;
   static var NONE = 0;
   static var ALL_IMMEDIATE = 1;
   static var AUTO = 2;
   static var CONCURRENT = 3;
   static var ALL_ONSTART = 4;
   static var PREEXISTING = 5;
   function OverwriteManager()
   {
   }
   static function init(defaultMode)
   {
      if(com.greensock.TweenLite.version < 11.6)
      {
         trace("Warning: Your TweenLite class needs to be updated to work with OverwriteManager (or you may need to clear your ASO files). Please download and install the latest version from http://www.tweenlite.com.");
      }
      com.greensock.TweenLite.overwriteManager = com.greensock.OverwriteManager;
      com.greensock.OverwriteManager.mode = defaultMode != undefined ? defaultMode : 2;
      com.greensock.OverwriteManager.enabled = true;
      return com.greensock.OverwriteManager.mode;
   }
   static function manageOverwrites(tween, props, targetTweens, mode)
   {
      var _loc3_;
      var _loc10_;
      var _loc1_;
      var _loc17_;
      if(mode >= 4)
      {
         _loc17_ = targetTweens.length;
         _loc3_ = 0;
         while(_loc3_ < _loc17_)
         {
            _loc1_ = targetTweens[_loc3_];
            if(_loc1_ != tween)
            {
               if(_loc1_.setEnabled(false,false))
               {
                  _loc10_ = true;
               }
            }
            else if(mode == 5)
            {
               break;
            }
            _loc3_ = _loc3_ + 1;
         }
         return _loc10_;
      }
      var _loc5_ = tween.cachedStartTime + 1e-10;
      var _loc9_ = [];
      var _loc13_ = [];
      var _loc15_ = 0;
      var _loc12_ = 0;
      _loc3_ = targetTweens.length;
      while((_loc3_ = _loc3_ - 1) > -1)
      {
         _loc1_ = targetTweens[_loc3_];
         if(!(_loc1_ == tween || _loc1_.gc || !_loc1_.initted && _loc5_ - _loc1_.cachedStartTime <= 2e-10))
         {
            if(_loc1_.timeline != tween.timeline)
            {
               if(!com.greensock.OverwriteManager.getGlobalPaused(_loc1_))
               {
                  _loc13_[_loc15_++] = _loc1_;
               }
            }
            else if(_loc1_.cachedStartTime <= _loc5_ && _loc1_.cachedStartTime + _loc1_.totalDuration + 1e-10 > _loc5_ && !_loc1_.cachedPaused && !(tween.cachedDuration == 0 && _loc5_ - _loc1_.cachedStartTime <= 2e-10))
            {
               _loc9_[_loc12_++] = _loc1_;
            }
         }
      }
      var _loc6_;
      var _loc7_;
      var _loc4_;
      var _loc11_;
      var _loc2_;
      if(_loc15_ != 0)
      {
         _loc6_ = tween.cachedTimeScale;
         _loc7_ = _loc5_;
         _loc2_ = tween.timeline;
         while(_loc2_)
         {
            _loc6_ *= _loc2_.cachedTimeScale;
            _loc7_ += _loc2_.cachedStartTime;
            _loc2_ = _loc2_.timeline;
         }
         _loc5_ = _loc6_ * _loc7_;
         _loc3_ = _loc15_;
         while((_loc3_ = _loc3_ - 1) > -1)
         {
            _loc4_ = _loc13_[_loc3_];
            _loc6_ = _loc4_.cachedTimeScale;
            _loc7_ = _loc4_.cachedStartTime;
            _loc2_ = _loc4_.timeline;
            while(_loc2_)
            {
               _loc6_ *= _loc2_.cachedTimeScale;
               _loc7_ += _loc2_.cachedStartTime;
               _loc2_ = _loc2_.timeline;
            }
            _loc11_ = _loc6_ * _loc7_;
            if(_loc11_ <= _loc5_ && (_loc11_ + _loc4_.totalDuration * _loc6_ + 1e-10 > _loc5_ || _loc4_.cachedDuration == 0))
            {
               _loc9_[_loc12_++] = _loc4_;
            }
         }
      }
      if(_loc12_ == 0)
      {
         return _loc10_;
      }
      _loc3_ = _loc12_;
      if(mode == 2)
      {
         while((_loc3_ = _loc3_ - 1) > -1)
         {
            _loc1_ = _loc9_[_loc3_];
            if(_loc1_.killVars(props))
            {
               _loc10_ = true;
            }
            if(_loc1_.cachedPT1 == undefined && _loc1_.initted)
            {
               _loc1_.setEnabled(false,false);
            }
         }
      }
      else
      {
         while((_loc3_ = _loc3_ - 1) > -1)
         {
            if(_loc9_[_loc3_].setEnabled(false,false))
            {
               _loc10_ = true;
            }
         }
      }
      return _loc10_;
   }
   static function getGlobalPaused(tween)
   {
      while(tween)
      {
         if(tween.cachedPaused)
         {
            return true;
         }
         tween = tween.timeline;
      }
      return false;
   }
}
