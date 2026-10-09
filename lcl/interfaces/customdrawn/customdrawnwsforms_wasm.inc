{ Browser platform backend for the LCL CustomDrawn interface.
  See COPYING.modifiedLGPL.txt for license and linking exception. }


{ TCDWSCustomForm }

procedure BrowserActiveForm(Form: PtrUInt); external 'lcl' name 'active_form';

class function TCDWSCustomForm.DoCreateHandle(const AWinControl: TWinControl;
  const AParams: TCreateParams): TLCLHandle;
begin
  Result := CreateHandle(AWinControl, AParams);
end;

class procedure TCDWSCustomForm.DoShowHide(const AWinControl: TWinControl);
begin
  ShowHide(AWinControl);
end;

class function TCDWSCustomForm.CreateHandle(const AWinControl: TWinControl;
  const AParams: TCreateParams): TLCLHandle;
begin
  {$ifdef VerboseCDForms}
  DebugLn(Format(':>[TCDWSCustomForm.CreateHandle] AWinControl=%x Name=%s: %s',
    [PtrInt(AWinControl), AWinControl.Name, AWinControl.ClassName]));
  {$endif}

  Result := TLCLHandle(AddNewForm(TCustomForm(AWinControl)));

  {$ifdef VerboseCDForms}
  DebugLn(Format(':<[TCDWSCustomForm.CreateHandle] Result=%x',
    [Result]));
  {$endif}
end;

class procedure TCDWSCustomForm.DestroyHandle(const AWinControl: TWinControl);
begin
  RemoveBrowserForm(TCDNonNativeForm(AWinControl.Handle));
end;

class procedure TCDWSCustomForm.SetBorderIcons(const AForm: TCustomForm;
 const ABorderIcons: TBorderIcons);
begin
end;

class procedure TCDWSCustomForm.SetFormBorderStyle(const AForm: TCustomForm;
          const AFormBorderStyle: TFormBorderStyle);
begin
  { The browser surface has no OS window border. }
end;

class procedure TCDWSCustomForm.SetBounds(const AWinControl: TWinControl;
    const ALeft, ATop, AWidth, AHeight: Integer);
begin
  {$ifdef VerboseCDForms}
  DebugLn(Format('[TCDWSCustomForm.SetBounds] AWinControl=%x ALeft=%d ATop=%d AWidth=%d AHeight=%d',
    [PtrInt(AWinControl), ALeft, ATop, AWidth, AHeight]));
  {$endif}
end;

class procedure TCDWSCustomForm.SetIcon(const AForm: TCustomForm; const Small, Big: HICON);
begin
end;

class procedure TCDWSCustomForm.SetShowInTaskbar(const AForm: TCustomForm;
  const AValue: TShowInTaskbar);
begin
end;

class procedure TCDWSCustomForm.ShowModal(const ACustomForm: TCustomForm);
begin
  ShowHide(ACustomForm);
end;

class procedure TCDWSCustomForm.ShowHide(const AWinControl: TWinControl);
var
  Form: TCDNonNativeForm;
begin
  Form := TCDNonNativeForm(AWinControl.Handle);
  if AWinControl.Visible then ShowForm(Form) else HideForm(Form);
  BrowserActiveForm(PtrUInt(GetCurrentForm));
end;

class function TCDWSCustomForm.GetText(const AWinControl: TWinControl; var AText: String): Boolean;
begin
  { The browser keeps no separate title per form; LCL uses its cached caption. }
  Result := False;
end;

class function TCDWSCustomForm.GetTextLen(const AWinControl: TWinControl; var ALength: Integer): Boolean;
begin
  Result := False;
end;

class procedure TCDWSCustomForm.SetText(const AWinControl: TWinControl; const AText: String);
begin
  if AWinControl = Application.MainForm then Application.Title := AText;
end;

class function TCDWSCustomForm.GetClientBounds(const AWincontrol: TWinControl; var ARect: TRect): Boolean;
begin
  ARect := Bounds(0, 0, AWinControl.Width, AWinControl.Height);
  Result := True;
end;

class function TCDWSCustomForm.GetClientRect(const AWincontrol: TWinControl; var ARect: TRect): Boolean;
begin
  Result := GetClientBounds(AWincontrol, ARect);
end;
