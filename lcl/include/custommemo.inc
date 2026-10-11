{%MainUnit ../stdctrls.pp}
{  $Id$  }

{******************************************************************************
                                     TCustomMemo
 ******************************************************************************
 
 *****************************************************************************
  This file is part of the Lazarus Component Library (LCL)

  See the file COPYING.modifiedLGPL.txt, included in this distribution,
  for details about the license.
 *****************************************************************************
}

{off $DEFINE DEBUG_MEMO}

constructor TCustomMemo.Create(AOwner: TComponent);
begin
  inherited Create(AOwner);
  fCompStyle := csMemo;
  FWantReturns := True;
  FWantTabs := False;
  FWordWrap := True;
  FLines := TTextStrings.Create;
  FVertScrollbar := TMemoScrollBar.Create(Self, sbVertical);
  FHorzScrollbar := TMemoScrollBar.Create(Self, sbHorizontal);
  AutoSelect := False;
  AutoSize := False;
end;

destructor TCustomMemo.Destroy;
begin
  FreeThenNil(FLines);
  FreeThenNil(FVertScrollbar);
  FreeThenNil(FHorzScrollbar);
  inherited Destroy;
end;

procedure TCustomMemo.Append(const AValue: String);
begin
  Lines.Add(AValue);
end;

procedure TCustomMemo.ScrollBy(DeltaX, DeltaY: Integer);
begin
  ScrollBy_WS(DeltaX, DeltaY);
end;

procedure TCustomMemo.SetCaretPos(const AValue: TPoint);
begin
  TWSCustomMemoClass(WidgetSetClass).SetCaretPos(Self, AValue);
end;

procedure TCustomMemo.SetWantReturns(const AValue: Boolean);
begin
  if FWantReturns = AValue then
    Exit;
  FWantReturns := AValue;
  if HandleAllocated then
    TWSCustomMemoClass(WidgetSetClass).SetWantReturns(Self, AValue);
end;

class procedure TCustomMemo.WSRegisterClass;
begin
  inherited WSRegisterClass;
  RegisterCustomMemo;
end;

function TCustomMemo.CanShowEmulatedTextHint: Boolean;
begin
  // CM_EXIT is sent in destroy -> this is called in destructor when Lines are already destroyed
  //Result := Assigned(Lines) and inherited CanShowEmulatedTextHint;
  // ToDo: Make this work. The line above does not work with GTK2 at least.
  Result := False;
end;

procedure TCustomMemo.CreateParams(var Params: TCreateParams);
begin
  inherited CreateParams(Params);
  Params.Style := (Params.Style and not ES_AUTOHSCROLL) or ES_AUTOVSCROLL or
    ES_MULTILINE or ES_WANTRETURN;
  case ScrollBars of
    ssHorizontal, ssAutoHorizontal:
      Params.Style := Params.Style or WS_HSCROLL;
    ssVertical, ssAutoVertical:
      Params.Style := Params.Style or WS_VSCROLL;
    ssBoth, ssAutoBoth:
      Params.Style := Params.Style or WS_HSCROLL or WS_VSCROLL;
  end;
  if WordWrap then
    Params.Style := Params.Style and not WS_HSCROLL
  else
    Params.Style := Params.Style or ES_AUTOHSCROLL;
end;

procedure TCustomMemo.InitializeWnd;
var
  NewStrings : TStrings;
  TextValue: TCaption;
  WidgetText: String;
begin
  {$ifdef DEBUG_MEMO}
  DebugLn('[TCustomMemo.InitializeWnd] A ',FLines.ClassName);
  {$endif}
  // fetch/create the interface item list
  TextValue := Lines.Text;
  NewStrings := TWSCustomMemoClass(WidgetSetClass).GetStrings(Self);
  // copy the items (text)
  NewStrings.Assign(Lines);

  // free old items
  FLines.Free;

  // new item list is the interface item list
  FLines:= NewStrings;

  // Preserve text that a widgetset cannot represent with its line list alone.
  if TWSCustomMemoClass(WidgetSetClass).GetText(Self, WidgetText) then
    TWSCustomMemoClass(WidgetSetClass).SetText(Self, TextValue);

  inherited InitializeWnd;
  {$ifdef DEBUG_MEMO}
  DebugLn('[TCustomMemo.InitializeWnd] END ',DbgSName(Self),' ',FLines.ClassName,' FLines.Count=',dbgs(FLines.Count));
  {$endif}
end;

procedure TCustomMemo.FinalizeWnd;
var
  NewStrings : TStrings;
  TextValue: String;
  I: Integer;
begin
  if Assigned(FLines) then
  begin
    {$ifdef DEBUG_MEMO}
    DebugLn('[TCustomMemo.FinalizeWnd] A ',DbgSName(Self),' ',FLines.ClassName,' FLines.Count=',dbgs(FLines.Count));
    {$endif}
    // create internal item list
    NewStrings := TTextStrings.Create;

    // copy items (text+objects) from the interface items list
    NewStrings.Assign(Lines);
    if TWSCustomMemoClass(WidgetSetClass).GetText(Self, TextValue) then
    begin
      NewStrings.Text := TextValue;
      for I := 0 to NewStrings.Count - 1 do
        if I < Lines.Count then NewStrings.Objects[I] := Lines.Objects[I];
    end;

    // Delete the interface item list
    TWSCustomMemoClass(WidgetSetClass).FreeStrings(FLines);

    // new item list is the internal item list
    FLines := NewStrings;
    {$ifdef DEBUG_MEMO}
    DebugLn('[TCustomMemo.FinalizeWnd] END ',DbgSName(Self),' ',FLines.ClassName,' FLines.Count=',dbgs(FLines.Count));
    {$endif}
  end;
  if FHorzScrollBar <> nil then FHorzScrollBar.InvalidateScrollInfo;
  if FVertScrollBar <> nil then FVertScrollBar.InvalidateScrollInfo;
  inherited FinalizeWnd;
end;

function TCustomMemo.RealGetText: TCaption;
begin
  { Prefer the live widget value when the widgetset can provide it. Some
    backends expose a real text buffer through Lines, while others (such as
    the browser textarea) use a line list that adds a structural final line
    break to Text. }
  if HandleAllocated and not (csLoading in ComponentState) and
    TWSCustomMemoClass(WidgetSetClass).GetText(Self, Result) then
    Exit;
  if FLines <> nil then Result := FLines.Text
  else Result := '';
  {$ifdef DEBUG_MEMO}
  debugln('TCustomMemo.RealGetText "',Result,'"');
  {$endif}
end;

procedure TCustomMemo.RealSetText(const AValue: TCaption);
var
  WidgetText: String;
begin
  {$ifdef DEBUG_MEMO}
  debugln('TCustomMemo.RealSetText "',AValue,'"');
  {$endif}
  fTExtChangedByRealSetText:=True;
  Lines.Text := AValue;
  if HandleAllocated and
    TWSCustomMemoClass(WidgetSetClass).GetText(Self, WidgetText) then
  begin
    if WidgetText <> AValue then
      TWSCustomMemoClass(WidgetSetClass).SetText(Self, AValue);
  end;
  fTExtChangedByRealSetText:=False;
end;

function TCustomMemo.GetCachedText(var CachedText: TCaption): boolean;
begin
  Result:= false;
end;

{------------------------------------------------------------------------------
  Getter for CaretPos
 ------------------------------------------------------------------------------}
function TCustomMemo.GetCaretPos: TPoint;
begin
  Result := TWSCustomMemoClass(WidgetSetClass).GetCaretPos(Self);
end;

{------------------------------------------------------------------------------
  Prevents false firing of the OnEditingDone event if the memo accepts the
  RETURN key for new-line input.
 ------------------------------------------------------------------------------}
procedure TCustomMemo.KeyUpAfterInterface(var Key: Word; Shift: TShiftState);
begin
  if (Key = VK_RETURN) and FWantReturns then
    Key := 0;
  inherited;
end;

procedure TCustomMemo.SetLines(const AValue: TStrings);
begin
  if (AValue <> nil) then
    FLines.Assign(AValue);
end;

procedure TCustomMemo.SetScrollBars(const AValue: TScrollStyle);
begin
  if AValue <> FScrollbars then begin
    FScrollbars:= AValue;
    if HandleAllocated and (not (csLoading in ComponentState)) then
      TWSCustomMemoClass(WidgetSetClass).SetScrollbars(Self, AValue);
  end;
end;

procedure TCustomMemo.Loaded;
begin
  inherited Loaded;
  if HandleAllocated then
  begin
    TWSCustomMemoClass(WidgetSetClass).SetScrollbars(Self, FScrollbars);
    TWSCustomMemoClass(WidgetSetClass).SetWordWrap(Self, FWordWrap);
  end;
end;

procedure TCustomMemo.CMWantSpecialKey(var Message: TCMWantSpecialKey);
begin
  case Message.CharCode of
    VK_RETURN: if WantReturns then Message.Result := 1;
    VK_TAB: if WantTabs then Message.Result := 1;
  else
    inherited;
  end;
end;

procedure TCustomMemo.WMGetDlgCode(var Message: TLMGetDlgCode);
begin
  inherited;
  case Message.CharCode of
    VK_TAB:
    begin
      if WantTabs then
        Message.Result := Message.Result or (DLGC_WANTTAB or DLGC_WANTALLKEYS)
      else
        Message.Result := Message.Result and not (DLGC_WANTTAB or DLGC_WANTALLKEYS);
    end;
    VK_RETURN:
    begin
      if WantReturns then
        Message.Result := Message.Result or DLGC_WANTALLKEYS
      else
        Message.Result := Message.Result and not DLGC_WANTALLKEYS;
    end;
    VK_ESCAPE:
      Message.Result := Message.Result and not DLGC_WANTALLKEYS;
  end;
end;

class function TCustomMemo.GetControlClassDefaultSize: TSize;
begin
  Result.CX := 150;
  Result.CY := 90;
end;

procedure TCustomMemo.UTF8KeyPress(var UTF8Key: TUTF8Char);
begin
  inherited UTF8KeyPress(UTF8Key);
  if not WantReturns and (UTF8Key = #13) then
    UTF8Key := '';
end;

procedure TCustomMemo.SetWantTabs(const NewWantTabs: boolean);
begin
  if FWantTabs = NewWantTabs then exit;
  FWantTabs := NewWantTabs;
  if HandleAllocated then
    TWSCustomMemoClass(WidgetSetClass).SetWantTabs(Self, NewWantTabs);
end;

procedure TCustomMemo.SetWordWrap(const AValue: boolean);
begin
  if AValue <> FWordWrap then
  begin
    {$ifdef DEBUG_MEMO}
    DebugLn(['TCustomMemo.SetWordWrap ',Name,' Old=',FWordWrap,' New=',AValue]);
    {$endif}
    FWordWrap := AValue;
    if HandleAllocated and (not (csLoading in ComponentState)) then
      TWSCustomMemoClass(WidgetSetClass).SetWordWrap(Self, AValue);
  end;    
end;

// included by stdctrls.pp

