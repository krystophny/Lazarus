{%MainUnit gtk2wsstdctrls.pp}

type
  PNativeIMEEndData = ^TNativeIMEEndData;
  TNativeIMEEndData = record
    MainWidget: PGtkWidget;
    Generation: guint;
    SourceID: guint;
  end;

  PNativeIMEState = ^TNativeIMEState;
  TNativeIMEState = record
    NativeWidget: PGtkWidget;
    Active: Boolean;
    Generation: guint;
    PendingEnd: PNativeIMEEndData;
  end;

const
  NativeIMEStateKey = 'lcl-native-ime-state';

{ Callbacks }

procedure Gtk2WS_FreeNativeIMEEndData(EndData: PNativeIMEEndData);
begin
  if EndData = nil then Exit;
  if EndData^.MainWidget <> nil then
    g_object_unref(PGObject(EndData^.MainWidget));
  Dispose(EndData);
end;

procedure Gtk2WS_CancelNativeIMEEnd(State: PNativeIMEState);
var
  EndData: PNativeIMEEndData;
begin
  if (State = nil) or (State^.PendingEnd = nil) then Exit;
  EndData := State^.PendingEnd;
  State^.PendingEnd := nil;
  if EndData^.SourceID <> 0 then
    g_source_remove(EndData^.SourceID);
  Gtk2WS_FreeNativeIMEEndData(EndData);
end;

procedure Gtk2WS_FreeNativeIMEState(Data: gpointer); cdecl;
var
  State: PNativeIMEState;
begin
  State := PNativeIMEState(Data);
  if State = nil then Exit;
  Gtk2WS_CancelNativeIMEEnd(State);
  if State^.NativeWidget <> nil then
    g_object_unref(PGObject(State^.NativeWidget));
  Dispose(State);
end;

function Gtk2WS_GetNativeIMEState(MainWidget, NativeWidget: PGtkWidget): PNativeIMEState;
begin
  Result := g_object_get_data(PGObject(MainWidget), NativeIMEStateKey);
  if Result <> nil then Exit;

  New(Result);
  FillChar(Result^, SizeOf(Result^), 0);
  Result^.NativeWidget := PGtkWidget(g_object_ref(PGObject(NativeWidget)));
  g_object_set_data_full(PGObject(MainWidget), NativeIMEStateKey, Result,
    @Gtk2WS_FreeNativeIMEState);
end;

function Gtk2WS_GetNativeIMEWidgetInfo(State: PNativeIMEState;
  out MainWidget: PGtkWidget): PWidgetInfo;
begin
  Result := nil;
  MainWidget := nil;
  if (State = nil) or (State^.NativeWidget = nil) then Exit;
  MainWidget := PGtkWidget(GetMainWidget(State^.NativeWidget));
  if MainWidget = nil then Exit;
  Result := GetWidgetInfo(MainWidget);
  if (Result = nil) or (Result^.CoreWidget <> State^.NativeWidget) then
    Result := nil;
end;

procedure Gtk2WS_DeliverNativeIME(ControlInfo: PWidgetInfo; Flags: WPARAM;
  Data: LPARAM);
var
  Mess: TLMessage;
begin
  if (ControlInfo = nil) or (ControlInfo^.LCLObject = nil) then Exit;
  FillChar(Mess, SizeOf(Mess), 0);
  Mess.Msg := LM_IM_COMPOSITION;
  Mess.WParam := Flags;
  Mess.LParam := Data;
  DeliverMessage(ControlInfo^.LCLObject, Mess);
end;

function Gtk2WS_NativeIMEEndIdle(Data: gpointer): gboolean; cdecl;
var
  EndData: PNativeIMEEndData;
  State: PNativeIMEState;
  ControlInfo: PWidgetInfo;
begin
  Result := gFalse;
  EndData := PNativeIMEEndData(Data);
  if EndData = nil then Exit;

  State := g_object_get_data(PGObject(EndData^.MainWidget), NativeIMEStateKey);
  if (State <> nil) and (State^.PendingEnd = EndData) then
  begin
    State^.PendingEnd := nil;
    if State^.Generation = EndData^.Generation then
    begin
      State^.Active := False;
      ControlInfo := GetWidgetInfo(EndData^.MainWidget);
      if (ControlInfo <> nil) and
         (ControlInfo^.CoreWidget = State^.NativeWidget) then
        Gtk2WS_DeliverNativeIME(ControlInfo, GTK_IM_FLAG_END, 0);
    end;
  end;
  Gtk2WS_FreeNativeIMEEndData(EndData);
end;

procedure Gtk2WS_NativePreeditChanged(NativeWidget: PGtkWidget;
  Preedit: Pgchar; Data: gpointer); cdecl;
var
  State: PNativeIMEState;
  ControlInfo: PWidgetInfo;
  MainWidget: PGtkWidget;
  Flags: WPARAM;
  EndData: PNativeIMEEndData;
begin
  State := PNativeIMEState(Data);
  if State = nil then Exit;
  if NativeWidget <> State^.NativeWidget then Exit;
  ControlInfo := Gtk2WS_GetNativeIMEWidgetInfo(State, MainWidget);
  if ControlInfo = nil then Exit;

  if (Preedit <> nil) and (Preedit^ <> #0) then
  begin
    if State^.PendingEnd <> nil then
    begin
      Inc(State^.Generation);
      Gtk2WS_CancelNativeIMEEnd(State);
      State^.Active := False;
    end;

    if State^.Active then
      Flags := GTK_IM_FLAG_PREEDIT or GTK_IM_FLAG_REPLACE
    else
    begin
      State^.Active := True;
      Inc(State^.Generation);
      Flags := GTK_IM_FLAG_START or GTK_IM_FLAG_PREEDIT;
    end;
    { Preedit is borrowed from GTK, so consumers must process it synchronously. }
    Gtk2WS_DeliverNativeIME(ControlInfo, Flags, LPARAM(Preedit));
  end
  else if State^.Active and (State^.PendingEnd = nil) then
  begin
    New(EndData);
    EndData^.MainWidget := PGtkWidget(g_object_ref(PGObject(MainWidget)));
    EndData^.Generation := State^.Generation;
    EndData^.SourceID := 0;
    State^.PendingEnd := EndData;
    { GTK commits to the native buffer in this dispatch; idle runs after its
      changed callback has delivered CM_TEXTCHANGED. Cancellation also drains. }
    EndData^.SourceID := g_idle_add(TGtkFunction(@Gtk2WS_NativeIMEEndIdle), EndData);
  end;
end;

procedure Gtk2WS_NativeWidgetDestroy(Widget: PGtkWidget; Data: gpointer); cdecl;
var
  State: PNativeIMEState;
begin
  State := PNativeIMEState(Data);
  if State = nil then Exit;
  Gtk2WS_CancelNativeIMEEnd(State);
  if State^.NativeWidget <> nil then
    g_signal_handlers_disconnect_matched(PGtkObject(State^.NativeWidget),
      G_SIGNAL_MATCH_DATA, 0, 0, nil, nil, State);
  g_signal_handlers_disconnect_matched(PGtkObject(Widget), G_SIGNAL_MATCH_DATA,
    0, 0, nil, nil, State);
  g_object_set_data_full(PGObject(Widget), NativeIMEStateKey, nil, nil);
end;

procedure Gtk2WS_DetachNativeIME(MainWidget: PGtkWidget);
var
  State: PNativeIMEState;
begin
  if MainWidget = nil then Exit;
  State := g_object_get_data(PGObject(MainWidget), NativeIMEStateKey);
  if State = nil then Exit;
  Gtk2WS_CancelNativeIMEEnd(State);
  if State^.NativeWidget <> nil then
    g_signal_handlers_disconnect_matched(PGtkObject(State^.NativeWidget),
      G_SIGNAL_MATCH_DATA, 0, 0, nil, nil, State);
  g_signal_handlers_disconnect_matched(PGtkObject(MainWidget), G_SIGNAL_MATCH_DATA,
    0, 0, nil, nil, State);
  g_object_set_data_full(PGObject(MainWidget), NativeIMEStateKey, nil, nil);
end;

procedure Gtk2WS_AttachNativeIME(MainWidget, NativeWidget: PGtkWidget);
var
  State: PNativeIMEState;
begin
  if (MainWidget = nil) or (NativeWidget = nil) or
     (g_signal_lookup('preedit-changed', GTK_OBJECT_TYPE(NativeWidget)) = 0) then
    Exit;
  if g_object_get_data(PGObject(MainWidget), NativeIMEStateKey) <> nil then Exit;
  State := Gtk2WS_GetNativeIMEState(MainWidget, NativeWidget);
  g_signal_connect(PGtkObject(NativeWidget), 'preedit-changed',
    TGTKSignalFunc(@Gtk2WS_NativePreeditChanged), State);
  g_signal_connect(PGtkObject(MainWidget), 'destroy',
    TGTKSignalFunc(@Gtk2WS_NativeWidgetDestroy), State);
end;

procedure Gtk2WS_MemoChanged({%H-}AGtkTextBuffer: PGtkTextBuffer; WidgetInfo: PWidgetInfo); cdecl;
var
  Mess: TLMessage;
begin
  EventTrace('Gtk2WS_MemoChanged', WidgetInfo^.LCLObject);
  if WidgetInfo^.ChangeLock > 0 then
  begin
    Dec(WidgetInfo^.ChangeLock);
    Exit;
  end;
  Mess.Msg := CM_TEXTCHANGED;
  DeliverMessage(WidgetInfo^.LCLObject, Mess);
end;

procedure Gtk2WS_MemoCutToClip(widget: PGtkWidget; WidgetInfo: PWidgetInfo); cdecl;
var
  Mess: TLMessage;
begin
  EventTrace('Gtk2WS_MemoCutToClip', WidgetInfo^.LCLObject);
  if (Widget=nil) then ;
  Mess.msg := LM_CUT;
  DeliverMessage(WidgetInfo^.LCLObject, Mess);
end;

procedure Gtk2WS_MemoCopyToClip(widget: PGtkWidget; WidgetInfo: PWidgetInfo); cdecl;
var
  Mess: TLMessage;
begin
  EventTrace('Gtk2WS_MemoCopyToClip', WidgetInfo^.LCLObject);
  if (Widget=nil) then ;
  Mess.msg := LM_COPY;
  DeliverMessage(WidgetInfo^.LCLObject, Mess);
end;

procedure Gtk2WS_MemoPasteFromClip(widget: PGtkWidget; WidgetInfo: PWidgetInfo); cdecl;
var
  Mess: TLMessage;
begin
  EventTrace('Gtk2WS_MemoPasteFromClip', WidgetInfo^.LCLObject);
  if (Widget=nil) then ;
  Mess.msg := LM_PASTE;
  DeliverMessage(WidgetInfo^.LCLObject, Mess);
  g_object_set_data(PGObject(widget), 'lcl-memo-paste-from-clip', gPointer(-1));
end;

procedure Gtk2WS_MemoTextInserting (Textbuffer: PGtkTextBuffer; StartIter: PGtkTextIter;
             thetext: pgchar; NewTextLength: gint;  WidgetInfo: PWidgetInfo); cdecl;
var
  BeginIter, EndIter: TGtkTextIter;
  Memo: TCustomMemo;
  CurrLength, CutLength: integer;
begin
  if g_object_get_data(PGObject(WidgetInfo^.CoreWidget), 'lcl-memo-paste-from-clip') <> nil then
  begin
    g_object_set_data(PGObject(WidgetInfo^.CoreWidget), 'lcl-memo-paste-from-clip', nil);
    gtk_text_buffer_get_selection_bounds(TextBuffer, @BeginIter, @EndIter);
    gtk_text_buffer_delete(TextBuffer, @BeginIter, @EndIter);
    gtk_text_buffer_insert(TextBuffer, @BeginIter, theText, NewTextLength);
    g_signal_stop_emission_by_name(PGtkObject(Textbuffer), 'insert-text');
    Exit;
  end;

  { GTK2 does not provide its own max. length for memos
    so we have to do our own. }

  if TControl(WidgetInfo^.LCLObject) is TCustomMemo then
  begin
    Memo := TCustomMemo(WidgetInfo^.LCLObject);
    if Memo.MaxLength <= 0 then Exit;

    CurrLength := gtk_text_buffer_get_char_count(TextBuffer);
    if CurrLength + NewTextLength <= Memo.MaxLength then
      Exit;

    CutLength := CurrLength + NewTextLength - Memo.MaxLength;

    if NewTextLength - CutLength > 0 then
      gtk_text_buffer_insert(TextBuffer, StartIter, TheText, NewTextLength - CutLength);

    g_signal_stop_emission_by_name(PGtkObject(Textbuffer), 'insert-text');
  end;
end;



{ TGtk2WSCustomMemo }

class procedure TGtk2WSCustomMemo.SetCallbacks(const AGtkWidget: PGtkWidget;
  const AWidgetInfo: PWidgetInfo);
var
  TextBuf: PGtkTextBuffer;
  TextView: PGtkWidget;
begin
  TextView := AWidgetInfo^.CoreWidget;
  TextBuf := gtk_text_view_get_buffer(PGtkTextView(TextView));

  //TGtkWSBaseScrollingWinControl.SetCallbacks(AGtkWidget, AWidgetInfo);

  TGtk2WSWinControl.SetCallbacks(PGtkObject(AGtkWidget), TComponent(AWidgetInfo^.LCLObject));
  
  SignalConnect(PGtkWidget(TextBuf), 'changed', @Gtk2WS_MemoChanged, AWidgetInfo);
  SignalConnect(PGtkWidget(TextBuf), 'insert-text', @Gtk2WS_MemoTextInserting, AWidgetInfo);

  Gtk2WS_AttachNativeIME(AGtkWidget, TextView);

  //SetCallback(LM_ACTIVATE, AGTKObject,ALCLObject);
  
  SignalConnect(AWidgetInfo^.CoreWidget, 'cut-clipboard', @Gtk2WS_MemoCutToClip, AWidgetInfo);
  SignalConnect(AWidgetInfo^.CoreWidget, 'copy-clipboard', @Gtk2WS_MemoCopyToClip, AWidgetInfo);
  SignalConnect(AWidgetInfo^.CoreWidget, 'paste-clipboard', @Gtk2WS_MemoPasteFromClip, AWidgetInfo);

  g_signal_connect_after(PGtkObject(AWidgetInfo^.CoreWidget), 'populate-popup',
    gtk_signal_func(@gtkDefaultPopupMenuCloseFix), AWidgetInfo);
end;

class procedure TGtk2WSCustomMemo.DestroyHandle(const AWinControl: TWinControl);
begin
  if AWinControl.HandleAllocated then
    Gtk2WS_DetachNativeIME(PGtkWidget(AWinControl.Handle));
  TGtk2WSWinControl.DestroyHandle(AWinControl);
end;

class function TGtk2WSCustomMemo.CreateHandle(const AWinControl: TWinControl;
  const AParams: TCreateParams): TLCLHandle;
var
  Widget,
  TempWidget: PGtkWidget;
  WidgetInfo: PWidgetInfo;
  SS:TPoint;
begin
  Widget := gtk_scrolled_window_new(nil, nil);
  Result := TLCLHandle({%H-}PtrUInt(Widget));
  if Result = 0 then Exit;
  {$IFDEF DebugLCLComponents}
  DebugGtkWidgets.MarkCreated(Widget,dbgsName(AWinControl));
  {$ENDIF}

  WidgetInfo := CreateWidgetInfo({%H-}Pointer(Result), AWinControl, AParams);

  TempWidget := gtk_text_view_new();
  gtk_container_add(PGtkContainer(Widget), TempWidget);

  GTK_WIDGET_UNSET_FLAGS(PGtkScrolledWindow(Widget)^.hscrollbar, GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS(PGtkScrolledWindow(Widget)^.vscrollbar, GTK_CAN_FOCUS);

  SS:=Gtk2TranslateScrollStyle(TCustomMemo(AWinControl).ScrollBars);
  gtk_scrolled_window_set_policy(PGtkScrolledWindow(Widget),SS.X, SS.Y);

  // add border for memo
  gtk_scrolled_window_set_shadow_type(PGtkScrolledWindow(Widget),
    BorderStyleShadowMap[TCustomEdit(AWinControl).BorderStyle]);
  
  SetMainWidget(Widget, TempWidget);
  GetOrCreateWidgetInfo(Widget)^.CoreWidget := TempWidget;

  gtk_text_buffer_set_text(gtk_text_view_get_buffer(PGtkTextView(TempWidget)), PChar(TCustomMemo(AWinControl).Text), -1);
  gtk_text_view_set_editable(PGtkTextView(TempWidget), not TCustomMemo(AWinControl).ReadOnly);
  gtk_text_view_set_justification(PGtkTextView(TempWidget), aGtkJustification[TCustomMemo(AWinControl).Alignment]);
  if TCustomMemo(AWinControl).WordWrap then
    gtk_text_view_set_wrap_mode(PGtkTextView(TempWidget), GTK_WRAP_WORD)
  else
    gtk_text_view_set_wrap_mode(PGtkTextView(TempWidget), GTK_WRAP_NONE);

  gtk_text_view_set_accepts_tab(PGtkTextView(TempWidget), TCustomMemo(AWinControl).WantTabs);
  
  gtk_widget_show_all(Widget);

  Set_RC_Name(AWinControl, Widget);
  if not AWinControl.HandleObjectShouldBeVisible and not (csDesigning in AWinControl.ComponentState) then
    gtk_widget_hide(Widget);
  SetCallbacks(Widget, WidgetInfo);
end;

class function TGtk2WSCustomMemo.GetStrings(const ACustomMemo: TCustomMemo): TStrings;
var
  TextView: PGtkTextView;
begin
  TextView := PGtkTextView(GetWidgetInfo({%H-}Pointer(ACustomMemo.Handle))^.CoreWidget);
  Result := TGtk2MemoStrings.Create(TextView, ACustomMemo);
end;

class procedure TGtk2WSCustomMemo.SetAlignment(const ACustomEdit: TCustomEdit;
  const AAlignment: TAlignment);
var
  TextView: PGtkTextView;
begin
  TextView := PGtkTextView(GetWidgetInfo({%H-}Pointer(ACustomEdit.Handle))^.CoreWidget);
  gtk_text_view_set_justification(TextView, aGtkJustification[AAlignment]);
end;


class procedure TGtk2WSCustomMemo.SetColor(const AWinControl: TWinControl);
var
  AWidget: PGTKWidget;
begin
  if not WSCheckHandleAllocated(AWinControl, 'SetColor') then
    Exit;
  AWidget := {%H-}PGtkWidget(AWinControl.Handle);
  AWidget := GetOrCreateWidgetInfo(AWidget)^.CoreWidget;
  Gtk2WidgetSet.SetWidgetColor(AWidget,
    AWinControl.Font.Color,
    AWinControl.Color,
    [GTK_STATE_NORMAL, GTK_STATE_ACTIVE, GTK_STATE_PRELIGHT, GTK_STYLE_BASE]);
end;

class procedure TGtk2WSCustomMemo.SetFont(const AWinControl: TWinControl; const AFont: TFont);
var
  AWidget: PGTKWidget;
begin
  if not WSCheckHandleAllocated(AWinControl, 'SetFont') then
    Exit;

  AWidget:= {%H-}PGtkWidget(AWinControl.Handle);
  AWidget:= GetWidgetInfo(AWidget)^.CoreWidget;

  if AWidget <> nil then
  begin
    Gtk2WidgetSet.SetWidgetColor(AWidget, AFont.color, clNone,
       [GTK_STATE_NORMAL,GTK_STATE_ACTIVE,GTK_STATE_PRELIGHT,GTK_STATE_SELECTED,
        GTK_STYLE_TEXT]);
    Gtk2WidgetSet.SetWidgetFont(AWidget, AFont);
  end;
end;

class procedure TGtk2WSCustomMemo.SetSelStart(const ACustomEdit: TCustomEdit;
  NewStart: integer);
var
  MemoStrings: TGtk2MemoStrings;
begin
  if not WSCheckHandleAllocated(ACustomEdit, 'SetSelStart') then
    Exit;

  MemoStrings := TCustomMemo(ACustomEdit).Lines as TGtk2MemoStrings;
  MemoStrings.QueueCursorMove(NewStart);
  MemoStrings.QueueSelectLength(0);
end;

class procedure TGtk2WSCustomMemo.SetSelLength(const ACustomEdit: TCustomEdit;
  NewLength: integer);
var
  MemoStrings: TGtk2MemoStrings;
begin
  if not WSCheckHandleAllocated(ACustomEdit, 'SetSelLength') then
    Exit;

  MemoStrings := TCustomMemo(ACustomEdit).Lines as TGtk2MemoStrings;
  MemoStrings.QueueSelectLength(NewLength);
end;

class procedure TGtk2WSCustomMemo.SetWantTabs(const ACustomMemo: TCustomMemo;
  const NewWantTabs: boolean);
var
  TextView: PGtkTextView;
begin
  if not WSCheckHandleAllocated(ACustomMemo, 'SetWantTabs') then
    Exit;

  TextView := PGtkTextView(GetWidgetInfo({%H-}Pointer(ACustomMemo.Handle))^.CoreWidget);
  gtk_text_view_set_accepts_tab(TextView, NewWantTabs);
end;

class procedure TGtk2WSCustomMemo.SetEchoMode(const ACustomEdit: TCustomEdit;
  NewMode: TEchoMode);
begin
  // not supported
end;

class procedure TGtk2WSCustomMemo.SetPasswordChar(
  const ACustomEdit: TCustomEdit; NewChar: char);
begin
  // not supported
end;

class procedure TGtk2WSCustomMemo.SetWordWrap(const ACustomMemo: TCustomMemo;
  const NewWordWrap: boolean);
var
  TextView: PGtkTextView;
begin
  if not WSCheckHandleAllocated(ACustomMemo, 'SetWordWrap') then
    Exit;
  TextView := PGtkTextView(GetWidgetInfo({%H-}Pointer(ACustomMemo.Handle))^.CoreWidget);
  if NewWordWrap then
    gtk_text_view_set_wrap_mode(PGtkTextView(TextView), GTK_WRAP_WORD)
  else
    gtk_text_view_set_wrap_mode(PGtkTextView(TextView), GTK_WRAP_NONE);
end;

class procedure TGtk2WSCustomMemo.SetCharCase(const ACustomEdit: TCustomEdit;
  NewCase: TEditCharCase);
begin
  // TODO: TGtk2WSCustomMemo.SetCharCase: implement me!
end;

class procedure TGtk2WSCustomMemo.SetScrollbars(const ACustomMemo: TCustomMemo;
  const NewScrollbars: TScrollStyle);
var
  SS:TPoint;
  ScrollWidget: PGtkScrolledWindow;
begin
  if not WSCheckHandleAllocated(ACustomMemo, 'SetScrollBars') then Exit;
  SS:=Gtk2TranslateScrollStyle(NewScrollBars);
  ScrollWidget:={%H-}PGtkScrolledWindow(ACustomMemo.Handle);
  gtk_scrolled_window_set_policy(ScrollWidget ,SS.X, SS.Y);
end;

class procedure TGtk2WSCustomMemo.SetMaxLength(const ACustomEdit: TCustomEdit;
  NewLength: integer);
var
  Widget: PGtkWidget;
begin
  Widget:={%H-}PGtkWidget(ACustomEdit.Handle);
  if GtkWidgetIsA(Widget, GTK_TYPE_ENTRY) then
    gtk_entry_set_max_length(GTK_ENTRY(Widget), guint16(NewLength));
end;

class procedure TGtk2WSCustomMemo.SetReadOnly(const ACustomEdit: TCustomEdit;
  NewReadOnly: boolean);
var
  TextView: PGtkTextView;
begin
  if not WSCheckHandleAllocated(ACustomEdit, 'SetReadOnly') then
    Exit;

  TextView := PGtkTextView(GetWidgetInfo({%H-}Pointer(ACustomEdit.Handle))^.CoreWidget);
  if TextView <> nil then
    gtk_text_view_set_editable(TextView, not NewReadOnly);
end;

class procedure TGtk2WSCustomMemo.SetSelText(const ACustomEdit: TCustomEdit;
  const NewSelText: string);
var
  MemoStrings: TGtk2MemoStrings;
  TextBuf: PGtkTextBuffer;
  SelStart, SelLength, Utf8Len, CutLen: Integer;
  StartIter, EndIter: TGtkTextIter;
begin
  if not WSCheckHandleAllocated(ACustomEdit, 'SetSelText') then
    Exit;
  MemoStrings := TCustomMemo(ACustomEdit).Lines as TGtk2MemoStrings;
  with GetOrCreateWidgetInfo({%H-}Pointer(ACustomEdit.Handle))^ do
    TextBuf := gtk_text_view_get_buffer(PGtkTextView(CoreWidget));
  SelStart := GetSelStart(ACustomEdit);
  SelLength := GetSelLength(ACustomEdit);
  gtk_text_buffer_get_iter_at_offset(TextBuf, @StartIter, SelStart);
  if SelLength > 0 then
  begin
    gtk_text_buffer_get_iter_at_offset(TextBuf, @EndIter, SelStart + SelLength);
    MemoStrings.QueueSelectLength(0);
    gtk_text_buffer_delete(TextBuf, @StartIter, @EndIter);
  end;
  Utf8Len := UTF8Length(NewSelText);
  SelStart := SelStart + Utf8Len;
  if ACustomEdit.MaxLength > 0 then
  begin
    CutLen := gtk_text_buffer_get_char_count(TextBuf) + Utf8Len - ACustomEdit.MaxLength;
    if CutLen > 0 then
      Dec(SelStart, CutLen)
  end;
  MemoStrings.QueueCursorMove(SelStart);
  gtk_text_buffer_insert(TextBuf, @StartIter, PChar(NewSelText), -1);
end;

class procedure TGtk2WSCustomMemo.SetText(const AWinControl: TWinControl; const AText: string);
var
  TextBuf: PGtkTextBuffer;
  StartIter: TGtkTextIter;
begin
  if not WSCheckHandleAllocated(AWinControl, 'SetText') then
    Exit;
  TextBuf := gtk_text_view_get_buffer(PGtkTextView(GetOrCreateWidgetInfo({%H-}Pointer(AWinControl.Handle))^.CoreWidget));
  gtk_text_buffer_set_text(TextBuf, PChar(AText), -1);
  gtk_text_buffer_get_start_iter(TextBuf, @StartIter);
  gtk_text_buffer_place_cursor(TextBuf, @StartIter);
end;

class procedure TGtk2WSCustomMemo.GetPreferredSize(
  const AWinControl: TWinControl; var PreferredWidth, PreferredHeight: integer;
  WithThemeSpace: Boolean);
begin
  GetGTKDefaultWidgetSize(AWinControl,PreferredWidth,PreferredHeight,
                          WithThemeSpace);
end;

class function TGtk2WSCustomMemo.GetSelStart(const ACustomEdit: TCustomEdit): integer;
var
  MemoStrings: TGtk2MemoStrings;
  TextView: PGtkTextView;
  TextBuffer: PGtkTextBuffer;
  TextMark: PGtkTextMark;
  TextIter: TGtkTextIter;
  StartIter, EndIter: TGtkTextIter;
  StartPos, EndPos: Integer;
begin
  Result := 0;
  if not WSCheckHandleAllocated(ACustomEdit, 'GetSelStart') then
    Exit;

  MemoStrings := TCustomMemo(ACustomEdit).Lines as TGtk2MemoStrings;
  Result := MemoStrings.QueueCursorMovePos;
  if Result > -1 then
    Exit;

  TextView := PGtkTextView(GetWidgetInfo({%H-}Pointer(ACustomEdit.Handle))^.CoreWidget);
  TextBuffer := gtk_text_view_get_buffer(TextView);
  TextMark := gtk_text_buffer_get_insert(TextBuffer);
  gtk_text_buffer_get_iter_at_mark(TextBuffer, @TextIter, TextMark);
  
  Result := gtk_text_iter_get_offset(@TextIter);
  if GetSelLength(ACustomEdit) = 0 then Exit;
  
  if not gtk_text_buffer_get_selection_bounds(TextBuffer, @StartIter, @EndIter) then Exit;
  
  StartPos := gtk_text_iter_get_offset(@StartIter);
  EndPos := gtk_text_iter_get_offset(@EndIter);
  
  Result := Min(StartPos, EndPos);
end;

class function TGtk2WSCustomMemo.GetSelLength(const ACustomEdit: TCustomEdit): integer;
var
  MemoStrings: TGtk2MemoStrings;
  TextView: PGtkTextView;
  TextBuffer: PGtkTextBuffer;
  StartIter, EndIter: TGtkTextIter;
begin
  Result := 0;
  if not WSCheckHandleAllocated(ACustomEdit, 'GetSelLength') then
    Exit;

  MemoStrings := TCustomMemo(ACustomEdit).Lines as TGtk2MemoStrings;
  Result := MemoStrings.QueueSelLength;

  if Result = -1 then
  begin
    Result := 0;
    TextView := PGtkTextView(GetWidgetInfo({%H-}Pointer(ACustomEdit.Handle))^.CoreWidget);
    TextBuffer := gtk_text_view_get_buffer(TextView);
    if not gtk_text_buffer_get_selection_bounds(TextBuffer, @StartIter, @EndIter) then Exit;

    Result := Abs(gtk_text_iter_get_offset(@EndIter) - gtk_text_iter_get_offset(@StartIter));
  end;
end;

class function TGtk2WSCustomMemo.GetCaretPos(const ACustomEdit: TCustomEdit): TPoint;
var
  TextView: PGtkTextView;
  TextBuffer: PGtkTextBuffer;
  Offset: Integer;
  TextIter: TGtkTextIter;
begin
  Result := Point(0, 0);
  if not WSCheckHandleAllocated(ACustomEdit, 'GetCaretPos') then
    Exit;
  TextView := PGtkTextView(GetWidgetInfo({%H-}Pointer(ACustomEdit.Handle))^.CoreWidget);
  TextBuffer := gtk_text_view_get_buffer(TextView);
  Offset := GetSelStart(ACustomEdit) + GetSelLength(ACustomEdit);
  gtk_text_buffer_get_iter_at_offset(TextBuffer, @TextIter, Offset);

  Result.X := gtk_text_iter_get_line_offset(@TextIter);
  Result.Y := gtk_text_iter_get_line(@TextIter);
end;

class procedure TGtk2WSCustomMemo.SetCaretPos(const ACustomEdit: TCustomEdit;
  const NewPos: TPoint);
var
  TextView: PGtkTextView;
  TextBuffer: PGtkTextBuffer;
  TextIter: TGtkTextIter;
begin
  if not WSCheckHandleAllocated(ACustomEdit, 'SetCaretPos') then
    Exit;
  TextView := PGtkTextView(GetWidgetInfo({%H-}Pointer(ACustomEdit.Handle))^.CoreWidget);
  TextBuffer := gtk_text_view_get_buffer(TextView);

  if (NewPos.X < 0) or (NewPos.Y < 0)
    then Exit;

{ this is quicker, but crashes if given invalid coords:
  gtk_text_buffer_get_iter_at_line_offset(TextBuffer, @TextIter, NewPos.Y, NewPos.X); }

  if (NewPos.Y >= gtk_text_buffer_get_line_count(TextBuffer))
    then Exit;
  gtk_text_buffer_get_iter_at_line(TextBuffer, @TextIter, NewPos.Y);
  if (NewPos.X >= gtk_text_iter_get_chars_in_line(@TextIter))
    then Exit;
  gtk_text_iter_set_line_offset(@TextIter, NewPos.X);

  SetSelStart(ACustomEdit, gtk_text_iter_get_offset(@TextIter));
end;
