{%MainUnit ../lclintf.pas}
{ WASI has no desktop process launcher. Browser backends can provide the
  existing widgetset hooks. See COPYING.modifiedLGPL.txt. }

function FindDefaultBrowser(out ABrowser, AParams: String): Boolean;
begin
  ABrowser := '';
  AParams := '';
  Result := False;
end;

function OpenURL(AURL: String): Boolean;
begin
  Result := Assigned(OpenURLWidgetsetImplementation);
  if Result then Result := OpenURLWidgetsetImplementation(AURL);
end;

function OpenDocument(APath: String): Boolean;
begin
  Result := Assigned(OpenDocumentWidgetsetImplementation);
  if Result then Result := OpenDocumentWidgetsetImplementation(APath);
end;

function SelectInFolder(AFullPath: String): Boolean;
begin
  Result := False;
end;
