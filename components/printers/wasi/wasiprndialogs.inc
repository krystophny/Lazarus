{%MainUnit ../printersdlgs.pp}

{ Browser builds have no system printer or page setup dialogs.
  Part of Printer4Lazarus; see COPYING.modifiedLGPL.txt for the license. }

function TPageSetupDialog.DoExecute: Boolean;
begin
  Result := False;
end;

function TPrinterSetupDialog.DoExecute: Boolean;
begin
  Result := False;
end;

function TPrintDialog.DoExecute: Boolean;
begin
  Result := False;
end;
