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

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <wx/wx.h>
#include <wx/evtloop.h>
#include <wx/font.h>
#include <wx/gbsizer.h>
#include <wx/wrapsizer.h>
#include <wx/statbox.h>

// nvgt_plugin.h emits *definitions* of plugin_version() and the
// asGetLibraryVersion / asAcquireExclusiveLock / nvgt_wait / ... function
// pointer table whenever it is included without NVGT_PLUGIN_INCLUDE
// defined. With the plugin split across multiple translation units those
// definitions would land in every TU and the linker would reject them as
// LNK2005 duplicates. Force the extern-declarations form here. Exactly
// one TU (the entry point, wx.cpp) is responsible for emitting the
// definitions; it does so by including nvgt_plugin.h *before* common.h
// without the gate. The #pragma once guard inside nvgt_plugin.h then
// makes this re-include a no-op and the definitions stay in wx.cpp.
#ifndef NVGT_PLUGIN_INCLUDE
#define NVGT_PLUGIN_INCLUDE
#endif
#include "../../../src/nvgt_plugin.h"

// ---------------------------------------------------------------------------
// Value types exposed to AngelScript.
//
// wx_point / wx_size / wx_rect are aliased directly to wxPoint / wxSize /
// wxRect. The underlying wxWidgets types are standard-layout aggregates of
// int (with trivial constructors that zero-init or copy the members), so
// AngelScript can register them as value types whose property layout
// matches the C++ object. wxSize stores its members as `x`/`y` internally
// but the AS-visible members are exposed as `width`/`height` to match the
// wx documentation. This avoids allocation across the boundary and keeps
// the member offsets stable.
//
// wx_colour is NOT aliased to wxColour. wxColour holds platform-specific
// payload (e.g. an HBRUSH/HBITMAP cache on MSW), so its layout is not
// portable across builds and cannot be safely exposed as a fixed-property
// AS value type. Instead wx_colour is a custom POD of four uint8s, with
// to_wx / from_wx helpers used at the boundary.
// ---------------------------------------------------------------------------
using wx_point = wxPoint;
using wx_size  = wxSize;
using wx_rect  = wxRect;

struct wx_colour {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};
static_assert(sizeof(wx_colour) == 4, "wx_colour must be packed 4 bytes");

inline wxColour to_wx(const wx_colour& c) {
    return wxColour(c.r, c.g, c.b, c.a);
}
inline wx_colour from_wx(const wxColour& c) {
    wx_colour out;
    out.r = c.Red();
    out.g = c.Green();
    out.b = c.Blue();
    out.a = c.Alpha();
    return out;
}

// wxGBPosition / wxGBSpan are used by wxGridBagSizer to address cells in
// a (row, column) grid and to span a sizer item across multiple cells.
// The wx classes hold the values in *private* members (m_row/m_col,
// m_rowspan/m_colspan), so AngelScript cannot register them as a value
// type with direct member access via offsetof — the layout would not be
// stable and the members are inaccessible anyway. Mirror the wx_colour
// pattern: expose AS-side POD structs with public int members and convert
// at the boundary with to_wx / from_wx. The AS-side names use `row`/`col`
// (wx-style) instead of `x`/`y` because grids are conceptually a (row,
// column) coordinate system, not a pixel one.
struct wx_gb_position {
    int row;
    int col;
};
static_assert(sizeof(wx_gb_position) == 2 * sizeof(int),
              "wx_gb_position must be a packed pair of ints");

struct wx_gb_span {
    int rowspan;
    int colspan;
};
static_assert(sizeof(wx_gb_span) == 2 * sizeof(int),
              "wx_gb_span must be a packed pair of ints");

inline wxGBPosition to_wx(const wx_gb_position& p) {
    return wxGBPosition(p.row, p.col);
}
inline wx_gb_position from_wx(const wxGBPosition& p) {
    return wx_gb_position{p.GetRow(), p.GetCol()};
}
inline wxGBSpan to_wx(const wx_gb_span& s) {
    // wxGBSpan asserts rowspan/colspan > 0; default-construct and round
    // negatives/zeros up to 1 so a poorly-validated script value cannot
    // assert in wxWidgets. Scripts that want the sentinel "default span"
    // can pass wx_gb_span{1, 1} explicitly.
    int r = s.rowspan > 0 ? s.rowspan : 1;
    int c = s.colspan > 0 ? s.colspan : 1;
    return wxGBSpan(r, c);
}
inline wx_gb_span from_wx(const wxGBSpan& s) {
    return wx_gb_span{s.GetRowspan(), s.GetColspan()};
}

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

// CleanupSizerEntries: walks a wxSizer and its sub-sizers and removes
// every (wxSizer*, wxSizerItem*) entry the bridge made in g_ref_counts /
// g_event_handlers. Sizers and sizer items do not fire wxEVT_DESTROY of
// their own, so without this call their bridge entries become stale the
// moment wxWidgets destroys the underlying object — see runtime.cpp.
// Idempotent: erase-on-not-present is a no-op, and re-running on the
// same sizer is safe.
void CleanupSizerEntries(wxSizer* sizer);

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

// EnsureTracked<T>: like Track, but only binds the wxEVT_DESTROY hook the
// FIRST time we see this pointer. Use when returning a borrowed wxWindow
// pointer that may or may not have been created through one of the
// `wx.create_*` factories — e.g. wx_window_get_parent / get_grandparent,
// where the parent is usually script-created (and so already tracked) but
// could in principle be a wxWidgets-internal intermediate window. Without
// a destroy hook, an untracked window would leave a stale entry in
// g_ref_counts after wxWidgets destroyed it, and Release()'s `if (!parent)
// Destroy()` path could destroy a window the bridge does not own.
template<typename T>
T* EnsureTracked(T* obj) {
    if (!obj) return nullptr;
    if (g_ref_counts.find(obj) == g_ref_counts.end()) {
        wxWindow* win = dynamic_cast<wxWindow*>(obj);
        if (win) win->Bind(wxEVT_DESTROY, &OnObjectDestroyed);
    }
    AddRef(obj);
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

// Geometry / colour adapters. All take and return value types and are
// registered with the AngelScript "property" keyword so scripts can read
// `win.size`, write `win.position = wx_point(x, y)`, etc.
wx_colour wx_window_get_background_colour(wxWindow* self);
void wx_window_set_background_colour(wxWindow* self, const wx_colour& c);
wx_colour wx_window_get_foreground_colour(wxWindow* self);
void wx_window_set_foreground_colour(wxWindow* self, const wx_colour& c);
wx_point wx_window_get_position(wxWindow* self);
void wx_window_set_position(wxWindow* self, const wx_point& p);
wx_size wx_window_get_size(wxWindow* self);
void wx_window_set_size(wxWindow* self, const wx_size& s);
wx_size wx_window_get_client_size(wxWindow* self);
void wx_window_set_client_size(wxWindow* self, const wx_size& s);
wx_size wx_window_get_min_size(wxWindow* self);
void wx_window_set_min_size(wxWindow* self, const wx_size& s);
wx_size wx_window_get_max_size(wxWindow* self);
void wx_window_set_max_size(wxWindow* self, const wx_size& s);
wx_size wx_window_get_min_client_size(wxWindow* self);
void wx_window_set_min_client_size(wxWindow* self, const wx_size& s);
wx_size wx_window_get_max_client_size(wxWindow* self);
void wx_window_set_max_client_size(wxWindow* self, const wx_size& s);
wx_size wx_window_get_virtual_size(wxWindow* self);
void wx_window_set_virtual_size(wxWindow* self, const wx_size& s);
wx_size wx_window_get_window_border_size(wxWindow* self);
wx_size wx_window_get_dpi(wxWindow* self);
wx_size wx_window_get_best_size(wxWindow* self);
wx_point wx_window_get_screen_position(wxWindow* self);
wx_rect wx_window_get_rect(wxWindow* self);
wx_rect wx_window_get_screen_rect(wxWindow* self);
wx_rect wx_window_get_client_rect(wxWindow* self);
wx_point wx_window_client_to_screen(wxWindow* self, const wx_point& p);
wx_point wx_window_screen_to_client(wxWindow* self, const wx_point& p);
void wx_window_move(wxWindow* self, const wx_point& p);
void wx_window_set_size_rect(wxWindow* self, const wx_rect& r);
void wx_window_set_size_hints(wxWindow* self, const wx_size& min_size, const wx_size& max_size);
void wx_window_set_initial_size(wxWindow* self, const wx_size& s);
wx_size wx_window_get_text_extent(wxWindow* self, const std::string& text);

// String-property adapters. Note: label is registered only on
// wx_control derivatives (REG_CONTROL_METHODS) to avoid clashing with
// the wx_control-specific override that also syncs the window Name;
// see helpers.cpp.
std::string wx_window_get_name(wxWindow* self);
void wx_window_set_name(wxWindow* self, const std::string& name);
std::string wx_window_get_help_text(wxWindow* self);
void wx_window_set_help_text(wxWindow* self, const std::string& text);

// Sizer + paint adapters.
wxSizer* wx_window_get_sizer(wxWindow* self);
void wx_window_set_sizer(wxWindow* self, wxSizer* sizer);
void wx_window_refresh(wxWindow* self);

// Font property. wxFont is exposed to AS as a value type (`wx_font`) in
// register.cpp; the get/set wrappers are needed because wxWindow::SetFont
// returns bool (which would force the property setter to be void via the
// usual property-setter rule, but here we also want to discard the bool
// silently — the script use-case for "did the platform accept the font"
// is too narrow to be worth widening the API surface for).
wxFont wx_window_get_font(wxWindow* self);
void wx_window_set_font(wxWindow* self, const wxFont& font);

// wxWindow::ScrollWindow takes a `const wxRect* = nullptr` last parameter
// that is dereferenced unconditionally on MSW; pass nullptr explicitly.
void wx_window_scroll_window(wxWindow* self, int dx, int dy);

// Parent navigation. Wrappers run EnsureTracked so the returned wxWindow
// gains a wxEVT_DESTROY hook the first time it crosses the boundary,
// even if it was created by wxWidgets internally rather than by a
// `wx.create_*` factory.
wxWindow* wx_window_get_parent(wxWindow* self);
wxWindow* wx_window_get_grandparent(wxWindow* self);

// Window-style flag adapters. wxWindow uses `long` for the style flag
// and the extra-style flag; AngelScript exposes them as `int`. On
// Windows MSVC x64 `int` and `long` are both 32-bit so a direct
// asMETHOD binding happens to work, but `long` is 64-bit on Linux
// x86_64 / macOS x86_64 / aarch64 (LP64), where the C++ side would
// then read garbage from the upper word. Wrap with explicit casts
// so the AS-visible signature is portable. See AGENTS.md "Things to
// know before changing the bridge" for the full rule.
int wx_window_get_window_style(wxWindow* self);
void wx_window_set_window_style(wxWindow* self, int style);
int wx_window_get_extra_style(wxWindow* self);
void wx_window_set_extra_style(wxWindow* self, int style);

std::string wx_control_get_label(wxControl* self);
void wx_control_set_label(wxControl* self, const std::string& label);
std::string wx_control_get_label_text(wxControl* self);
void wx_control_set_label_text(wxControl* self, const std::string& text);
bool wx_control_set_label_markup(wxControl* self, const std::string& markup);
void wx_control_command(wxControl* self, wxCommandEvent* e);

std::string wx_tlw_get_title(wxTopLevelWindow* self);
void wx_tlw_set_title(wxTopLevelWindow* self, const std::string& title);
bool wx_tlw_show_full_screen(wxTopLevelWindow* self, bool show);

void wx_sizer_add_window(wxSizer* self, wxWindow* window, int proportion, int flag, int border);
void wx_sizer_add_sizer(wxSizer* self, wxSizer* sizer, int proportion, int flag, int border);
wxSizerItem* wx_sizer_add_spacer(wxSizer* self, int size);
wxSizerItem* wx_sizer_add_stretch_spacer(wxSizer* self, int prop);
bool wx_sizer_show_window(wxSizer* self, wxWindow* win, bool show, bool recursive);
bool wx_sizer_show_sizer(wxSizer* self, wxSizer* s, bool show, bool recursive);
bool wx_sizer_show_index(wxSizer* self, int index, bool show);
bool wx_sizer_hide_window(wxSizer* self, wxWindow* win, bool recursive);
bool wx_sizer_hide_sizer(wxSizer* self, wxSizer* s, bool recursive);
bool wx_sizer_hide_index(wxSizer* self, int index);
bool wx_sizer_is_shown_window(wxSizer* self, wxWindow* win);
bool wx_sizer_is_shown_sizer(wxSizer* self, wxSizer* s);
bool wx_sizer_is_shown_index(wxSizer* self, int index);
void wx_sizer_show_all(wxSizer* self, bool show);
void wx_sizer_show_items(wxSizer* self, bool show);
bool wx_sizer_are_any_items_shown(wxSizer* self);
bool wx_sizer_replace_window(wxSizer* self, wxWindow* oldwin, wxWindow* newwin, bool recursive);
bool wx_sizer_replace_sizer(wxSizer* self, wxSizer* oldsizer, wxSizer* newsizer, bool recursive);
bool wx_sizer_detach_index(wxSizer* self, int index);
bool wx_sizer_detach_window(wxSizer* self, wxWindow* win);
bool wx_sizer_detach_sizer(wxSizer* self, wxSizer* s);
// Note: wxSizer::Remove(int) is intentionally not exposed — since
// wxWidgets 3.0 it is functionally identical to Detach(int), and a
// duplicate `remove(int)` registration is just an API trap. Use
// `detach(int)` for index-based removal. Only `remove(wx_sizer@)`
// remains because it has distinct semantics from
// `detach(wx_sizer@)` (Remove deletes the sub-sizer; Detach does not).
bool wx_sizer_remove_sizer(wxSizer* self, wxSizer* s);
void wx_sizer_clear(wxSizer* self, bool delete_windows);
void wx_sizer_delete_windows(wxSizer* self);
void wx_sizer_insert_window(wxSizer* self, int index, wxWindow* win, int proportion, int flag, int border);
void wx_sizer_insert_sizer(wxSizer* self, int index, wxSizer* s, int proportion, int flag, int border);
wxSizerItem* wx_sizer_insert_spacer(wxSizer* self, int index, int size);
wxSizerItem* wx_sizer_insert_stretch_spacer(wxSizer* self, int index, int prop);
void wx_sizer_prepend_window(wxSizer* self, wxWindow* win, int proportion, int flag, int border);
void wx_sizer_prepend_sizer(wxSizer* self, wxSizer* s, int proportion, int flag, int border);
wxSizerItem* wx_sizer_prepend_spacer(wxSizer* self, int size);
wxSizerItem* wx_sizer_prepend_stretch_spacer(wxSizer* self, int prop);
wxSizerItem* wx_sizer_get_item_window(wxSizer* self, wxWindow* win);
wxSizerItem* wx_sizer_get_item_sz(wxSizer* self, wxSizer* s);
wxSizerItem* wx_sizer_get_item_index(wxSizer* self, int index);
int wx_sizer_find_window(wxSizer* self, wxWindow* win);
int wx_sizer_find_sizer(wxSizer* self, wxSizer* s);
wx_point wx_sizer_get_position(wxSizer* self);
wx_size wx_sizer_get_size(wxSizer* self);
wx_size wx_sizer_get_min_size(wxSizer* self);
void wx_sizer_set_min_size(wxSizer* self, const wx_size& s);
bool wx_sizer_set_item_min_size_window(wxSizer* self, wxWindow* win, const wx_size& s);
bool wx_sizer_set_item_min_size_sizer(wxSizer* self, wxSizer* sz, const wx_size& s);
bool wx_sizer_set_item_min_size_index(wxSizer* self, int index, const wx_size& s);
wx_size wx_sizer_compute_fitting_client_size(wxSizer* self, wxWindow* win);
wx_size wx_sizer_compute_fitting_window_size(wxSizer* self, wxWindow* win);
wx_size wx_sizer_fit(wxSizer* self, wxWindow* win);
void wx_sizer_fit_inside(wxSizer* self, wxWindow* win);
void wx_sizer_set_size_hints(wxSizer* self, wxWindow* win);

wxWindow* wx_sizer_item_get_window(wxSizerItem* self);
wxSizer* wx_sizer_item_get_sizer(wxSizerItem* self);
wx_size wx_sizer_item_get_size(wxSizerItem* self);
wx_size wx_sizer_item_calc_min(wxSizerItem* self);
wx_size wx_sizer_item_get_min_size(wxSizerItem* self);
wx_size wx_sizer_item_get_min_size_with_border(wxSizerItem* self);
wx_size wx_sizer_item_get_max_size(wxSizerItem* self);
wx_size wx_sizer_item_get_max_size_with_border(wxSizerItem* self);
void wx_sizer_item_set_min_size(wxSizerItem* self, const wx_size& s);
wx_rect wx_sizer_item_get_rect(wxSizerItem* self);
wx_point wx_sizer_item_get_position(wxSizerItem* self);
wx_size wx_sizer_item_get_spacer(wxSizerItem* self);
void wx_sizer_item_set_init_size(wxSizerItem* self, const wx_size& s);
void wx_sizer_item_set_ratio_size(wxSizerItem* self, const wx_size& s);
void wx_sizer_item_set_ratio_float(wxSizerItem* self, float ratio);
void wx_sizer_item_detach_window(wxSizerItem* self);
void wx_sizer_item_detach_sizer(wxSizerItem* self);
void wx_sizer_item_delete_windows(wxSizerItem* self);

// ---------------------------------------------------------------------------
// wxGridSizer / wxFlexGridSizer / wxGridBagSizer / wxWrapSizer /
// wxStaticBoxSizer adapters. Most accessors that take/return geometry
// or pure ints are bound directly via asMETHOD; the wrappers here cover
// signatures with wx-only types (wxGBPosition / wxGBSpan / wxFlexSizerGrowMode),
// the size_t -> int conversion for index-based getters and the few wx
// methods whose C++ defaults asMETHOD does not propagate.
// ---------------------------------------------------------------------------
int wx_grid_sizer_get_effective_cols_count(wxGridSizer* self);
int wx_grid_sizer_get_effective_rows_count(wxGridSizer* self);
void wx_grid_sizer_calc_rows_cols(wxGridSizer* self, int& rows, int& cols);

void wx_flex_grid_sizer_add_growable_row(wxFlexGridSizer* self, int idx, int proportion);
void wx_flex_grid_sizer_remove_growable_row(wxFlexGridSizer* self, int idx);
void wx_flex_grid_sizer_add_growable_col(wxFlexGridSizer* self, int idx, int proportion);
void wx_flex_grid_sizer_remove_growable_col(wxFlexGridSizer* self, int idx);
bool wx_flex_grid_sizer_is_row_growable(wxFlexGridSizer* self, int idx);
bool wx_flex_grid_sizer_is_col_growable(wxFlexGridSizer* self, int idx);
int wx_flex_grid_sizer_get_non_flexible_grow_mode(wxFlexGridSizer* self);
void wx_flex_grid_sizer_set_non_flexible_grow_mode(wxFlexGridSizer* self, int mode);

void wx_grid_bag_sizer_add_window(wxGridBagSizer* self, wxWindow* win,
                                  const wx_gb_position& pos, const wx_gb_span& span,
                                  int flag, int border);
void wx_grid_bag_sizer_add_sizer(wxGridBagSizer* self, wxSizer* sizer,
                                 const wx_gb_position& pos, const wx_gb_span& span,
                                 int flag, int border);
void wx_grid_bag_sizer_add_spacer(wxGridBagSizer* self, int width, int height,
                                  const wx_gb_position& pos, const wx_gb_span& span,
                                  int flag, int border);
wx_size wx_grid_bag_sizer_get_empty_cell_size(wxGridBagSizer* self);
void wx_grid_bag_sizer_set_empty_cell_size(wxGridBagSizer* self, const wx_size& s);
wx_size wx_grid_bag_sizer_get_cell_size(wxGridBagSizer* self, int row, int col);
wx_gb_position wx_grid_bag_sizer_get_item_position_window(wxGridBagSizer* self, wxWindow* win);
wx_gb_position wx_grid_bag_sizer_get_item_position_sizer(wxGridBagSizer* self, wxSizer* s);
wx_gb_position wx_grid_bag_sizer_get_item_position_index(wxGridBagSizer* self, int index);
bool wx_grid_bag_sizer_set_item_position_window(wxGridBagSizer* self, wxWindow* win, const wx_gb_position& pos);
bool wx_grid_bag_sizer_set_item_position_sizer(wxGridBagSizer* self, wxSizer* s, const wx_gb_position& pos);
bool wx_grid_bag_sizer_set_item_position_index(wxGridBagSizer* self, int index, const wx_gb_position& pos);
wx_gb_span wx_grid_bag_sizer_get_item_span_window(wxGridBagSizer* self, wxWindow* win);
wx_gb_span wx_grid_bag_sizer_get_item_span_sizer(wxGridBagSizer* self, wxSizer* s);
wx_gb_span wx_grid_bag_sizer_get_item_span_index(wxGridBagSizer* self, int index);
bool wx_grid_bag_sizer_set_item_span_window(wxGridBagSizer* self, wxWindow* win, const wx_gb_span& span);
bool wx_grid_bag_sizer_set_item_span_sizer(wxGridBagSizer* self, wxSizer* s, const wx_gb_span& span);
bool wx_grid_bag_sizer_set_item_span_index(wxGridBagSizer* self, int index, const wx_gb_span& span);
wxSizerItem* wx_grid_bag_sizer_find_item_window(wxGridBagSizer* self, wxWindow* win);
wxSizerItem* wx_grid_bag_sizer_find_item_sizer(wxGridBagSizer* self, wxSizer* s);
wxSizerItem* wx_grid_bag_sizer_find_item_at_position(wxGridBagSizer* self, const wx_gb_position& pos);
wxSizerItem* wx_grid_bag_sizer_find_item_at_point(wxGridBagSizer* self, const wx_point& pt);
bool wx_grid_bag_sizer_check_for_intersection_pos(wxGridBagSizer* self, const wx_gb_position& pos, const wx_gb_span& span);

// wxStaticBoxSizer: GetStaticBox returns a borrowed pointer to the static
// box owned by the sizer's parent window. EnsureTracked attaches a
// destroy hook the first time the bridge sees the box, so we don't end
// up with a stale entry after the window dies.
wxStaticBox* wx_static_box_sizer_get_static_box(wxStaticBoxSizer* self);

wxTextEntry* GetEntry(wxWindow* win);
std::string wx_text_entry_get_value(wxWindow* self);
void wx_text_entry_set_value(wxWindow* self, const std::string& value);
void wx_text_entry_change_value(wxWindow* self, const std::string& value);
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
bool wx_text_entry_has_selection(wxWindow* self);
void wx_text_entry_set_max_length(wxWindow* self, int len);
int wx_text_entry_get_insertion_point(wxWindow* self);
void wx_text_entry_set_insertion_point(wxWindow* self, int pos);
void wx_text_entry_set_insertion_point_end(wxWindow* self);
int wx_text_entry_get_last_position(wxWindow* self);
void wx_text_entry_force_upper(wxWindow* self);
std::string wx_text_entry_get_hint(wxWindow* self);
// wxTextEntry::SetHint returns bool to signal whether the platform
// accepted the hint, but AngelScript property setters must return
// `void` (otherwise either the engine refuses the registration or
// `entry.hint = "..."` silently discards the bool). Drop the bool
// here; if a script needs the rarely-useful platform-supported flag
// it can call wxTextEntry::SetHint through a future explicit binding.
void wx_text_entry_set_hint(wxWindow* self, const std::string& hint);
bool wx_text_entry_auto_complete_file_names(wxWindow* self);
bool wx_text_entry_auto_complete_directories(wxWindow* self);

wxRadioButton* wx_radio_button_get_first_in_group(wxRadioButton* self);
wxRadioButton* wx_radio_button_get_last_in_group(wxRadioButton* self);
wxRadioButton* wx_radio_button_get_next_in_group(wxRadioButton* self);
wxRadioButton* wx_radio_button_get_previous_in_group(wxRadioButton* self);

// ---------------------------------------------------------------------------
// wxFont adapters (helpers.cpp). wxFont is exposed as a value type
// (`wx_font`) on the AngelScript side; the C++ wxFont is internally
// reference-counted (wxGDIObject), so copy/destruct are cheap pointer
// operations and value-type semantics are appropriate.
//
// Property accessors covered here that need wrapping:
//   * `face_name`           — wxString <-> std::string conversion;
//   * `family`/`style`/`weight` — wx*Family/Style/Weight enums need to
//     be cast through int because AngelScript registers them as `int`
//     enums and forwarding a strongly-typed enum across the boundary
//     would force a templated trampoline per enum type;
//   * `pixel_size`          — returns wx_size by value.
//
// The modifier methods (Bold, Italic, …) are wrapped because the wx
// macro `wxDECLARE_COMMON_FONT_METHODS` declares them inside `wxFont`
// (after expansion), but they are inherited from `wxFontBase`. asMETHOD
// resolution prefers the most-derived declaration, which on at least
// MSVC produces an ambiguous overload set when the same name appears
// on both base and derived. Going through free wrappers avoids the
// ambiguity and gives us a single binding site.
// ---------------------------------------------------------------------------
std::string wx_font_get_face_name(const wxFont* self);
// SetFaceName returns bool to signal "platform recognised the font name";
// AngelScript property setters must return void, so the property form
// drops the bool. Scripts that need it can compare `font.face_name`
// against the requested string after assignment.
void wx_font_set_face_name(wxFont* self, const std::string& face);
int wx_font_get_family(const wxFont* self);
void wx_font_set_family(wxFont* self, int family);
int wx_font_get_style(const wxFont* self);
void wx_font_set_style(wxFont* self, int style);
int wx_font_get_weight(const wxFont* self);
void wx_font_set_weight(wxFont* self, int weight);
wx_size wx_font_get_pixel_size(const wxFont* self);
void wx_font_set_pixel_size(wxFont* self, const wx_size& size);

// Note: only the modifier methods that don't collide with a same-named
// property are exposed. wxFont's `Underlined()` and `Strikethrough()`
// return a fresh wxFont with the bool attribute toggled, but AS-side
// `font.underlined`/`font.strikethrough` are already exposed as
// settable properties — registering the modifier under the same name
// would either shadow the property or be ambiguous to the parser.
// Scripts can write `wx_font copy = font; copy.underlined = true;` to
// achieve the same result with one extra line.
wxFont wx_font_bold(const wxFont* self);
wxFont wx_font_italic(const wxFont* self);
wxFont wx_font_larger(const wxFont* self);
wxFont wx_font_smaller(const wxFont* self);
wxFont wx_font_scaled(const wxFont* self, float factor);
wxFont wx_font_get_base_font(const wxFont* self);

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
