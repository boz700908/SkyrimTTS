class gfx.utils.Constraints
{
   var elements;
   var scope;
   static var LEFT = 1;
   static var RIGHT = 2;
   static var TOP = 4;
   static var BOTTOM = 8;
   static var ALL = gfx.utils.Constraints.LEFT | gfx.utils.Constraints.RIGHT | gfx.utils.Constraints.TOP | gfx.utils.Constraints.BOTTOM;
   var scaled = false;
   function Constraints(scope, scaled)
   {
      this.scope = scope;
      this.scaled = scaled;
      this.elements = [];
   }
   function addElement(clip, edges)
   {
      var _loc7_;
      var _loc8_;
      var _loc4_;
      var _loc5_;
      var _loc6_;
      var _loc16_;
      if(clip != null)
      {
         _loc7_ = 100 / this.scope._xscale;
         _loc8_ = 100 / this.scope._yscale;
         _loc4_ = this.scope._width;
         _loc5_ = this.scope._height;
         if(this.scope == _root)
         {
            _loc4_ = Stage.width;
            _loc5_ = Stage.height;
         }
         _loc6_ = {clip:clip,edges:edges,metrics:{left:clip._x,top:clip._y,right:_loc4_ * _loc7_ - (clip._x + clip._width),bottom:_loc5_ * _loc8_ - (clip._y + clip._height),xscale:clip._xscale,yscale:clip._yscale}};
         _loc16_ = _loc6_.metrics;
         this.elements.push(_loc6_);
      }
   }
   function removeElement(clip)
   {
      var _loc2_ = 0;
      while(_loc2_ < this.elements.length)
      {
         if(this.elements[_loc2_].clip == clip)
         {
            this.elements.splice(_loc2_,1);
            return undefined;
         }
         _loc2_ = _loc2_ + 1;
      }
      return undefined;
   }
   function getElement(clip)
   {
      var _loc2_ = 0;
      while(_loc2_ < this.elements.length)
      {
         if(this.elements[_loc2_].clip == clip)
         {
            return this.elements[_loc2_];
         }
         _loc2_ = _loc2_ + 1;
      }
      return null;
   }
   function update(width, height)
   {
      var _loc9_ = 100 / this.scope._xscale;
      var _loc8_ = 100 / this.scope._yscale;
      if(!this.scaled)
      {
         this.scope._xscale = 100;
         this.scope._yscale = 100;
      }
      var _loc14_ = 0;
      var _loc7_;
      var _loc4_;
      var _loc2_;
      var _loc3_;
      var _loc12_;
      var _loc13_;
      var _loc5_;
      var _loc6_;
      while(_loc14_ < this.elements.length)
      {
         _loc7_ = this.elements[_loc14_];
         _loc4_ = _loc7_.edges;
         _loc2_ = _loc7_.clip;
         _loc3_ = _loc7_.metrics;
         _loc12_ = _loc2_.width != null ? "width" : "_width";
         _loc13_ = _loc2_.height != null ? "height" : "_height";
         if(this.scaled)
         {
            _loc2_._xscale = _loc3_.xscale * _loc9_;
            _loc2_._yscale = _loc3_.yscale * _loc8_;
            if((_loc4_ & gfx.utils.Constraints.LEFT) > 0)
            {
               _loc2_._x = _loc3_.left * _loc9_;
               if((_loc4_ & gfx.utils.Constraints.RIGHT) > 0)
               {
                  _loc5_ = width - _loc3_.left - _loc3_.right;
                  if(!(_loc2_ instanceof TextField))
                  {
                     _loc5_ *= _loc9_;
                  }
                  _loc2_[_loc12_] = _loc5_;
               }
            }
            else if((_loc4_ & gfx.utils.Constraints.RIGHT) > 0)
            {
               _loc2_._x = (width - _loc3_.right) * _loc9_ - _loc2_._width;
            }
            if((_loc4_ & gfx.utils.Constraints.TOP) > 0)
            {
               _loc2_._y = _loc3_.top * _loc8_;
               if((_loc4_ & gfx.utils.Constraints.BOTTOM) > 0)
               {
                  _loc6_ = height - _loc3_.top - _loc3_.bottom;
                  if(!(_loc2_ instanceof TextField))
                  {
                     _loc6_ *= _loc8_;
                  }
                  _loc2_[_loc13_] = _loc6_;
               }
            }
            else if((_loc4_ & gfx.utils.Constraints.BOTTOM) > 0)
            {
               _loc2_._y = (height - _loc3_.bottom) * _loc8_ - _loc2_._height;
            }
         }
         else
         {
            if((_loc4_ & gfx.utils.Constraints.RIGHT) > 0)
            {
               if((_loc4_ & gfx.utils.Constraints.LEFT) > 0)
               {
                  _loc2_[_loc12_] = width - _loc3_.left - _loc3_.right;
               }
               else
               {
                  _loc2_._x = width - _loc2_._width - _loc3_.right;
               }
            }
            if((_loc4_ & gfx.utils.Constraints.BOTTOM) > 0)
            {
               if((_loc4_ & gfx.utils.Constraints.TOP) > 0)
               {
                  _loc2_[_loc13_] = height - _loc3_.top - _loc3_.bottom;
               }
               else
               {
                  _loc2_._y = height - _loc2_._height - _loc3_.bottom;
               }
            }
         }
         _loc14_ = _loc14_ + 1;
      }
      return undefined;
   }
   function toString()
   {
      return "[Scaleform Constraints]";
   }
}
