{ SPDX-License-Identifier: MIT
  Entry point for the ordinary LCL form used by the browser example. }
{$ifdef CPUWASM32}library demo;{$else}program demo;{$endif}
{$mode objfpc}{$H+}
uses Interfaces, Forms, DemoMain;
begin
  Application.Initialize;
  Application.CreateForm(TDemoForm, DemoForm);
  DemoForm.Show;
  Application.Run;
end.
