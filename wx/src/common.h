// Shared types, globals and inline helpers used by every translation unit
// of the wx plugin. Concrete definitions live in:
//   - runtime.cpp  : g_context_pool, g_ref_counts, g_event_handlers,
//                    GetContextFromPool/ReturnContextToPool,
//                    AddRef/Release/CleanupHandlersFor,
//                    OnObjectDestroyed, OnWxEvent,
//                    wx_window_bind/wx_window_unbind.
//   - helpers.cpp  : every wx_* free-function adapter registered via
//                    asCALL_CDECL_OBJFIRST.
//   - manager.cpp  : NVGTApp, WxManager (constructor/destructor and
//                    create_* factories) plus WxConstructor/WxDestructor.
//   - register.cpp : register_all_types() — all enum, type and method
//                    registrations.
// wx.cpp keeps only the plugin_main() entry point.

#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <wx/wx.h>
#include <wx/evtloop.h>

#include "../../../src/nvgt_plugin.h"

// ---------------------------------------------------------------------------
// Script context pool (defined in runtime.cpp).
// ---------------------------------------------------------------------------
extern std::vector<asIScriptContext*> g_context_pool;
asIScriptContext* GetContextFromPool(asIScriptEngine* engine);
void ReturnContextToPool(asIScriptContext* ctx);

// RAII wrapper around the context pool.
class ContextGuard {
    asIScriptContext* ctx;
public:
    explicit ContextGuard(asIScriptEngine* eng) : ctx(GetContextFromPool(eng)) {}
    ~ContextGuard() { if (ctx) ReturnContextToPool(ctx); }
    asIScriptContext* get() { return ctx; }
};

// ---------------------------------------------------------------------------
// Reference counting for wx objects exposed to AngelScript (runtime.cpp).
// ---------------------------------------------------------------------------
extern std::map<void*, int> g_ref_counts;

void AddRef(void* ptr);
void Release(void* ptr);
void CleanupHandlersFor(void* ptr);

// ---------------------------------------------------------------------------
// Script event dispatch (runtime.cpp).
// ---------------------------------------------------------------------------
struct ScriptEventData {
    asIScriptFunction* callback;
    asIScriptEngine* engine;
};

typedef std::pair<void*, int> EventKey;
extern std::map<EventKey, ScriptEventData> g_event_handlers;

void OnObjectDestroyed(wxWindowDestroyEvent& event);
void OnWxEvent(wxEvent& event);

void wx_window_bind(wxWindow* self, int event_type, asIScriptFunction* callback);
void wx_window_unbind(wxWindow* self, int event_type);

// ---------------------------------------------------------------------------
// Casting helpers used by the registration macros.
// Templates must live in a header so each translation unit instantiates
// them on demand for whichever wx subclass it registers.
// ---------------------------------------------------------------------------
template<typename T> wxWindow* to_window(T* obj) { return static_cast<wxWindow*>(obj); }
template<typename T> wxControl* to_control(T* obj) { return static_cast<wxControl*>(obj); }
template<typename T> wxSizer* to_sizer(T* obj) { return static_cast<wxSizer*>(obj); }
template<typename T> wxTopLevelWindow* to_tlw(T* obj) { return static_cast<wxTopLevelWindow*>(obj); }
template<typename T> wxWindow* to_text_entry_as_win(T* obj) { return static_cast<wxWindow*>(obj); }

template<typename T>
T* window_to_derived(wxWindow* win) {
    if (!win) return nullptr;
    T* derived = dynamic_cast<T*>(win);
    if (derived) AddRef(derived);
    return derived;
}

template<typename T>
T* event_to_derived(wxEvent* e) {
    return dynamic_cast<T*>(e);
}

// Track<T>: AddRef the object and bind a wxEVT_DESTROY hook so the ref-count
// table is cleared when wxWidgets destroys the underlying object outside of
// our control. Only used by manager.cpp's create_* factories.
template<typename T>
T* Track(T* obj) {
    if (obj) {
        wxWindow* win = dynamic_cast<wxWindow*>(obj);
        if (win) win->Bind(wxEVT_DESTROY, &OnObjectDestroyed);
        AddRef(obj);
    }
    return obj;
}

// ---------------------------------------------------------------------------
// Free-function adapters (helpers.cpp). Declared here so register.cpp's
// macros can asFUNCTION() them without including helpers.cpp's bodies.
// ---------------------------------------------------------------------------
wxWindow* wx_event_get_event_object(wxEvent* self);

bool wx_key_event_control_down(wxKeyEvent* self);
bool wx_key_event_shift_down(wxKeyEvent* self);
bool wx_key_event_alt_down(wxKeyEvent* self);

int wx_mouse_event_get_x(wxMouseEvent* self);
int wx_mouse_event_get_y(wxMouseEvent* self);
bool wx_mouse_event_left_is_down(wxMouseEvent* self);
bool wx_mouse_event_middle_is_down(wxMouseEvent* self);
bool wx_mouse_event_right_is_down(wxMouseEvent* self);
bool wx_mouse_event_aux1_is_down(wxMouseEvent* self);
bool wx_mouse_event_aux2_is_down(wxMouseEvent* self);
bool wx_mouse_event_button(wxMouseEvent* self);
bool wx_mouse_event_button_down(wxMouseEvent* self);
bool wx_mouse_event_button_up(wxMouseEvent* self);
bool wx_mouse_event_button_dclick(wxMouseEvent* self);

std::string wx_command_event_get_string(wxCommandEvent* self);
void wx_command_event_set_string(wxCommandEvent* self, const std::string& s);
int wx_command_event_get_int(wxCommandEvent* self);
void wx_command_event_set_int(wxCommandEvent* self, int value);
int wx_command_event_get_extra_long(wxCommandEvent* self);
void wx_command_event_set_extra_long(wxCommandEvent* self, int value);

std::string wx_window_get_tool_tip(wxWindow* self);
void wx_window_set_tool_tip(wxWindow* self, const std::string& text);
void wx_window_get_background_colour(wxWindow* self, int& r, int& g, int& b);
void wx_window_set_background_colour(wxWindow* self, int r, int g, int b);
void wx_window_get_foreground_colour(wxWindow* self, int& r, int& g, int& b);
void wx_window_set_foreground_colour(wxWindow* self, int r, int g, int b);
void wx_window_get_position(wxWindow* self, int& x, int& y);
void wx_window_set_position(wxWindow* self, int x, int y);
void wx_window_get_size(wxWindow* self, int& out_w, int& out_h);
void wx_window_set_size(wxWindow* self, int w, int h);
wxSizer* wx_window_get_sizer(wxWindow* self);
void wx_window_set_sizer(wxWindow* self, wxSizer* sizer);
void wx_window_refresh(wxWindow* self);

std::string wx_control_get_label(wxControl* self);
void wx_control_set_label(wxControl* self, const std::string& label);

std::string wx_tlw_get_title(wxTopLevelWindow* self);
void wx_tlw_set_title(wxTopLevelWindow* self, const std::string& title);
bool wx_tlw_show_full_screen(wxTopLevelWindow* self, bool show);

void wx_sizer_add_window(wxSizer* self, wxWindow* window, int proportion, int flag, int border);
void wx_sizer_add_sizer(wxSizer* self, wxSizer* sizer, int proportion, int flag, int border);
bool wx_sizer_show_window(wxSizer* self, wxWindow* win, bool show, bool recursive);
bool wx_sizer_show_sizer(wxSizer* self, wxSizer* s, bool show, bool recursive);
bool wx_sizer_hide_window(wxSizer* self, wxWindow* win, bool recursive);
bool wx_sizer_hide_sizer(wxSizer* self, wxSizer* s, bool recursive);
bool wx_sizer_replace_window(wxSizer* self, wxWindow* oldwin, wxWindow* newwin, bool recursive);
bool wx_sizer_replace_sizer(wxSizer* self, wxSizer* oldsizer, wxSizer* newsizer, bool recursive);
bool wx_sizer_detach_index(wxSizer* self, int index);
bool wx_sizer_detach_window(wxSizer* self, wxWindow* win);
bool wx_sizer_detach_sizer(wxSizer* self, wxSizer* s);
void wx_sizer_insert_window(wxSizer* self, int index, wxWindow* win, int proportion, int flag, int border);
void wx_sizer_insert_sizer(wxSizer* self, int index, wxSizer* s, int proportion, int flag, int border);
void wx_sizer_prepend_window(wxSizer* self, wxWindow* win, int proportion, int flag, int border);
void wx_sizer_prepend_sizer(wxSizer* self, wxSizer* s, int proportion, int flag, int border);
wxSizerItem* wx_sizer_get_item_window(wxSizer* self, wxWindow* win);
wxSizerItem* wx_sizer_get_item_sz(wxSizer* self, wxSizer* s);
wxSizerItem* wx_sizer_get_item_index(wxSizer* self, int index);
int wx_sizer_find_window(wxSizer* self, wxWindow* win);
int wx_sizer_find_sizer(wxSizer* self, wxSizer* s);
void wx_sizer_get_position(wxSizer* self, int& x, int& y);
void wx_sizer_get_size(wxSizer* self, int& w, int& h);

wxWindow* wx_sizer_item_get_window(wxSizerItem* self);
wxSizer* wx_sizer_item_get_sizer(wxSizerItem* self);

wxTextEntry* GetEntry(wxWindow* win);
std::string wx_text_entry_get_value(wxWindow* self);
void wx_text_entry_set_value(wxWindow* self, const std::string& value);
void wx_text_entry_write_text(wxWindow* self, const std::string& text);
void wx_text_entry_append_text(wxWindow* self, const std::string& text);
void wx_text_entry_clear(wxWindow* self);
void wx_text_entry_copy(wxWindow* self);
void wx_text_entry_cut(wxWindow* self);
void wx_text_entry_paste(wxWindow* self);
void wx_text_entry_remove(wxWindow* self, int from, int to);
void wx_text_entry_undo(wxWindow* self);
void wx_text_entry_redo(wxWindow* self);
bool wx_text_entry_can_copy(wxWindow* self);
bool wx_text_entry_can_cut(wxWindow* self);
bool wx_text_entry_can_paste(wxWindow* self);
bool wx_text_entry_can_undo(wxWindow* self);
bool wx_text_entry_can_redo(wxWindow* self);
std::string wx_text_entry_get_range(wxWindow* self, int from, int to);
std::string wx_text_entry_get_string_selection(wxWindow* self);
void wx_text_entry_get_selection(wxWindow* self, int& from, int& to);
void wx_text_entry_set_selection(wxWindow* self, int from, int to);
bool wx_text_entry_is_editable(wxWindow* self);
void wx_text_entry_set_editable(wxWindow* self, bool editable);
bool wx_text_entry_is_empty(wxWindow* self);
void wx_text_entry_replace(wxWindow* self, int from, int to, const std::string& value);
void wx_text_entry_select_all(wxWindow* self);
void wx_text_entry_select_none(wxWindow* self);
void wx_text_entry_set_max_length(wxWindow* self, int len);

wxRadioButton* wx_radio_button_get_first_in_group(wxRadioButton* self);
wxRadioButton* wx_radio_button_get_last_in_group(wxRadioButton* self);
wxRadioButton* wx_radio_button_get_next_in_group(wxRadioButton* self);
wxRadioButton* wx_radio_button_get_previous_in_group(wxRadioButton* self);

// ---------------------------------------------------------------------------
// WxManager registration (manager.cpp). The class definition is private to
// manager.cpp because asMETHOD pointers to its methods only need to resolve
// inside that translation unit; register_all_types() simply calls the
// helper below at the right point in the bring-up sequence.
// ---------------------------------------------------------------------------
void register_wx_manager(asIScriptEngine* engine);

// ---------------------------------------------------------------------------
// Top-level registration entry point (register.cpp). Called from plugin_main().
// ---------------------------------------------------------------------------
void register_all_types(asIScriptEngine* engine);
