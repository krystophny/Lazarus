{ Browser platform backend for the LCL CustomDrawn interface.
  See COPYING.modifiedLGPL.txt for license and linking exception. }


function BrowserFontSize(DC: HDC): Integer;
begin
  if DC = 0 then DC := HDC(CDWidgetSet.ScreenDC);
  Result := Abs(TLazCanvas(DC).Font.Size);
  if Result = 0 then Result := 13;
end;

function BrowserControl(Handle: HWND): TWinControl;
begin
  Result := nil;
  if Handle <> 0 then Result := TCDBaseControl(Handle).GetWinControl;
end;

function TCDWidgetSet.ClientToScreen(Handle: HWND; var P: TPoint) : Boolean;
var
  Control: TWinControl;
begin
  Control := BrowserControl(Handle);
  Result := Control <> nil;
  if Result and not (Control is TCustomForm) then
    P := P + FindControlPositionRelativeToTheForm(Control);
end;

function TCDWidgetSet.ClipboardGetData(ClipboardType: TClipboardType;
  FormatID: TClipboardFormat; Stream: TStream): boolean;
begin
  raise ENotSupportedException.Create('CustomDrawn WASM: ClipboardGetData is not implemented');
end;

function TCDWidgetSet.ClipboardGetOwnerShip(ClipboardType: TClipboardType;
  OnRequestProc: TClipboardRequestEvent; FormatCount: integer;
  Formats: PClipboardFormat): boolean;
begin
  raise ENotSupportedException.Create('CustomDrawn WASM: ClipboardGetOwnerShip is not implemented');
end;


function TCDWidgetSet.EnumFontFamiliesEx(DC: HDC; lpLogFont: PLogFont; Callback: FontEnumExProc; Lparam: LParam; Flags: dword): longint;
begin
  Result := 0; { Font enumeration is outside this initial backend slice. }
end;

function TCDWidgetSet.ExtTextOut(DC: HDC; X, Y: Integer; Options: Longint;
  Rect: PRect; Str: PChar; Count: Longint; Dx: PInteger): Boolean;
var
  Canvas: TLazCanvas;
  Raw: TRawImage;
  Clip: TRect;
  Color: LongInt;
begin
  Result := IsValidDC(DC);
  if not Result then Exit;
  Canvas := TLazCanvas(DC);
  TLazIntfImage(Canvas.Image).GetRawImage(Raw);
  Clip := Bounds(0, 0, Canvas.Width, Canvas.Height);
  if Canvas.Clipping then Clip := Canvas.ClipRect;
  if ((Options and ETO_CLIPPED) <> 0) and (Rect <> nil) then
    IntersectRect(Clip, Clip, Rect^);
  Color := (Canvas.Font.FPColor.Red shr 8) shl 16
    or (Canvas.Font.FPColor.Green shr 8) shl 8 or (Canvas.Font.FPColor.Blue shr 8);
  BrowserText(Raw.Data, Canvas.Width, Canvas.Height,
    X + Canvas.WindowOrg.X + Canvas.BaseWindowOrg.X,
    Y + Canvas.WindowOrg.Y + Canvas.BaseWindowOrg.Y,
    Str, Count, BrowserFontSize(DC), Color,
    Clip.Left, Clip.Top, Clip.Right, Clip.Bottom);
end;

function TCDWidgetSet.BackendGetClientBounds(handle : HWND; var ARect : TRect) : Boolean;
var
  Control: TWinControl;
begin
  Control := BrowserControl(Handle);
  Result := Control <> nil;
  if Result then ARect := Bounds(0, 0, Control.Width, Control.Height);
end;

function TCDWidgetSet.GetCursorPos(var lpPoint: TPoint ): Boolean;
begin
  lpPoint := BrowserMouse;
  Result := True;
end;


function TCDWidgetSet.GetDeviceCaps(DC: HDC; Index: Integer): Integer;
begin
  case Index of
    LOGPIXELSX, LOGPIXELSY: Result := 96;
    BITSPIXEL: Result := 32;
    PLANES: Result := 1;
    HORZRES: Result := BrowserWidth;
    VERTRES: Result := BrowserHeight;
    else Result := 0;
  end;
end;

function TCDWidgetSet.GetKeyState(nVirtKey: Integer): Smallint;
begin
  Result := 0;
  case nVirtKey of
    $10: if (BrowserMods and 1) <> 0 then Result := Smallint($8000);  { VK_SHIFT }
    $11: if (BrowserMods and 2) <> 0 then Result := Smallint($8000);  { VK_CONTROL }
    $12: if (BrowserMods and 4) <> 0 then Result := Smallint($8000);  { VK_MENU }
    $01: if (BrowserMods and 8) <> 0 then Result := Smallint($8000);  { VK_LBUTTON }
  end;
end;

function TCDWidgetSet.GetSystemMetrics(nIndex: Integer): Integer;
begin
  case nIndex of
    SM_CXSCREEN, SM_CXVIRTUALSCREEN: Result := BrowserWidth;
    SM_CYSCREEN, SM_CYVIRTUALSCREEN: Result := BrowserHeight;
    SM_CMONITORS: Result := 1;
    SM_CMOUSEBUTTONS: Result := 3;
    SM_CXVSCROLL, SM_CYHSCROLL: Result := 16;
    SM_CXDOUBLECLK, SM_CYDOUBLECLK, SM_CXDRAG, SM_CYDRAG: Result := 4;
    else Result := 0;
  end;
end;

function TCDWidgetSet.GetTextExtentExPoint(DC: HDC; Str: PChar; Count,
  MaxWidth: Integer; MaxCount, PartialWidths: PInteger; var Size: Types.TSize
  ): Boolean;
var
  I, Width: Integer;
begin
  Result := GetTextExtentPoint(DC, Str, Count, Size);
  if MaxCount <> nil then MaxCount^ := 0;
  for I := 1 to Count do
  begin
    Width := BrowserMeasure(Str, I, BrowserFontSize(DC));
    if PartialWidths <> nil then PartialWidths[I-1] := Width;
    if (MaxCount <> nil) and (Width <= MaxWidth) then MaxCount^ := I;
  end;
end;

function TCDWidgetSet.GetTextExtentPoint(DC: HDC; Str: PChar; Count: Integer;
  var Size: Types.TSize): Boolean;
begin
  Size.cx := BrowserMeasure(Str, Count, BrowserFontSize(DC));
  Size.cy := Ceil(BrowserFontSize(DC) * 1.25);
  Result := True;
end;

function TCDWidgetSet.GetTextMetrics(DC: HDC; var TM: TTextMetric): Boolean;
begin
  FillChar(TM, SizeOf(TM), 0);
  TM.tmHeight := Ceil(BrowserFontSize(DC) * 1.25);
  TM.tmAscent := BrowserFontSize(DC);
  TM.tmDescent := TM.tmHeight - TM.tmAscent;
  TM.tmAveCharWidth := BrowserMeasure('n', 1, BrowserFontSize(DC));
  TM.tmMaxCharWidth := BrowserMeasure('W', 1, BrowserFontSize(DC));
  TM.tmWeight := 400;
  TM.tmDigitizedAspectX := 96;
  TM.tmDigitizedAspectY := 96;
  Result := True;
end;

function TCDWidgetSet.BackendGetWindowRelativePosition(Handle: HWND; var Left, Top: integer): boolean;
var
  Control: TWinControl;
begin
  Control := BrowserControl(Handle);
  Result := Control <> nil;
  if Result then begin Left := Control.Left; Top := Control.Top; end;
end;

function TCDWidgetSet.BackendGetWindowSize(Handle: hwnd; var Width, Height: integer): boolean;
var
  Control: TWinControl;
begin
  Control := BrowserControl(Handle);
  Result := Control <> nil;
  if Result then begin Width := Control.Width; Height := Control.Height; end;
end;

function TCDWidgetSet.BackendInvalidateRect(aHandle: HWND; Rect: pRect; bErase: Boolean): Boolean;
begin
  BrowserDirty := True;
  BrowserInvalidate;
  Result := True;
end;

function TCDWidgetSet.MessageBox(hWnd: HWND; lpText, lpCaption: PChar; uType: Cardinal): integer;
var
  Text, Caption: string;
begin
  { A raise here killed the whole instance: any application that reports an error
    or a disabled feature (TpX does this for LaTeX, sam2p, MetaPost and printing,
    all unavailable in a browser) died with a host-side application error instead
    of telling the user anything. Show the text through the host and answer with
    the button the flags ask for, so the caller's logic keeps working. }
  Text := '';
  if lpText <> nil then Text := lpText;
  Caption := '';
  if lpCaption <> nil then Caption := lpCaption;
  BrowserMessage(PChar(Text), PChar(Caption), Length(Text), Length(Caption),
    Integer(uType));
  if (uType and $0000000F) = MB_YESNO then
    Result := mrYes
  else
    Result := mrOk;
end;


function TCDWidgetSet.ScreenToClient(Handle : HWND; var P : TPoint) : Integer;
begin
  P.X := -P.X; P.Y := -P.Y;
  Result := Ord(ClientToScreen(Handle, P));
  P.X := -P.X; P.Y := -P.Y;
end;

function TCDWidgetSet.BackendSetFocus(hWnd: HWND): HWND;
begin
  Result := hWnd; { LCL owns internal focus; the host owns canvas focus. }
end;
