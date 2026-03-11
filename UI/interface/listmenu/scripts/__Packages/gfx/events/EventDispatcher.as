class gfx.events.EventDispatcher
{
   var _listeners;
   static var _instance;
   function EventDispatcher()
   {
   }
   static function initialize(target)
   {
      if(gfx.events.EventDispatcher._instance == undefined)
      {
         gfx.events.EventDispatcher._instance = new gfx.events.EventDispatcher();
      }
      target.dispatchEvent = gfx.events.EventDispatcher._instance.dispatchEvent;
      target.dispatchQueue = gfx.events.EventDispatcher._instance.dispatchQueue;
      target.hasEventListener = gfx.events.EventDispatcher._instance.hasEventListener;
      target.addEventListener = gfx.events.EventDispatcher._instance.addEventListener;
      target.removeEventListener = gfx.events.EventDispatcher._instance.removeEventListener;
      target.removeAllEventListeners = gfx.events.EventDispatcher._instance.removeAllEventListeners;
      target.cleanUpEvents = gfx.events.EventDispatcher._instance.cleanUpEvents;
      _global.ASSetPropFlags(target,"dispatchQueue",1);
   }
   static function indexOfListener(listeners, scope, callBack)
   {
      var _loc3_ = listeners.length;
      var _loc2_ = -1;
      var _loc1_;
      while((_loc2_ = _loc2_ + 1) < _loc3_)
      {
         _loc1_ = listeners[_loc2_];
         if(_loc1_.listenerObject == scope && _loc1_.listenerFunction == callBack)
         {
            return _loc2_;
         }
      }
      return -1;
   }
   function addEventListener(event, scope, callBack)
   {
      if(this._listeners == undefined)
      {
         this._listeners = {};
         _global.ASSetPropFlags(this,"_listeners",1);
      }
      var _loc3_ = this._listeners[event];
      if(_loc3_ == undefined)
      {
         this._listeners[event] = _loc3_ = [];
      }
      if(gfx.events.EventDispatcher.indexOfListener(_loc3_,scope,callBack) == -1)
      {
         _loc3_.push({listenerObject:scope,listenerFunction:callBack});
      }
   }
   function removeEventListener(event, scope, callBack)
   {
      var _loc2_ = this._listeners[event];
      var _loc3_;
      if(_loc2_ != undefined)
      {
         _loc3_ = gfx.events.EventDispatcher.indexOfListener(_loc2_,scope,callBack);
         if(_loc3_ != -1)
         {
            _loc2_.splice(_loc3_,1);
         }
      }
   }
   function dispatchEvent(event)
   {
      if(event.type != "all")
      {
         if(event.target == undefined)
         {
            event.target = this;
         }
         this.dispatchQueue(this,event);
      }
   }
   function hasEventListener(event)
   {
      return this._listeners[event] != null && this._listeners[event].length > 0;
   }
   function removeAllEventListeners(event)
   {
      if(event == undefined)
      {
         delete this._listeners;
         return undefined;
      }
      delete this._listeners[event];
   }
   function dispatchQueue(dispatch, event)
   {
      var _loc1_ = dispatch._listeners[event.type];
      if(_loc1_ != undefined)
      {
         gfx.events.EventDispatcher.$dispatchEvent(dispatch,_loc1_,event);
      }
      _loc1_ = dispatch._listeners.all;
      if(_loc1_ != undefined)
      {
         gfx.events.EventDispatcher.$dispatchEvent(dispatch,_loc1_,event);
      }
   }
   static function $dispatchEvent(dispatch, listeners, event)
   {
      var _loc7_ = listeners.length;
      var _loc4_ = 0;
      var _loc1_;
      var _loc5_;
      var _loc2_;
      while(_loc4_ < _loc7_)
      {
         _loc1_ = listeners[_loc4_].listenerObject;
         _loc5_ = typeof _loc1_;
         _loc2_ = listeners[_loc4_].listenerFunction;
         if(_loc2_ == undefined)
         {
            _loc2_ = event.type;
         }
         if(_loc5_ == "function")
         {
            if(_loc1_[_loc2_] == null)
            {
               _loc1_.apply(dispatch,[event]);
            }
            else
            {
               _loc1_[_loc2_](event);
            }
         }
         else if(_loc1_.handleEvent != undefined && _loc2_ == undefined)
         {
            _loc1_.handleEvent(event);
         }
         else
         {
            _loc1_[_loc2_](event);
         }
         _loc4_ = _loc4_ + 1;
      }
      return undefined;
   }
   function cleanUp()
   {
      this.cleanUpEvents();
   }
   function cleanUpEvents()
   {
      this.removeAllEventListeners();
   }
}
