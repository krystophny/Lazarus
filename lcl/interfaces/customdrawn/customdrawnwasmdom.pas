{ LCL browser controls realized through Free Pascal JOB.
  See COPYING.modifiedLGPL.txt for license and linking exception. }
unit CustomDrawnWasmDOM;
{$mode objfpc}{$H+}
interface
uses Classes, SysUtils, Types, Math, Controls, StdCtrls, Forms,
  LCLType, WSStdCtrls, CustomDrawnProc, Job.JS;
type
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
implementation
uses CustomDrawnWSControls, LCLMessageGlue, LCLIntf, CustomDrawnInt;
type
  TDOMControl = class
    Control: TWinControl;
    Element, CaptionElement, Root: IJSObject;
    procedure Activate;
    procedure Sync;
    destructor Destroy; override;
  end;
  TDOMHandler = procedure of object;
var
  Entries: TFPList;
  Document, Surface: IJSObject;

function DispatchDOM(const Method: TMethod; var H: TJOBCallbackHelper): PByte;
var Event: IJSObject;
begin
  if H.Count > 0 then Event := H.GetObject(TJSObject) as IJSObject;
  TDOMHandler(Method)();
  Result := H.AllocUndefined;
end;

function FindEntry(Control: TWinControl): TDOMControl;
var I: Integer;
begin
  Result := nil;
  if Entries = nil then Exit;
  for I := 0 to Entries.Count-1 do
    if TDOMControl(Entries[I]).Control = Control then Exit(TDOMControl(Entries[I]));
end;

procedure TDOMControl.Activate;
begin
  if not Control.Enabled then Exit;
  { LM_CHANGED retrieves the browser state through RetrieveState. }
  if Control is TCustomCheckBox then LCLSendChangedMsg(Control)
  else LCLSendClickedMsg(Control);
  LCLIntf.InvalidateRect(Control.Handle, nil, False);
  LCLWasmIdle;
end;

procedure TDOMControl.Sync;
var Style: IJSObject; Position: TPoint;
begin
  Position := FindControlPositionRelativeToTheForm(Control);
  Style := Root.ReadJSPropertyObject('style', TJSObject) as IJSObject;
  Style.WriteJSPropertyUTF8String('left', IntToStr(Position.X)+'px');
  Style.WriteJSPropertyUTF8String('top', IntToStr(Position.Y)+'px');
  Style.WriteJSPropertyUTF8String('width', IntToStr(Control.Width)+'px');
  Style.WriteJSPropertyUTF8String('height', IntToStr(Control.Height)+'px');
  Style.WriteJSPropertyUTF8String('fontSize', IntToStr(Max(13, Abs(Control.Font.Height)))+'px');
  if Control.IsVisible then Style.WriteJSPropertyUTF8String('display', '')
  else Style.WriteJSPropertyUTF8String('display', 'none');
  CaptionElement.WriteJSPropertyUTF8String('textContent', Control.Caption);
  Element.WriteJSPropertyBoolean('disabled', not Control.Enabled);
end;

destructor TDOMControl.Destroy;
begin
  { Detach the handler before its Pascal target can be freed. }
  Element.InvokeJSNoResult('onclick', [nil], jiSet);
  Root.InvokeJSNoResult('remove', []);
  inherited Destroy;
end;

function CreateDOMControl(Control: TWinControl; const Params: TCreateParams;
  CheckBox: Boolean): TLCLHandle;
var Entry: TDOMControl; Callback: TJOB_Method; Style: IJSObject;
begin
  Result := TCDWSWinControl.CreateHandle(Control, Params);
  TCDWinControl(Result).BrowserDOM := True;
  if Entries = nil then
  begin
    Entries := TFPList.Create;
    Document := TJSObject.JOBCreateGlobal('document') as IJSObject;
    Surface := Document.InvokeJSObjectResult('getElementById', ['lcl-dom'], TJSObject) as IJSObject;
  end;
  Entry := TDOMControl.Create;
  Entry.Control := Control;
  if CheckBox then
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
  Entry.Element.WriteJSPropertyUTF8String('id', 'lcl-'+Control.Name);
  Style := Entry.Root.ReadJSPropertyObject('style', TJSObject) as IJSObject;
  Style.WriteJSPropertyUTF8String('position', 'absolute');
  Callback := TJOB_Method.Create(TMethod(@Entry.Activate), @DispatchDOM);
  try Entry.Element.InvokeJSNoResult('onclick', [Callback], jiSet);
  finally Callback.Free; end;
  Entries.Add(Entry);
  Surface.InvokeJSNoResult('appendChild', [Entry.Root]);
  Entry.Sync;
end;

procedure DestroyDOMControl(Control: TWinControl);
var Entry: TDOMControl; List: TFPList; ParentHandle: TCDWinControl;
begin
  Entry := FindEntry(Control);
  if Entry = nil then Exit;
  Entries.Remove(Entry);
  Entry.Free;
  if Control.Parent is TCustomForm then
  begin
    List := GetCDWinControlList(TCustomForm(Control.Parent));
    if List <> nil then List.Remove(Pointer(Control.Handle));
  end else if Control.Parent <> nil then
  begin
    ParentHandle := TCDWinControl(Control.Parent.Handle);
    ParentHandle.Region.Childs.Remove(TCDWinControl(Control.Handle).Region);
    if ParentHandle.Children <> nil then ParentHandle.Children.Remove(Pointer(Control.Handle));
  end;
  TCDWinControl(Control.Handle).Free;
end;

procedure SyncBrowserDOM;
var I: Integer;
begin
  if Entries = nil then Exit;
  for I := 0 to Entries.Count-1 do TDOMControl(Entries[I]).Sync;
end;

class function TBrowserWSButton.CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle;
begin Result := CreateDOMControl(Control, Params, False); end;
class procedure TBrowserWSButton.DestroyHandle(const Control: TWinControl);
begin DestroyDOMControl(Control); end;
class procedure TBrowserWSButton.SetBounds(const Control: TWinControl; const X, Y, Width, Height: Integer);
begin TCDWSWinControl.SetBounds(Control, X, Y, Width, Height); FindEntry(Control).Sync; end;
class procedure TBrowserWSButton.ShowHide(const Control: TWinControl);
begin TCDWSWinControl.ShowHide(Control); FindEntry(Control).Sync; end;
class procedure TBrowserWSButton.SetText(const Control: TWinControl; const Text: String);
begin FindEntry(Control).CaptionElement.WriteJSPropertyUTF8String('textContent', Text); end;
class function TBrowserWSButton.GetText(const Control: TWinControl; var Text: String): Boolean;
begin Text := FindEntry(Control).CaptionElement.ReadJSPropertyUTF8String('textContent'); Result := True; end;

class function TBrowserWSCheckBox.CreateHandle(const Control: TWinControl; const Params: TCreateParams): TLCLHandle;
begin Result := CreateDOMControl(Control, Params, True); end;
class procedure TBrowserWSCheckBox.DestroyHandle(const Control: TWinControl);
begin DestroyDOMControl(Control); end;
class procedure TBrowserWSCheckBox.SetBounds(const Control: TWinControl; const X, Y, Width, Height: Integer);
begin TCDWSWinControl.SetBounds(Control, X, Y, Width, Height); FindEntry(Control).Sync; end;
class procedure TBrowserWSCheckBox.ShowHide(const Control: TWinControl);
begin TCDWSWinControl.ShowHide(Control); FindEntry(Control).Sync; end;
class procedure TBrowserWSCheckBox.SetText(const Control: TWinControl; const Text: String);
begin FindEntry(Control).CaptionElement.WriteJSPropertyUTF8String('textContent', Text); end;
class function TBrowserWSCheckBox.GetText(const Control: TWinControl; var Text: String): Boolean;
begin Text := FindEntry(Control).CaptionElement.ReadJSPropertyUTF8String('textContent'); Result := True; end;

class function TBrowserWSCheckBox.RetrieveState(const Control: TCustomCheckBox): TCheckBoxState;
begin
  if FindEntry(Control).Element.ReadJSPropertyBoolean('checked') then Result := cbChecked
  else Result := cbUnchecked;
end;
class procedure TBrowserWSCheckBox.SetState(const Control: TCustomCheckBox; const State: TCheckBoxState);
begin
  FindEntry(Control).Element.WriteJSPropertyBoolean('checked', State = cbChecked);
  FindEntry(Control).Element.WriteJSPropertyBoolean('indeterminate', State = cbGrayed);
end;
end.
