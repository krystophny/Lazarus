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
