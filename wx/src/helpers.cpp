// Free-function adapters bridging wx C++ APIs to the AngelScript bindings.
// Each function here is registered through asFUNCTION/asCALL_CDECL_OBJFIRST
// in register.cpp. The adapters add null-checks, choose explicit defaults
// for arguments wxWidgets would otherwise leave to C++ defaults (asMETHOD
// does not propagate them), and convert between std::string and wxString.

#include "common.h"

// ---------------------------------------------------------------------------
// wxEvent base.
// ---------------------------------------------------------------------------
// Bridge-side handle for the window that fired the event. Most events
// originate from a script-created widget (already tracked through
// Track<T>), but a wxWidgets-internal sub-window can also surface here
// — e.g. a paint event on a control's child whose lifetime the bridge
// has not seen before. EnsureTracked binds the wxEVT_DESTROY hook on
// first sight so the entry cannot outlive the underlying object; on
// already-tracked windows it is just an AddRef.
wxWindow* wx_event_get_event_object(wxEvent* self) {
    if (!self) return nullptr;
    wxWindow* win = dynamic_cast<wxWindow*>(self->GetEventObject());
    return EnsureTracked(win);
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

// GetInt/SetInt/GetExtraLong/SetExtraLong are inherited by wxCommandEvent
// from wxEventBasicPayloadMixin via multiple inheritance. asMETHOD takes
// &wxCommandEvent::GetInt which yields a pointer-to-member-of-base, and
// MSVC then refuses to convert that to a pointer-to-member-of-wxCommandEvent
// because MI member-pointers have a different representation. Wrap as
// free functions to sidestep the conversion entirely.
//
// GetExtraLong/SetExtraLong upstream uses C++ `long`. On MSVC x64 long is
// 32-bit so int matches the ABI; on Linux x86_64 long is 64-bit and the
// raw asMETHOD binding would silently truncate. The wrappers cast through
// the native wx type so adding a non-Windows build later just needs to
// flip the AS signature to int64 (no other code change).
int wx_command_event_get_int(wxCommandEvent* self) {
    return self ? self->GetInt() : 0;
}

void wx_command_event_set_int(wxCommandEvent* self, int value) {
    if (self) self->SetInt(value);
}

int wx_command_event_get_extra_long(wxCommandEvent* self) {
    return self ? static_cast<int>(self->GetExtraLong()) : 0;
}

void wx_command_event_set_extra_long(wxCommandEvent* self, int value) {
    if (self) self->SetExtraLong(static_cast<long>(value));
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

// Geometry/colour adapters all use value types now. Each function returns
// a default-constructed value type when self is null so AS-side property
// reads remain safe across the bridge. wx_size and wx_point are aliased to
// wxSize / wxPoint so the conversion is a no-op; wx_colour goes through
// to_wx / from_wx.
wx_colour wx_window_get_background_colour(wxWindow* self) {
    return self ? from_wx(self->GetBackgroundColour()) : wx_colour{0, 0, 0, 255};
}

void wx_window_set_background_colour(wxWindow* self, const wx_colour& c) {
    if (self) {
        self->SetBackgroundColour(to_wx(c));
        self->Refresh();
    }
}

wx_colour wx_window_get_foreground_colour(wxWindow* self) {
    return self ? from_wx(self->GetForegroundColour()) : wx_colour{0, 0, 0, 255};
}

void wx_window_set_foreground_colour(wxWindow* self, const wx_colour& c) {
    if (self) {
        self->SetForegroundColour(to_wx(c));
        self->Refresh();
    }
}

wx_point wx_window_get_position(wxWindow* self) {
    return self ? self->GetPosition() : wxPoint(0, 0);
}

void wx_window_set_position(wxWindow* self, const wx_point& p) {
    if (self) self->SetPosition(p);
}

wx_size wx_window_get_size(wxWindow* self) {
    return self ? self->GetSize() : wxSize(0, 0);
}

void wx_window_set_size(wxWindow* self, const wx_size& s) {
    if (self) self->SetSize(s);
}

wx_size wx_window_get_client_size(wxWindow* self) {
    return self ? self->GetClientSize() : wxSize(0, 0);
}

void wx_window_set_client_size(wxWindow* self, const wx_size& s) {
    if (self) self->SetClientSize(s);
}

wx_size wx_window_get_min_size(wxWindow* self) {
    return self ? self->GetMinSize() : wxSize(-1, -1);
}

void wx_window_set_min_size(wxWindow* self, const wx_size& s) {
    if (self) self->SetMinSize(s);
}

wx_size wx_window_get_max_size(wxWindow* self) {
    return self ? self->GetMaxSize() : wxSize(-1, -1);
}

void wx_window_set_max_size(wxWindow* self, const wx_size& s) {
    if (self) self->SetMaxSize(s);
}

wx_size wx_window_get_min_client_size(wxWindow* self) {
    return self ? self->GetMinClientSize() : wxSize(-1, -1);
}

void wx_window_set_min_client_size(wxWindow* self, const wx_size& s) {
    if (self) self->SetMinClientSize(s);
}

wx_size wx_window_get_max_client_size(wxWindow* self) {
    return self ? self->GetMaxClientSize() : wxSize(-1, -1);
}

void wx_window_set_max_client_size(wxWindow* self, const wx_size& s) {
    if (self) self->SetMaxClientSize(s);
}

wx_size wx_window_get_virtual_size(wxWindow* self) {
    return self ? self->GetVirtualSize() : wxSize(0, 0);
}

void wx_window_set_virtual_size(wxWindow* self, const wx_size& s) {
    if (self) self->SetVirtualSize(s);
}

wx_size wx_window_get_window_border_size(wxWindow* self) {
    return self ? self->GetWindowBorderSize() : wxSize(0, 0);
}

wx_size wx_window_get_dpi(wxWindow* self) {
    return self ? self->GetDPI() : wxSize(96, 96);
}

wx_size wx_window_get_best_size(wxWindow* self) {
    return self ? self->GetBestSize() : wxSize(0, 0);
}

wx_point wx_window_get_screen_position(wxWindow* self) {
    return self ? self->GetScreenPosition() : wxPoint(0, 0);
}

wx_rect wx_window_get_rect(wxWindow* self) {
    return self ? self->GetRect() : wxRect(0, 0, 0, 0);
}

wx_rect wx_window_get_screen_rect(wxWindow* self) {
    return self ? self->GetScreenRect() : wxRect(0, 0, 0, 0);
}

wx_rect wx_window_get_client_rect(wxWindow* self) {
    return self ? self->GetClientRect() : wxRect(0, 0, 0, 0);
}

wx_point wx_window_client_to_screen(wxWindow* self, const wx_point& p) {
    return self ? self->ClientToScreen(p) : p;
}

wx_point wx_window_screen_to_client(wxWindow* self, const wx_point& p) {
    return self ? self->ScreenToClient(p) : p;
}

void wx_window_move(wxWindow* self, const wx_point& p) {
    if (self) self->Move(p);
}

void wx_window_set_size_rect(wxWindow* self, const wx_rect& r) {
    if (self) self->SetSize(r);
}

void wx_window_set_size_hints(wxWindow* self, const wx_size& min_size, const wx_size& max_size) {
    if (self) self->SetSizeHints(min_size, max_size);
}

void wx_window_set_initial_size(wxWindow* self, const wx_size& s) {
    if (self) self->SetInitialSize(s);
}

wx_size wx_window_get_text_extent(wxWindow* self, const std::string& text) {
    return self ? self->GetTextExtent(wxString::FromUTF8(text.c_str())) : wxSize(0, 0);
}

// Note: there is intentionally no wx_window_get_label / set_label here.
// wxControl overrides SetLabel and the script-side `label` property is
// registered only on wx_control derivatives (REG_CONTROL_METHODS), with
// the wx_control wrapper additionally syncing the window Name. Adding a
// wx_window-level binding would either collide with REG_CONTROL_METHODS
// (duplicate registration) or silently drop the SetName side effect.

std::string wx_window_get_name(wxWindow* self) {
    return self ? std::string(self->GetName().utf8_str()) : "";
}

void wx_window_set_name(wxWindow* self, const std::string& name) {
    if (self) self->SetName(wxString::FromUTF8(name.c_str()));
}

std::string wx_window_get_help_text(wxWindow* self) {
    return self ? std::string(self->GetHelpText().utf8_str()) : "";
}

void wx_window_set_help_text(wxWindow* self, const std::string& text) {
    if (self) self->SetHelpText(wxString::FromUTF8(text.c_str()));
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

// wxWindow::ScrollWindow takes a `const wxRect* = nullptr`. Same nullptr
// hazard as Refresh; wrap to pass it explicitly.
void wx_window_scroll_window(wxWindow* self, int dx, int dy) {
    if (self) self->ScrollWindow(dx, dy, nullptr);
}

wxWindow* wx_window_get_parent(wxWindow* self) {
    wxWindow* p = self ? self->GetParent() : nullptr;
    return EnsureTracked(p);
}

wxWindow* wx_window_get_grandparent(wxWindow* self) {
    wxWindow* p = self ? self->GetGrandParent() : nullptr;
    return EnsureTracked(p);
}

// wxWindow style flags. wxWidgets stores these as `long`; AngelScript
// only knows fixed-width int. asMETHOD-binding the wx accessors directly
// happens to work on Windows MSVC x64 because LLP64 makes int and long
// the same width there, but on LP64 platforms (Linux x86_64, macOS,
// most Unixes on aarch64) `long` is 64-bit and a plain int caller would
// truncate setters and read garbage from getters. Wrap explicitly.
int wx_window_get_window_style(wxWindow* self) {
    return self ? static_cast<int>(self->GetWindowStyleFlag()) : 0;
}

void wx_window_set_window_style(wxWindow* self, int style) {
    if (self) self->SetWindowStyleFlag(static_cast<long>(style));
}

int wx_window_get_extra_style(wxWindow* self) {
    return self ? static_cast<int>(self->GetExtraStyle()) : 0;
}

void wx_window_set_extra_style(wxWindow* self, int style) {
    if (self) self->SetExtraStyle(static_cast<long>(style));
}

// wxWindow::SetFont returns bool to signal whether the platform accepted
// the font; the wrapper drops it for the property-setter form (AS
// property setters must return void). Scripts that need the rarely-useful
// "did the platform like this font" answer can compare the script-side
// font with what `win.font` returns afterwards.
wxFont wx_window_get_font(wxWindow* self) {
    return self ? self->GetFont() : wxFont();
}

void wx_window_set_font(wxWindow* self, const wxFont& font) {
    if (self) self->SetFont(font);
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

// SetLabelText/GetLabelText differ from SetLabel/GetLabel: they treat any
// `&` in the input as a literal ampersand instead of a mnemonic marker.
// Useful when the script has an arbitrary user string it wants to display
// verbatim. We do not also touch SetName here, unlike SetLabel.
std::string wx_control_get_label_text(wxControl* self) {
    return self ? std::string(self->GetLabelText().utf8_str()) : "";
}

void wx_control_set_label_text(wxControl* self, const std::string& text) {
    if (self) self->SetLabelText(wxString::FromUTF8(text.c_str()));
}

bool wx_control_set_label_markup(wxControl* self, const std::string& markup) {
    return self ? self->SetLabelMarkup(wxString::FromUTF8(markup.c_str())) : false;
}

// wxControl::Command(wxCommandEvent&) takes the event by reference.
// asMETHOD cannot deduce a pointer-to-reference parameter automatically;
// the wrapper takes wx_command_event@ from script and dereferences.
void wx_control_command(wxControl* self, wxCommandEvent* e) {
    if (self && e) self->Command(*e);
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
// wxSizer. Add/Insert/Prepend overloads that take a window or sub-sizer
// do not return a wxSizerItem* on the AS side: scripts can recover the
// item with `get_item(...)` afterwards if they need it, and the more
// common idiom is `sizer.add(button, ...)` without juggling a sizer-item
// handle. Spacer overloads, in contrast, return the wxSizerItem so the
// script can tweak the proportion later; those wrappers AddRef the
// returned item so the wx_sizer_item bridge type (registered with the
// custom AddRef/Release) sees a balanced refcount. The item lives in
// the sizer's child list; CleanupSizerEntries strips it from
// g_ref_counts before the wxSizer destructor runs.
// ---------------------------------------------------------------------------
void wx_sizer_add_window(wxSizer* self, wxWindow* window, int proportion, int flag, int border) {
    if (self && window) self->Add(window, proportion, flag, border);
}

void wx_sizer_add_sizer(wxSizer* self, wxSizer* sizer, int proportion, int flag, int border) {
    if (self && sizer) self->Add(sizer, proportion, flag, border);
}

wxSizerItem* wx_sizer_add_spacer(wxSizer* self, int size) {
    wxSizerItem* item = self ? self->AddSpacer(size) : nullptr;
    if (item) AddRef(item);
    return item;
}

wxSizerItem* wx_sizer_add_stretch_spacer(wxSizer* self, int prop) {
    wxSizerItem* item = self ? self->AddStretchSpacer(prop) : nullptr;
    if (item) AddRef(item);
    return item;
}

bool wx_sizer_show_window(wxSizer* self, wxWindow* win, bool show, bool recursive) {
    return self ? self->Show(win, show, recursive) : false;
}

bool wx_sizer_show_sizer(wxSizer* self, wxSizer* s, bool show, bool recursive) {
    return self ? self->Show(s, show, recursive) : false;
}

bool wx_sizer_show_index(wxSizer* self, int index, bool show) {
    return self ? self->Show(static_cast<size_t>(index), show) : false;
}

bool wx_sizer_hide_window(wxSizer* self, wxWindow* win, bool recursive) {
    return self ? self->Hide(win, recursive) : false;
}

bool wx_sizer_hide_sizer(wxSizer* self, wxSizer* s, bool recursive) {
    return self ? self->Hide(s, recursive) : false;
}

bool wx_sizer_hide_index(wxSizer* self, int index) {
    return self ? self->Hide(static_cast<size_t>(index)) : false;
}

bool wx_sizer_is_shown_window(wxSizer* self, wxWindow* win) {
    return self ? self->IsShown(win) : false;
}

bool wx_sizer_is_shown_sizer(wxSizer* self, wxSizer* s) {
    return self ? self->IsShown(s) : false;
}

bool wx_sizer_is_shown_index(wxSizer* self, int index) {
    return self ? self->IsShown(static_cast<size_t>(index)) : false;
}

void wx_sizer_show_all(wxSizer* self, bool show) {
    if (self) self->Show(show);
}

void wx_sizer_show_items(wxSizer* self, bool show) {
    if (self) self->ShowItems(show);
}

bool wx_sizer_are_any_items_shown(wxSizer* self) {
    return self ? self->AreAnyItemsShown() : false;
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

bool wx_sizer_remove_sizer(wxSizer* self, wxSizer* s) {
    return self ? self->Remove(s) : false;
}

void wx_sizer_clear(wxSizer* self, bool delete_windows) {
    if (self) self->Clear(delete_windows);
}

void wx_sizer_delete_windows(wxSizer* self) {
    if (self) self->DeleteWindows();
}

void wx_sizer_insert_window(wxSizer* self, int index, wxWindow* win, int proportion, int flag, int border) {
    if (self && win) self->Insert(index, win, proportion, flag, border);
}

void wx_sizer_insert_sizer(wxSizer* self, int index, wxSizer* s, int proportion, int flag, int border) {
    if (self && s) self->Insert(index, s, proportion, flag, border);
}

wxSizerItem* wx_sizer_insert_spacer(wxSizer* self, int index, int size) {
    wxSizerItem* item = self ? self->InsertSpacer(index, size) : nullptr;
    if (item) AddRef(item);
    return item;
}

wxSizerItem* wx_sizer_insert_stretch_spacer(wxSizer* self, int index, int prop) {
    wxSizerItem* item = self ? self->InsertStretchSpacer(index, prop) : nullptr;
    if (item) AddRef(item);
    return item;
}

void wx_sizer_prepend_window(wxSizer* self, wxWindow* win, int proportion, int flag, int border) {
    if (self && win) self->Prepend(win, proportion, flag, border);
}

void wx_sizer_prepend_sizer(wxSizer* self, wxSizer* s, int proportion, int flag, int border) {
    if (self && s) self->Prepend(s, proportion, flag, border);
}

wxSizerItem* wx_sizer_prepend_spacer(wxSizer* self, int size) {
    wxSizerItem* item = self ? self->PrependSpacer(size) : nullptr;
    if (item) AddRef(item);
    return item;
}

wxSizerItem* wx_sizer_prepend_stretch_spacer(wxSizer* self, int prop) {
    wxSizerItem* item = self ? self->PrependStretchSpacer(prop) : nullptr;
    if (item) AddRef(item);
    return item;
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

wx_point wx_sizer_get_position(wxSizer* self) {
    return self ? self->GetPosition() : wxPoint(0, 0);
}

wx_size wx_sizer_get_size(wxSizer* self) {
    return self ? self->GetSize() : wxSize(0, 0);
}

wx_size wx_sizer_get_min_size(wxSizer* self) {
    return self ? self->GetMinSize() : wxSize(0, 0);
}

void wx_sizer_set_min_size(wxSizer* self, const wx_size& s) {
    if (self) self->SetMinSize(s);
}

bool wx_sizer_set_item_min_size_window(wxSizer* self, wxWindow* win, const wx_size& s) {
    return self ? self->SetItemMinSize(win, s) : false;
}

bool wx_sizer_set_item_min_size_sizer(wxSizer* self, wxSizer* sz, const wx_size& s) {
    return self ? self->SetItemMinSize(sz, s) : false;
}

bool wx_sizer_set_item_min_size_index(wxSizer* self, int index, const wx_size& s) {
    return self ? self->SetItemMinSize(static_cast<size_t>(index), s) : false;
}

wx_size wx_sizer_compute_fitting_client_size(wxSizer* self, wxWindow* win) {
    return (self && win) ? self->ComputeFittingClientSize(win) : wxSize(0, 0);
}

wx_size wx_sizer_compute_fitting_window_size(wxSizer* self, wxWindow* win) {
    return (self && win) ? self->ComputeFittingWindowSize(win) : wxSize(0, 0);
}

wx_size wx_sizer_fit(wxSizer* self, wxWindow* win) {
    return (self && win) ? self->Fit(win) : wxSize(0, 0);
}

void wx_sizer_fit_inside(wxSizer* self, wxWindow* win) {
    if (self && win) self->FitInside(win);
}

void wx_sizer_set_size_hints(wxSizer* self, wxWindow* win) {
    if (self && win) self->SetSizeHints(win);
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

wx_size wx_sizer_item_get_size(wxSizerItem* self) {
    return self ? self->GetSize() : wxSize(0, 0);
}

wx_size wx_sizer_item_calc_min(wxSizerItem* self) {
    return self ? self->CalcMin() : wxSize(0, 0);
}

wx_size wx_sizer_item_get_min_size(wxSizerItem* self) {
    return self ? self->GetMinSize() : wxSize(0, 0);
}

wx_size wx_sizer_item_get_min_size_with_border(wxSizerItem* self) {
    return self ? self->GetMinSizeWithBorder() : wxSize(0, 0);
}

wx_size wx_sizer_item_get_max_size(wxSizerItem* self) {
    return self ? self->GetMaxSize() : wxSize(-1, -1);
}

wx_size wx_sizer_item_get_max_size_with_border(wxSizerItem* self) {
    return self ? self->GetMaxSizeWithBorder() : wxSize(-1, -1);
}

void wx_sizer_item_set_min_size(wxSizerItem* self, const wx_size& s) {
    if (self) self->SetMinSize(s);
}

wx_rect wx_sizer_item_get_rect(wxSizerItem* self) {
    return self ? self->GetRect() : wxRect(0, 0, 0, 0);
}

wx_point wx_sizer_item_get_position(wxSizerItem* self) {
    return self ? self->GetPosition() : wxPoint(0, 0);
}

wx_size wx_sizer_item_get_spacer(wxSizerItem* self) {
    return self ? self->GetSpacer() : wxSize(0, 0);
}

void wx_sizer_item_set_init_size(wxSizerItem* self, const wx_size& s) {
    if (self) self->SetInitSize(s.x, s.y);
}

// SetRatio has three overloads in wxSizerItem: (int w, int h), (wxSize),
// (float). The float overload is the most useful from script (e.g. 16/9
// for a video preview), so we expose it under the no-suffix name and add
// the wxSize variant under _size.
void wx_sizer_item_set_ratio_size(wxSizerItem* self, const wx_size& s) {
    if (self) self->SetRatio(s);
}

void wx_sizer_item_set_ratio_float(wxSizerItem* self, float ratio) {
    if (self) self->SetRatio(ratio);
}

void wx_sizer_item_detach_window(wxSizerItem* self) {
    if (self) self->DetachWindow();
}

void wx_sizer_item_detach_sizer(wxSizerItem* self) {
    if (self) self->DetachSizer();
}

void wx_sizer_item_delete_windows(wxSizerItem* self) {
    if (self) self->DeleteWindows();
}

// ---------------------------------------------------------------------------
// wxGridSizer.
// ---------------------------------------------------------------------------
int wx_grid_sizer_get_effective_cols_count(wxGridSizer* self) {
    return self ? self->GetEffectiveColsCount() : 0;
}

int wx_grid_sizer_get_effective_rows_count(wxGridSizer* self) {
    return self ? self->GetEffectiveRowsCount() : 0;
}

void wx_grid_sizer_calc_rows_cols(wxGridSizer* self, int& rows, int& cols) {
    if (self) self->CalcRowsCols(rows, cols);
}

// ---------------------------------------------------------------------------
// wxFlexGridSizer. wxWidgets' AddGrowableRow/Col take size_t and a
// proportion that defaults to 0; the index-typed AS-side is `int`, so
// negative values are clamped at 0 to avoid implementation-defined
// signed-to-unsigned conversion. The non-flexible-grow-mode property
// is bound through wrappers because asMETHOD on the strongly-typed
// wxFlexSizerGrowMode enum forces a templated trampoline per enum,
// which we already dodged for wxFontFamily/Style/Weight in Phase 0.
// ---------------------------------------------------------------------------
void wx_flex_grid_sizer_add_growable_row(wxFlexGridSizer* self, int idx, int proportion) {
    if (self && idx >= 0) self->AddGrowableRow(static_cast<size_t>(idx), proportion);
}

void wx_flex_grid_sizer_remove_growable_row(wxFlexGridSizer* self, int idx) {
    if (self && idx >= 0) self->RemoveGrowableRow(static_cast<size_t>(idx));
}

void wx_flex_grid_sizer_add_growable_col(wxFlexGridSizer* self, int idx, int proportion) {
    if (self && idx >= 0) self->AddGrowableCol(static_cast<size_t>(idx), proportion);
}

void wx_flex_grid_sizer_remove_growable_col(wxFlexGridSizer* self, int idx) {
    if (self && idx >= 0) self->RemoveGrowableCol(static_cast<size_t>(idx));
}

bool wx_flex_grid_sizer_is_row_growable(wxFlexGridSizer* self, int idx) {
    return (self && idx >= 0) ? self->IsRowGrowable(static_cast<size_t>(idx)) : false;
}

bool wx_flex_grid_sizer_is_col_growable(wxFlexGridSizer* self, int idx) {
    return (self && idx >= 0) ? self->IsColGrowable(static_cast<size_t>(idx)) : false;
}

int wx_flex_grid_sizer_get_non_flexible_grow_mode(wxFlexGridSizer* self) {
    return self ? static_cast<int>(self->GetNonFlexibleGrowMode()) : 0;
}

void wx_flex_grid_sizer_set_non_flexible_grow_mode(wxFlexGridSizer* self, int mode) {
    if (self) self->SetNonFlexibleGrowMode(static_cast<wxFlexSizerGrowMode>(mode));
}

// ---------------------------------------------------------------------------
// wxGridBagSizer. The AS-side `add(...)` overloads do not return the
// wxSizerItem* — same convention as wxSizer's add wrappers, so a script
// can write `gb.add(button, wx_gb_position(0, 0));` without dealing with
// a sizer-item handle. Scripts that need the item back can call
// `find_item(button)` afterwards. wxObject* userData is intentionally
// not exposed on the AS side because the bridge has no mechanism for
// passing arbitrary script handles through wxObject.
// ---------------------------------------------------------------------------
void wx_grid_bag_sizer_add_window(wxGridBagSizer* self, wxWindow* win,
                                  const wx_gb_position& pos, const wx_gb_span& span,
                                  int flag, int border) {
    if (self && win) self->Add(win, to_wx(pos), to_wx(span), flag, border);
}

void wx_grid_bag_sizer_add_sizer(wxGridBagSizer* self, wxSizer* sizer,
                                 const wx_gb_position& pos, const wx_gb_span& span,
                                 int flag, int border) {
    if (self && sizer) self->Add(sizer, to_wx(pos), to_wx(span), flag, border);
}

void wx_grid_bag_sizer_add_spacer(wxGridBagSizer* self, int width, int height,
                                  const wx_gb_position& pos, const wx_gb_span& span,
                                  int flag, int border) {
    if (self) self->Add(width, height, to_wx(pos), to_wx(span), flag, border);
}

wx_size wx_grid_bag_sizer_get_empty_cell_size(wxGridBagSizer* self) {
    return self ? self->GetEmptyCellSize() : wxSize(0, 0);
}

void wx_grid_bag_sizer_set_empty_cell_size(wxGridBagSizer* self, const wx_size& s) {
    if (self) self->SetEmptyCellSize(s);
}

wx_size wx_grid_bag_sizer_get_cell_size(wxGridBagSizer* self, int row, int col) {
    return self ? self->GetCellSize(row, col) : wxSize(0, 0);
}

wx_gb_position wx_grid_bag_sizer_get_item_position_window(wxGridBagSizer* self, wxWindow* win) {
    return (self && win) ? from_wx(self->GetItemPosition(win)) : wx_gb_position{0, 0};
}

wx_gb_position wx_grid_bag_sizer_get_item_position_sizer(wxGridBagSizer* self, wxSizer* s) {
    return (self && s) ? from_wx(self->GetItemPosition(s)) : wx_gb_position{0, 0};
}

wx_gb_position wx_grid_bag_sizer_get_item_position_index(wxGridBagSizer* self, int index) {
    return (self && index >= 0)
        ? from_wx(self->GetItemPosition(static_cast<size_t>(index)))
        : wx_gb_position{0, 0};
}

bool wx_grid_bag_sizer_set_item_position_window(wxGridBagSizer* self, wxWindow* win,
                                                const wx_gb_position& pos) {
    return (self && win) ? self->SetItemPosition(win, to_wx(pos)) : false;
}

bool wx_grid_bag_sizer_set_item_position_sizer(wxGridBagSizer* self, wxSizer* s,
                                               const wx_gb_position& pos) {
    return (self && s) ? self->SetItemPosition(s, to_wx(pos)) : false;
}

bool wx_grid_bag_sizer_set_item_position_index(wxGridBagSizer* self, int index,
                                               const wx_gb_position& pos) {
    return (self && index >= 0)
        ? self->SetItemPosition(static_cast<size_t>(index), to_wx(pos))
        : false;
}

wx_gb_span wx_grid_bag_sizer_get_item_span_window(wxGridBagSizer* self, wxWindow* win) {
    return (self && win) ? from_wx(self->GetItemSpan(win)) : wx_gb_span{1, 1};
}

wx_gb_span wx_grid_bag_sizer_get_item_span_sizer(wxGridBagSizer* self, wxSizer* s) {
    return (self && s) ? from_wx(self->GetItemSpan(s)) : wx_gb_span{1, 1};
}

wx_gb_span wx_grid_bag_sizer_get_item_span_index(wxGridBagSizer* self, int index) {
    return (self && index >= 0)
        ? from_wx(self->GetItemSpan(static_cast<size_t>(index)))
        : wx_gb_span{1, 1};
}

bool wx_grid_bag_sizer_set_item_span_window(wxGridBagSizer* self, wxWindow* win,
                                            const wx_gb_span& span) {
    return (self && win) ? self->SetItemSpan(win, to_wx(span)) : false;
}

bool wx_grid_bag_sizer_set_item_span_sizer(wxGridBagSizer* self, wxSizer* s,
                                           const wx_gb_span& span) {
    return (self && s) ? self->SetItemSpan(s, to_wx(span)) : false;
}

bool wx_grid_bag_sizer_set_item_span_index(wxGridBagSizer* self, int index,
                                           const wx_gb_span& span) {
    return (self && index >= 0)
        ? self->SetItemSpan(static_cast<size_t>(index), to_wx(span))
        : false;
}

wxSizerItem* wx_grid_bag_sizer_find_item_window(wxGridBagSizer* self, wxWindow* win) {
    if (!self || !win) return nullptr;
    wxSizerItem* item = self->FindItem(win);
    if (item) AddRef(item);
    return item;
}

wxSizerItem* wx_grid_bag_sizer_find_item_sizer(wxGridBagSizer* self, wxSizer* s) {
    if (!self || !s) return nullptr;
    wxSizerItem* item = self->FindItem(s);
    if (item) AddRef(item);
    return item;
}

wxSizerItem* wx_grid_bag_sizer_find_item_at_position(wxGridBagSizer* self,
                                                    const wx_gb_position& pos) {
    if (!self) return nullptr;
    wxSizerItem* item = self->FindItemAtPosition(to_wx(pos));
    if (item) AddRef(item);
    return item;
}

wxSizerItem* wx_grid_bag_sizer_find_item_at_point(wxGridBagSizer* self, const wx_point& pt) {
    if (!self) return nullptr;
    wxSizerItem* item = self->FindItemAtPoint(pt);
    if (item) AddRef(item);
    return item;
}

bool wx_grid_bag_sizer_check_for_intersection_pos(wxGridBagSizer* self,
                                                  const wx_gb_position& pos,
                                                  const wx_gb_span& span) {
    return self ? self->CheckForIntersection(to_wx(pos), to_wx(span), nullptr) : false;
}

// ---------------------------------------------------------------------------
// wxStaticBoxSizer. The static box returned by GetStaticBox is owned by
// the parent window and lives for the duration of the static-box-sizer's
// life; EnsureTracked binds wxEVT_DESTROY on first sight so a stale entry
// in g_ref_counts cannot survive the parent's destruction.
// ---------------------------------------------------------------------------
wxStaticBox* wx_static_box_sizer_get_static_box(wxStaticBoxSizer* self) {
    if (!self) return nullptr;
    wxStaticBox* box = self->GetStaticBox();
    if (box) EnsureTracked(box);
    return box;
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

bool wx_text_entry_has_selection(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? e->HasSelection() : false;
}

void wx_text_entry_set_max_length(wxWindow* self, int len) {
    if (auto e = GetEntry(self)) e->SetMaxLength((unsigned long)len);
}

// ChangeValue is the silent counterpart of SetValue: no EVT_TEXT is fired.
// Useful when programmatically syncing a text field from a model without
// triggering the user-edit handler.
void wx_text_entry_change_value(wxWindow* self, const std::string& value) {
    if (auto e = GetEntry(self))
        e->ChangeValue(wxString::FromUTF8(value.c_str()));
}

int wx_text_entry_get_insertion_point(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? static_cast<int>(e->GetInsertionPoint()) : 0;
}

void wx_text_entry_set_insertion_point(wxWindow* self, int pos) {
    if (auto e = GetEntry(self)) e->SetInsertionPoint(static_cast<long>(pos));
}

void wx_text_entry_set_insertion_point_end(wxWindow* self) {
    if (auto e = GetEntry(self)) e->SetInsertionPointEnd();
}

int wx_text_entry_get_last_position(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? static_cast<int>(e->GetLastPosition()) : 0;
}

void wx_text_entry_force_upper(wxWindow* self) {
    if (auto e = GetEntry(self)) e->ForceUpper();
}

std::string wx_text_entry_get_hint(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? std::string(e->GetHint().utf8_str()) : "";
}

void wx_text_entry_set_hint(wxWindow* self, const std::string& hint) {
    if (auto e = GetEntry(self)) {
        // The bool result from wxTextEntry::SetHint is intentionally
        // discarded — see common.h for the rationale.
        e->SetHint(wxString::FromUTF8(hint.c_str()));
    }
}

bool wx_text_entry_auto_complete_file_names(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? e->AutoCompleteFileNames() : false;
}

bool wx_text_entry_auto_complete_directories(wxWindow* self) {
    auto e = GetEntry(self);
    return e ? e->AutoCompleteDirectories() : false;
}

// ---------------------------------------------------------------------------
// wxRadioButton.
// ---------------------------------------------------------------------------
// Sibling navigation on wxRadioButton walks a group of buttons whose
// individual lifetimes the bridge may not have seen yet — a script can
// reach a sibling that was assembled from a layout helper rather than
// produced by `wx.create_radio_button(...)`. Use EnsureTracked rather
// than bare AddRef so the wxEVT_DESTROY hook is wired the first time
// the bridge sees each sibling; otherwise an untracked button would
// leave a stale entry in g_ref_counts after wxWidgets destroys it, and
// the eventual Release() could call Destroy() on a window the bridge
// does not own. See AGENTS.md "Reference counting" for the full rule.
wxRadioButton* wx_radio_button_get_first_in_group(wxRadioButton* self) {
    wxRadioButton* rb = self ? self->GetFirstInGroup() : nullptr;
    return EnsureTracked(rb);
}

wxRadioButton* wx_radio_button_get_last_in_group(wxRadioButton* self) {
    wxRadioButton* rb = self ? self->GetLastInGroup() : nullptr;
    return EnsureTracked(rb);
}

wxRadioButton* wx_radio_button_get_next_in_group(wxRadioButton* self) {
    wxRadioButton* rb = self ? self->GetNextInGroup() : nullptr;
    return EnsureTracked(rb);
}

wxRadioButton* wx_radio_button_get_previous_in_group(wxRadioButton* self) {
    wxRadioButton* rb = self ? self->GetPreviousInGroup() : nullptr;
    return EnsureTracked(rb);
}

// ---------------------------------------------------------------------------
// wxFont. Exposed on the AngelScript side as a value type (`wx_font`).
// All wrappers null-check `self` so a default-constructed wx_font that
// failed to back itself with a real font (e.g. `wx_font(0, ...)`) does
// not crash the bridge. wxFont's accessors are declared in wxFontBase
// (the platform-agnostic base) and inherited by wxFont; calling them
// through the derived pointer is safe and matches the wx documented API.
// ---------------------------------------------------------------------------
std::string wx_font_get_face_name(const wxFont* self) {
    return self ? std::string(self->GetFaceName().utf8_str()) : "";
}

// wxFont::SetFaceName returns bool to signal whether the platform
// accepted the face name, but AS property setters must return void.
// Drop the bool on the bridge — scripts that care can compare
// `font.face_name` against the requested string after assignment.
void wx_font_set_face_name(wxFont* self, const std::string& face) {
    if (self) self->SetFaceName(wxString::FromUTF8(face.c_str()));
}

// wx*Family/Style/Weight are strongly-typed enums in C++; AngelScript
// registers them as plain `int` enums and passes them via int across the
// boundary. Cast through int explicitly so the AS signature stays
// portable across ABIs (the underlying enum type is implementation-
// defined; on some compilers it is `unsigned int`, on others it is
// derived from `int`).
int wx_font_get_family(const wxFont* self) {
    return self ? static_cast<int>(self->GetFamily()) : 0;
}

void wx_font_set_family(wxFont* self, int family) {
    if (self) self->SetFamily(static_cast<wxFontFamily>(family));
}

int wx_font_get_style(const wxFont* self) {
    return self ? static_cast<int>(self->GetStyle()) : 0;
}

void wx_font_set_style(wxFont* self, int style) {
    if (self) self->SetStyle(static_cast<wxFontStyle>(style));
}

int wx_font_get_weight(const wxFont* self) {
    return self ? static_cast<int>(self->GetWeight()) : 0;
}

void wx_font_set_weight(wxFont* self, int weight) {
    if (self) self->SetWeight(static_cast<wxFontWeight>(weight));
}

wx_size wx_font_get_pixel_size(const wxFont* self) {
    return self ? self->GetPixelSize() : wxSize(0, 0);
}

void wx_font_set_pixel_size(wxFont* self, const wx_size& size) {
    if (self) self->SetPixelSize(size);
}

// Modifier methods (`Bold`, `Italic`, …) come from the wxFontBase macro
// `wxDECLARE_COMMON_FONT_METHODS`, expanded inside wxFont. They return
// a fresh wxFont by value with the modification applied; because wxFont
// is internally reference-counted, the returned object is the size of a
// single pointer. Wrap in free functions for asMETHOD-binding clarity
// and to give us a stable C-callable address (asMETHOD on wx-macro-
// expanded names occasionally trips up MSVC's overload resolution).
wxFont wx_font_bold(const wxFont* self) {
    return self ? self->Bold() : wxFont();
}

wxFont wx_font_italic(const wxFont* self) {
    return self ? self->Italic() : wxFont();
}

wxFont wx_font_larger(const wxFont* self) {
    return self ? self->Larger() : wxFont();
}

wxFont wx_font_smaller(const wxFont* self) {
    return self ? self->Smaller() : wxFont();
}

wxFont wx_font_scaled(const wxFont* self, float factor) {
    return self ? self->Scaled(factor) : wxFont();
}

wxFont wx_font_get_base_font(const wxFont* self) {
    return self ? self->GetBaseFont() : wxFont();
}
