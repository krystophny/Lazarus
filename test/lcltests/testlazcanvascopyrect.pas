{ Regression test for clipping in TLazCanvas.CanvasCopyRect.
  See COPYING.modifiedLGPL.txt for the license and linking exception. }

unit TestLazCanvasCopyRect;

{$mode objfpc}{$H+}

interface

uses
  Types, fpcunit, TestGlobals, FPImage, FPCanvas, GraphType, IntfGraphics, LazCanvas, LazRegions;

type
  TCountingImage = class(TLazIntfImage)
  public
    Reads: Integer;
    function GetInternalColor(X, Y: Integer): TFPColor; override;
  end;

  TCountingCanvas = class(TLazCanvas)
  public
    Writes: Integer;
    procedure SetColor(X,Y: Integer; const Value: TFPColor); override;
  end;

  TTestLazCanvasCopyRect = class(TTestCase)
  private
    FDestImage: TLazIntfImage;
    FSourceImage: TCountingImage;
    FDestCanvas: TCountingCanvas;
    FSourceCanvas: TLazCanvas;
  protected
    procedure SetUp; override;
    procedure TearDown; override;
  published
    procedure Clipping;
    procedure ClippedCopyWork;
    procedure ClippedFillWork;
    procedure WindowOrigin;
    procedure NegativeSource;
    procedure ComplexClip;
    procedure ScanlineStride;
    procedure ClippedAxisLine;
    procedure AlphaFill;
    procedure OpaqueARGBBitmap;
  end;

implementation

function TCountingImage.GetInternalColor(X, Y: Integer): TFPColor;
begin
  Inc(Reads);
  Result := inherited GetInternalColor(X, Y);
end;

procedure TCountingCanvas.SetColor(X,Y: Integer; const Value: TFPColor);
begin
  Inc(Writes);
  inherited SetColor(X,Y,Value);
end;

procedure TTestLazCanvasCopyRect.SetUp;
var
  Desc: TRawImageDescription;
begin
  Desc.Init_BPP32_A8R8G8B8_BIO_TTB(8, 8);
  FDestImage := TLazIntfImage.Create(0, 0);
  FDestImage.DataDescription := Desc;
  FDestImage.FillPixels(colBlack);
  FSourceImage := TCountingImage.Create(0, 0);
  FSourceImage.DataDescription := Desc;
  FSourceImage.FillPixels(colRed);
  FDestCanvas := TCountingCanvas.Create(FDestImage);
  FSourceCanvas := TLazCanvas.Create(FSourceImage);
  FDestCanvas.ImageFormat := clfBGRA32;
  FSourceCanvas.ImageFormat := clfBGRA32;
end;

procedure TTestLazCanvasCopyRect.TearDown;
begin
  FSourceCanvas.Free;
  FDestCanvas.Free;
  FSourceImage.Free;
  FDestImage.Free;
end;

procedure TTestLazCanvasCopyRect.Clipping;
begin
  FDestCanvas.ClipRect := Rect(4, 4, 7, 7);
  FDestCanvas.Clipping := True;
  FDestCanvas.CanvasCopyRect(FSourceCanvas, 0, 0, 0, 0, 8, 8);
  AssertEquals('copied pixel inside clip', $FFFF, FDestImage.Colors[5, 5].Red);
  AssertEquals('copied pixel outside clip', 0, FDestImage.Colors[1, 1].Red);
end;

procedure TTestLazCanvasCopyRect.ClippedCopyWork;
begin
  { Force conversion so the pixel reader independently measures the work. }
  FSourceCanvas.ImageFormat := clfARGB32;
  FDestCanvas.ClipRegion := TLazRegion.Create;
  TLazRegion(FDestCanvas.ClipRegion).SetAsSimpleRectRegion(Rect(4, 4, 6, 6));
  FDestCanvas.Clipping := True;
  FSourceImage.Reads := 0;
  FDestCanvas.CanvasCopyRect(FSourceCanvas, 0, 0, 0, 0, 8, 8);
  AssertEquals('read only the nine damaged pixels', 9, FSourceImage.Reads);
  AssertEquals('inside clip', $FFFF, FDestImage.Colors[6, 6].Red);
  AssertEquals('outside clip', 0, FDestImage.Colors[7, 6].Red);
end;

procedure TTestLazCanvasCopyRect.ClippedFillWork;
var X,Y: Integer;
begin
  FDestCanvas.ClipRegion := TLazRegion.Create;
  TLazRegion(FDestCanvas.ClipRegion).SetAsSimpleRectRegion(Rect(2,3,4,5));
  FDestCanvas.Clipping := True;
  FDestCanvas.WindowOrg := Point(-2,1);
  FDestCanvas.Brush.FPColor := colRed;
  FDestCanvas.Writes := 0;
  FDestCanvas.FillRect(0,0,12,12);
  AssertTrue('write at most nine damaged pixels', FDestCanvas.Writes <= 9);
  for Y := 0 to 7 do
    for X := 0 to 7 do
      if (X >= 2) and (X <= 4) and (Y >= 3) and (Y <= 5) then
        AssertEquals('inside fill clip', $FFFF, FDestImage.Colors[X,Y].Red)
      else
        AssertEquals('outside fill clip', 0, FDestImage.Colors[X,Y].Red);
end;

procedure TTestLazCanvasCopyRect.WindowOrigin;
var X, Y: Integer;
begin
  FDestCanvas.ClipRegion := TLazRegion.Create;
  TLazRegion(FDestCanvas.ClipRegion).SetAsSimpleRectRegion(Rect(2, 3, 4, 5));
  FDestCanvas.Clipping := True;
  FDestCanvas.WindowOrg := Point(-2, 1);
  FDestCanvas.CanvasCopyRect(FSourceCanvas, 1, 1, 0, 0, 8, 8);
  for Y := 0 to 7 do
    for X := 0 to 7 do
      if (X >= 2) and (X <= 4) and (Y >= 3) and (Y <= 5) then
        AssertEquals('inside translated clip', $FFFF, FDestImage.Colors[X,Y].Red)
      else
        AssertEquals('outside translated clip', 0, FDestImage.Colors[X,Y].Red);
end;

procedure TTestLazCanvasCopyRect.NegativeSource;
var X, Y: Integer;
begin
  FDestCanvas.Clipping := False;
  FDestCanvas.CanvasCopyRect(FSourceCanvas, 0, 0, -2, -1, 8, 8);
  for Y := 0 to 7 do
    for X := 0 to 7 do
      if (X >= 2) and (Y >= 1) then
        AssertEquals('valid source pixel', $FFFF, FDestImage.Colors[X,Y].Red)
      else
        AssertEquals('outside source', 0, FDestImage.Colors[X,Y].Red);
end;

procedure TTestLazCanvasCopyRect.ComplexClip;
var Region: TLazRegion; X,Y: Integer;
begin
  Region := TLazRegion.Create;
  Region.AddRectangle(Rect(1,1,2,2));
  Region.AddRectangle(Rect(5,5,6,6));
  FDestCanvas.ClipRegion := Region;
  FDestCanvas.Clipping := True;
  FDestCanvas.CanvasCopyRect(FSourceCanvas, 0, 0, 0, 0, 8, 8);
  for Y := 0 to 7 do
    for X := 0 to 7 do
      if Region.IsPointInRegion(X,Y) then
        AssertEquals('inside complex clip', $FFFF, FDestImage.Colors[X,Y].Red)
      else
        AssertEquals('outside complex clip', 0, FDestImage.Colors[X,Y].Red);
end;

procedure TTestLazCanvasCopyRect.ScanlineStride;
var Desc: TRawImageDescription; X,Y: Integer; Color: TFPColor;
begin
  Desc := FSourceImage.DataDescription;
  Desc.Width := 5;
  Desc.Height := 6;
  Desc.LineOrder := riloBottomToTop;
  FSourceImage.DataDescription := Desc;
  for Y := 0 to 5 do
    for X := 0 to 4 do
    begin
      Color := colRed;
      Color.Red := (X + Y * 10) * 257;
      FSourceImage.Colors[X,Y] := Color;
    end;
  FDestCanvas.ClipRegion := TLazRegion.Create;
  TLazRegion(FDestCanvas.ClipRegion).SetAsSimpleRectRegion(Rect(1,1,3,4));
  FDestCanvas.Clipping := True;
  FDestCanvas.CanvasCopyRect(FSourceCanvas,0,0,0,0,5,6);
  for Y := 0 to 7 do
    for X := 0 to 7 do
      if (X >= 1) and (X <= 3) and (Y >= 1) and (Y <= 4) then
        AssertEquals('copied padded bottom-up source', (X + Y * 10) * 257,
          FDestImage.Colors[X,Y].Red)
      else
        AssertEquals('outside stride clip', 0, FDestImage.Colors[X,Y].Red);
end;

procedure TTestLazCanvasCopyRect.ClippedAxisLine;
var X,Y: Integer;
begin
  FDestCanvas.ClipRegion := TLazRegion.Create;
  TLazRegion(FDestCanvas.ClipRegion).SetAsSimpleRectRegion(Rect(2,3,4,5));
  FDestCanvas.Clipping := True;
  FDestCanvas.WindowOrg := Point(-2,1);
  FDestCanvas.Pen.FPColor := colRed;
  FDestCanvas.Pen.Width := 1;
  FDestCanvas.Writes := 0;
  FDestCanvas.Line(10,3,0,3);
  AssertTrue('write at most three damaged line pixels', FDestCanvas.Writes <= 3);
  for Y := 0 to 7 do
    for X := 0 to 7 do
      if (X >= 2) and (X <= 4) and (Y = 4) then
        AssertEquals('inside translated line clip', $FFFF, FDestImage.Colors[X,Y].Red)
      else
        AssertEquals('outside translated line clip', 0, FDestImage.Colors[X,Y].Red);
end;

procedure TTestLazCanvasCopyRect.AlphaFill;
var Color: TFPColor;
begin
  Color := colRed;
  Color.Alpha := $8000;
  FDestCanvas.DrawingMode := dmAlphaBlend;
  FDestCanvas.Brush.FPColor := Color;
  FDestCanvas.FillRect(2,2,6,6);
  AssertTrue('keep alpha blending for translucent solid fill',
    (FDestImage.Colors[3,3].Red >= $7000) and
    (FDestImage.Colors[3,3].Red <= $9000));
  AssertEquals('outside translucent fill', 0, FDestImage.Colors[0,0].Red);
end;

procedure TTestLazCanvasCopyRect.OpaqueARGBBitmap;
var Desc: TRawImageDescription; X,Y: Integer;
begin
  Desc := FSourceImage.DataDescription;
  Desc.Depth := 24;
  Desc.AlphaPrec := 0;
  FSourceImage.DataDescription := Desc;
  FSourceImage.FillPixels(colRed);
  FSourceCanvas.ImageFormat := clfOther;
  FDestCanvas.ImageFormat := clfARGB32;
  FDestCanvas.ClipRegion := TLazRegion.Create;
  TLazRegion(FDestCanvas.ClipRegion).SetAsSimpleRectRegion(Rect(2,3,4,5));
  FDestCanvas.Clipping := True;
  FSourceImage.Reads := 0;
  FDestCanvas.CanvasCopyRect(FSourceCanvas,0,0,0,0,8,8);
  AssertEquals('opaque bitmap copy avoids pixel conversion', 0, FSourceImage.Reads);
  for Y := 0 to 7 do
    for X := 0 to 7 do
      if (X >= 2) and (X <= 4) and (Y >= 3) and (Y <= 5) then
      begin
        AssertEquals('copied opaque color', $FFFF, FDestImage.Colors[X,Y].Red);
        AssertEquals('supplied opaque alpha', $FFFF, FDestImage.Colors[X,Y].Alpha);
      end
      else
        AssertEquals('outside opaque copy clip', 0, FDestImage.Colors[X,Y].Red);
end;

initialization
  AddToLCLTestSuite(TTestLazCanvasCopyRect);

end.
