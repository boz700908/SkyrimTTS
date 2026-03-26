class LoginDialog extends MovieClip
{
   var BottomButtons_mc;
   var LoginHolder_mc;
   var LoginMenu;
   var codeObj;
   var onEnterFrame;
   var _DataArray = [];
   var iPlatform = 0;
   var PS3Switch = false;
   var _CurrScrollPage = 0;
   var _codeObjInitialized = false;
   var _LoggedIn = false;
   function LoginDialog()
   {
      super();
      trace("LoginDialog::LoginDialog");
      _global.gfxExtensions = true;
      Shared.GlobalFunc.MaintainTextFormat();
      _root.CodeObj = this.codeObj = new Object();
      _root.InitExtensions = Shared.Proxy.create(this,this.InitExtensions);
      _root.onCodeObjectInit = Shared.Proxy.create(this,this.onCodeObjectInit);
      _root.ReleaseCodeObject = Shared.Proxy.create(this,this.ReleaseCodeObject);
      _root.SetPlatform = Shared.Proxy.create(this,this.SetPlatform);
      _root.onRightStickInput = Shared.Proxy.create(this,this.OnRightStickInput);
      this.DisplayScreen(null);
      this.onEnterFrame = Shared.Proxy.create(this,this.Init);
   }
   function OnLoginSuccess()
   {
      trace("LoginDialog::OnLoginSuccess");
      this._LoggedIn = true;
   }
   function onEULACanceled(event)
   {
      trace("LoginDialog::onEULACanceled");
      this.onDialogCancel();
   }
   function onInitDataComplete()
   {
      trace("LoginDialog::onInitDataComplete");
   }
   function onVKBTextEntered(astrEnteredText)
   {
      this.LoginMenu.onVKBTextEntered(astrEnteredText);
   }
   function Init()
   {
      trace("LoginDialog::Init");
      var _loc4_;
      var _loc3_;
      var _loc2_;
      if(this._codeObjInitialized)
      {
         this.onEnterFrame = null;
         this.BottomButtons_mc.SetPlatform(this.iPlatform,this.PS3Switch);
         this.BottomButtons_mc.addEventListener(BottomButtons.BUTTON_CLICKED,Shared.Proxy.create(this,this.OnBottomButtonClicked));
         _loc4_ = new Object();
         _loc4_.onLoadInit = Shared.Proxy.create(this,this.OnLoginLoadInit);
         _loc3_ = new MovieClipLoader();
         _loc3_.addListener(_loc4_);
         _loc3_.loadClip("BethesdaNetLogin.swf",this.LoginHolder_mc);
         _loc2_ = new Object();
         _loc2_.onMouseWheel = Shared.Proxy.create(this,this.onMouseWheel);
         Mouse.addListener(_loc2_);
      }
   }
   function handleInput(details, pathToFocus)
   {
      trace("LoginDialog::handleInput");
      var _loc3_ = false;
      if(Shared.GlobalFunc.IsKeyPressed(details))
      {
         _loc3_ = this.DoHandleInput(details.navEquivalent,details.code);
      }
      if(!_loc3_)
      {
         pathToFocus[0].handleInput(details,pathToFocus.slice(1));
      }
      return true;
   }
   function DoHandleInput(nav, keyCode, usingMouse)
   {
      trace("LoginDialog::handleInput " + nav);
      var _loc1_ = false;
      return _loc1_;
   }
   function InitExtensions()
   {
      trace("LoginDialog::InitExtensions");
   }
   function onCodeObjectInit()
   {
      trace("LoginDialog::onCodeObjectInit");
      this._codeObjInitialized = true;
   }
   function ReleaseCodeObject()
   {
      trace("LoginDialog::ReleaseCodeObject");
      this.LoginMenu.Destroy();
      delete this.codeObj;
      delete _root.CodeObj;
   }
   function SetPlatform(aiPlatform, abPS3Switch)
   {
      trace("LoginDialog::SetPlatform " + aiPlatform);
      this.iPlatform = aiPlatform;
      this.PS3Switch = abPS3Switch;
      if(this._codeObjInitialized)
      {
         this.BottomButtons_mc.SetPlatform(aiPlatform,abPS3Switch);
      }
   }
   function OnRightStickInput(afXDelta, afYDelta)
   {
   }
   function OnLoginLoadInit(mc)
   {
      trace("LoginDialog::OnLoginLoadInit");
      mc._visible = false;
      mc.onEnterFrame = Shared.Proxy.create(this,this.OnLoginLoadInitFinished,mc);
   }
   function OnLoginLoadInitFinished(mc)
   {
      trace("LoginDialog::OnLoginLoadInitFinished");
      if(this.LoginHolder_mc.LoginMenu_mc.Constructed)
      {
         this.LoginHolder_mc.onEnterFrame = null;
         this.LoginHolder_mc._visible = false;
         this.LoginMenu = this.LoginHolder_mc.LoginMenu_mc;
         this.LoginMenu.InitView();
         this.LoginMenu.CodeObject = this.codeObj;
         this.LoginMenu.SetPlatform(this.iPlatform,this.PS3Switch);
         this.LoginMenu.SetBottomButtons(this.BottomButtons_mc);
         this.LoginMenu.addEventListener(BethesdaNetLogin.LOGIN_ACTIVATED,Shared.Proxy.create(this,this.onLoginActivated));
         this.LoginMenu.addEventListener(BethesdaNetLogin.LOGIN_CANCELED,Shared.Proxy.create(this,this.onLoginCanceled));
         this.LoginMenu.addEventListener(BethesdaNetLogin.LOGIN_ERROR,Shared.Proxy.create(this,this.onLoginError));
         this.LoginMenu.addEventListener(BethesdaNetLogin.EULA_CANCELED,Shared.Proxy.create(this,this.onEULACanceled));
         this.codeObj.InitLoginDialog(this,this.LoginMenu);
      }
   }
   function onPlaySound(event, scope)
   {
      this.codeObj.PlaySound(event.sound);
   }
   function onLoginActivated(event)
   {
      trace("LoginDialog::onLoginActivated");
      this.LoginMenu.SetBottomButtons(this.BottomButtons_mc);
      this.DisplayScreen(this.LoginHolder_mc);
   }
   function onLoginCanceled(event)
   {
      trace("LoginDialog::onLoginCanceled");
      this.onDialogCancel();
   }
   function onLoginError(event)
   {
      trace("LoginDialog::onLoginError");
      this._LoggedIn = false;
   }
   function onDialogCancel()
   {
      this.codeObj.PlaySound("UIMenuCancel");
      this.codeObj.CloseDialog();
   }
   function DisplayScreen(mc)
   {
      trace("LoginDialog::DisplayScreen");
      this.LoginHolder_mc._visible = this.LoginHolder_mc == mc;
   }
   function OnBottomButtonClicked(event)
   {
      var _loc2_ = event.data;
      this.DoHandleInput(null,_loc2_.KeyCode,true);
   }
   function onUnregisterImage(event)
   {
      this.codeObj.UnregisterImage(event.data);
   }
   function onDisplayImage(event)
   {
      this.codeObj.onDisplayImage(event.data);
   }
   function onMouseWheel(delta)
   {
      var _loc1_ = false;
      return _loc1_;
   }
}
