{ Regression tests for LazCanvas clipping, state and pixel-copy semantics.
  See COPYING.modifiedLGPL.txt for the license and linking exception. }

unit TestLazCanvas;

{$mode objfpc}{$H+}

interface

uses
  SysUtils, Types, fpcunit, TestGlobals, FPImage, FPCanvas,
  GraphType, IntfGraphics, LazCanvas, LazRegions;

type
  TLazCanvasTest = class(TTestCase)
  private
    FImage: TLazIntfImage;
    FCanvas: TLazCanvas;
  protected
    procedure SetUp; override;
    procedure TearDown; override;
  published
    procedure ComplexClipFill;
    procedure RectClipState;
    procedure ComplexClipState;
    procedure CopyAndFillPreservePadding;
    procedure CopyAliasedPixels;
  end;

implementation

procedure TLazCanvasTest.SetUp;
var
  Desc: TRawImageDescription;
begin
  Desc.Init_BPP32_A8R8G8B8_BIO_TTB(8, 8);
  FImage := TLazIntfImage.Create(0, 0);
  FImage.DataDescription := Desc;
  FImage.FillPixels(colBlack);
  FCanvas := TLazCanvas.Create(FImage);
end;

procedure TLazCanvasTest.TearDown;
begin
  FCanvas.Free;
  FImage.Free;
end;

procedure TLazCanvasTest.ComplexClipFill;
var
  Region: TLazRegion;
begin
  Region := TLazRegion.Create;
  Region.AddRectangle(Rect(4, 4, 6, 6));
  FCanvas.ClipRegion := Region;
  FCanvas.Clipping := True;
  FCanvas.Brush.FPColor := colRed;
  FCanvas.FillRect(0, 0, 8, 8);
  AssertEquals('inside complex clip', $FFFF, FImage.Colors[5, 5].Red);
  AssertEquals('outside complex clip', 0, FImage.Colors[1, 1].Red);
end;

procedure TLazCanvasTest.RectClipState;
var
  State: Integer;
begin
  FCanvas.ClipRect := Rect(4, 4, 7, 7);
  FCanvas.Clipping := True;
  State := FCanvas.SaveState;
  FCanvas.ClipRect := Rect(0, 0, 1, 1);
  FCanvas.RestoreState(State);
  AssertNotNull('restored region', FCanvas.ClipRegion);
  FCanvas.Colors[5, 5] := colRed;
  FCanvas.Colors[1, 1] := colRed;
  AssertEquals('restored inside pixel', $FFFF, FImage.Colors[5, 5].Red);
  AssertEquals('restored outside pixel', 0, FImage.Colors[1, 1].Red);
end;

procedure TLazCanvasTest.ComplexClipState;
var
  Region: TLazRegion;
  Points: GraphType.TPointArray;
  State, I: Integer;
begin
  Region := TLazRegion.Create;
  Region.AddRectangle(Rect(1, 1, 2, 2));
  Region.AddEllipse(4, 1, 7, 4);
  SetLength(Points, 4);
  Points[0] := Point(1, 5); Points[1] := Point(3, 5);
  Points[2] := Point(3, 7); Points[3] := Point(1, 7);
  Region.AddPolygon(Points, rfmOddEven);
  FCanvas.ClipRegion := Region;
  FCanvas.Clipping := True;
  State := FCanvas.SaveState;
  for I := 0 to High(Points) do
    TLazRegionPolygon(Region.Parts[2]).Points[I] := Point(0, 0);
  FCanvas.ClipRect := Rect(0, 0, 1, 1);
  FCanvas.RestoreState(State);
  Region := TLazRegion(FCanvas.ClipRegion);
  Region.Assign(Region);
  AssertTrue('rectangle snapshot', Region.IsPointInRegion(1, 1));
  AssertTrue('ellipse snapshot', Region.IsPointInRegion(5, 2));
  AssertTrue('polygon snapshot', Region.IsPointInRegion(2, 6));
  AssertFalse('outside snapshot', Region.IsPointInRegion(7, 7));
end;

procedure TLazCanvasTest.CopyAndFillPreservePadding;
var
  Desc: TRawImageDescription;
  Source: TLazIntfImage;
  Canvas: TLazCanvas;
begin
  Desc := FImage.DataDescription;
  Desc.AlphaPrec := 0;
  Desc.Depth := 24;
  FImage.DataDescription := Desc;
  FillByte(FImage.PixelData^, Desc.BytesPerLine * Desc.Height, $AB);
  Source := TLazIntfImage.Create(0, 0);
  Source.DataDescription := Desc;
  Canvas := TLazCanvas.Create(Source);
  try
    Source.Colors[0, 0] := colRed;
    FCanvas.CanvasCopyRect(Canvas, 0, 0, 0, 0, 1, 1);
    AssertEquals('copied channel', $FFFF, FImage.Colors[0, 0].Red);
    AssertEquals('destination padding', $AB, FImage.PixelData[0]);
    FCanvas.Brush.FPColor := colBlue;
    FCanvas.FillRect(0, 0, 2, 2);
    AssertEquals('filled channel', $FFFF, FImage.Colors[0, 0].Blue);
    AssertEquals('filled padding', $AB, FImage.PixelData[0]);
  finally
    Canvas.Free;
    Source.Free;
  end;
end;

procedure TLazCanvasTest.CopyAliasedPixels;
var
  X: Integer;
begin
  FImage.Colors[0, 0] := colRed;
  FImage.Colors[1, 0] := colBlue;
  FCanvas.CanvasCopyRect(FCanvas, 1, 0, 0, 0, 3, 1);
  for X := 1 to 3 do
    AssertEquals('pixel copy order', $FFFF, FImage.Colors[X, 0].Red);
end;

initialization
  AddToLCLTestSuite(TLazCanvasTest);

end.
