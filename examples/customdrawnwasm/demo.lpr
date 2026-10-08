{ SPDX-License-Identifier: MIT }
{$ifdef CPUWASM32}library demo;{$else}program demo;{$endif}
{$mode objfpc}{$H+}
uses Interfaces, Forms, DemoMain;
begin
  Application.Initialize;
  Application.CreateForm(TDemoForm, DemoForm);
  DemoForm.Show;
  Application.Run;
end.
