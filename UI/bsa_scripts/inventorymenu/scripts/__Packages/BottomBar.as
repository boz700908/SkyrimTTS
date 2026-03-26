class BottomBar extends MovieClip
{
   var Buttons;
   var HealthMeter;
   var LevelMeter;
   var MagickaMeter;
   var PlayerInfoCard_mc;
   var PlayerInfoObj;
   var StaminaMeter;
   var iLastItemType;
   var iLeftOffset;
   function BottomBar()
   {
      super();
      trace("BottomBar::BottomBar");
      this.PlayerInfoCard_mc = this.PlayerInfoCard_mc;
      this.iLastItemType = InventoryDefines.ICT_NONE;
      this.HealthMeter = new Components.Meter(this.PlayerInfoCard_mc.HealthRect.MeterInstance.Meter_mc);
      this.MagickaMeter = new Components.Meter(this.PlayerInfoCard_mc.MagickaRect.MeterInstance.Meter_mc);
      this.StaminaMeter = new Components.Meter(this.PlayerInfoCard_mc.StaminaRect.MeterInstance.Meter_mc);
      this.LevelMeter = new Components.Meter(this.PlayerInfoCard_mc.LevelMeterInstance.Meter_mc);
      var _loc3_ = 0;
      this.Buttons = new Array();
      while(this["Button" + _loc3_] != undefined)
      {
         this.Buttons.push(this["Button" + _loc3_]);
         _loc3_ = _loc3_ + 1;
      }
      Shared.ButtonMapping.Initialize("BottomBar");
   }
   function PositionElements(aiLeftOffset, aiRightOffset)
   {
      this.iLeftOffset = aiLeftOffset;
      this.PositionButtons();
      this.PlayerInfoCard_mc._x = aiRightOffset - this.PlayerInfoCard_mc._width;
   }
   function ShowPlayerInfo()
   {
      this.PlayerInfoCard_mc._alpha = 100;
   }
   function HidePlayerInfo()
   {
      this.PlayerInfoCard_mc._alpha = 0;
   }
   function UpdatePerItemInfo(aItemUpdateObj)
   {
      var _loc3_ = aItemUpdateObj.type;
      var _loc5_ = true;
      if(_loc3_ != undefined)
      {
         this.iLastItemType = _loc3_;
      }
      else
      {
         _loc3_ = this.iLastItemType;
         if(aItemUpdateObj == undefined)
         {
            aItemUpdateObj = {type:this.iLastItemType};
         }
      }
      var _loc4_;
      var _loc7_;
      var _loc8_;
      var _loc6_;
      var _loc10_;
      var _loc9_;
      var _loc11_;
      if(this.PlayerInfoObj != undefined && aItemUpdateObj != undefined)
      {
         switch(_loc3_)
         {
            case InventoryDefines.ICT_ARMOR:
               this.PlayerInfoCard_mc.gotoAndStop("Armor");
               _loc4_ = Math.floor(this.PlayerInfoObj.armor).toString();
               if(aItemUpdateObj.armorChange != undefined)
               {
                  _loc7_ = Math.round(aItemUpdateObj.armorChange);
                  if(_loc7_ > 0)
                  {
                     _loc4_ = _loc4_ + " <font color=\'#189515\'>(+" + _loc7_.toString() + ")</font>";
                  }
                  else if(_loc7_ < 0)
                  {
                     _loc4_ = _loc4_ + " <font color=\'#FF0000\'>(" + _loc7_.toString() + ")</font>";
                  }
               }
               this.PlayerInfoCard_mc.ArmorRatingValue.textAutoSize = "shrink";
               this.PlayerInfoCard_mc.ArmorRatingValue.html = true;
               this.PlayerInfoCard_mc.ArmorRatingValue.SetText(_loc4_,true);
               _loc4_ = this.PlayerInfoObj.warmth == undefined ? "0" : Math.floor(this.PlayerInfoObj.warmth).toString();
               if(aItemUpdateObj.warmthChange != undefined)
               {
                  _loc8_ = Math.round(aItemUpdateObj.warmthChange);
                  if(_loc8_ > 0)
                  {
                     _loc4_ = _loc4_ + " <font color=\'#189515\'>(+" + _loc8_.toString() + ")</font>";
                  }
                  else if(_loc8_ < 0)
                  {
                     _loc4_ = _loc4_ + " <font color=\'#FF0000\'>(" + _loc8_.toString() + ")</font>";
                  }
               }
               this.PlayerInfoCard_mc.WarmthRatingLabel._visible = this.PlayerInfoObj.warmth != undefined;
               this.PlayerInfoCard_mc.WarmthRatingValue._visible = this.PlayerInfoObj.warmth != undefined;
               this.PlayerInfoCard_mc.WarmthRatingValue.textAutoSize = "shrink";
               this.PlayerInfoCard_mc.WarmthRatingValue.html = true;
               this.PlayerInfoCard_mc.WarmthRatingValue.SetText(_loc4_,true);
               break;
            case InventoryDefines.ICT_WEAPON:
               this.PlayerInfoCard_mc.gotoAndStop("Weapon");
               _loc4_ = Math.floor(this.PlayerInfoObj.damage).toString();
               if(aItemUpdateObj.damageChange != undefined)
               {
                  _loc6_ = Math.round(aItemUpdateObj.damageChange);
                  if(_loc6_ > 0)
                  {
                     _loc4_ = _loc4_ + " <font color=\'#189515\'>(+" + _loc6_.toString() + ")</font>";
                  }
                  else if(_loc6_ < 0)
                  {
                     _loc4_ = _loc4_ + " <font color=\'#FF0000\'>(" + _loc6_.toString() + ")</font>";
                  }
               }
               this.PlayerInfoCard_mc.DamageValue.textAutoSize = "shrink";
               this.PlayerInfoCard_mc.DamageValue.html = true;
               this.PlayerInfoCard_mc.DamageValue.SetText(_loc4_,true);
               break;
            case InventoryDefines.ICT_POTION:
            case InventoryDefines.ICT_FOOD:
               _loc10_ = 0;
               _loc9_ = 1;
               _loc11_ = 2;
               if(aItemUpdateObj.potionType == _loc9_)
               {
                  this.PlayerInfoCard_mc.gotoAndStop("MagickaPotion");
                  break;
               }
               if(aItemUpdateObj.potionType == _loc11_)
               {
                  this.PlayerInfoCard_mc.gotoAndStop("StaminaPotion");
                  break;
               }
               if(aItemUpdateObj.potionType == _loc10_)
               {
                  this.PlayerInfoCard_mc.gotoAndStop("HealthPotion");
               }
               break;
            case InventoryDefines.ICT_BOOK:
            case InventoryDefines.ICT_INGREDIENT:
            case InventoryDefines.ICT_MISC:
            case InventoryDefines.ICT_KEY:
            default:
               this.PlayerInfoCard_mc.gotoAndStop("Default");
               break;
            case InventoryDefines.ICT_SPELL_DEFAULT:
            case InventoryDefines.ICT_ACTIVE_EFFECT:
               this.PlayerInfoCard_mc.gotoAndStop("Magic");
               _loc5_ = false;
               break;
            case InventoryDefines.ICT_SPELL:
               this.PlayerInfoCard_mc.gotoAndStop("MagicSkill");
               if(aItemUpdateObj.magicSchoolName != undefined)
               {
                  this.UpdateSkillBar(aItemUpdateObj.magicSchoolName,aItemUpdateObj.magicSchoolLevel,aItemUpdateObj.magicSchoolPct);
               }
               _loc5_ = false;
               break;
            case InventoryDefines.ICT_SHOUT:
               this.PlayerInfoCard_mc.gotoAndStop("Shout");
               this.PlayerInfoCard_mc.DragonSoulTextInstance.SetText(this.PlayerInfoObj.dragonSoulText);
               _loc5_ = false;
         }
         if(_loc5_)
         {
            this.PlayerInfoCard_mc.CarryWeightValue.textAutoSize = "shrink";
            this.PlayerInfoCard_mc.CarryWeightValue.SetText(Math.ceil(this.PlayerInfoObj.encumbrance) + "/" + Math.floor(this.PlayerInfoObj.maxEncumbrance));
            this.PlayerInfoCard_mc.PlayerGoldValue.SetText(this.PlayerInfoObj.gold.toString());
            this.PlayerInfoCard_mc.PlayerGoldLabel._x = this.PlayerInfoCard_mc.PlayerGoldValue._x + this.PlayerInfoCard_mc.PlayerGoldValue.getLineMetrics(0).x - this.PlayerInfoCard_mc.PlayerGoldLabel._width;
            this.PlayerInfoCard_mc.CarryWeightValue._x = this.PlayerInfoCard_mc.PlayerGoldLabel._x + this.PlayerInfoCard_mc.PlayerGoldLabel.getLineMetrics(0).x - this.PlayerInfoCard_mc.CarryWeightValue._width - 5;
            this.PlayerInfoCard_mc.CarryWeightLabel._x = this.PlayerInfoCard_mc.CarryWeightValue._x + this.PlayerInfoCard_mc.CarryWeightValue.getLineMetrics(0).x - this.PlayerInfoCard_mc.CarryWeightLabel._width;
            switch(_loc3_)
            {
               case InventoryDefines.ICT_ARMOR:
                  this.PlayerInfoCard_mc.ArmorRatingValue._x = this.PlayerInfoCard_mc.CarryWeightLabel._x + this.PlayerInfoCard_mc.CarryWeightLabel.getLineMetrics(0).x - this.PlayerInfoCard_mc.ArmorRatingValue._width - 5;
                  this.PlayerInfoCard_mc.ArmorRatingLabel._x = this.PlayerInfoCard_mc.ArmorRatingValue._x + this.PlayerInfoCard_mc.ArmorRatingValue.getLineMetrics(0).x - this.PlayerInfoCard_mc.ArmorRatingLabel._width;
                  this.PlayerInfoCard_mc.WarmthRatingValue._x = this.PlayerInfoCard_mc.ArmorRatingLabel._x + this.PlayerInfoCard_mc.ArmorRatingLabel.getLineMetrics(0).x - this.PlayerInfoCard_mc.WarmthRatingValue._width - 5;
                  this.PlayerInfoCard_mc.WarmthRatingLabel._x = this.PlayerInfoCard_mc.WarmthRatingValue._x + this.PlayerInfoCard_mc.WarmthRatingValue.getLineMetrics(0).x - this.PlayerInfoCard_mc.WarmthRatingLabel._width;
                  break;
               case InventoryDefines.ICT_WEAPON:
                  this.PlayerInfoCard_mc.DamageValue._x = this.PlayerInfoCard_mc.CarryWeightLabel._x + this.PlayerInfoCard_mc.CarryWeightLabel.getLineMetrics(0).x - this.PlayerInfoCard_mc.DamageValue._width - 5;
                  this.PlayerInfoCard_mc.DamageLabel._x = this.PlayerInfoCard_mc.DamageValue._x + this.PlayerInfoCard_mc.DamageValue.getLineMetrics(0).x - this.PlayerInfoCard_mc.DamageLabel._width;
            }
         }
         this.UpdateStatMeter(this.PlayerInfoCard_mc.HealthRect,this.HealthMeter,this.PlayerInfoObj.health,this.PlayerInfoObj.maxHealth,this.PlayerInfoObj.healthColor);
         this.UpdateStatMeter(this.PlayerInfoCard_mc.MagickaRect,this.MagickaMeter,this.PlayerInfoObj.magicka,this.PlayerInfoObj.maxMagicka,this.PlayerInfoObj.magickaColor);
         this.UpdateStatMeter(this.PlayerInfoCard_mc.StaminaRect,this.StaminaMeter,this.PlayerInfoObj.stamina,this.PlayerInfoObj.maxStamina,this.PlayerInfoObj.staminaColor);
      }
   }
   function UpdatePlayerInfo(aPlayerUpdateObj, aItemUpdateObj)
   {
      this.PlayerInfoObj = aPlayerUpdateObj;
      this.UpdatePerItemInfo(aItemUpdateObj);
   }
   function UpdateSkillBar(aSkillName, aiLevelStart, afLevelPercent)
   {
      this.PlayerInfoCard_mc.SkillLevelLabel.SetText(aSkillName);
      this.PlayerInfoCard_mc.SkillLevelCurrent.SetText(aiLevelStart);
      this.PlayerInfoCard_mc.SkillLevelNext.SetText(aiLevelStart + 1);
      this.PlayerInfoCard_mc.LevelMeterInstance.gotoAndStop("Pause");
      this.LevelMeter.SetPercent(afLevelPercent);
   }
   function UpdateCraftingInfo(aSkillName, aiLevelStart, afLevelPercent)
   {
      this.PlayerInfoCard_mc.gotoAndStop("Crafting");
      this.UpdateSkillBar(aSkillName,aiLevelStart,afLevelPercent);
   }
   function UpdateStatMeter(aMeterRect, aMeterObj, aiCurrValue, aiMaxValue, aColor)
   {
      if(aColor == undefined)
      {
         aColor = "#FFFFFF";
      }
      if(aMeterRect._alpha > 0)
      {
         if(aMeterRect.MeterText != undefined)
         {
            aMeterRect.MeterText.textAutoSize = "shrink";
            aMeterRect.MeterText.html = true;
            aMeterRect.MeterText.SetText("<font color=\'" + aColor + "\'>" + Math.floor(aiCurrValue) + "/" + Math.floor(aiMaxValue) + "</font>",true);
         }
         aMeterRect.MeterInstance.gotoAndStop("Pause");
         aMeterObj.SetPercent(aiCurrValue / aiMaxValue * 100);
      }
   }
   function SetBarterInfo(aiPlayerGold, aiVendorGold, aiGoldDelta, astrVendorName)
   {
      if(this.PlayerInfoCard_mc._currentframe == 1)
      {
         this.PlayerInfoCard_mc.gotoAndStop("Barter");
      }
      this.PlayerInfoCard_mc.PlayerGoldValue.textAutoSize = "shrink";
      this.PlayerInfoCard_mc.VendorGoldValue.textAutoSize = "shrink";
      if(aiGoldDelta == undefined)
      {
         this.PlayerInfoCard_mc.PlayerGoldValue.SetText(aiPlayerGold.toString(),true);
      }
      else if(aiGoldDelta >= 0)
      {
         this.PlayerInfoCard_mc.PlayerGoldValue.SetText(aiPlayerGold.toString() + " <font color=\'#189515\'>(+" + aiGoldDelta.toString() + ")</font>",true);
      }
      else
      {
         this.PlayerInfoCard_mc.PlayerGoldValue.SetText(aiPlayerGold.toString() + " <font color=\'#FF0000\'>(" + aiGoldDelta.toString() + ")</font>",true);
      }
      this.PlayerInfoCard_mc.VendorGoldValue.SetText(aiVendorGold.toString());
      if(astrVendorName != undefined)
      {
         this.PlayerInfoCard_mc.VendorGoldLabel.SetText("$Gold");
         this.PlayerInfoCard_mc.VendorGoldLabel.SetText(astrVendorName + " " + this.PlayerInfoCard_mc.VendorGoldLabel.text);
      }
      this.PlayerInfoCard_mc.VendorGoldLabel._x = this.PlayerInfoCard_mc.VendorGoldValue._x + this.PlayerInfoCard_mc.VendorGoldValue.getLineMetrics(0).x - this.PlayerInfoCard_mc.VendorGoldLabel._width - 5;
      this.PlayerInfoCard_mc.PlayerGoldValue._x = this.PlayerInfoCard_mc.VendorGoldLabel._x + this.PlayerInfoCard_mc.VendorGoldLabel.getLineMetrics(0).x - this.PlayerInfoCard_mc.PlayerGoldValue._width - 20;
      this.PlayerInfoCard_mc.PlayerGoldLabel._x = this.PlayerInfoCard_mc.PlayerGoldValue._x + this.PlayerInfoCard_mc.PlayerGoldValue.getLineMetrics(0).x - this.PlayerInfoCard_mc.PlayerGoldLabel._width - 5;
   }
   function SetBarterPerItemInfo(aItemUpdateObj, aPlayerInfoObj)
   {
      var _loc3_;
      var _loc6_;
      var _loc7_;
      var _loc4_;
      if(aItemUpdateObj != undefined)
      {
         switch(aItemUpdateObj.type)
         {
            case InventoryDefines.ICT_ARMOR:
               this.PlayerInfoCard_mc.gotoAndStop("Barter_Armor");
               _loc3_ = Math.floor(aPlayerInfoObj.armor).toString();
               if(aItemUpdateObj.armorChange != undefined)
               {
                  _loc6_ = Math.round(aItemUpdateObj.armorChange);
                  if(_loc6_ > 0)
                  {
                     _loc3_ = _loc3_ + " <font color=\'#189515\'>(+" + _loc6_.toString() + ")</font>";
                  }
                  else if(_loc6_ < 0)
                  {
                     _loc3_ = _loc3_ + " <font color=\'#FF0000\'>(" + _loc6_.toString() + ")</font>";
                  }
               }
               this.PlayerInfoCard_mc.ArmorRatingValue.textAutoSize = "shrink";
               this.PlayerInfoCard_mc.ArmorRatingValue.html = true;
               this.PlayerInfoCard_mc.ArmorRatingValue.SetText(_loc3_,true);
               _loc3_ = aPlayerInfoObj.warmth == undefined ? "0" : Math.floor(aPlayerInfoObj.warmth).toString();
               if(aItemUpdateObj.warmthChange != undefined)
               {
                  _loc7_ = Math.round(aItemUpdateObj.warmthChange);
                  if(_loc7_ > 0)
                  {
                     _loc3_ = _loc3_ + " <font color=\'#189515\'>(+" + _loc7_.toString() + ")</font>";
                  }
                  else if(_loc7_ < 0)
                  {
                     _loc3_ = _loc3_ + " <font color=\'#FF0000\'>(" + _loc7_.toString() + ")</font>";
                  }
               }
               this.PlayerInfoCard_mc.WarmthRatingLabel._visible = aPlayerInfoObj.warmth != undefined;
               this.PlayerInfoCard_mc.WarmthRatingValue._visible = aPlayerInfoObj.warmth != undefined;
               this.PlayerInfoCard_mc.WarmthRatingValue.textAutoSize = "shrink";
               this.PlayerInfoCard_mc.WarmthRatingValue.html = true;
               this.PlayerInfoCard_mc.WarmthRatingValue.SetText(_loc3_,true);
               break;
            case InventoryDefines.ICT_WEAPON:
               this.PlayerInfoCard_mc.gotoAndStop("Barter_Weapon");
               _loc3_ = Math.floor(aPlayerInfoObj.damage).toString();
               if(aItemUpdateObj.damageChange != undefined)
               {
                  _loc4_ = Math.round(aItemUpdateObj.damageChange);
                  if(_loc4_ > 0)
                  {
                     _loc3_ = _loc3_ + " <font color=\'#189515\'>(+" + _loc4_.toString() + ")</font>";
                  }
                  else if(_loc4_ < 0)
                  {
                     _loc3_ = _loc3_ + " <font color=\'#FF0000\'>(" + _loc4_.toString() + ")</font>";
                  }
               }
               this.PlayerInfoCard_mc.DamageValue.textAutoSize = "shrink";
               this.PlayerInfoCard_mc.DamageValue.html = true;
               this.PlayerInfoCard_mc.DamageValue.SetText(_loc3_,true);
               break;
            default:
               this.PlayerInfoCard_mc.gotoAndStop("Barter");
         }
      }
      switch(aItemUpdateObj.type)
      {
         case InventoryDefines.ICT_ARMOR:
            this.PlayerInfoCard_mc.ArmorRatingValue._x = this.PlayerInfoCard_mc.PlayerGoldLabel._x + this.PlayerInfoCard_mc.PlayerGoldLabel.getLineMetrics(0).x - this.PlayerInfoCard_mc.ArmorRatingValue._width - 200;
            this.PlayerInfoCard_mc.ArmorRatingLabel._x = this.PlayerInfoCard_mc.ArmorRatingValue._x + this.PlayerInfoCard_mc.ArmorRatingValue.getLineMetrics(0).x - this.PlayerInfoCard_mc.ArmorRatingLabel._width;
            this.PlayerInfoCard_mc.WarmthRatingValue._x = this.PlayerInfoCard_mc.ArmorRatingLabel._x + this.PlayerInfoCard_mc.ArmorRatingLabel.getLineMetrics(0).x - this.PlayerInfoCard_mc.WarmthRatingValue._width - 15;
            this.PlayerInfoCard_mc.WarmthRatingLabel._x = this.PlayerInfoCard_mc.WarmthRatingValue._x + this.PlayerInfoCard_mc.WarmthRatingValue.getLineMetrics(0).x - this.PlayerInfoCard_mc.WarmthRatingLabel._width;
            break;
         case InventoryDefines.ICT_WEAPON:
            this.PlayerInfoCard_mc.DamageValue._x = this.PlayerInfoCard_mc.PlayerGoldLabel._x + this.PlayerInfoCard_mc.PlayerGoldLabel.getLineMetrics(0).x - this.PlayerInfoCard_mc.DamageValue._width - 200;
            this.PlayerInfoCard_mc.DamageLabel._x = this.PlayerInfoCard_mc.DamageValue._x + this.PlayerInfoCard_mc.DamageValue.getLineMetrics(0).x - this.PlayerInfoCard_mc.DamageLabel._width;
         default:
            return;
      }
   }
   function SetGiftInfo(aiFavorPoints)
   {
      this.PlayerInfoCard_mc.gotoAndStop("Gift");
   }
   function SetPlatform(aiPlatform, abPS3Switch)
   {
      trace("BottomBar::SetPlatform " + aiPlatform.toString());
      var _loc2_ = 0;
      while(_loc2_ < this.Buttons.length)
      {
         this.Buttons[_loc2_].SetPlatform(aiPlatform,abPS3Switch);
         _loc2_ = _loc2_ + 1;
      }
   }
   function ShowButtons()
   {
      trace("BottomBar::ShowButtons");
      var _loc2_ = 0;
      while(_loc2_ < this.Buttons.length)
      {
         this.Buttons[_loc2_]._visible = this.Buttons[_loc2_].label.length > 0;
         _loc2_ = _loc2_ + 1;
      }
   }
   function HideButtons()
   {
      trace("BottomBar::HideButtons");
      var _loc2_ = 0;
      while(_loc2_ < this.Buttons.length)
      {
         this.Buttons[_loc2_]._visible = false;
         _loc2_ = _loc2_ + 1;
      }
   }
   function SetButtonsText()
   {
      trace("BottomBar::SetButtonsText");
      var _loc3_ = 0;
      while(_loc3_ < this.Buttons.length)
      {
         this.Buttons[_loc3_].label = _loc3_ >= arguments.length ? "" : arguments[_loc3_];
         this.Buttons[_loc3_]._visible = this.Buttons[_loc3_].label.length > 0;
         _loc3_ = _loc3_ + 1;
      }
      this.PositionButtons();
   }
   function SetButtonText(aText, aIndex)
   {
      trace("BottomBar::SetButtonText " + aIndex.toString() + " = " + aText);
      if(aIndex < this.Buttons.length)
      {
         this.Buttons[aIndex].label = aText;
         this.Buttons[aIndex]._visible = aText.length > 0;
         this.PositionButtons();
      }
   }
   function SetButtonsArt(aButtonArt)
   {
      trace("BottomBar::SetButtonsArt");
      var _loc2_ = 0;
      while(_loc2_ < aButtonArt.length)
      {
         this.SetButtonArt(aButtonArt[_loc2_],_loc2_);
         _loc2_ = _loc2_ + 1;
      }
   }
   function GetButtonsArt()
   {
      var _loc3_ = new Array(this.Buttons.length);
      var _loc2_ = 0;
      while(_loc2_ < this.Buttons.length)
      {
         _loc3_[_loc2_] = this.Buttons[_loc2_].GetArt();
         _loc2_ = _loc2_ + 1;
      }
      return _loc3_;
   }
   function SetButtonArt(aPlatformArt, aIndex)
   {
      trace("BottomBar::SetButtonArt " + aIndex.toString() + " to " + aPlatformArt.PCArt);
      var _loc2_;
      if(aIndex < this.Buttons.length)
      {
         _loc2_ = this.Buttons[aIndex];
         _loc2_.PCArt = aPlatformArt.PCArt;
         _loc2_.XBoxArt = aPlatformArt.XBoxArt;
         _loc2_.PS3Art = aPlatformArt.PS3Art;
         Shared.ButtonMapping.CorrectLabel(_loc2_);
         _loc2_.RefreshArt();
      }
   }
   function PositionButtons()
   {
      trace("BottomBar::PositionButtons");
      var _loc4_ = 10;
      var _loc3_ = this.iLeftOffset;
      var _loc2_ = 0;
      while(_loc2_ < this.Buttons.length)
      {
         if(this.Buttons[_loc2_].label.length > 0)
         {
            this.Buttons[_loc2_]._x = _loc3_ + this.Buttons[_loc2_].ButtonArt._width;
            _loc3_ = this.Buttons[_loc2_]._x + this.Buttons[_loc2_].textField.getLineMetrics(0).width + _loc4_;
         }
         _loc2_ = _loc2_ + 1;
      }
   }
}
