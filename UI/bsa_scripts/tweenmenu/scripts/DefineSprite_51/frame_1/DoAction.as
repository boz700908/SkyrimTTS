var Particles = new Array();
this.onEnterFrame = function()
{
   var _loc6_ = false;
   var _loc2_ = 0;
   var _loc3_;
   var _loc5_;
   var _loc4_;
   while(_loc2_ < NumParticles)
   {
      if(!_loc6_ && (Particles[_loc2_] == undefined || Particles[_loc2_].animating != true))
      {
         if(Particles[_loc2_] == undefined)
         {
            dot.duplicateMovieClip("dot" + _loc2_,this.getNextHighestDepth());
            Particles[_loc2_] = this["dot" + _loc2_];
         }
         Particles[_loc2_]._x = dot._x;
         Particles[_loc2_]._y = dot._y;
         Particles[_loc2_]._alpha = Math.random() * 25 + 75;
         Particles[_loc2_]._xscale = Math.random() * 100 + 550;
         Particles[_loc2_]._yscale = Math.random() * 100 + 550;
         Particles[_loc2_].targetScale = 350;
         Particles[_loc2_].directionX = Math.random() * 55 - 10;
         Particles[_loc2_].directionY = Math.random() * 10 - 5;
         Particles[_loc2_].assimilationRate = Math.random() * 6 + 10;
         Particles[_loc2_].animating = true;
         _loc6_ = true;
      }
      if(Particles[_loc2_] && Particles[_loc2_].animating)
      {
         Particles[_loc2_]._alpha = (Particles[_loc2_].assimilationRate - 1) * Particles[_loc2_]._alpha / Particles[_loc2_].assimilationRate;
         Particles[_loc2_]._xscale = ((Particles[_loc2_].assimilationRate - 1) * Particles[_loc2_]._alpha + Particles[_loc2_].targetScale) / Particles[_loc2_].assimilationRate;
         Particles[_loc2_]._yscale = ((Particles[_loc2_].assimilationRate - 1) * Particles[_loc2_]._alpha + Particles[_loc2_].targetScale) / Particles[_loc2_].assimilationRate;
         _loc3_ = new flash.geom.Point(Particles[_loc2_]._x,Particles[_loc2_]._y);
         _loc5_ = new flash.geom.Point(Math.cos(_loc2_ / 2),Math.sin(_loc2_ / 2));
         random;
         _loc4_ = new flash.geom.Point(Particles[_loc2_].directionX,Particles[_loc2_].directionY);
         _loc3_ = _loc3_.add(_loc4_.add(_loc5_));
         Particles[_loc2_].directionX /= 1.2;
         Particles[_loc2_].directionY /= 1.2;
         Particles[_loc2_]._x = _loc3_.x;
         Particles[_loc2_]._y = _loc3_.y;
         if(Particles[_loc2_]._alpha < 10)
         {
            Particles[_loc2_].animating = false;
         }
         false;
         false;
         false;
      }
      _loc2_ = _loc2_ + 1;
   }
};
