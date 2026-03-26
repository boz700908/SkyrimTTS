class ModLibraryPage extends Components.BSUIComponent
{
   var DescriptionLabelInputCatcher_mc;
   var DescriptionLabel_tf;
   var Description_tf;
   var EmptyWarning_tf;
   var FreeSpaceLabel_tf;
   var FreeSpace_tf;
   var List_mc;
   var ReorderIcon_mc;
   var TextScrollDeltaAccum;
   var TextScrollDown;
   var TextScrollUp;
   var TotalModSpaceLabel_tf;
   var TotalModSpace_tf;
   var _ReorderingMode;
   var _xmouse;
   var _ymouse;
   var bottomButtons;
   var dispatchEvent;
   var focusEnabled;
   var onEnterFrame;
   static var PLAY_SOUND = "PlaySound";
   static var LIBRARY_ITEM_PRESSED = "LibraryItemPressed";
   var _FreeSpace = 0;
   var _TotalModSpace = 0;
   var RIGHT_INPUT_SCROLL_THRESHOLD = 3;
   function ModLibraryPage()
   {
      super();
      this.focusEnabled = true;
      this._ReorderingMode = false;
      this.EmptyWarning_tf._visible = false;
      this.ReorderIcon_mc._visible = false;
      this.onEnterFrame = Shared.Proxy.create(this,this.Init);
   }
   function get dataArray()
   {
      return this.List_mc.entryList;
   }
   function get reorderMode()
   {
      return this._ReorderingMode;
   }
   function set reorderMode(aVal)
   {
      if(!aVal || this.List_mc.selectedEntry != undefined && !this.List_mc.selectedEntry.disabled)
      {
         this._ReorderingMode = aVal;
         this.List_mc.disableInput = this._ReorderingMode;
         this.UpdateReorderIconPosition();
      }
   }
   function get freeSpace()
   {
      return this._FreeSpace;
   }
   function set freeSpace(aVal)
   {
      this._FreeSpace = aVal;
      this.InvalidateData(false);
   }
   function GetModSpaceRemaining()
   {
      return this._FreeSpace - this._TotalModSpace;
   }
   function SetBottomButtons(buttons)
   {
      this.bottomButtons = buttons;
   }
   function handleInput(details, pathToFocus)
   {
      var _loc2_ = this.List_mc.selectedIndex;
      var _loc4_;
      if(Shared.GlobalFunc.IsKeyPressed(details))
      {
         if(details.navEquivalent == gfx.ui.NavigationCode.UP && this.List_mc.selectedIndex > 0)
         {
            this.List_mc.moveSelectionUp();
         }
         else if(details.navEquivalent == gfx.ui.NavigationCode.DOWN && this.List_mc.selectedIndex < this.List_mc.entryList.length - 1)
         {
            this.List_mc.moveSelectionDown();
         }
         if(_loc2_ != this.List_mc.selectedIndex && this.reorderMode)
         {
            _loc4_ = this.List_mc.entryList.splice(_loc2_,1)[0];
            this.List_mc.entryList.splice(this.List_mc.selectedIndex,0,_loc4_);
            this.List_mc.UpdateList();
            this.UpdateReorderIconPosition();
         }
         if(details.navEquivalent == gfx.ui.NavigationCode.ENTER && !this.reorderMode && this.List_mc.selectedEntry)
         {
            this.onItemPress();
         }
      }
      return true;
   }
   function OnRightStickInput(afXDelta, afYDelta)
   {
      this.TextScrollDeltaAccum += Math.abs(afYDelta);
      if(this.TextScrollDeltaAccum >= this.RIGHT_INPUT_SCROLL_THRESHOLD)
      {
         this.TextScrollDeltaAccum = 0;
         if(afYDelta > 0.1)
         {
            this.Description_tf.scroll--;
         }
         if(afYDelta < -0.1)
         {
            this.Description_tf.scroll = this.Description_tf.scroll + 1;
         }
         this.UpdateTextScrollIndicators();
      }
   }
   function Refresh()
   {
      this.onSelectionChange();
      this.InvalidateData();
   }
   function Init()
   {
      this.onEnterFrame = null;
      this.List_mc.ScrollUp.onRelease = Shared.Proxy.create(this,this.onListScrollUp);
      this.List_mc.ScrollDown.onRelease = Shared.Proxy.create(this,this.onListScrollDown);
      this.List_mc.addEventListener("selectionChange",Shared.Proxy.create(this,this.onSelectionChange));
      this.List_mc.addEventListener("itemPress",Shared.Proxy.create(this,this.onItemPress));
      this.List_mc.addEventListener(ListEntryBase.LOAD_THUMBNAIL,Shared.Proxy.create(this,this.BubbleEvent));
      this.TextScrollUp.onRelease = Shared.Proxy.create(this,this.onTextScrollUpClicked);
      this.TextScrollDown.onRelease = Shared.Proxy.create(this,this.onTextScrollDownClicked);
      var _loc2_ = new Object();
      _loc2_.onMouseWheel = Shared.Proxy.create(this,this.onMouseWheel);
      Mouse.addListener(_loc2_);
   }
   function UpdateReorderIconPosition()
   {
      if(!this._ReorderingMode || this.List_mc.selectedIndex == -1)
      {
         this.ReorderIcon_mc._visible = false;
      }
      else
      {
         this.ReorderIcon_mc._y = this.List_mc._y + this.List_mc.GetClipByIndex(this.List_mc.selectedEntry.clipIndex)._y + this.List_mc.GetClipByIndex(this.List_mc.selectedEntry.clipIndex)._height / 2;
         this.ReorderIcon_mc._visible = true;
      }
   }
   function InvalidateData()
   {
      this.List_mc.InvalidateData();
      this._TotalModSpace = 0;
      for(var _loc2_ in this.List_mc.entryList)
      {
         this._TotalModSpace += this.List_mc.entryList[_loc2_].fileSizeDisplay;
      }
      var _loc3_ = this._FreeSpace - this._TotalModSpace;
      this.TotalModSpace_tf.SetText(ModUtils.GetFileSizeString(this._TotalModSpace),false);
      this.TotalModSpace_tf._x = this.TotalModSpaceLabel_tf._x + this.TotalModSpaceLabel_tf.textWidth + 4;
      this.FreeSpace_tf.SetText(ModUtils.GetFileSizeString(_loc3_),false);
      this.FreeSpaceLabel_tf._x = this.FreeSpace_tf._x - this.FreeSpace_tf.textWidth;
      this.FreeSpaceLabel_tf._visible = this.FreeSpace_tf._visible = _loc3_ > 0;
      this.EmptyWarning_tf._visible = this.List_mc.entryList.length == 0;
      if(this.List_mc.entryList.length == 0)
      {
         this.Description_tf.SetText("");
      }
      if(this.List_mc.selectedEntry && this.List_mc.selectedEntry.dataObj instanceof Object)
      {
         this.setDescription(this.List_mc.selectedEntry.dataObj);
      }
   }
   function onDataObjectChange(aUpdatedObj)
   {
      var _loc2_ = 0;
      while(_loc2_ < this.List_mc.entryList.length)
      {
         if(this.List_mc.entryList[_loc2_].dataObj == aUpdatedObj)
         {
            this.List_mc.UpdateEntry(this.List_mc.entryList[_loc2_]);
         }
         _loc2_ = _loc2_ + 1;
      }
   }
   function ClearArray()
   {
      var _loc2_ = 0;
      while(_loc2_ < this.List_mc.entryList.length)
      {
         this.List_mc.entryList[_loc2_].dataObj = null;
         _loc2_ = _loc2_ + 1;
      }
      this.List_mc.ClearList();
   }
   function onListScrollUp()
   {
      this.List_mc.moveSelectionUp();
   }
   function onListScrollDown()
   {
      this.List_mc.moveSelectionDown();
   }
   function onSelectionChange()
   {
      this.Description_tf.SetText("");
      if(!this._ReorderingMode)
      {
         if(this.List_mc.selectedEntry != null && this.List_mc.selectedEntry != undefined)
         {
            if(this.List_mc.selectedEntry.dataObj instanceof Object)
            {
               this.setDescription(this.List_mc.selectedEntry.dataObj);
            }
            if(!ModManager.MinimalMode)
            {
               this.bottomButtons.GetButtonByIndex(0).label = !this.List_mc.selectedEntry.checked ? "$Mod_LibraryEnable" : "$Mod_LibraryDisable";
            }
            this.bottomButtons.GetButtonByIndex(0).invalidate();
         }
         this.UpdateTextScrollIndicators();
         this.dispatchEvent({type:ModLibraryPage.PLAY_SOUND,target:this,sound:"UIMenuFocus"});
      }
   }
   function setDescription(dataObj)
   {
      this.Description_tf.text = "$Version:";
      var _loc5_ = dataObj.releaseNotes.length <= 0 ? "" : "\r\n" + dataObj.releaseNotes;
      var _loc4_ = this.Description_tf.text + dataObj.version + _loc5_ + "\r\n\r\n";
      this.Description_tf._visible = dataObj.description.length > 0;
      this.DescriptionLabel_tf._visible = dataObj.description.length > 0;
      var _loc3_;
      if(dataObj.description.length > 0)
      {
         if(this.Description_tf.html == false)
         {
            _loc3_ = this.parseMarkdownToHTML(_loc4_ + dataObj.description);
         }
         else
         {
            _loc3_ = _loc4_ + dataObj.description;
         }
         this.Description_tf.SetText(_loc3_,true);
         this.Description_tf.html = true;
         trace("Save Text:\n" + _loc3_);
         trace("HTML Text:\n" + this.Description_tf.htmlText);
      }
   }
   function parseMarkdownToHTML(descriptionText)
   {
      var _loc3_ = descriptionText.split("\n");
      trace("Array Length: " + _loc3_.length);
      var _loc4_ = false;
      descriptionText = "";
      var _loc2_ = 0;
      for(; _loc2_ < _loc3_.length; _loc2_ = _loc2_ + 1)
      {
         trace("Loop: " + _loc2_ + " of " + _loc3_.length);
         trace("Text to parse: " + _loc3_[_loc2_]);
         if(_loc3_[_loc2_] == " \n" || _loc3_[_loc2_] == "\n")
         {
            if(_loc4_)
            {
               _loc3_[_loc2_] = "</ul>";
               _loc4_ = false;
            }
         }
         else if(_loc3_[_loc2_] == "" || _loc3_[_loc2_] == " ")
         {
            if(_loc4_)
            {
               _loc3_[_loc2_] = "</ul>";
               _loc4_ = false;
            }
            _loc3_[_loc2_] += "<br>";
         }
         else
         {
            if(this.CheckHeader(_loc3_,_loc2_))
            {
               if(_loc4_)
               {
                  _loc3_[_loc2_] = "</ul>" + _loc3_[_loc2_];
                  _loc4_ = false;
               }
            }
            else if(_loc2_ <= 0 ? false : this.CheckAltHeader(_loc3_,_loc2_))
            {
               if(!_loc4_)
               {
                  _loc3_.splice(_loc2_,1);
                  _loc2_ = _loc2_ - 1;
                  continue;
               }
               _loc3_[_loc2_ - 1] = "</ul>" + _loc3_[_loc2_ - 1];
               _loc4_ = false;
            }
            else if(this.CheckRule(_loc3_,_loc2_))
            {
               if(_loc4_)
               {
                  _loc3_[_loc2_] = "</ul>";
                  _loc4_ = false;
               }
            }
            else if(this.CheckUList(_loc3_,_loc2_))
            {
               if(!_loc4_)
               {
                  _loc3_[_loc2_] = "<ul>" + _loc3_[_loc2_];
                  _loc4_ = true;
               }
            }
            else if(this.CheckOList(_loc3_,_loc2_))
            {
               if(_loc4_)
               {
                  _loc3_[_loc2_] = "</ul>" + _loc3_[_loc2_];
                  _loc4_ = false;
               }
            }
            else
            {
               _loc3_[_loc2_] = "<p>" + _loc3_[_loc2_] + "</p>";
               if(_loc4_)
               {
                  _loc3_[_loc2_] = "</ul>" + _loc3_[_loc2_];
                  _loc4_ = false;
               }
            }
            this.AddEmphasisBoldMarks(_loc3_,_loc2_);
            trace("Current Line: \n" + _loc3_[_loc2_]);
         }
      }
      if(_loc4_)
      {
         _loc3_.push("</ul>");
         _loc4_ = false;
      }
      return _loc3_.join("");
   }
   function CheckHeader(line, i)
   {
      var _loc4_;
      var _loc1_;
      if(line[i].charAt(0) == "#")
      {
         _loc4_ = 0;
         _loc1_ = 0;
         while(_loc1_ < line[i].length)
         {
            if(line[i].charAt(_loc1_) != "#")
            {
               if(line[i].charAt(_loc1_) == " ")
               {
                  line[i] = "<h" + _loc4_.toString() + ">" + line[i].substring(_loc1_ + 1,line[i].length) + "</h" + _loc4_.toString() + ">";
                  return true;
               }
               trace("Okay, not a header it seems: " + line[i].charAt(_loc1_));
               return false;
            }
            _loc4_ = _loc4_ + 1;
            if(_loc4_ > 6)
            {
               return false;
            }
            _loc1_ = _loc1_ + 1;
         }
      }
      return false;
   }
   function CheckAltHeader(line, i)
   {
      var _loc7_ = 1;
      var _loc6_ = "=";
      var _loc4_ = line[i].indexOf(_loc6_);
      if(_loc4_ == -1)
      {
         _loc6_ = "-";
         _loc7_ = 4;
         _loc4_ = line[i].indexOf(_loc6_);
      }
      if(_loc4_ == -1)
      {
         return false;
      }
      var _loc1_ = 0;
      while(_loc1_ < _loc4_)
      {
         if(line[i].charAt(_loc1_) != " ")
         {
            return false;
         }
         _loc1_ = _loc1_ + 1;
      }
      var _loc5_ = false;
      _loc1_ = _loc4_;
      while(_loc1_ < line[i].length)
      {
         if(line[i].charAt(_loc1_) != _loc6_)
         {
            if(line[i].charAt(_loc1_) != " ")
            {
               return false;
            }
            _loc5_ = true;
         }
         if(_loc5_ && line[i].charAt(_loc1_) != " ")
         {
            return false;
         }
         _loc1_ = _loc1_ + 1;
      }
      if(line[i - 1] == " " || line[i - 1] == "")
      {
         return false;
      }
      line[i - 1] = "<h" + _loc7_.toString() + ">" + line[i - 1] + "</h" + _loc7_.toString() + ">";
      return true;
   }
   function CheckRule(line, i)
   {
      var _loc6_ = line[i].indexOf("*");
      var _loc7_ = line[i].indexOf("-");
      var _loc5_ = line[i].indexOf("_");
      var _loc4_ = 0;
      var _loc3_;
      if(_loc6_ != -1)
      {
         _loc3_ = 0;
         while(_loc3_ < line[i].length)
         {
            if(line[i].charAt(_loc3_) != "*")
            {
               if(line[i].charAt(_loc3_) != " ")
               {
                  return false;
               }
            }
            else
            {
               _loc4_ = _loc4_ + 1;
            }
            _loc3_ = _loc3_ + 1;
         }
      }
      else if(_loc7_ != -1)
      {
         _loc3_ = 0;
         while(_loc3_ < line[i].length)
         {
            if(line[i].charAt(_loc3_) != "-")
            {
               if(line[i].charAt(_loc3_) != " ")
               {
                  return false;
               }
            }
            else
            {
               _loc4_ = _loc4_ + 1;
            }
            _loc3_ = _loc3_ + 1;
         }
      }
      else if(_loc5_ != -1)
      {
         _loc3_ = 0;
         while(_loc3_ < line[i].length)
         {
            if(line[i].charAt(_loc3_) != "_")
            {
               if(line[i].charAt(_loc3_) != " ")
               {
                  return false;
               }
            }
            else
            {
               _loc4_ = _loc4_ + 1;
            }
            _loc3_ = _loc3_ + 1;
         }
      }
      if(_loc4_ < 3)
      {
         return false;
      }
      line[i] = "<hr>";
      return true;
   }
   function CheckUList(line, i)
   {
      var _loc4_ = line[i].indexOf("+");
      if(_loc4_ == -1)
      {
         _loc4_ = line[i].indexOf("-");
      }
      if(_loc4_ == -1)
      {
         _loc4_ = line[i].indexOf("*");
      }
      if(_loc4_ == -1)
      {
         return false;
      }
      var _loc1_ = 0;
      while(_loc1_ < _loc4_)
      {
         if(line[i].charAt(_loc1_) != " ")
         {
            if(line[i].charAt(_loc1_) != "/t")
            {
               return false;
            }
         }
         _loc1_ = _loc1_ + 1;
      }
      if(line[i].charAt(_loc4_ + 1) != " ")
      {
         return false;
      }
      line[i] = "<li>" + line[i].substring(_loc1_ + 2,line[i].length) + "</li>";
      return true;
   }
   function CheckOList(line, Index)
   {
      var _loc6_ = line[Index].length;
      var _loc7_ = false;
      var _loc2_ = 0;
      var _loc3_;
      while(_loc2_ < 10)
      {
         _loc3_ = line[Index].indexOf(_loc2_.toString());
         if(_loc3_ > -1 && _loc3_ < _loc6_)
         {
            _loc6_ = _loc3_;
            _loc7_ = true;
         }
         _loc2_ = _loc2_ + 1;
      }
      if(!_loc7_)
      {
         return false;
      }
      var _loc1_ = 0;
      while(_loc1_ < _loc6_)
      {
         if(line[Index].charAt(_loc1_) != " ")
         {
            if(line[Index].charAt(_loc1_) != "/t")
            {
               return false;
            }
         }
         _loc1_ = _loc1_ + 1;
      }
      if(line[Index].charAt(_loc6_ + 1) != ".")
      {
         return false;
      }
      if(line[Index].charAt(_loc6_ + 2) != " ")
      {
         return false;
      }
      line[Index] = "<p> " + line[Index] + "</p>";
      return true;
   }
   function AddEmphasisBoldMarks(line, i)
   {
      trace("Add Bolds and Italics");
      var _loc7_ = "";
      var _loc6_ = this.FindCurr(line[i],0);
      if(_loc6_ == -1)
      {
         return false;
      }
      var _loc2_ = 0;
      var _loc9_ = 0;
      var _loc3_;
      var _loc8_;
      for(; _loc6_ != -1; _loc6_ = this.FindCurr(line[i],_loc2_))
      {
         _loc7_ += line[i].substring(_loc2_,_loc6_);
         _loc3_ = line[i].substr(_loc6_,3);
         trace("sub string: " + _loc3_);
         if(_loc3_ == "***" || _loc3_ == "___" || _loc3_ == "**_" || _loc3_ == "*__" || _loc3_ == "__*" || _loc3_ == "_**")
         {
            _loc2_ = line[i].indexOf(_loc3_,_loc6_ + 3);
            if(_loc2_ == -1)
            {
               _loc8_ = _loc3_.charAt(2) + _loc3_.charAt(1) + _loc3_.charAt(0);
               _loc2_ = line[i].indexOf(_loc8_,_loc6_ + 3);
               if(_loc2_ == -1)
               {
                  _loc2_ = line[i].length;
                  continue;
               }
            }
            trace("BoldItalic: " + line[i].substring(_loc6_ + 3,_loc2_));
            _loc7_ = _loc7_ + "<i><b>" + line[i].substring(_loc6_ + 3,_loc2_) + "</i></b>";
            _loc2_ += 3;
         }
         else if(_loc3_.substr(0,2) == "**" || _loc3_.substr(0,2) == "__")
         {
            _loc2_ = line[i].indexOf(_loc3_.substr(0,2),_loc6_ + 2);
            if(_loc2_ == -1)
            {
               _loc8_ = _loc3_.charAt(1) + _loc3_.charAt(0);
               _loc2_ = line[i].indexOf(_loc8_,_loc6_ + 2);
               if(_loc2_ == -1)
               {
                  _loc2_ = line[i].length;
                  continue;
               }
            }
            trace("Bold: " + line[i].substring(_loc6_ + 2,_loc2_));
            _loc7_ = _loc7_ + "<b>" + line[i].substring(_loc6_ + 2,_loc2_) + "</b>";
            _loc2_ += 2;
         }
         else if(_loc3_.substr(0,2) == "*_" || _loc3_.substr(0,2) == "_*")
         {
            _loc2_ = line[i].indexOf(_loc3_.substr(0,2),_loc6_ + 2);
            if(_loc2_ == -1)
            {
               _loc8_ = _loc3_.charAt(1) + _loc3_.charAt(0);
               _loc2_ = line[i].indexOf(_loc8_,_loc6_ + 2);
               if(_loc2_ == -1)
               {
                  _loc2_ = line[i].length;
                  continue;
               }
            }
            trace("Italic: " + line[i].substring(_loc6_ + 2,_loc2_));
            _loc7_ = _loc7_ + "<i>" + line[i].substring(_loc6_ + 2,_loc2_) + "</i>";
            _loc2_ += 2;
         }
         else if(_loc3_.charAt(0) == "*" || _loc3_.charAt(0) == "_")
         {
            _loc2_ = line[i].indexOf(_loc3_.charAt(0),_loc6_ + 1);
            if(_loc2_ == -1)
            {
               _loc2_ = line[i].length;
            }
            else
            {
               trace("Italic: " + line[i].substring(_loc6_ + 1,_loc2_));
               _loc7_ = _loc7_ + "<i>" + line[i].substring(_loc6_ + 1,_loc2_) + "</i>";
               _loc2_ = _loc2_ + 1;
            }
         }
      }
      trace("Parse after loop: " + _loc7_);
      _loc7_ += line[i].substring(_loc2_);
      trace("Parse plus substring: " + _loc7_);
      line[i] = _loc7_;
   }
   function FindCurr(line, end)
   {
      var _loc1_ = end;
      while(_loc1_ < line.length)
      {
         if(line.charAt(_loc1_) == "*" || line.charAt(_loc1_) == "_")
         {
            return _loc1_;
         }
         _loc1_ = _loc1_ + 1;
      }
      return -1;
   }
   function onItemPress()
   {
      if(this.List_mc.selectedEntry != null && this.List_mc.selectedEntry.disabled != true)
      {
         this.dispatchEvent({type:ModLibraryPage.LIBRARY_ITEM_PRESSED,target:this,data:this.List_mc.selectedEntry});
      }
   }
   function UpdateTextScrollIndicators()
   {
      this.TextScrollUp._visible = this.Description_tf.scroll > 1;
      this.TextScrollDown._visible = this.Description_tf.scroll < this.Description_tf.maxscroll;
   }
   function onTextScrollUpClicked()
   {
      this.Description_tf.scroll--;
      this.UpdateTextScrollIndicators();
   }
   function onTextScrollDownClicked()
   {
      this.Description_tf.scroll = this.Description_tf.scroll + 1;
      this.UpdateTextScrollIndicators();
   }
   function onMouseWheel(delta)
   {
      if(this.DescriptionLabelInputCatcher_mc.hitTest(this._xmouse,this._ymouse,true))
      {
         if(delta < 0)
         {
            this.Description_tf.scroll = this.Description_tf.scroll + 1;
         }
         else if(delta > 0)
         {
            this.Description_tf.scroll--;
         }
         this.UpdateTextScrollIndicators();
      }
   }
   function BubbleEvent(event)
   {
      this.dispatchEvent({type:event.type,target:event.target,data:event.data});
   }
}
