{ Browser platform backend for the LCL CustomDrawn interface.
  See COPYING.modifiedLGPL.txt for license and linking exception. }

procedure BrowserPresent(Data: Pointer; Width, Height, Left, Top, Right, Bottom, Last: LongInt); external 'lcl' name 'present';
procedure BrowserInvalidate; external 'lcl' name 'invalidate';
procedure BrowserWaitMessage(Form: PtrUInt); external 'lcl' name 'wait_message';
procedure BrowserTitle(Text: PChar; Count: LongInt); external 'lcl' name 'title';
function BrowserMessage(Text, Caption: PChar; CountText, CountCaption: LongInt;
  Flags: LongInt): LongInt; external 'lcl' name 'message';
function BrowserDialog(Data: PChar; Count: LongInt): LongInt; external 'lcl' name 'dialog';
procedure BrowserMenus(Data: PChar; Count: LongInt); external 'lcl' name 'menus';
function BrowserTimer(Handle, Interval: LongInt): LongInt; external 'lcl' name 'timer';
procedure BrowserClearTimer(Handle: LongInt); external 'lcl' name 'clear_timer';
procedure BrowserFont(Name: PChar; Count, Size, Style, Angle: LongInt); external 'lcl' name 'font';
function BrowserMeasure(Text: PChar; Count, FontSize: LongInt): LongInt; external 'lcl' name 'measure';
procedure BrowserText(Data: Pointer; Width, Height, X, Y: LongInt;
  Text: PChar; Count, FontSize, Color, ClipLeft, ClipTop, ClipRight, ClipBottom: LongInt);
  external 'lcl' name 'text';

type
  TBrowserApplication = class(TApplication);

var
  { Set by any invalidation; frames without it skip composing and presenting. }
  BrowserDirty: Boolean = True;
  BrowserFullDamage: Boolean = True;
  BrowserDamage: array[0..31] of TRect;
  BrowserDamageCount: Integer = 0;
  BrowserMenusDirty: Boolean = True;
  BrowserMenuForm: TCustomForm = nil;
  BrowserInIdle: Boolean = False;
  BrowserClipboardFormats: array[TClipboardType] of array of TClipboardFormat;
  BrowserClipboardRequest: array[TClipboardType] of TClipboardRequestEvent;
  BrowserWidth: LongInt = 720;
  BrowserHeight: LongInt = 480;
  BrowserMouse: TPoint;
  { Modifier mask reported by the browser host: 1 shift, 2 ctrl, 4 alt,
    8 left button. CustomDrawn routing asks GetKeyState for these. }
  BrowserMods: LongInt = 0;

function BrowserShiftState(Mask: LongInt): TShiftState;
begin
  Result := [];
  if (Mask and 1) <> 0 then Include(Result, ssShift);
  if (Mask and 2) <> 0 then Include(Result, ssCtrl);
  if (Mask and 4) <> 0 then Include(Result, ssAlt);
  if (Mask and 8) <> 0 then Include(Result, ssLeft);
end;

{ One Unicode scalar as UTF-8, without pulling a codepage converter into the
  browser target. }
function BrowserUtf8(CodePoint: LongInt): TUTF8Char;
begin
  Result := '';
  if CodePoint < $80 then
    Result := Chr(CodePoint)
  else if CodePoint < $800 then
    Result := Chr($C0 or (CodePoint shr 6)) + Chr($80 or (CodePoint and $3F))
  else if CodePoint < $10000 then
    Result := Chr($E0 or (CodePoint shr 12)) + Chr($80 or ((CodePoint shr 6) and $3F)) +
      Chr($80 or (CodePoint and $3F))
  else if CodePoint < $110000 then
    Result := Chr($F0 or (CodePoint shr 18)) + Chr($80 or ((CodePoint shr 12) and $3F)) +
      Chr($80 or ((CodePoint shr 6) and $3F)) + Chr($80 or (CodePoint and $3F));
end;

{ Browser events arrive as separate callbacks without a native message loop.
  Run the idle work TApplication.HandleMessage performs after each message:
  queued async calls, released components, OnIdle and action updates. }
procedure LCLWasmIdle;
const
  MaxPasses = 16;
var
  Pass: Integer;
begin
  if (Application = nil) or BrowserInIdle then Exit;
  BrowserInIdle := True;
  try
    Pass := 0;
    repeat
      Application.Idle(False);
      Inc(Pass);
    until not TBrowserApplication(Application).HasAsyncCallQueued or (Pass >= MaxPasses);
  finally
    BrowserInIdle := False;
  end;
  BrowserInvalidate;
end;

function BrowserMenuJSON(Item: TMenuItem): TJSONObject;
var Children: TJSONArray; I: Integer;
begin
  Result := TJSONObject.Create(['id', PtrInt(Item), 'caption', Item.Caption,
    'enabled', Item.Enabled, 'visible', Item.Visible, 'checked', Item.Checked]);
  Children := TJSONArray.Create;
  Result.Add('children', Children);
  for I := 0 to Item.Count-1 do Children.Add(BrowserMenuJSON(Item.Items[I]));
end;

procedure SyncBrowserMenus;
var Form: TCDNonNativeForm; Data: TJSONObject; JSON: String;
begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
  if not BrowserMenusDirty and (BrowserMenuForm = Form.LCLForm) then Exit;
  BrowserMenusDirty := False;
  BrowserMenuForm := Form.LCLForm;
  if Form.LCLForm.Menu = nil then
  begin
    JSON := '{"children":[]}';
    BrowserMenus(PChar(JSON), Length(JSON));
    Exit;
  end;
  Data := BrowserMenuJSON(Form.LCLForm.Menu.Items);
  try
    JSON := Data.AsJSON;
    BrowserMenus(PChar(JSON), Length(JSON));
  finally
    Data.Free;
  end;
end;

procedure LCLWasmMenu(Handle: LongInt); cdecl;
var Item: TMenuItem; Form: TCDNonNativeForm;
  function FindItem(Root: TMenuItem): TMenuItem;
  var I: Integer;
  begin
    if PtrUInt(Root) = PtrUInt(Handle) then Exit(Root);
    Result := nil;
    for I := 0 to Root.Count-1 do
    begin
      Result := FindItem(Root.Items[I]);
      if Result <> nil then Exit;
    end;
  end;
begin
  Form := GetCurrentForm;
  if (Form = nil) or Form.InputDisabled or (Form.LCLForm.Menu = nil) then Exit;
  Item := FindItem(Form.LCLForm.Menu.Items);
  if (Item = nil) or not Item.Enabled or not Item.Visible then Exit;
  BrowserMenusDirty := True;
  Item.Click;
  LCLWasmIdle;
  SyncBrowserMenus;
end;

procedure LCLWasmFocus(Handle: LongInt); cdecl;
begin
  FocusBrowserDOMControl(PtrUInt(Handle));
end;

procedure LCLWasmControl(Handle: LongInt); cdecl;
begin
  DispatchBrowserDOMControl(PtrUInt(Handle));
  LCLWasmIdle;
end;

procedure LCLWasmLeave; cdecl;
var
  Form: TCDNonNativeForm;
begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
  CallbackMouseLeave(Form);
  LCLWasmIdle;
end;




procedure LCLWasmRender; cdecl;
var
  Form: TCDNonNativeForm;
  Raw: TRawImage;
  Damage: array[0..31] of TRect;
  Count, I: Integer;
  FullDamage: Boolean;
  procedure MarkControls(List: TFPList);
  var J: Integer; Control: TCDWinControl;
  begin
    if List = nil then Exit;
    for J := 0 to List.Count-1 do
    begin
      Control := TCDWinControl(List[J]);
      Control.BrowserNeedsPaint := FullDamage or (Control.InvalidateCount > 0)
        or (Control.ControlImage = nil);
      MarkControls(Control.Children);
    end;
  end;

begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
  SyncBrowserDOM;
  SyncBrowserMenus;
  if not BrowserDirty then Exit;
  FullDamage := BrowserFullDamage or (Form.Image = nil);
  if Form.Image <> nil then
    FullDamage := FullDamage or (Form.Image.Width <> Form.LCLForm.ClientWidth)
      or (Form.Image.Height <> Form.LCLForm.ClientHeight);
  Count := BrowserDamageCount;
  Damage := BrowserDamage;
  BrowserDirty := False;
  BrowserFullDamage := False;
  BrowserDamageCount := 0;
  UpdateControlLazImageAndCanvas(Form.Image, Form.Canvas,
    Form.LCLForm.ClientWidth, Form.LCLForm.ClientHeight, clfARGB32);
  if (Form.Image = nil) or (Form.Image.Width <= 0) or (Form.Image.Height <= 0) then
  begin
    BrowserDirty := True;
    BrowserFullDamage := True;
    Exit;
  end;
  if FullDamage then
  begin
    Count := 1;
    Damage[0] := Rect(0, 0, Form.Image.Width, Form.Image.Height);
  end;
  MarkControls(GetCDWinControlList(Form.LCLForm));
  for I := 0 to Count - 1 do
  begin
    RenderForm(Form.Image, Form.Canvas, Form.LCLForm, @Damage[I]);
    Form.Image.GetRawImage(Raw);
    BrowserPresent(Raw.Data, Form.Image.Width, Form.Image.Height,
      Damage[I].Left, Damage[I].Top, Damage[I].Right, Damage[I].Bottom, Ord(I = Count-1));
  end;
end;

procedure LCLWasmPointer(Kind, X, Y, Button, Modifiers: LongInt); cdecl;
var
  Form: TCDNonNativeForm;
  MouseButton: TMouseButton;
begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
  if Kind <> 2 then BrowserMenusDirty := True;
  BrowserMouse := Point(X, Y);
  BrowserMods := Modifiers;
  MouseButton := mbLeft;
  if Button = 1 then MouseButton := mbMiddle;
  if Button = 2 then MouseButton := mbRight;
  case Kind of
    0: CallbackMouseDown(Form, X, Y, MouseButton, BrowserShiftState(Modifiers));
    1: CallbackMouseUp(Form, X, Y, MouseButton, BrowserShiftState(Modifiers));
    2: CallbackMouseMove(Form, X, Y, BrowserShiftState(Modifiers));
    3..5: CallbackMouseDown(Form, X, Y, MouseButton, BrowserShiftState(Modifiers), Kind-1);
  end;
  if Kind = 2 then
  begin
    TBrowserApplication(Application).ReleaseComponents;
    TBrowserApplication(Application).ProcessAsyncCallQueue;
  end
  else LCLWasmIdle;
end;

{ Kind: 0 key down, 1 key up, 2 character input. Key carries a Windows virtual
  key code, CharCode a Unicode scalar for kind 2. }
procedure LCLWasmKey(Kind, Key, CharCode, Modifiers: LongInt); cdecl;
var
  Form: TCDNonNativeForm;
begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
  BrowserMenusDirty := True;
  BrowserMods := Modifiers;
  case Kind of
    0: CallbackKeyDown(Form, Word(Key));
    1: CallbackKeyUp(Form, Word(Key));
    2: CallbackKeyChar(Form, Word(Key), BrowserUtf8(CharCode));
  end;
  LCLWasmIdle;
end;

procedure LCLWasmWheel(X, Y, Delta, Modifiers: LongInt); cdecl;
var
  Form: TCDNonNativeForm;
begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
  BrowserMouse := Point(X, Y);
  BrowserMods := Modifiers;
  LCLSendMouseWheelMsg(Form.LCLForm, X, Y, Delta, BrowserShiftState(Modifiers));
  LCLWasmIdle;
end;

procedure LCLWasmResize(Width, Height: LongInt); cdecl;
var
  Form: TCDNonNativeForm;
begin
  BrowserWidth := Max(1, Width);
  BrowserHeight := Max(1, Height);
  BrowserDirty := True;
  BrowserFullDamage := True;
  Form := GetCurrentForm;
  if Application.MainForm <> nil then
    Application.MainForm.SetBounds(0, 0, BrowserWidth, BrowserHeight);
  LCLWasmIdle;
end;

procedure LCLWasmTimer(Handle: LongInt); cdecl;
begin
  TCDTimer(PtrUInt(Handle)).TimerFunc;
  LCLWasmIdle;
end;

function TCDWidgetSet.GetAppHandle: TLCLHandle;
begin
  Result := TLCLHandle(Self);
end;

procedure TCDWidgetSet.BackendCreate;
begin
  ScreenBitmapRawImage.Init;
  ScreenBitmapRawImage.Description.Init_BPP32_A8R8G8B8_BIO_TTB(1, 1);
  ScreenBitmapRawImage.CreateData(True);
  ScreenImage := TLazIntfImage.Create(0, 0);
  ScreenImage.SetRawImage(ScreenBitmapRawImage);
  ScreenDC := TLazCanvas.Create(ScreenImage);
end;

procedure TCDWidgetSet.BackendDestroy;
begin
  FreeAndNil(ScreenDC);
  FreeAndNil(ScreenImage);
end;

procedure TCDWidgetSet.AppInit(var ScreenInfo: TScreenInfo);
begin
  GenericAppInit;
  ScreenInfo.PixelsPerInchX := 96;
  ScreenInfo.PixelsPerInchY := 96;
  ScreenInfo.ColorDepth := 32;
  ScreenInfo.Initialized := True;
end;

procedure TCDWidgetSet.AppRun(const ALoop: TApplicationMainLoop);
begin
  { The browser owns the outer loop; the WASM reactor stays alive after returning. }
  LCLWasmIdle;
end;

procedure TCDWidgetSet.AppMinimize;
begin
  raise ENotSupportedException.Create('Browser windows cannot be minimized by LCL');
end;

procedure TCDWidgetSet.AppRestore;
begin
  BrowserDirty := True;
  BrowserInvalidate;
end;

procedure TCDWidgetSet.AppBringToFront;
begin
  BrowserDirty := True;
  BrowserInvalidate;
end;

procedure TCDWidgetSet.AppProcessMessages;
begin
  LCLWasmIdle;
end;

procedure TCDWidgetSet.AppWaitMessage;
begin
  BrowserWaitMessage(PtrUInt(GetCurrentForm));
end;

procedure TCDWidgetSet.AppTerminate;
begin
  FTerminating := True;
end;

procedure TCDWidgetSet.AppSetIcon(const Small, Big: HICON);
begin
  { Browser tab icons belong to the HTML host. }
end;

procedure TCDWidgetSet.AppSetTitle(const ATitle: string);
begin
  BrowserTitle(PChar(ATitle), Length(ATitle));
end;

procedure TCDWidgetSet.AppSetVisible(const AVisible: Boolean);
begin
  BrowserDirty := True;
  BrowserInvalidate;
end;

function TCDWidgetSet.AppRemoveStayOnTopFlags(const ASystemTopAlso: Boolean = False): Boolean;
begin
  Result := False;
end;

function TCDWidgetSet.AppRestoreStayOnTopFlags(const ASystemTopAlso: Boolean = False): Boolean;
begin
  Result := False;
end;

procedure TCDWidgetSet.AppSetMainFormOnTaskBar(const DoSet: Boolean);
begin
  { Browser forms are surfaces within their tab, not taskbar windows. }
end;

function TCDWidgetSet.CreateTimer(Interval: integer; TimerFunc: TWSTimerProc): TLCLHandle;
var
  Timer: TCDTimer;
begin
  Timer := TCDTimer.Create;
  Timer.TimerFunc := TimerFunc;
  Timer.NativeHandle := BrowserTimer(PtrUInt(Timer), Max(1, Interval));
  Result := TLCLHandle(Timer);
end;

function TCDWidgetSet.DestroyTimer(TimerHandle: TLCLHandle): boolean;
begin
  Result := TimerHandle <> 0;
  if not Result then Exit;
  BrowserClearTimer(TCDTimer(TimerHandle).NativeHandle);
  TCDTimer(TimerHandle).Free;
end;
