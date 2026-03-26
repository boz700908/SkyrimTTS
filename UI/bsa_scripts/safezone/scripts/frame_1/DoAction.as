function InitExtensions()
{
   var _loc5_ = Stage.visibleRect.x + Stage.safeRect.x;
   var _loc3_ = Stage.visibleRect.x + Stage.visibleRect.width - Stage.safeRect.x;
   var _loc4_ = Stage.visibleRect.y + Stage.safeRect.y;
   var _loc2_ = Stage.visibleRect.y + Stage.visibleRect.height - Stage.safeRect.y;
   this.createEmptyMovieClip("lineTop_mc",this.getNextHighestDepth());
   lineTop_mc.lineStyle(1,16777215,100);
   lineTop_mc.moveTo(_loc5_,_loc4_);
   lineTop_mc.lineTo(_loc3_,_loc4_);
   lineTop_mc._x = 0;
   lineTop_mc._y = 0;
   this.createEmptyMovieClip("lineBottom_mc",this.getNextHighestDepth());
   lineBottom_mc.lineStyle(1,16777215,100);
   lineBottom_mc.moveTo(_loc5_,_loc2_);
   lineBottom_mc.lineTo(_loc3_,_loc2_);
   lineBottom_mc._x = 0;
   lineBottom_mc._y = 0;
   this.createEmptyMovieClip("lineLeft_mc",this.getNextHighestDepth());
   lineLeft_mc.lineStyle(1,16777215,100);
   lineLeft_mc.moveTo(_loc5_,_loc4_);
   lineLeft_mc.lineTo(_loc5_,_loc2_);
   lineLeft_mc._x = 0;
   lineLeft_mc._y = 0;
   this.createEmptyMovieClip("lineRight_mc",this.getNextHighestDepth());
   lineRight_mc.lineStyle(1,16777215,100);
   lineRight_mc.moveTo(_loc3_,_loc4_);
   lineRight_mc.lineTo(_loc3_,_loc2_);
   lineRight_mc._x = 0;
   lineRight_mc._y = 0;
}
_global.gfxExtensions = true;
stop();
