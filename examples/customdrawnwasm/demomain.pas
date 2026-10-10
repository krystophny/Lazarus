{ SPDX-License-Identifier: MIT
  Counter and drawing controls for the LCL browser example. }
unit DemoMain;
{$mode objfpc}{$H+}
interface
uses Classes, SysUtils, Forms, Controls, StdCtrls, ExtCtrls, Graphics, Types,
  Dialogs;
type
  TDemoForm = class(TForm)
    CounterButton: TButton;
    ResetButton: TButton;
    SaveFileButton: TButton;
    SaveAgainButton: TButton;
    FilledCheckBox: TCheckBox;
    AtomicRenameCheckBox: TCheckBox;
    FailRenameCheckBox: TCheckBox;
    Drawing: TPaintBox;
    HintLabel: TLabel;
    procedure CounterButtonClick(Sender: TObject);
    procedure ResetButtonClick(Sender: TObject);
    procedure SaveFileButtonClick(Sender: TObject);
    procedure SaveAgainButtonClick(Sender: TObject);
    procedure FilledCheckBoxChange(Sender: TObject);
    procedure DrawingPaint(Sender: TObject);
    procedure DrawingMouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
  private
    FCount, FMarkerX, FMarkerY: Integer;
    FHasMarker: Boolean;
    FLastSaveFileName: string;
    FLastSaveAtomic: Boolean;
    function WriteBrowserFile(const FileName: string; Atomic,
      FailRename: Boolean): Boolean;
  end;
var
  DemoForm: TDemoForm;
implementation
{$R *.lfm}
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
end.
