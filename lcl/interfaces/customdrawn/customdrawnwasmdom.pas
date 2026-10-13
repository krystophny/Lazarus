{ LCL browser controls realized through Free Pascal JOB.
  See COPYING.modifiedLGPL.txt for license and linking exception. }
unit CustomDrawnWasmDOM;
{$mode objfpc}{$H+}
interface
uses Classes, SysUtils, Types, Math, Controls, StdCtrls, ExtCtrls, Forms,
  LCLType, TextStrings, WSStdCtrls, CustomDrawnWSStdCtrls, CustomDrawnProc, LazUTF8, Job.JS;
type
  TBrowserWSEdit = class(TCDWSCustomEdit)
  published
    class function CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle; override;
    class procedure DestroyHandle(const Control: TWinControl); override;
    class procedure SetText(const Control: TWinControl; const Text: String); override;
    class procedure SetSelStart(const Control: TCustomEdit; NewStart: Integer); override;
    class procedure SetSelLength(const Control: TCustomEdit; NewLength: Integer); override;
  end;
  TBrowserWSMemo = class(TWSCustomMemo)
  published
    class function CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle; override;
    class procedure DestroyHandle(const Control: TWinControl); override;
    class function GetStrings(const Control: TCustomMemo): TStrings; override;
  end;
  TBrowserWSButton = class(TWSButton)
  published
    class function CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle; override;
    class procedure DestroyHandle(const Control: TWinControl); override;
    class procedure SetBounds(const Control: TWinControl; const X, Y, Width, Height: Integer); override;
    class procedure ShowHide(const Control: TWinControl); override;
    class procedure SetText(const Control: TWinControl; const Text: String); override;
  end;
  TBrowserWSCheckBox = class(TWSCustomCheckBox)
  published
    class function CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle; override;
    class procedure DestroyHandle(const Control: TWinControl); override;
    class procedure SetBounds(const Control: TWinControl; const X, Y, Width, Height: Integer); override;
    class procedure ShowHide(const Control: TWinControl); override;
    class procedure SetText(const Control: TWinControl; const Text: String); override;
    class function RetrieveState(const Control: TCustomCheckBox): TCheckBoxState; override;
    class procedure SetState(const Control: TCustomCheckBox; const State: TCheckBoxState); override;
  end;
procedure SyncBrowserDOM;
procedure DispatchBrowserDOMControl(Handle: PtrUInt);
procedure DispatchBrowserDOMComposition(Handle: PtrUInt; Started: Boolean);
procedure FocusBrowserDOMControl(Handle: PtrUInt);
procedure ForgetBrowserFocusReveal(Control: TWinControl);
implementation
uses CustomDrawnWSControls, CustomDrawnPrivate, LCLMessageGlue, LCLIntf, LCLProc,
  LMessages, CustomDrawnInt;
type
  TDOMControl = class
    Control: TWinControl;
    Element, CaptionElement, Root: IJSObject;
    SyncedStyle, SyncedText, SyncedLabel: String;
    SyncedStart, SyncedFinish: Integer;
    SyncedEnabled, SyncedReadOnly, Initialized, Updating, Hidden,
      IsComposing: Boolean;
    SyncedMaxLength: Integer;
    procedure Activate;
    procedure Sync;
    destructor Destroy; override;
  end;
procedure BrowserControlBind(Handle: LongInt; ID: PChar; Count: LongInt); external 'lcl' name 'control_bind';
procedure BrowserControlUnbind(Handle: LongInt); external 'lcl' name 'control_unbind';

var
  Entries: TFPList;
  Document, Surface, CanvasElement: IJSObject;
  LastDOMFocus: TWinControl;
  LastRevealedForm: TCustomForm;
  LastRevealedControl: TWinControl;

procedure RevealFocusedControl;
var
  CurrentForm: TCDNonNativeForm;
  FocusedControl, ParentControl: TWinControl;
begin
  CurrentForm := GetCurrentForm;
  if CurrentForm = nil then
  begin
    LastRevealedForm := nil;
    LastRevealedControl := nil;
    Exit;
  end;
  FocusedControl := CurrentForm.FocusedControl;
  if (LastRevealedForm = CurrentForm.LCLForm)
    and (LastRevealedControl = FocusedControl) then Exit;
  LastRevealedForm := CurrentForm.LCLForm;
  LastRevealedControl := FocusedControl;
  if (FocusedControl = nil)
    or (GetParentForm(FocusedControl) <> CurrentForm.LCLForm) then Exit;
  ParentControl := FocusedControl.Parent;
  while ParentControl <> nil do
  begin
    if ParentControl is TScrollingWinControl then
      TScrollingWinControl(ParentControl).ScrollInView(FocusedControl);
    ParentControl := ParentControl.Parent;
  end;
end;

procedure ForgetBrowserFocusReveal(Control: TWinControl);
begin
  if LastRevealedControl = Control then
  begin
    LastRevealedControl := nil;
    LastRevealedForm := nil;
  end;
  if LastRevealedForm = Control then
    LastRevealedForm := nil;
end;

function FindEntry(Control: TWinControl): TDOMControl;
var
  I: Integer;
begin
  Result := nil;
  if Entries = nil then Exit;
  for I := 0 to Entries.Count-1 do
    if TDOMControl(Entries[I]).Control = Control then Exit(TDOMControl(Entries[I]));
end;

procedure SyncControl(Control: TWinControl);
var
  Entry: TDOMControl;
begin
  Entry := FindEntry(Control);
  if Entry <> nil then Entry.Sync;
end;

function SetDOMCaption(Control: TWinControl; const Text: String): Boolean;
var
  Entry: TDOMControl;
begin
  Entry := FindEntry(Control);
  Result := Entry <> nil;
  if not Result then Exit;
  Entry.SyncedText := Text;
  Entry.CaptionElement.WriteJSPropertyUTF8String('textContent', RemoveAmpersands(Text));
end;

procedure FocusCanvasForPaintedControl;
var
  CurrentForm: TCDNonNativeForm;
  FocusedControl: TWinControl;
  LastEntry: TDOMControl;
  ActiveElement: IJSObject;
begin
  if (LastDOMFocus = nil) or (CanvasElement = nil) then Exit;
  CurrentForm := GetCurrentForm;
  if CurrentForm = nil then Exit;
  FocusedControl := CurrentForm.FocusedControl;
  if (FocusedControl = nil) or (FindEntry(FocusedControl) <> nil) then Exit;
  if GetParentForm(FocusedControl) <> CurrentForm.LCLForm then Exit;

  LastEntry := FindEntry(LastDOMFocus);
  if LastEntry = nil then Exit;
  ActiveElement := Document.ReadJSPropertyObject('activeElement', TJSObject) as IJSObject;
  if (ActiveElement = nil)
    or not ActiveElement.InvokeJSBooleanResult('isSameNode', [LastEntry.Element]) then Exit;

  { Painted controls have no DOM node to take focus from the last overlay
    input. Move browser focus to the canvas so document key events reach LCL. }
  LastEntry.Element.InvokeJSNoResult('blur', []);
  CanvasElement.InvokeJSNoResult('focus', []);
  LastDOMFocus := nil;
end;

procedure DispatchBrowserDOMControl(Handle: PtrUInt);
var
  I: Integer;
begin
  if Entries = nil then Exit;
  for I := 0 to Entries.Count-1 do
    if PtrUInt(TDOMControl(Entries[I]).Control) = Handle then
    begin
      TDOMControl(Entries[I]).Activate;
      Exit;
    end;
end;

procedure DispatchBrowserDOMComposition(Handle: PtrUInt; Started: Boolean);
var
  I, Flags: Integer;
  Entry: TDOMControl;
begin
  if Entries = nil then Exit;
  if Started then Flags := GTK_IM_FLAG_START else Flags := GTK_IM_FLAG_END;
  for I := 0 to Entries.Count-1 do
  begin
    Entry := TDOMControl(Entries[I]);
    if PtrUInt(Entry.Control) <> Handle then Continue;
    if not (Entry.Control is TCustomEdit) then Exit;
    Entry.IsComposing := Started;
    Entry.Control.Perform(LM_IM_COMPOSITION, Flags, 0);
    Exit;
  end;
end;

procedure FocusBrowserDOMControl(Handle: PtrUInt);
var
  I: Integer;
begin
  if Entries = nil then Exit;
  for I := 0 to Entries.Count-1 do
    if PtrUInt(TDOMControl(Entries[I]).Control) = Handle then
    begin
      LastDOMFocus := TDOMControl(Entries[I]).Control;
      if LastDOMFocus.CanFocus then LastDOMFocus.SetFocus;
      Exit;
    end;
end;

procedure TDOMControl.Activate;
var
  Value, Prefix: String;
  Start, Finish: Integer;
  NotifyTextChanged: Boolean;
begin
  if not Control.Enabled then Exit;
  { Callbacks may destroy their dynamic DOM-backed control. Schedule repaint
    before dispatching into application code so no Handle access follows it. }
  LCLIntf.InvalidateRect(Control.Handle, nil, False);
  { LM_CHANGED retrieves the browser state through RetrieveState. }
  NotifyTextChanged := False;
  if Control is TCustomEdit then
  begin
    Value := Element.ReadJSPropertyUTF8String('value');
    Start := Element.ReadJSPropertyLongInt('selectionStart');
    Finish := Element.ReadJSPropertyLongInt('selectionEnd');
    Updating := True;
    try
      SyncedText := Value;
      SyncedStart := Start; SyncedFinish := Finish;
      if TCustomEdit(Control).Text = Value then
      else if Control is TCustomMemo then
      begin
        TCustomMemo(Control).Lines.Text := Value;
        NotifyTextChanged := True;
      end
      else
      begin
        { Set the widget value without sending CM_TEXTCHANGED yet. The message
          invokes OnChange, which may destroy both Control and this entry. }
        TBrowserWSEdit.SetText(Control, Value);
        NotifyTextChanged := True;
      end;
      Prefix := UTF8Encode(Copy(UTF8Decode(Value), 1, Start));
      TCDWSCustomEdit.SetSelLength(TCustomEdit(Control), 0);
      TCDWSCustomEdit.SetSelStart(TCustomEdit(Control), UTF8Length(Prefix));
      Prefix := UTF8Encode(Copy(UTF8Decode(Value), Start+1, Finish-Start));
      TCDWSCustomEdit.SetSelLength(TCustomEdit(Control), UTF8Length(Prefix));
    finally
      Updating := False;
    end;
    { This application callback may destroy both Control and Self; keep it
      as the final operation that touches either one. }
    if NotifyTextChanged then Control.Perform(CM_TEXTCHANGED, 0, 0);
  end
  else if Control is TCustomCheckBox then LCLSendChangedMsg(Control)
  else LCLSendClickedMsg(Control);
end;

procedure TDOMControl.Sync;
var
  Style: IJSObject;
  ActiveElement: IJSObject;
  Position, ParentPosition: TPoint;
  ControlRect, ClipRect, ParentRect, Intersection: TRect;
  ParentControl: TWinControl;
  Value, NewStyle, LabelText, ClipPath: String;
  Start, Finish, FontSize, ClipTop, ClipRight, ClipBottom, ClipLeft: Integer;
  HasScrollClip, IsClippedOut: Boolean;
  Edit: TCustomEdit;
begin
  if Updating or (GetCurrentForm = nil) or not Control.HandleAllocated then Exit;
  if not Control.IsVisible or (GetParentForm(Control) <> GetCurrentForm.LCLForm) then
  begin
    if not Hidden then
    begin
      Style := Root.ReadJSPropertyObject('style', TJSObject) as IJSObject;
      Style.WriteJSPropertyUTF8String('display', 'none');
      Hidden := True;
      SyncedStyle := '';
    end;
    Exit;
  end;
  Hidden := False;
  if Control is TCustomLabeledEdit then LabelText := TCustomLabeledEdit(Control).EditLabel.Caption
  else if Control.Hint <> '' then LabelText := Control.Hint
  else LabelText := 'Edit text';
  if (Control is TCustomEdit) and (LabelText <> SyncedLabel) then
  begin
    Element.InvokeJSNoResult('setAttribute', ['aria-label', LabelText]);
    SyncedLabel := LabelText;
  end;
  Position := FindControlPositionRelativeToTheForm(Control, True);
  ControlRect := Bounds(Position.X, Position.Y, Control.Width, Control.Height);
  ClipRect := ControlRect;
  HasScrollClip := False;
  IsClippedOut := False;
  ParentControl := Control.Parent;
  while ParentControl <> nil do
  begin
    if ParentControl is TScrollingWinControl then
    begin
      HasScrollClip := True;
      ParentPosition := FindControlPositionRelativeToTheForm(ParentControl, True);
      ParentRect := Bounds(ParentPosition.X, ParentPosition.Y,
        ParentControl.ClientWidth, ParentControl.ClientHeight);
      if IntersectRect(Intersection, ClipRect, ParentRect) then
        ClipRect := Intersection
      else
      begin
        IsClippedOut := True;
        Break;
      end;
    end;
    ParentControl := ParentControl.Parent;
  end;
  ClipPath := 'none';
  if HasScrollClip then
  begin
    if IsClippedOut then
      ClipPath := 'inset(50% 50% 50% 50%)'
    else
    begin
      ClipTop := Max(0, ClipRect.Top-ControlRect.Top);
      ClipRight := Max(0, ControlRect.Right-ClipRect.Right);
      ClipBottom := Max(0, ControlRect.Bottom-ClipRect.Bottom);
      ClipLeft := Max(0, ClipRect.Left-ControlRect.Left);
      if (ClipTop > 0) or (ClipRight > 0) or (ClipBottom > 0) or (ClipLeft > 0) then
        ClipPath := Format('inset(%dpx %dpx %dpx %dpx)',
          [ClipTop, ClipRight, ClipBottom, ClipLeft]);
    end;
  end;
  FontSize := Max(13, Abs(Control.Font.Height));
  NewStyle := Format('%d,%d,%d,%d,%d,%s',
    [Position.X, Position.Y, Control.Width, Control.Height, FontSize, ClipPath]);
  if not Initialized or (NewStyle <> SyncedStyle) then
  begin
    Style := Root.ReadJSPropertyObject('style', TJSObject) as IJSObject;
    Style.WriteJSPropertyUTF8String('left', IntToStr(Position.X)+'px');
    Style.WriteJSPropertyUTF8String('top', IntToStr(Position.Y)+'px');
    Style.WriteJSPropertyUTF8String('width', IntToStr(Control.Width)+'px');
    Style.WriteJSPropertyUTF8String('height', IntToStr(Control.Height)+'px');
    Style.WriteJSPropertyUTF8String('fontSize', IntToStr(FontSize)+'px');
    Style.WriteJSPropertyUTF8String('display', '');
    Style.WriteJSPropertyUTF8String('clipPath', ClipPath);
    SyncedStyle := NewStyle;
  end;
  if Control is TCustomEdit then
  begin
    Edit := TCustomEdit(Control);
    if not IsComposing then
    begin
      Value := Edit.Text;
      if not Initialized or (SyncedText <> Value) then Element.WriteJSPropertyUTF8String('value', Value);
      SyncedText := Value;
      Start := TCDWSCustomEdit.GetSelStart(Edit);
      Finish := Start + TCDWSCustomEdit.GetSelLength(Edit);
      Start := Length(UTF8Decode(UTF8Copy(Value, 1, Start)));
      Finish := Length(UTF8Decode(UTF8Copy(Value, 1, Finish)));
      if not Initialized or (SyncedStart <> Start) then Element.WriteJSPropertyLongInt('selectionStart', Start);
      if not Initialized or (SyncedFinish <> Finish) then Element.WriteJSPropertyLongInt('selectionEnd', Finish);
      SyncedStart := Start; SyncedFinish := Finish;
    end;
    if not Initialized or (SyncedReadOnly <> Edit.ReadOnly) then Element.WriteJSPropertyBoolean('readOnly', Edit.ReadOnly);
    SyncedReadOnly := Edit.ReadOnly;
    if Edit.MaxLength <> SyncedMaxLength then
    begin
      if Edit.MaxLength > 0 then Element.WriteJSPropertyLongInt('maxLength', Edit.MaxLength)
      else Element.InvokeJSNoResult('removeAttribute', ['maxlength']);
      SyncedMaxLength := Edit.MaxLength;
    end;
  end
  else if not Initialized or (SyncedText <> Control.Caption) then
  begin
    SyncedText := Control.Caption;
    CaptionElement.WriteJSPropertyUTF8String('textContent', RemoveAmpersands(SyncedText));
  end;
  if not Initialized or (SyncedEnabled <> Control.Enabled) then Element.WriteJSPropertyBoolean('disabled', not Control.Enabled);
  SyncedEnabled := Control.Enabled;
  { Keep DOM focus in step with LCL focus for every DOM-backed control.
    Buttons and checkboxes can become the LCL focus target through Tab just
    like edits; focusing their element also blurs the previously focused edit. }
  if Control.CanFocus and (GetCurrentForm.FocusedControl = Control) then
  begin
    if LastDOMFocus <> Control then
    begin
      LastDOMFocus := Control;
      Element.InvokeJSNoResult('focus', []);
    end
    else if CanvasElement <> nil then
    begin
      { Pointerdown focuses the canvas; restore an unchanged LCL focus only
        when that is still the browser's active element. }
      ActiveElement := Document.ReadJSPropertyObject('activeElement', TJSObject) as IJSObject;
      if (ActiveElement <> nil)
        and ActiveElement.InvokeJSBooleanResult('isSameNode', [CanvasElement]) then
        Element.InvokeJSNoResult('focus', []);
    end;
  end;
  Initialized := True;
end;

destructor TDOMControl.Destroy;
begin
  if LastDOMFocus = Control then LastDOMFocus := nil;
  { Detach the handler before its Pascal target can be freed. }
  BrowserControlUnbind(PtrUInt(Control));
  Element.WriteJSPropertyObject('onclick', nil);
  Element.WriteJSPropertyObject('oninput', nil);
  Element.WriteJSPropertyObject('onfocus', nil);
  Element.WriteJSPropertyObject('onselect', nil);
  Element.WriteJSPropertyObject('onkeydown', nil);
  Element.WriteJSPropertyObject('onkeyup', nil);
  Root.InvokeJSNoResult('remove', []);
  inherited Destroy;
end;

function CreateDOMControl(Control: TWinControl; const Params: TCreateParams;
  CheckBox: Boolean): TLCLHandle;
var
  Entry: TDOMControl;
  Style: IJSObject;
  ID: String;
begin
  if Control is TCustomEdit then Result := TCDWSCustomEdit.CreateHandle(Control, Params)
  else Result := TCDWSWinControl.CreateHandle(Control, Params);
  TCDWinControl(Result).BrowserDOM := True;
  if Entries = nil then
  begin
    Entries := TFPList.Create;
    Document := TJSObject.JOBCreateGlobal('document') as IJSObject;
    Surface := Document.InvokeJSObjectResult('getElementById', ['lcl-dom'], TJSObject) as IJSObject;
    CanvasElement := Document.InvokeJSObjectResult('getElementById', ['lcl'], TJSObject) as IJSObject;
  end;
  Entry := TDOMControl.Create;
  Entry.Control := Control;
  if Control is TCustomEdit then
  begin
    if Control is TCustomMemo then ID := 'textarea' else ID := 'input';
    Entry.Element := Document.InvokeJSObjectResult('createElement', [ID], TJSObject) as IJSObject;
    Entry.Element.InvokeJSNoResult('setAttribute', ['aria-label', 'Edit text']);
    Entry.Root := Entry.Element;
    Entry.CaptionElement := Entry.Element;
    TCDWSCustomEdit.InjectCDControl(Control, TCDWinControl(Result).CDControl);
    TCDWinControl(Result).CDControlInjected := True;
  end
  else if CheckBox then
  begin
    Entry.Root := Document.InvokeJSObjectResult('createElement', ['label'], TJSObject) as IJSObject;
    Entry.Element := Document.InvokeJSObjectResult('createElement', ['input'], TJSObject) as IJSObject;
    Entry.Element.WriteJSPropertyUTF8String('type', 'checkbox');
    Entry.Element.WriteJSPropertyBoolean('checked', TCheckBox(Control).Checked);
    Entry.Root.InvokeJSNoResult('appendChild', [Entry.Element]);
    Entry.CaptionElement := Document.InvokeJSObjectResult('createElement', ['span'], TJSObject) as IJSObject;
    Entry.Root.InvokeJSNoResult('appendChild', [Entry.CaptionElement]);
  end else
  begin
    Entry.Element := Document.InvokeJSObjectResult('createElement', ['button'], TJSObject) as IJSObject;
    Entry.Element.WriteJSPropertyUTF8String('type', 'button');
    Entry.Root := Entry.Element;
    Entry.CaptionElement := Entry.Element;
  end;
  ID := 'lcl-'+IntToStr(PtrUInt(Control));
  Entry.Element.WriteJSPropertyUTF8String('id', ID);
  Style := Entry.Root.ReadJSPropertyObject('style', TJSObject) as IJSObject;
  Style.WriteJSPropertyUTF8String('position', 'absolute');
  Entries.Add(Entry);
  Surface.InvokeJSNoResult('appendChild', [Entry.Root]);
  BrowserControlBind(PtrUInt(Control), PChar(ID), Length(ID));
  Entry.Sync;
end;

procedure DestroyDOMControl(Control: TWinControl);
var
  Entry: TDOMControl;
begin
  Entry := FindEntry(Control);
  if Entry = nil then Exit;
  Entries.Remove(Entry);
  Entry.Free;
  if Control is TCustomEdit then TCDWSCustomEdit.DestroyHandle(Control)
  else TCDWinControl(Control.Handle).Free;
end;

procedure SyncBrowserDOM;
var
  I: Integer;
begin
  RevealFocusedControl;
  if Entries = nil then Exit;
  for I := 0 to Entries.Count-1 do TDOMControl(Entries[I]).Sync;
  FocusCanvasForPaintedControl;
end;

class function TBrowserWSEdit.CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle;
begin
  Result := CreateDOMControl(Control, Params, False);
end;

class procedure TBrowserWSEdit.DestroyHandle(const Control: TWinControl);
begin
  DestroyDOMControl(Control);
end;

class procedure TBrowserWSEdit.SetText(const Control: TWinControl; const Text: String);
begin
  inherited SetText(Control, Text);
  SyncControl(Control);
end;

class procedure TBrowserWSEdit.SetSelStart(const Control: TCustomEdit; NewStart: Integer);
begin
  inherited SetSelStart(Control, NewStart);
  SyncControl(Control);
end;

class procedure TBrowserWSEdit.SetSelLength(const Control: TCustomEdit; NewLength: Integer);
begin
  inherited SetSelLength(Control, NewLength);
  SyncControl(Control);
end;

class function TBrowserWSMemo.CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle;
begin
  Result := CreateDOMControl(Control, Params, False);
end;

class procedure TBrowserWSMemo.DestroyHandle(const Control: TWinControl);
begin
  DestroyDOMControl(Control);
end;

{ TTextStrings keeps the exact text, including a final line break, and
  TCustomMemo copies it unchanged when the handle is created or destroyed. }
class function TBrowserWSMemo.GetStrings(const Control: TCustomMemo): TStrings;
begin
  Result := TTextStrings.Create;
end;

class function TBrowserWSButton.CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle;
begin
  Result := CreateDOMControl(Control, Params, False);
end;

class procedure TBrowserWSButton.DestroyHandle(const Control: TWinControl);
begin
  DestroyDOMControl(Control);
end;

class procedure TBrowserWSButton.SetBounds(const Control: TWinControl; const X, Y, Width, Height: Integer);
begin
  TCDWSWinControl.SetBounds(Control, X, Y, Width, Height);
  SyncControl(Control);
end;

class procedure TBrowserWSButton.ShowHide(const Control: TWinControl);
begin
  TCDWSWinControl.ShowHide(Control);
  SyncControl(Control);
end;

class procedure TBrowserWSButton.SetText(const Control: TWinControl; const Text: String);
begin
  { Descendants such as TBitBtn have no DOM element; keep their drawn caption. }
  if not SetDOMCaption(Control, Text) then TCDWSButton.SetText(Control, Text);
end;

class function TBrowserWSCheckBox.CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle;
begin
  Result := CreateDOMControl(Control, Params, True);
end;

class procedure TBrowserWSCheckBox.DestroyHandle(const Control: TWinControl);
begin
  DestroyDOMControl(Control);
end;

class procedure TBrowserWSCheckBox.SetBounds(const Control: TWinControl; const X, Y, Width, Height: Integer);
begin
  TCDWSWinControl.SetBounds(Control, X, Y, Width, Height);
  SyncControl(Control);
end;

class procedure TBrowserWSCheckBox.ShowHide(const Control: TWinControl);
begin
  TCDWSWinControl.ShowHide(Control);
  SyncControl(Control);
end;

class procedure TBrowserWSCheckBox.SetText(const Control: TWinControl; const Text: String);
begin
  SetDOMCaption(Control, Text);
end;

class function TBrowserWSCheckBox.RetrieveState(const Control: TCustomCheckBox): TCheckBoxState;
var
  Entry: TDOMControl;
begin
  Entry := FindEntry(Control);
  if (Entry <> nil) and Entry.Element.ReadJSPropertyBoolean('checked') then Result := cbChecked
  else Result := cbUnchecked;
end;

class procedure TBrowserWSCheckBox.SetState(const Control: TCustomCheckBox; const State: TCheckBoxState);
var
  Entry: TDOMControl;
begin
  Entry := FindEntry(Control);
  if Entry = nil then Exit;
  Entry.Element.WriteJSPropertyBoolean('checked', State = cbChecked);
  Entry.Element.WriteJSPropertyBoolean('indeterminate', State = cbGrayed);
end;
end.
