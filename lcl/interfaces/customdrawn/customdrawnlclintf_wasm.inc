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
    for I := 0 to Buttons.Count-1 do Choices.Add(Buttons[I].ModalResult);
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
begin
  Data := TJSONObject.Create(['kind', 'message', 'caption', DialogCaption,
    'message', DialogMessage, 'escape', EscapeResult, 'default', DefaultIndex]);
  try
    Choices := TJSONArray.Create;
    Data.Add('buttons', Choices);
    for I := 0 to ButtonCount-1 do Choices.Add(Buttons[I]);
    JSON := Data.AsJSON;
    Result := BrowserDialog(PChar(JSON), Length(JSON));
  finally
    Data.Free;
  end;
end;

function TCDWidgetSet.RawImage_FromDevice(out ARawImage: TRawImage; ADC: HDC; const ARect: TRect): Boolean;
begin
  raise ENotSupportedException.Create('CustomDrawn WASM: RawImage_FromDevice is not implemented');
end;


procedure TCDWidgetSet.ShowVirtualKeyboard;
begin
  { Browser input focus controls the platform keyboard. }
end;
