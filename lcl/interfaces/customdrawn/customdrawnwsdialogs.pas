{
 *****************************************************************************
 *                          CustomDrawnWSDialogs.pp                          *
 *                              --------------                               * 
 *                                                                           *
 *                                                                           *
 *****************************************************************************

 *****************************************************************************
  This file is part of the Lazarus Component Library (LCL)

  See the file COPYING.modifiedLGPL.txt, included in this distribution,
  for details about the license.
 *****************************************************************************
}
unit CustomDrawnWSDialogs;

{$mode objfpc}{$H+}
{$I customdrawndefines.inc}

interface

uses
  // RTL
  SysUtils, Classes, Types,
//  {$ifdef CD_Windows}Windows, customdrawn_WinProc,{$endif}
//  {$ifdef CD_Cocoa}MacOSAll, CocoaAll, customdrawn_cocoaproc, CocoaGDIObjects,{$endif}
//  {$ifdef CD_X11}X, XLib, XUtil, BaseUnix, customdrawn_x11proc,{$ifdef CD_UseNativeText}xft, fontconfig,{$endif}{$endif}
//  {$ifdef CD_Android}customdrawn_androidproc, jni, bitmap, log, keycodes,{$endif}
  // LCL
  // RTL + LCL
  LCLType, LCLProc, Dialogs, Controls, Forms, Graphics,
  // Widgetset
  WSDialogs, WSLCLClasses,
  customdrawncontrols, customdrawnwscontrols, customdrawnproc;

type

  { TCDWSCommonDialog }

  TCDWSCommonDialog = class(TWSCommonDialog)
  published
    {$ifdef CD_Wasm}
    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;
    {$endif}
{    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure DestroyHandle(const ACommonDialog: TCommonDialog); override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;}
  end;

  { TCDWSFileDialog }

  TCDWSFileDialog = class(TWSFileDialog)
  published
    {$ifdef CD_Wasm}
    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;
    {$endif}
{    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;}
  end;

  { TCDWSOpenDialog }

  TCDWSOpenDialog = class(TWSOpenDialog)
  published
    {$ifdef CD_Wasm}
    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;
    {$endif}
  end;

  { TCDWSSaveDialog }

  TCDWSSaveDialog = class(TWSSaveDialog)
  published
    {$ifdef CD_Wasm}
    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;
    {$endif}
  end;

  { TCDWSSelectDirectoryDialog }

  TCDWSSelectDirectoryDialog = class(TWSSelectDirectoryDialog)
  published
{    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;}
  end;

  { TCDWSColorDialog }

  TCDWSColorDialog = class(TWSColorDialog)
  published
    {$ifdef CD_Wasm}
    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;
    {$endif}
{    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;}
  end;

  { TCDWSColorButton }

  TCDWSColorButton = class(TWSColorButton)
  published
  end;

  { TCDWSFontDialog }

  TCDWSFontDialog = class(TWSFontDialog)
  published
    {$ifdef CD_Wasm}
    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;
    {$endif}
{    class function CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle; override;
    class procedure ShowModal(const ACommonDialog: TCommonDialog); override;}
  end;


implementation

{$ifdef CD_Wasm}
uses FPJSON, JSONParser;

function BrowserCommonDialog(Request: PChar; RequestLength: LongInt;
  Response: PChar; Capacity: LongInt): LongInt; external 'lcl' name 'common_dialog';

class function TCDWSCommonDialog.CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle;
begin
  Result := PtrUInt(ACommonDialog);
end;

class procedure TCDWSCommonDialog.ShowModal(const ACommonDialog: TCommonDialog);
var
  Request: TJSONObject;
  Response: TJSONData;
  JSON: String;
  Buffer: array[0..8191] of Char;
  Count: Integer;
  FileDialog: TFileDialog;
  FontDialog: TFontDialog;
begin
  Request := TJSONObject.Create(['caption', ACommonDialog.Title]);
  try
    if ACommonDialog is TFileDialog then
    begin
      FileDialog := TFileDialog(ACommonDialog);
      if ACommonDialog is TSaveDialog then Request.Add('kind', 'save')
      else Request.Add('kind', 'open');
      Request.Add('filename', ExtractFileName(FileDialog.FileName));
      Request.Add('filter', FileDialog.Filter);
      Request.Add('filterIndex', FileDialog.FilterIndex);
      Request.Add('defaultExt', FileDialog.DefaultExt);
    end
    else if ACommonDialog is TColorDialog then
    begin
      Request.Add('kind', 'color');
      Request.Add('color', ColorToRGB(TColorDialog(ACommonDialog).Color));
    end
    else if ACommonDialog is TFontDialog then
    begin
      FontDialog := TFontDialog(ACommonDialog);
      Request.Add('kind', 'font');
      Request.Add('name', FontDialog.Font.Name);
      Request.Add('size', FontDialog.Font.Size);
      Request.Add('bold', fsBold in FontDialog.Font.Style);
      Request.Add('italic', fsItalic in FontDialog.Font.Style);
    end
    else Exit;
    JSON := Request.AsJSON;
    Count := BrowserCommonDialog(PChar(JSON), Length(JSON), @Buffer[0], SizeOf(Buffer));
    ACommonDialog.UserChoice := mrCancel;
    if Count <= 0 then Exit;
    SetString(JSON, PChar(@Buffer[0]), Count);
    Response := GetJSON(JSON);
    try
      if ACommonDialog is TFileDialog then
      begin
        FileDialog.FileName := Response.FindPath('filename').AsString;
        FileDialog.FilterIndex := Response.FindPath('filterIndex').AsInteger;
        FileDialog.Files.Clear;
        FileDialog.Files.Add(FileDialog.FileName);
      end
      else if ACommonDialog is TColorDialog then
        TColorDialog(ACommonDialog).Color := Response.FindPath('color').AsInteger
      else if ACommonDialog is TFontDialog then
      begin
        FontDialog.Font.Name := Response.FindPath('name').AsString;
        FontDialog.Font.Size := Response.FindPath('size').AsInteger;
        FontDialog.Font.Style := [];
        if Response.FindPath('bold').AsBoolean then FontDialog.Font.Style := FontDialog.Font.Style + [fsBold];
        if Response.FindPath('italic').AsBoolean then FontDialog.Font.Style := FontDialog.Font.Style + [fsItalic];
      end;
      ACommonDialog.UserChoice := mrOK;
    finally
      Response.Free;
    end;
  finally
    Request.Free;
  end;
end;

class function TCDWSFileDialog.CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle;
begin Result := TCDWSCommonDialog.CreateHandle(ACommonDialog); end;
class procedure TCDWSFileDialog.ShowModal(const ACommonDialog: TCommonDialog);
begin TCDWSCommonDialog.ShowModal(ACommonDialog); end;

class function TCDWSOpenDialog.CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle;
begin Result := TCDWSCommonDialog.CreateHandle(ACommonDialog); end;
class procedure TCDWSOpenDialog.ShowModal(const ACommonDialog: TCommonDialog);
begin TCDWSCommonDialog.ShowModal(ACommonDialog); end;

class function TCDWSSaveDialog.CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle;
begin Result := TCDWSCommonDialog.CreateHandle(ACommonDialog); end;
class procedure TCDWSSaveDialog.ShowModal(const ACommonDialog: TCommonDialog);
begin TCDWSCommonDialog.ShowModal(ACommonDialog); end;

class function TCDWSColorDialog.CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle;
begin Result := TCDWSCommonDialog.CreateHandle(ACommonDialog); end;
class procedure TCDWSColorDialog.ShowModal(const ACommonDialog: TCommonDialog);
begin TCDWSCommonDialog.ShowModal(ACommonDialog); end;

class function TCDWSFontDialog.CreateHandle(const ACommonDialog: TCommonDialog): TLCLHandle;
begin Result := TCDWSCommonDialog.CreateHandle(ACommonDialog); end;
class procedure TCDWSFontDialog.ShowModal(const ACommonDialog: TCommonDialog);
begin TCDWSCommonDialog.ShowModal(ACommonDialog); end;
{$endif}

end.
