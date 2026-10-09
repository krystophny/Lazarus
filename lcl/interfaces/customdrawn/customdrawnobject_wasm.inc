{ Browser platform backend for the LCL CustomDrawn interface.
  See COPYING.modifiedLGPL.txt for license and linking exception. }

procedure BrowserPresent(Data: Pointer; Width, Height: LongInt); external 'lcl' name 'present';
procedure BrowserInvalidate; external 'lcl' name 'invalidate';
procedure BrowserTitle(Text: PChar; Count: LongInt); external 'lcl' name 'title';
function BrowserTimer(Handle, Interval: LongInt): LongInt; external 'lcl' name 'timer';
procedure BrowserClearTimer(Handle: LongInt); external 'lcl' name 'clear_timer';
function BrowserMeasure(Text: PChar; Count, FontSize: LongInt): LongInt; external 'lcl' name 'measure';
procedure BrowserText(Data: Pointer; Width, Height, X, Y: LongInt;
  Text: PChar; Count, FontSize, Color, ClipLeft, ClipTop, ClipRight, ClipBottom: LongInt);
  external 'lcl' name 'text';

type
  TBrowserApplication = class(TApplication);

var
  { Set by any invalidation; frames without it skip composing and presenting. }
  BrowserDirty: Boolean = True;
  BrowserInIdle: Boolean = False;
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
begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
  SyncBrowserDOM;
  if not BrowserDirty then Exit;
  BrowserDirty := False;
  UpdateControlLazImageAndCanvas(Form.Image, Form.Canvas,
    Form.LCLForm.ClientWidth, Form.LCLForm.ClientHeight, clfARGB32);
  RenderForm(Form.Image, Form.Canvas, Form.LCLForm);
  Form.Image.GetRawImage(Raw);
  BrowserPresent(Raw.Data, Form.Image.Width, Form.Image.Height);
end;

procedure LCLWasmPointer(Kind, X, Y, Button, Modifiers: LongInt); cdecl;
var
  Form: TCDNonNativeForm;
  MouseButton: TMouseButton;
begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
  BrowserMouse := Point(X, Y);
  BrowserMods := Modifiers;
  MouseButton := mbLeft;
  if Button = 1 then MouseButton := mbMiddle;
  if Button = 2 then MouseButton := mbRight;
  case Kind of
    0: CallbackMouseDown(Form, X, Y, MouseButton, BrowserShiftState(Modifiers));
    1: CallbackMouseUp(Form, X, Y, MouseButton, BrowserShiftState(Modifiers));
    2: CallbackMouseMove(Form, X, Y, BrowserShiftState(Modifiers));
  end;
  LCLWasmIdle;
end;

{ Kind: 0 key down, 1 key up, 2 character input. Key carries a Windows virtual
  key code, CharCode a Unicode scalar for kind 2. }
procedure LCLWasmKey(Kind, Key, CharCode, Modifiers: LongInt); cdecl;
var
  Form: TCDNonNativeForm;
begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
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
  BrowserDirty := True;
  BrowserWidth := Max(1, Width);
  BrowserHeight := Max(1, Height);
  Form := GetCurrentForm;
  if Form <> nil then Form.LCLForm.SetBounds(0, 0, BrowserWidth, BrowserHeight);
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
  { Browser events are delivered through exported callbacks, without a nested loop. }
end;

procedure TCDWidgetSet.AppWaitMessage;
begin
  raise ENotSupportedException.Create('Blocking message loops are unsupported in the browser');
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
