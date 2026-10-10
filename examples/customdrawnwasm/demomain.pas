{ SPDX-License-Identifier: MIT
  Counter and drawing controls for the LCL browser example. }
unit DemoMain;
{$mode objfpc}{$H+}
interface
uses Classes, SysUtils, Forms, Controls, StdCtrls, ExtCtrls, Graphics, Types,
  Dialogs, LMessages, LCLType;
type
  TCompositionMemo = class(TMemo)
  private
    FIsComposing: Boolean;
    FOnCompositionChange: TNotifyEvent;
  protected
    procedure IMComposition(var Msg: TLMessage); message LM_IM_COMPOSITION;
  published
    property OnCompositionChange: TNotifyEvent read FOnCompositionChange
      write FOnCompositionChange;
  public
    property IsComposing: Boolean read FIsComposing;
  end;

  TDemoForm = class(TForm)
    CounterButton: TButton;
    ResetButton: TButton;
    SaveFileButton: TButton;
    SaveAgainButton: TButton;
    CompositionSaveButton: TButton;
    FilledCheckBox: TCheckBox;
    AtomicRenameCheckBox: TCheckBox;
    FailRenameCheckBox: TCheckBox;
    Drawing: TPaintBox;
    HintLabel: TLabel;
    CompositionMemo: TCompositionMemo;
    CompositionStatus: TEdit;
    CompositionValue: TEdit;
    CompositionSaved: TEdit;
    procedure CounterButtonClick(Sender: TObject);
    procedure ResetButtonClick(Sender: TObject);
    procedure SaveFileButtonClick(Sender: TObject);
    procedure SaveAgainButtonClick(Sender: TObject);
    procedure CompositionSaveButtonClick(Sender: TObject);
    procedure FilledCheckBoxChange(Sender: TObject);
    procedure DrawingPaint(Sender: TObject);
    procedure DrawingMouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure CompositionMemoChange(Sender: TObject);
    procedure CompositionMemoCompositionChange(Sender: TObject);
    procedure CompositionMemoKeyDown(Sender: TObject; var Key: Word;
      Shift: TShiftState);
  private
    FCount, FMarkerX, FMarkerY: Integer;
    FHasMarker: Boolean;
    FCompositionSavePending: Boolean;
    FLastSaveFileName: string;
    FLastSaveAtomic: Boolean;
    function WriteBrowserFile(const FileName: string; Atomic,
      FailRename: Boolean): Boolean;
  end;
var
  DemoForm: TDemoForm;
implementation
{$R *.lfm}

procedure TCompositionMemo.IMComposition(var Msg: TLMessage);
var
  NewValue: Boolean;
begin
  NewValue := FIsComposing;
  if (Msg.WParam and GTK_IM_FLAG_START) <> 0 then NewValue := True;
  if (Msg.WParam and GTK_IM_FLAG_END) <> 0 then NewValue := False;
  if NewValue = FIsComposing then Exit;
  FIsComposing := NewValue;
  if Assigned(FOnCompositionChange) then FOnCompositionChange(Self);
end;

procedure TDemoForm.CounterButtonClick(Sender: TObject);
begin
  Inc(FCount);
  CounterButton.Caption := 'Count: '+IntToStr(FCount);
end;
procedure TDemoForm.ResetButtonClick(Sender: TObject);
begin
  FCount := 0;
  FHasMarker := False;
  CounterButton.Caption := 'Count: 0';
  Drawing.Invalidate;
end;
procedure TDemoForm.SaveFileButtonClick(Sender: TObject);
var
  Dialog: TSaveDialog;
  FileName: string;
  Atomic: Boolean;
begin
  Dialog := TSaveDialog.Create(Self);
  try
    Dialog.Title := 'Save browser file';
    Dialog.Filter := 'Text files (*.txt)|*.txt';
    Dialog.DefaultExt := 'txt';
    Dialog.FileName := 'download.txt';
    if not Dialog.Execute then Exit;
    FileName := Dialog.FileName;
    Atomic := AtomicRenameCheckBox.Checked;
    if WriteBrowserFile(FileName, Atomic, FailRenameCheckBox.Checked) then
    begin
      FLastSaveFileName := FileName;
      FLastSaveAtomic := Atomic;
      SaveAgainButton.Enabled := True;
    end;
  finally
    Dialog.Free;
  end;
end;

procedure TDemoForm.SaveAgainButtonClick(Sender: TObject);
begin
  if FLastSaveFileName <> '' then
    WriteBrowserFile(FLastSaveFileName, FLastSaveAtomic, False);
end;

function TDemoForm.WriteBrowserFile(const FileName: string; Atomic,
  FailRename: Boolean): Boolean;
var
  StagedFileName: string;
  Payload: RawByteString;
  Stream: TFileStream;
begin
  Result := False;
  if FailRename then
  begin
    StagedFileName := FileName + '.stage';
    if RenameFile(StagedFileName, FileName) then
      raise Exception.Create('Unexpectedly renamed a missing staged file');
    Exit;
  end;
  Payload := 'WASI browser save payload'#10;
  if Atomic then
  begin
    StagedFileName := FileName + '.stage';
    Stream := TFileStream.Create(StagedFileName, fmCreate);
    try
      Stream.WriteBuffer(Payload[1], Length(Payload));
    finally
      Stream.Free;
    end;
    if not RenameFile(StagedFileName, FileName) then
      raise Exception.Create('Could not commit staged browser file');
  end
  else
  begin
    Stream := TFileStream.Create(FileName, fmCreate);
    try
      Stream.WriteBuffer(Payload[1], Length(Payload));
    finally
      Stream.Free;
    end;
  end;
  Result := True;
end;
procedure TDemoForm.FilledCheckBoxChange(Sender: TObject);
begin
  Drawing.Invalidate;
end;
procedure TDemoForm.DrawingPaint(Sender: TObject);
begin
  Drawing.Canvas.Brush.Color := clWhite;
  Drawing.Canvas.FillRect(Drawing.ClientRect);
  Drawing.Canvas.Pen.Color := $B45B24;
  Drawing.Canvas.Pen.Width := 3;
  Drawing.Canvas.Brush.Color := $EACFAF;
  if FilledCheckBox.Checked then Drawing.Canvas.Brush.Style := bsSolid
  else Drawing.Canvas.Brush.Style := bsClear;
  Drawing.Canvas.Ellipse(44, 44, 224, 194);
  Drawing.Canvas.Brush.Style := bsSolid;
  if FHasMarker then
  begin
    Drawing.Canvas.Pen.Color := $387A26;
    Drawing.Canvas.Brush.Color := $387A26;
    Drawing.Canvas.Ellipse(FMarkerX-6, FMarkerY-6, FMarkerX+7, FMarkerY+7);
  end;
end;
procedure TDemoForm.DrawingMouseDown(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
begin
  if Button <> mbLeft then Exit;
  FMarkerX := X;
  FMarkerY := Y;
  FHasMarker := True;
  Drawing.Invalidate;
end;

procedure TDemoForm.CompositionMemoChange(Sender: TObject);
begin
  CompositionValue.Text := 'Value: ' + CompositionMemo.Text;
end;

procedure TDemoForm.CompositionMemoCompositionChange(Sender: TObject);
begin
  if CompositionMemo.IsComposing then
    CompositionStatus.Text := 'Composition: active'
  else begin
    CompositionStatus.Text := 'Composition: idle';
    if FCompositionSavePending then
    begin
      CompositionSaved.Text := 'Saved: ' + CompositionMemo.Text;
      FCompositionSavePending := False;
    end;
  end;
end;

procedure TDemoForm.CompositionSaveButtonClick(Sender: TObject);
begin
  if CompositionMemo.IsComposing then
  begin
    FCompositionSavePending := True;
    CompositionSaved.Text := 'Save pending';
  end
  else
    CompositionSaved.Text := 'Saved: ' + CompositionMemo.Text;
end;

procedure TDemoForm.CompositionMemoKeyDown(Sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
  if (Key = VK_RETURN) and (ssCtrl in Shift) then
  begin
    CompositionStatus.Text := 'Ctrl+Enter routed';
    Key := 0;
  end;
end;

initialization
  RegisterClass(TCompositionMemo);
end.
