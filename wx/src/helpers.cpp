// Free-function adapters bridging wx C++ APIs to the AngelScript bindings.
// Each function here is registered through asFUNCTION/asCALL_CDECL_OBJFIRST
// in register.cpp. The adapters add null-checks, choose explicit defaults
// for arguments wxWidgets would otherwise leave to C++ defaults (asMETHOD
// does not propagate them), and convert between std::string and wxString.

#include "common.h"

// ---------------------------------------------------------------------------
// wxEvent base.
// ---------------------------------------------------------------------------
wxWindow* wx_event_get_event_object(wxEvent* self) {
    wxWindow* win = dynamic_cast<wxWindow*>(self->GetEventObject());
    if (win) AddRef(win);
    return win;
}

// ---------------------------------------------------------------------------
// wxKeyEvent.
// ---------------------------------------------------------------------------
bool wx_key_event_control_down(wxKeyEvent* self) {
    return self->ControlDown();
}

bool wx_key_event_shift_down(wxKeyEvent* self) {
    return self->ShiftDown();
}

bool wx_key_event_alt_down(wxKeyEvent* self) {
    return self->AltDown();
}

// ---------------------------------------------------------------------------
// wxMouseEvent.
// ---------------------------------------------------------------------------
int wx_mouse_event_get_x(wxMouseEvent* self) { return self->GetX(); }
int wx_mouse_event_get_y(wxMouseEvent* self) { return self->GetY(); }
bool wx_mouse_event_left_is_down(wxMouseEvent* self) { return self->LeftIsDown(); }
bool wx_mouse_event_middle_is_down(wxMouseEvent* self) { return self->MiddleIsDown(); }
bool wx_mouse_event_right_is_down(wxMouseEvent* self) { return self->RightIsDown(); }
bool wx_mouse_event_aux1_is_down(wxMouseEvent* self) { return self->Aux1IsDown(); }
bool wx_mouse_event_aux2_is_down(wxMouseEvent* self) { return self->Aux2IsDown(); }

// wxMouseEvent::Button{,Down,Up,DClick} take an int parameter. Button has
// no C++ default; the others default to wxMOUSE_BTN_ANY. asMETHOD does
// not propagate C++ defaults, so wrap them with explicit defaults so the
// AS-side zero-arg signatures don't read garbage.
bool wx_mouse_event_button(wxMouseEvent* self) {
    return self ? self->Button(wxMOUSE_BTN_ANY) : false;
}
bool wx_mouse_event_button_down(wxMouseEvent* self) {
    return self ? self->ButtonDown(wxMOUSE_BTN_ANY) : false;
}
bool wx_mouse_event_button_up(wxMouseEvent* self) {
    return self ? self->ButtonUp(wxMOUSE_BTN_ANY) : false;
}
bool wx_mouse_event_button_dclick(wxMouseEvent* self) {
    return self ? self->ButtonDClick(wxMOUSE_BTN_ANY) : false;
}

// ---------------------------------------------------------------------------
// wxCommandEvent. wxCommandEvent is fired by most controls (buttons,
// checkboxes, text controls, radio buttons, choices, list boxes, menus,
// sliders, ...). Scripts need cheap access to its payload.
// ---------------------------------------------------------------------------
std::string wx_command_event_get_string(wxCommandEvent* self) {
    return self ? std::string(self->GetString().utf8_str()) : "";
}

void wx_command_event_set_string(wxCommandEvent* self, const std::string& s) {
    if (self) self->SetString(wxString::FromUTF8(s.c_str()));
}

// ---------------------------------------------------------------------------
// wxWindow.
// ---------------------------------------------------------------------------
std::string wx_window_get_tool_tip(wxWindow* self) {
    return self ? std::string(self->GetToolTipText().utf8_str()) : "";
}

void wx_window_set_tool_tip(wxWindow* self, const std::string& text) {
    if (self) self->SetToolTip(wxString::FromUTF8(text.c_str()));
}

void wx_window_get_background_colour(wxWindow* self, int& r, int& g, int& b) {
    if (self) {
        wxColour c = self->GetBackgroundColour();
        r = c.Red(); g = c.Green(); b = c.Blue();
    }
}

void wx_window_set_background_colour(wxWindow* self, int r, int g, int b) {
    if (self) {
        self->SetBackgroundColour(wxColour(r, g, b));
        self->Refresh();
    }
}

void wx_window_get_foreground_colour(wxWindow* self, int& r, int& g, int& b) {
    if (self) {
        wxColour c = self->GetForegroundColour();
        r = c.Red(); g = c.Green(); b = c.Blue();
    }
}

void wx_window_set_foreground_colour(wxWindow* self, int r, int g, int b) {
    if (self) {
        self->SetForegroundColour(wxColour(r, g, b));
        self->Refresh();
    }
}

void wx_window_get_position(wxWindow* self, int& x, int& y) {
    if (self) { wxPoint p = self->GetPosition(); x = p.x; y = p.y; }
}

void wx_window_set_position(wxWindow* self, int x, int y) {
    if (self) self->SetPosition(wxPoint(x, y));
}

void wx_window_get_size(wxWindow* self, int& out_w, int& out_h) {
    if (self) self->GetSize(&out_w, &out_h);
}

void wx_window_set_size(wxWindow* self, int w, int h) {
    if (self) self->SetSize(w, h);
}

wxSizer* wx_window_get_sizer(wxWindow* self) {
    wxSizer* s = self ? self->GetSizer() : nullptr;
    if (s) AddRef(s);
    return s;
}

// Forwards to wxWindow::SetSizer with delete_old = false. The wx default is
// true, but the plugin manages sizer lifetimes via its own ref counter, so
// letting wxWidgets free a sizer underneath an outstanding AngelScript
// handle would cause a use-after-free. The script-side bridge will delete
// any orphaned sizer once its last handle is released.
void wx_window_set_sizer(wxWindow* self, wxSizer* sizer) {
    if (self) self->SetSizer(sizer, false);
}

// wxWindow::Refresh has signature
//   Refresh(bool eraseBackground = true, const wxRect* rect = nullptr).
// asMETHOD does not propagate the C++ defaults: when AngelScript calls a
// zero-arg refresh() the C++ side still reads `rect` from whatever happens
// to be in the parameter register, and on MSW the implementation
// dereferences it unconditionally. Wrap with explicit defaults so the
// AS-side call passes nullptr through.
void wx_window_refresh(wxWindow* self) {
    if (self) self->Refresh(true, nullptr);
}

// ---------------------------------------------------------------------------
// wxControl.
// ---------------------------------------------------------------------------
std::string wx_control_get_label(wxControl* self) {
    return self ? std::string(self->GetLabel().utf8_str()) : "";
}

void wx_control_set_label(wxControl* self, const std::string& label) {
    if (self) {
        wxString wx_s = wxString::FromUTF8(label.c_str());
        self->SetLabel(wx_s);
        self->SetName(wx_s);
    }
}

// ---------------------------------------------------------------------------
// wxTopLevelWindow.
// ---------------------------------------------------------------------------
std::string wx_tlw_get_title(wxTopLevelWindow* self) {
    return self ? std::string(self->GetTitle().utf8_str()) : "";
}

void wx_tlw_set_title(wxTopLevelWindow* self, const std::string& title) {
    if (self) self->SetTitle(wxString::FromUTF8(title.c_str()));
}

// Forwards to wxTopLevelWindow::ShowFullScreen with the wx default style
// wxFULLSCREEN_ALL. asMETHOD does not propagate C++ default arguments,
// so binding ShowFullScreen directly would let the second parameter
// read uninitialized stack/registers.
bool wx_tlw_show_full_screen(wxTopLevelWindow* self, bool show) {
    return self ? self->ShowFullScreen(show, wxFULLSCREEN_ALL) : false;
}

// ---------------------------------------------------------------------------
// wxSizer.
// ---------------------------------------------------------------------------
void wx_sizer_add_window(wxSizer* self, wxWindow* window, int proportion, int flag, int border) {
    if (self && window) self->Add(window, proportion, flag, border);
}

void wx_sizer_add_sizer(wxSizer* self, wxSizer* sizer, int proportion, int flag, int border) {
    if (self && sizer) self->Add(sizer, proportion, flag, border);
}

bool wx_sizer_show_window(wxSizer* self, wxWindow* win, bool show, bool recursive) {
    return self ? self->Show(win, show, recursive) : false;
}

bool wx_sizer_show_sizer(wxSizer* self, wxSizer* s, bool show, bool recursive) {
    return self ? self->Show(s, show, recursive) : false;
}

bool wx_sizer_hide_window(wxSizer* self, wxWindow* win, bool recursive) {
    return self ? self->Hide(win, recursive) : false;
}

bool wx_sizer_hide_sizer(wxSizer* self, wxSizer* s, bool recursive) {
    return self ? self->Hide(s, recursive) : false;
}

bool wx_sizer_replace_window(wxSizer* self, wxWindow* oldwin, wxWindow* newwin, bool recursive) {
    return self ? self->Replace(oldwin, newwin, recursive) : false;
}

bool wx_sizer_replace_sizer(wxSizer* self, wxSizer* oldsizer, wxSizer* newsizer, bool recursive) {
    return self ? self->Replace(oldsizer, newsizer, recursive) : false;
}

bool wx_sizer_detach_index(wxSizer* self, int index) {
    return self ? self->Detach(index) : false;
}

bool wx_sizer_detach_window(wxSizer* self, wxWindow* win) {
    return self ? self->Detach(win) : false;
}

bool wx_sizer_detach_sizer(wxSizer* self, wxSizer* s) {
    return self ? self->Detach(s) : false;
}

void wx_sizer_insert_window(wxSizer* self, int index, wxWindow* win, int proportion, int flag, int border) {
    if (self && win) self->Insert(index, win, proportion, flag, border);
}

void wx_sizer_insert_sizer(wxSizer* self, int index, wxSizer* s, int proportion, int flag, int border) {
    if (self && s) self->Insert(index, s, proportion, flag, border);
}

void wx_sizer_prepend_window(wxSizer* self, wxWindow* win, int proportion, int flag, int border) {
    if (self && win) self->Prepend(win, proportion, flag, border);
}

void wx_sizer_prepend_sizer(wxSizer* self, wxSizer* s, int proportion, int flag, int border) {
    if (self && s) self->Prepend(s, proportion, flag, border);
}

wxSizerItem* wx_sizer_get_item_window(wxSizer* self, wxWindow* win) {
    wxSizerItem* item = self ? self->GetItem(win) : nullptr;
    if (item) AddRef(item);
    return item;
}

wxSizerItem* wx_sizer_get_item_sz(wxSizer* self, wxSizer* s) {
    wxSizerItem* item = self ? self->GetItem(s) : nullptr;
    if (item) AddRef(item);
    return item;
}

wxSizerItem* wx_sizer_get_item_index(wxSizer* self, int index) {
    wxSizerItem* item = self ? self->GetItem(index) : nullptr;
    if (item) AddRef(item);
    return item;
}

int wx_sizer_find_window(wxSizer* self, wxWindow* win) {
    if (!self || !win) return -1;
    int count = self->GetItemCount();
    for (int i = 0; i < count; ++i) {
        wxSizerItem* item = self->GetItem(i);
        if (item && item->GetWindow() == win) return i;
    }
    return -1;
}

int wx_sizer_find_sizer(wxSizer* self, wxSizer* s) {
    if (!self || !s) return -1;
    int count = self->GetItemCount();
    for (int i = 0; i < count; ++i) {
        wxSizerItem* item = self->GetItem(i);
        if (item && item->GetSizer() == s) return i;
    }
    return -1;
}

void wx_sizer_get_position(wxSizer* self, int& x, int& y) {
    if (self) { wxPoint p = self->GetPosition(); x = p.x; y = p.y; }
}

void wx_sizer_get_size(wxSizer* self, int& w, int& h) {
    if (self) { wxSize s = self->GetSize(); w = s.x; h = s.y; }
}

// ---------------------------------------------------------------------------
// wxSizerItem.
// ---------------------------------------------------------------------------
wxWindow* wx_sizer_item_get_window(wxSizerItem* self) {
    wxWindow* w = self ? self->GetWindow() : nullptr;
    if (w) AddRef(w);
    return w;
}

wxSizer* wx_sizer_item_get_sizer(wxSizerItem* self) {
    wxSizer* s = self ? self->GetSizer() : nullptr;
    if (s) AddRef(s);
    return s;
}

// ---------------------------------------------------------------------------
// wxTextEntry. The control side of wxTextEntry is a mix-in, so the wrappers
// take a wxWindow* and dynamic_cast it to wxTextEntry. This lets the same
// REG_TEXT_ENTRY_METHODS macro register the API on every concrete type
// that derives from both wxWindow and wxTextEntry.
// ---------------------------------------------------------------------------
wxTextEntry* GetEntry(wxWindow* win) {
    return dynamic_cast<wxTextEntry*>(win);
}

std::string wx_text_entry_get_value(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? std::string(e->GetValue().utf8_str()) : "";
}

void wx_text_entry_set_value(wxWindow* self, const std::string& value) {
    if (auto e = GetEntry(self))
        e->SetValue(wxString::FromUTF8(value.c_str()));
}

void wx_text_entry_write_text(wxWindow* self, const std::string& text) {
    if (auto e = GetEntry(self))
        e->WriteText(wxString::FromUTF8(text.c_str()));
}

void wx_text_entry_append_text(wxWindow* self, const std::string& text) {
    if (auto e = GetEntry(self))
        e->AppendText(wxString::FromUTF8(text.c_str()));
}

void wx_text_entry_clear(wxWindow* self) {
    if (auto e = GetEntry(self))
        e->Clear();
}
void wx_text_entry_copy(wxWindow* self) {
    if (auto e = GetEntry(self)) e->Copy();
}

void wx_text_entry_cut(wxWindow* self) {
    if (auto e = GetEntry(self)) e->Cut();
}

void wx_text_entry_paste(wxWindow* self) {
    if (auto e = GetEntry(self)) e->Paste();
}

void wx_text_entry_remove(wxWindow* self, int from, int to) {
    if (auto e = GetEntry(self)) e->Remove((long)from, (long)to);
}

void wx_text_entry_undo(wxWindow* self) {
    if (auto e = GetEntry(self)) e->Undo();
}

void wx_text_entry_redo(wxWindow* self) {
    if (auto e = GetEntry(self)) e->Redo();
}

bool wx_text_entry_can_copy(wxWindow* self) { auto e = GetEntry(self); return e ? e->CanCopy() : false; }
bool wx_text_entry_can_cut(wxWindow* self) { auto e = GetEntry(self); return e ? e->CanCut() : false; }
bool wx_text_entry_can_paste(wxWindow* self) { auto e = GetEntry(self); return e ? e->CanPaste() : false; }
bool wx_text_entry_can_undo(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? e->CanUndo() : false;
}

bool wx_text_entry_can_redo(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? e->CanRedo() : false;
}

std::string wx_text_entry_get_range(wxWindow* self, int from, int to) {
    auto e = GetEntry(self);
    return e ? std::string(e->GetRange((long)from, (long)to).utf8_str()) : "";
}

std::string wx_text_entry_get_string_selection(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? std::string(e->GetStringSelection().utf8_str()) : "";
}

void wx_text_entry_get_selection(wxWindow* self, int& from, int& to) {
    if (auto e = GetEntry(self)) {
        long f, t;
        e->GetSelection(&f, &t);
        from = (int)f;
        to = (int)t;
    }
}

void wx_text_entry_set_selection(wxWindow* self, int from, int to) {
    if (auto e = GetEntry(self)) e->SetSelection((long)from, (long)to);
}

bool wx_text_entry_is_editable(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? e->IsEditable() : false;
}

void wx_text_entry_set_editable(wxWindow* self, bool editable) {
    if (auto e = GetEntry(self)) e->SetEditable(editable);
}

bool wx_text_entry_is_empty(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? e->IsEmpty() : false;
}

void wx_text_entry_replace(wxWindow* self, int from, int to, const std::string& value) {
    if (auto e = GetEntry(self))
        e->Replace((long)from, (long)to, wxString::FromUTF8(value.c_str()));
}

void wx_text_entry_select_all(wxWindow* self) {
    if (auto e = GetEntry(self)) e->SelectAll();
}

void wx_text_entry_select_none(wxWindow* self) {
    if (auto e = GetEntry(self)) e->SelectNone();
}

void wx_text_entry_set_max_length(wxWindow* self, int len) {
    if (auto e = GetEntry(self)) e->SetMaxLength((unsigned long)len);
}

// ---------------------------------------------------------------------------
// wxRadioButton.
// ---------------------------------------------------------------------------
wxRadioButton* wx_radio_button_get_first_in_group(wxRadioButton* self) {
    wxRadioButton* rb = self ? self->GetFirstInGroup() : nullptr;
    if (rb) AddRef(rb);
    return rb;
}

wxRadioButton* wx_radio_button_get_last_in_group(wxRadioButton* self) {
    wxRadioButton* rb = self ? self->GetLastInGroup() : nullptr;
    if (rb) AddRef(rb);
    return rb;
}

wxRadioButton* wx_radio_button_get_next_in_group(wxRadioButton* self) {
    wxRadioButton* rb = self ? self->GetNextInGroup() : nullptr;
    if (rb) AddRef(rb);
    return rb;
}

wxRadioButton* wx_radio_button_get_previous_in_group(wxRadioButton* self) {
    wxRadioButton* rb = self ? self->GetPreviousInGroup() : nullptr;
    if (rb) AddRef(rb);
    return rb;
}
