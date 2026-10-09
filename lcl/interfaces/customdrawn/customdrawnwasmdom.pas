{ LCL browser controls realized through Free Pascal JOB.
  See COPYING.modifiedLGPL.txt for license and linking exception. }
unit CustomDrawnWasmDOM;
{$mode objfpc}{$H+}
interface
uses Classes, SysUtils, Types, Math, Controls, StdCtrls, ExtCtrls, Forms,
  LCLType, WSStdCtrls, CustomDrawnWSStdCtrls, CustomDrawnProc, LazUTF8, Job.JS;
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
    class procedure FreeStrings(var Strings: TStrings); override;
    class procedure AppendText(const Control: TCustomMemo; const Text: String); override;
  end;
  TBrowserWSButton = class(TWSButton)
  published
    class function CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle; override;
    class procedure DestroyHandle(const Control: TWinControl); override;
    class procedure SetBounds(const Control: TWinControl; const X, Y, Width, Height: Integer); override;
    class procedure ShowHide(const Control: TWinControl); override;
    class procedure SetText(const Control: TWinControl; const Text: String); override;
    class function GetText(const Control: TWinControl; var Text: String): Boolean; override;
  end;
  TBrowserWSCheckBox = class(TWSCustomCheckBox)
  published
    class function CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle; override;
    class procedure DestroyHandle(const Control: TWinControl); override;
    class procedure SetBounds(const Control: TWinControl; const X, Y, Width, Height: Integer); override;
    class procedure ShowHide(const Control: TWinControl); override;
    class procedure SetText(const Control: TWinControl; const Text: String); override;
    class function GetText(const Control: TWinControl; var Text: String): Boolean; override;
    class function RetrieveState(const Control: TCustomCheckBox): TCheckBoxState; override;
    class procedure SetState(const Control: TCustomCheckBox; const State: TCheckBoxState); override;
  end;
procedure SyncBrowserDOM;
procedure DispatchBrowserDOMControl(Handle: PtrUInt);
procedure FocusBrowserDOMControl(Handle: PtrUInt);
implementation
uses CustomDrawnWSControls, CustomDrawnPrivate, LCLMessageGlue, LCLIntf, CustomDrawnInt;
type
  TDOMControl = class
    Control: TWinControl;
    Element, CaptionElement, Root: IJSObject;
    SyncedStyle, SyncedText, SyncedLabel: String;
    SyncedStart, SyncedFinish: Integer;
    SyncedEnabled, SyncedReadOnly, Initialized, Updating, Hidden: Boolean;
    SyncedMaxLength: Integer;
    procedure Activate;
    procedure Sync;
    destructor Destroy; override;
  end;
procedure BrowserControlBind(Handle: LongInt; ID: PChar; Count: LongInt); external 'lcl' name 'control_bind';

var
  Entries: TFPList;
  Document, Surface: IJSObject;
  LastDOMFocus: TWinControl;

function FindEntry(Control: TWinControl): TDOMControl;
var
  I: Integer;
begin
  Result := nil;
  if Entries = nil then Exit;
  for I := 0 to Entries.Count-1 do
    if TDOMControl(Entries[I]).Control = Control then Exit(TDOMControl(Entries[I]));
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
begin
  if not Control.Enabled then Exit;
  { LM_CHANGED retrieves the browser state through RetrieveState. }
  if Control is TCustomEdit then
  begin
    Value := Element.ReadJSPropertyUTF8String('value');
    Start := Element.ReadJSPropertyLongInt('selectionStart');
    Finish := Element.ReadJSPropertyLongInt('selectionEnd');
    Updating := True;
    try
      SyncedText := Value;
      SyncedStart := Start; SyncedFinish := Finish;
      if TCustomEdit(Control).Text <> Value then TCustomEdit(Control).Text := Value;
      Prefix := UTF8Encode(Copy(UTF8Decode(Value), 1, Start));
      TCDWSCustomEdit.SetSelLength(TCustomEdit(Control), 0);
      TCDWSCustomEdit.SetSelStart(TCustomEdit(Control), UTF8Length(Prefix));
      Prefix := UTF8Encode(Copy(UTF8Decode(Value), Start+1, Finish-Start));
      TCDWSCustomEdit.SetSelLength(TCustomEdit(Control), UTF8Length(Prefix));
    finally
      Updating := False;
    end;
  end
  else if Control is TCustomCheckBox then LCLSendChangedMsg(Control)
  else LCLSendClickedMsg(Control);
  LCLIntf.InvalidateRect(Control.Handle, nil, False);
end;

procedure TDOMControl.Sync;
var
  Style: IJSObject;
  Position: TPoint;
  Value, NewStyle, Display, LabelText: String;
  Start, Finish, FontSize: Integer;
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
  Position := FindControlPositionRelativeToTheForm(Control);
  FontSize := Max(13, Abs(Control.Font.Height));
  Display := '';
  NewStyle := Format('%d,%d,%d,%d,%d,%s', [Position.X, Position.Y, Control.Width, Control.Height, FontSize, Display]);
  if not Initialized or (NewStyle <> SyncedStyle) then
  begin
    Style := Root.ReadJSPropertyObject('style', TJSObject) as IJSObject;
    Style.WriteJSPropertyUTF8String('left', IntToStr(Position.X)+'px');
    Style.WriteJSPropertyUTF8String('top', IntToStr(Position.Y)+'px');
    Style.WriteJSPropertyUTF8String('width', IntToStr(Control.Width)+'px');
    Style.WriteJSPropertyUTF8String('height', IntToStr(Control.Height)+'px');
    Style.WriteJSPropertyUTF8String('fontSize', IntToStr(FontSize)+'px');
    Style.WriteJSPropertyUTF8String('display', Display);
    SyncedStyle := NewStyle;
  end;
  if Control is TCustomEdit then
  begin
    Edit := TCustomEdit(Control);
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
    if not Initialized or (SyncedReadOnly <> Edit.ReadOnly) then Element.WriteJSPropertyBoolean('readOnly', Edit.ReadOnly);
    SyncedReadOnly := Edit.ReadOnly;
    if Edit.MaxLength <> SyncedMaxLength then
    begin
      if Edit.MaxLength > 0 then Element.WriteJSPropertyLongInt('maxLength', Edit.MaxLength)
      else Element.InvokeJSNoResult('removeAttribute', ['maxlength']);
      SyncedMaxLength := Edit.MaxLength;
    end;
    if Control.CanFocus and (GetCurrentForm.FocusedControl = Control) and (LastDOMFocus <> Control) then
    begin
      LastDOMFocus := Control;
      Element.InvokeJSNoResult('focus', []);
    end;
  end
  else if not Initialized or (SyncedText <> Control.Caption) then
  begin
    SyncedText := Control.Caption;
    CaptionElement.WriteJSPropertyUTF8String('textContent', SyncedText);
  end;
  if not Initialized or (SyncedEnabled <> Control.Enabled) then Element.WriteJSPropertyBoolean('disabled', not Control.Enabled);
  SyncedEnabled := Control.Enabled;
  Initialized := True;
end;

destructor TDOMControl.Destroy;
begin
  if LastDOMFocus = Control then LastDOMFocus := nil;
  { Detach the handler before its Pascal target can be freed. }
  Element.WriteJSPropertyObject('onclick', nil);
  Element.WriteJSPropertyObject('oninput', nil);
  Element.WriteJSPropertyObject('onfocus', nil);
  Element.WriteJSPropertyObject('onselect', nil);
  Element.WriteJSPropertyObject('onkeydown', nil);
  Element.WriteJSPropertyObject('onkeyup', nil);
  Element.WriteJSPropertyObject('oncompositionend', nil);
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
  if Entries = nil then Exit;
  for I := 0 to Entries.Count-1 do TDOMControl(Entries[I]).Sync;
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
  if FindEntry(Control) <> nil then
    FindEntry(Control).Sync;
end;

class procedure TBrowserWSEdit.SetSelStart(const Control: TCustomEdit; NewStart: Integer);
begin
  inherited SetSelStart(Control, NewStart);
  if FindEntry(Control) <> nil then
    FindEntry(Control).Sync;
end;

class procedure TBrowserWSEdit.SetSelLength(const Control: TCustomEdit; NewLength: Integer);
begin
  inherited SetSelLength(Control, NewLength);
  if FindEntry(Control) <> nil then
    FindEntry(Control).Sync;
end;

class function TBrowserWSMemo.CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle;
begin
  Result := CreateDOMControl(Control, Params, False);
end;

class procedure TBrowserWSMemo.DestroyHandle(const Control: TWinControl);
begin
  DestroyDOMControl(Control);
end;

class function TBrowserWSMemo.GetStrings(const Control: TCustomMemo): TStrings;
begin
  Result := TCDIntfEdit(TCDWinControl(Control.Handle).CDControl).Lines;
end;

class procedure TBrowserWSMemo.FreeStrings(var Strings: TStrings);
begin
  Strings := nil;
end;

class procedure TBrowserWSMemo.AppendText(const Control: TCustomMemo; const Text: String);
begin
  TBrowserWSEdit.SetText(Control, Control.Text + Text);
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
  if FindEntry(Control) <> nil then
    FindEntry(Control).Sync;
end;

class procedure TBrowserWSButton.ShowHide(const Control: TWinControl);
begin
  TCDWSWinControl.ShowHide(Control);
  if FindEntry(Control) <> nil then
    FindEntry(Control).Sync;
end;

class procedure TBrowserWSButton.SetText(const Control: TWinControl; const Text: String);
begin
  if FindEntry(Control) <> nil then
    FindEntry(Control).CaptionElement.WriteJSPropertyUTF8String('textContent', Text);
end;

class function TBrowserWSButton.GetText(const Control: TWinControl; var Text: String): Boolean;
begin
  Result := FindEntry(Control) <> nil;
  if Result then
    Text := FindEntry(Control).CaptionElement.ReadJSPropertyUTF8String('textContent');
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
  if FindEntry(Control) <> nil then
    FindEntry(Control).Sync;
end;

class procedure TBrowserWSCheckBox.ShowHide(const Control: TWinControl);
begin
  TCDWSWinControl.ShowHide(Control);
  if FindEntry(Control) <> nil then
    FindEntry(Control).Sync;
end;

class procedure TBrowserWSCheckBox.SetText(const Control: TWinControl; const Text: String);
begin
  if FindEntry(Control) <> nil then
    FindEntry(Control).CaptionElement.WriteJSPropertyUTF8String('textContent', Text);
end;

class function TBrowserWSCheckBox.GetText(const Control: TWinControl; var Text: String): Boolean;
begin
  Result := FindEntry(Control) <> nil;
  if Result then
    Text := FindEntry(Control).CaptionElement.ReadJSPropertyUTF8String('textContent');
end;

class function TBrowserWSCheckBox.RetrieveState(const Control: TCustomCheckBox): TCheckBoxState;
begin
  if (FindEntry(Control) <> nil) and FindEntry(Control).Element.ReadJSPropertyBoolean('checked') then Result := cbChecked
  else Result := cbUnchecked;
end;

class procedure TBrowserWSCheckBox.SetState(const Control: TCustomCheckBox; const State: TCheckBoxState);
begin
  if FindEntry(Control) = nil then Exit;
  FindEntry(Control).Element.WriteJSPropertyBoolean('checked', State = cbChecked);
  FindEntry(Control).Element.WriteJSPropertyBoolean('indeterminate', State = cbGrayed);
end;
end.
