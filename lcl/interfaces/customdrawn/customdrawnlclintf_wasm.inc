{ Browser platform backend for the LCL CustomDrawn interface.
  See COPYING.modifiedLGPL.txt for license and linking exception. }

function TCDWidgetSet.AskUser(const DialogCaption, DialogMessage: string; DialogType:
   LongInt; Buttons: TDialogButtons; HelpCtx: Longint): LongInt;
var
  Data: TJSONObject;
  Choices: TJSONArray;
  I: Integer;
  JSON: String;
begin
  Data := TJSONObject.Create(['kind', 'message', 'caption', DialogCaption, 'message', DialogMessage]);
  try
    Choices := TJSONArray.Create;
    Data.Add('buttons', Choices);
    for I := 0 to Buttons.Count-1 do
    begin
      Choices.Add(TJSONObject.Create(['id', Buttons[I].ModalResult, 'caption', Buttons[I].Caption]));
      if Buttons[I].Default then Data.Add('default', I);
      if Buttons[I].Cancel then Data.Add('escape', Buttons[I].ModalResult);
    end;
    JSON := Data.AsJSON;
    Result := BrowserDialog(PChar(JSON), Length(JSON));
  finally
    Data.Free;
  end;
end;

function TCDWidgetSet.CreateStandardCursor(ACursor: SmallInt): HCURSOR;
begin
  { Nonzero symbolic handles prevent TScreen falling back to native .cur
    resources, which are not needed for the browser's CSS cursors. }
  if (ACursor >= crLow) and (ACursor <= crHigh) then
    Result := 1 - ACursor
  else
    Result := 0;
end;

procedure TCDWidgetSet.UpdateBrowserCursor(AControl: TWinControl);
var
  Form: TCDNonNativeForm;
  Control: TWinControl;
  Cursor: HCURSOR;
begin
  Form := GetCurrentForm;
  if (Form = nil) or (Screen = nil) then Exit;
  if Screen.RealCursor <> crDefault then
  begin
    ApplyBrowserCursor(Screen.Cursors[Screen.RealCursor]);
    Exit;
  end;
  Control := AControl;
  if Control = nil then Control := Form.LastMouseDownControl;
  if Control = nil then
    Control := FindControlWhichReceivedEvent(Form.LCLForm, Form.Children,
      BrowserMouse.X, BrowserMouse.Y);
  while Control <> nil do
  begin
    Cursor := 0;
    if Control.HandleAllocated then
      Cursor := HCURSOR(TCDBaseControl(Control.Handle).Props['BrowserCursor']);
    if Cursor > 1 then
    begin
      ApplyBrowserCursor(Cursor);
      Exit;
    end;
    Control := Control.Parent;
  end;
  ApplyBrowserCursor(Screen.Cursors[crDefault]);
end;

procedure TCDWidgetSet.HideVirtualKeyboard();
begin
  { Browser input focus controls the platform keyboard. }
end;

function TCDWidgetSet.IsMobilePlatform: Boolean;
begin
  Result := False;
end;

function TCDWidgetSet.PromptUser(const DialogCaption : string;
                    const DialogMessage : string;
                          DialogType    : LongInt;
                          Buttons       : PLongInt;
                          ButtonCount   : LongInt;
                          DefaultIndex  : LongInt;
                          EscapeResult  : LongInt) : LongInt;
var
  Data: TJSONObject;
  Choices: TJSONArray;
  JSON: String;
  I: Integer;
  function ButtonCaption(ID: Integer): String;
  begin
    case ID of
      idButtonOK: Result := rsmbOK;
      idButtonCancel: Result := rsmbCancel;
      idButtonHelp: Result := rsmbHelp;
      idButtonYes: Result := rsmbYes;
      idButtonNo: Result := rsmbNo;
      idButtonClose: Result := rsmbClose;
      idButtonAbort: Result := rsmbAbort;
      idButtonRetry: Result := rsmbRetry;
      idButtonIgnore: Result := rsmbIgnore;
      idButtonAll: Result := rsmbAll;
      idButtonYesToAll: Result := rsmbYesToAll;
      idButtonNoToAll: Result := rsmbNoToAll;
      idButtonOpen: Result := rsmbOpen;
      idButtonSave: Result := rsmbSave;
      idButtonContinue: Result := 'Continue';
      idButtonTryAgain: Result := 'Try again';
    else Result := IntToStr(ID);
    end;
  end;
begin
  Data := TJSONObject.Create(['kind', 'message', 'caption', DialogCaption,
    'message', DialogMessage, 'escape', EscapeResult, 'default', DefaultIndex]);
  try
    Choices := TJSONArray.Create;
    Data.Add('buttons', Choices);
    for I := 0 to ButtonCount-1 do
      Choices.Add(TJSONObject.Create(['id', Buttons[I], 'caption', ButtonCaption(Buttons[I])]));
    JSON := Data.AsJSON;
    Result := BrowserDialog(PChar(JSON), Length(JSON));
  finally
    Data.Free;
  end;
end;

function TCDWidgetSet.RawImage_FromDevice(out ARawImage: TRawImage; ADC: HDC; const ARect: TRect): Boolean;
var
  Source: TRawImage;
  Clip: TRect;
begin
  ARawImage.Init;
  Result := IsValidDC(ADC);
  if not Result then Exit;
  TLazIntfImage(TLazCanvas(ADC).Image).GetRawImage(Source);
  Result := IntersectRect(Clip, ARect, Bounds(0, 0, Source.Description.Width, Source.Description.Height));
  if Result then Source.ExtractRect(Clip, ARawImage);
end;


procedure TCDWidgetSet.ShowVirtualKeyboard;
begin
  { Browser input focus controls the platform keyboard. }
end;

function TCDWidgetset.ShowSelectItemDialog(const AItems: TStrings; APos: TPoint): Boolean;
var
  Data: TJSONObject;
  Items: TJSONArray;
  JSON: String;
  I, Selected: Integer;
begin
  Result := False;
  if AItems.Count = 0 then Exit;
  Data := TJSONObject.Create(['kind', 'select', 'caption', 'Choose item']);
  try
    Items := TJSONArray.Create;
    Data.Add('items', Items);
    for I := 0 to AItems.Count-1 do Items.Add(AItems[I]);
    JSON := Data.AsJSON;
    Selected := BrowserDialog(PChar(JSON), Length(JSON));
    Result := (Selected >= 0) and (Selected < AItems.Count);
    if Result and Assigned(OnShowSelectItemDialogResult) then
      OnShowSelectItemDialogResult(Selected);
  finally
    Data.Free;
  end;
end;
