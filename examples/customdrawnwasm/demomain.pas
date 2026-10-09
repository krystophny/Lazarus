{ SPDX-License-Identifier: MIT
  Counter and drawing controls for the LCL browser example. }
unit DemoMain;
{$mode objfpc}{$H+}
interface
uses Classes, SysUtils, Forms, Controls, StdCtrls, ExtCtrls, Graphics, Types;
type
  TDemoForm = class(TForm)
    CounterButton: TButton;
    ResetButton: TButton;
    FilledCheckBox: TCheckBox;
    Drawing: TPaintBox;
    HintLabel: TLabel;
    procedure CounterButtonClick(Sender: TObject);
    procedure ResetButtonClick(Sender: TObject);
    procedure FilledCheckBoxChange(Sender: TObject);
    procedure DrawingPaint(Sender: TObject);
    procedure DrawingMouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
  private
    FCount, FMarkerX, FMarkerY: Integer;
    FHasMarker: Boolean;
  end;
var DemoForm: TDemoForm;
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
