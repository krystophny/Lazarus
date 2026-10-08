{ Browser platform backend for the LCL CustomDrawn interface.
  See COPYING.modifiedLGPL.txt for license and linking exception. }

function TCDWidgetSet.AskUser(const DialogCaption, DialogMessage: string; DialogType:
   LongInt; Buttons: TDialogButtons; HelpCtx: Longint): LongInt;
begin
  raise ENotSupportedException.Create('CustomDrawn WASM: AskUser is not implemented');
end;

procedure TCDWidgetSet.HideVirtualKeyboard();
begin
  raise ENotSupportedException.Create('CustomDrawn WASM: HideVirtualKeyboard is not implemented');
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
begin
  raise ENotSupportedException.Create('CustomDrawn WASM: PromptUser is not implemented');
end;

function TCDWidgetSet.RawImage_FromDevice(out ARawImage: TRawImage; ADC: HDC; const ARect: TRect): Boolean;
begin
  raise ENotSupportedException.Create('CustomDrawn WASM: RawImage_FromDevice is not implemented');
end;


procedure TCDWidgetSet.ShowVirtualKeyboard;
begin
  raise ENotSupportedException.Create('CustomDrawn WASM: ShowVirtualKeyboard is not implemented');
end;
