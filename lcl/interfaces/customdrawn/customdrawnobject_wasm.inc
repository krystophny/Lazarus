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

var
  BrowserWidth: LongInt = 720;
  BrowserHeight: LongInt = 480;
  BrowserMouse: TPoint;

procedure LCLWasmRender; cdecl;
var
  Form: TCDNonNativeForm;
  Raw: TRawImage;
begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
  UpdateControlLazImageAndCanvas(Form.Image, Form.Canvas,
    Form.LCLForm.ClientWidth, Form.LCLForm.ClientHeight, clfARGB32);
  SyncBrowserDOM;
  RenderForm(Form.Image, Form.Canvas, Form.LCLForm);
  Form.Image.GetRawImage(Raw);
  BrowserPresent(Raw.Data, Form.Image.Width, Form.Image.Height);
end;

procedure LCLWasmPointer(Kind, X, Y, Button, Modifiers: LongInt); cdecl;
var
  Form: TCDNonNativeForm;
  MouseButton: TMouseButton;
  Shift: TShiftState;
begin
  Form := GetCurrentForm;
  if Form = nil then Exit;
  BrowserMouse := Point(X, Y);
  MouseButton := mbLeft;
  if Button = 1 then MouseButton := mbMiddle;
  if Button = 2 then MouseButton := mbRight;
  Shift := [];
  if (Modifiers and 1) <> 0 then Include(Shift, ssShift);
  if (Modifiers and 2) <> 0 then Include(Shift, ssCtrl);
  if (Modifiers and 4) <> 0 then Include(Shift, ssAlt);
  if (Modifiers and 8) <> 0 then Include(Shift, ssLeft);
  case Kind of
    0: CallbackMouseDown(Form, X, Y, MouseButton, Shift);
    1: CallbackMouseUp(Form, X, Y, MouseButton, Shift);
    2: CallbackMouseMove(Form, X, Y, Shift);
  end;
  BrowserInvalidate;
end;

procedure LCLWasmResize(Width, Height: LongInt); cdecl;
var
  Form: TCDNonNativeForm;
begin
  BrowserWidth := Max(1, Width);
  BrowserHeight := Max(1, Height);
  Form := GetCurrentForm;
  if Form <> nil then Form.LCLForm.SetBounds(0, 0, BrowserWidth, BrowserHeight);
  BrowserInvalidate;
end;

procedure LCLWasmTimer(Handle: LongInt); cdecl;
begin
  TCDTimer(PtrUInt(Handle)).TimerFunc;
  BrowserInvalidate;
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
  BrowserInvalidate;
end;

procedure TCDWidgetSet.AppMinimize;
begin
  raise ENotSupportedException.Create('Browser windows cannot be minimized by LCL');
end;

procedure TCDWidgetSet.AppRestore;
begin
  BrowserInvalidate;
end;

procedure TCDWidgetSet.AppBringToFront;
begin
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
