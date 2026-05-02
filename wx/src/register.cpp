// AngelScript registration: every enum, type, method and behaviour exposed
// to scripts is registered here. The registration macros wrap the
// boilerplate for declaring a wx-derived ref type plus its base-class
// implicit-cast method, and reuse the per-base-class REG_* method blocks
// so the same set of methods is consistent across every concrete control.
//
// register_wx_manager() (manager.cpp) handles the value-type `wx` itself.

#include "common.h"

#include <string>

namespace {

// ---------------------------------------------------------------------------
// Value-type constructors. AngelScript needs a CONSTRUCT behaviour for any
// non-default constructor we expose. The default constructors are also
// registered explicitly so AS-side `wx_point p;` resolves consistently
// across compilers (asGetTypeTraits would mark wxPoint/wxSize/wxRect as
// asOBJ_APP_CLASS_C, which means construct/destruct must be registered).
// ---------------------------------------------------------------------------
void wx_point_default_ctor(void* mem) { new (mem) wxPoint(); }
void wx_point_xy_ctor(int x, int y, void* mem) { new (mem) wxPoint(x, y); }
void wx_point_copy_ctor(const wxPoint& other, void* mem) { new (mem) wxPoint(other); }
void wx_point_dtor(void* mem) { static_cast<wxPoint*>(mem)->~wxPoint(); }

void wx_size_default_ctor(void* mem) { new (mem) wxSize(); }
void wx_size_wh_ctor(int w, int h, void* mem) { new (mem) wxSize(w, h); }
void wx_size_copy_ctor(const wxSize& other, void* mem) { new (mem) wxSize(other); }
void wx_size_dtor(void* mem) { static_cast<wxSize*>(mem)->~wxSize(); }

void wx_rect_default_ctor(void* mem) { new (mem) wxRect(); }
void wx_rect_xywh_ctor(int x, int y, int w, int h, void* mem) { new (mem) wxRect(x, y, w, h); }
void wx_rect_copy_ctor(const wxRect& other, void* mem) { new (mem) wxRect(other); }
void wx_rect_dtor(void* mem) { static_cast<wxRect*>(mem)->~wxRect(); }

void wx_colour_default_ctor(void* mem) {
    auto* p = static_cast<wx_colour*>(mem);
    p->r = 0; p->g = 0; p->b = 0; p->a = 255;
}
void wx_colour_rgb_ctor(unsigned char r, unsigned char g, unsigned char b, void* mem) {
    auto* p = static_cast<wx_colour*>(mem);
    p->r = r; p->g = g; p->b = b; p->a = 255;
}
void wx_colour_rgba_ctor(unsigned char r, unsigned char g, unsigned char b, unsigned char a, void* mem) {
    auto* p = static_cast<wx_colour*>(mem);
    p->r = r; p->g = g; p->b = b; p->a = a;
}
void wx_colour_copy_ctor(const wx_colour& other, void* mem) {
    new (mem) wx_colour(other);
}
void wx_colour_dtor(void* mem) { static_cast<wx_colour*>(mem)->~wx_colour(); }

void register_value_types(asIScriptEngine* engine) {
    // wx_point: constructor is the only non-trivial special member.
    engine->RegisterObjectType("wx_point", sizeof(wxPoint),
        asOBJ_VALUE | asOBJ_APP_CLASS_CK | asOBJ_APP_CLASS_ALLINTS);
    engine->RegisterObjectBehaviour("wx_point", asBEHAVE_CONSTRUCT, "void f()",
        asFUNCTION(wx_point_default_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_point", asBEHAVE_CONSTRUCT, "void f(int x, int y)",
        asFUNCTION(wx_point_xy_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_point", asBEHAVE_CONSTRUCT, "void f(const wx_point &in)",
        asFUNCTION(wx_point_copy_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_point", asBEHAVE_DESTRUCT, "void f()",
        asFUNCTION(wx_point_dtor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectMethod("wx_point", "wx_point &opAssign(const wx_point &in)",
        asMETHODPR(wxPoint, operator=, (const wxPoint&), wxPoint&), asCALL_THISCALL);
    engine->RegisterObjectProperty("wx_point", "int x", offsetof(wxPoint, x));
    engine->RegisterObjectProperty("wx_point", "int y", offsetof(wxPoint, y));

    // wx_size: AS-visible properties are width/height while the underlying
    // wxSize stores them as x/y. Map AS names to the C++ offsets so the
    // value type is properties-by-position correct.
    engine->RegisterObjectType("wx_size", sizeof(wxSize),
        asOBJ_VALUE | asOBJ_APP_CLASS_CK | asOBJ_APP_CLASS_ALLINTS);
    engine->RegisterObjectBehaviour("wx_size", asBEHAVE_CONSTRUCT, "void f()",
        asFUNCTION(wx_size_default_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_size", asBEHAVE_CONSTRUCT, "void f(int width, int height)",
        asFUNCTION(wx_size_wh_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_size", asBEHAVE_CONSTRUCT, "void f(const wx_size &in)",
        asFUNCTION(wx_size_copy_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_size", asBEHAVE_DESTRUCT, "void f()",
        asFUNCTION(wx_size_dtor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectMethod("wx_size", "wx_size &opAssign(const wx_size &in)",
        asMETHODPR(wxSize, operator=, (const wxSize&), wxSize&), asCALL_THISCALL);
    // wxSize stores members as x/y internally, but the AS-visible API uses
    // width/height to match the wx documentation.
    engine->RegisterObjectProperty("wx_size", "int width", offsetof(wxSize, x));
    engine->RegisterObjectProperty("wx_size", "int height", offsetof(wxSize, y));

    engine->RegisterObjectType("wx_rect", sizeof(wxRect),
        asOBJ_VALUE | asOBJ_APP_CLASS_CK | asOBJ_APP_CLASS_ALLINTS);
    engine->RegisterObjectBehaviour("wx_rect", asBEHAVE_CONSTRUCT, "void f()",
        asFUNCTION(wx_rect_default_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_rect", asBEHAVE_CONSTRUCT, "void f(int x, int y, int width, int height)",
        asFUNCTION(wx_rect_xywh_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_rect", asBEHAVE_CONSTRUCT, "void f(const wx_rect &in)",
        asFUNCTION(wx_rect_copy_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_rect", asBEHAVE_DESTRUCT, "void f()",
        asFUNCTION(wx_rect_dtor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectMethod("wx_rect", "wx_rect &opAssign(const wx_rect &in)",
        asMETHODPR(wxRect, operator=, (const wxRect&), wxRect&), asCALL_THISCALL);
    engine->RegisterObjectProperty("wx_rect", "int x", offsetof(wxRect, x));
    engine->RegisterObjectProperty("wx_rect", "int y", offsetof(wxRect, y));
    engine->RegisterObjectProperty("wx_rect", "int width", offsetof(wxRect, width));
    engine->RegisterObjectProperty("wx_rect", "int height", offsetof(wxRect, height));

    // wx_colour: custom POD; default alpha is 255 in the no-alpha ctor to
    // match wx defaults.
    engine->RegisterObjectType("wx_colour", sizeof(wx_colour),
        asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CK | asOBJ_APP_CLASS_ALLINTS);
    engine->RegisterObjectBehaviour("wx_colour", asBEHAVE_CONSTRUCT, "void f()",
        asFUNCTION(wx_colour_default_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_colour", asBEHAVE_CONSTRUCT, "void f(uint8 r, uint8 g, uint8 b)",
        asFUNCTION(wx_colour_rgb_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_colour", asBEHAVE_CONSTRUCT, "void f(uint8 r, uint8 g, uint8 b, uint8 a)",
        asFUNCTION(wx_colour_rgba_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_colour", asBEHAVE_CONSTRUCT, "void f(const wx_colour &in)",
        asFUNCTION(wx_colour_copy_ctor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx_colour", asBEHAVE_DESTRUCT, "void f()",
        asFUNCTION(wx_colour_dtor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectProperty("wx_colour", "uint8 r", offsetof(wx_colour, r));
    engine->RegisterObjectProperty("wx_colour", "uint8 g", offsetof(wx_colour, g));
    engine->RegisterObjectProperty("wx_colour", "uint8 b", offsetof(wx_colour, b));
    engine->RegisterObjectProperty("wx_colour", "uint8 a", offsetof(wx_colour, a));
}

// ---------------------------------------------------------------------------
// REG_BASE_REF — AddRef/Release behaviours, applied to every wx ref type.
// ---------------------------------------------------------------------------
#define REG_BASE_REF(name) \
    engine->RegisterObjectBehaviour(name, asBEHAVE_ADDREF, "void f()", asFUNCTION(AddRef), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectBehaviour(name, asBEHAVE_RELEASE, "void f()", asFUNCTION(Release), asCALL_CDECL_OBJFIRST);

// ---------------------------------------------------------------------------
// REG_WINDOW_METHODS — wxWindow surface, applied to every wx_window-derived
// type.
// ---------------------------------------------------------------------------
#define REG_WINDOW_METHODS(name) \
    engine->RegisterObjectMethod(name, "bool show(bool visible = true)", asMETHOD(wxWindow, Show), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool hide()", asMETHOD(wxWindow, Hide), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool enable(bool enable = true)", asMETHOD(wxWindow, Enable), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool disable()", asMETHOD(wxWindow, Disable), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool has_focus()", asMETHOD(wxWindow, HasFocus), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void set_focus()", asMETHOD(wxWindow, SetFocus), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void layout()", asMETHOD(wxWindow, Layout), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void center(int direction = 3)", asMETHOD(wxWindow, Center), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "int get_id() const property", asMETHOD(wxWindow, GetId), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void refresh()", asFUNCTION(wx_window_refresh), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void update()", asMETHOD(wxWindow, Update), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool destroy()", asMETHOD(wxWindow, Destroy), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool close(bool force = false)", asMETHOD(wxWindow, Close), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void bind(wx_event_type, wx_callback@)", asFUNCTION(wx_window_bind), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void unbind(wx_event_type)", asFUNCTION(wx_window_unbind), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool navigate(wx_navigation flag = WX_NAVIGATION_FORWARD)", asMETHOD(wxWindow, Navigate), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "string get_tool_tip() const property", asFUNCTION(wx_window_get_tool_tip), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_tool_tip(const string &in tool_tip) property", asFUNCTION(wx_window_set_tool_tip), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void unset_tool_tip()", asMETHOD(wxWindow, UnsetToolTip), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void get_background_colour(int &out r, int &out g, int &out b)", asFUNCTION(wx_window_get_background_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_background_colour(int r, int g, int b)", asFUNCTION(wx_window_set_background_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void get_foreground_colour(int &out r, int &out g, int &out b)", asFUNCTION(wx_window_get_foreground_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_foreground_colour(int r, int g, int b)", asFUNCTION(wx_window_set_foreground_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "wx_sizer@ get_sizer() const property", asFUNCTION(wx_window_get_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_sizer(wx_sizer@) property", asFUNCTION(wx_window_set_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void get_position(int &out width, int &out height)", asFUNCTION(wx_window_get_position), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_position(int width, int height)", asFUNCTION(wx_window_set_position), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void get_size(int &out width, int &out height)", asFUNCTION(wx_window_get_size), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_size(int width, int height)", asFUNCTION(wx_window_set_size), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool is_shown()", asMETHOD(wxWindow, IsShown), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_enabled()", asMETHOD(wxWindow, IsEnabled), asCALL_THISCALL);

// ---------------------------------------------------------------------------
// REG_TLW_METHODS — wxTopLevelWindow surface (frame/dialog/...).
// ---------------------------------------------------------------------------
#define REG_TLW_METHODS(name) \
    engine->RegisterObjectMethod(name, "string get_title() const property", asFUNCTION(wx_tlw_get_title), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_title(const string &in title) property", asFUNCTION(wx_tlw_set_title), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool full_screen(bool show)", asFUNCTION(wx_tlw_show_full_screen), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void maximize(bool maximize = true)", asMETHOD(wxTopLevelWindow, Maximize), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void iconize(bool iconize = true)", asMETHOD(wxTopLevelWindow, Iconize), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_full_screen()", asMETHOD(wxTopLevelWindow, IsFullScreen), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_maximized()", asMETHOD(wxTopLevelWindow, IsMaximized), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_iconized()", asMETHOD(wxTopLevelWindow, IsIconized), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool enable_close_button(bool enable = true)", asMETHOD(wxTopLevelWindow, EnableCloseButton), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool enable_maximize_button(bool enable = true)", asMETHOD(wxTopLevelWindow, EnableMaximizeButton), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool enable_minimize_button(bool enable = true)", asMETHOD(wxTopLevelWindow, EnableMinimizeButton), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void request_user_attention(wx_user_attention flags = WX_USER_ATTENTION_INFO)", asMETHOD(wxTopLevelWindow, RequestUserAttention), asCALL_THISCALL);

// ---------------------------------------------------------------------------
// REG_CONTROL_METHODS — wxControl surface (just label for now).
// ---------------------------------------------------------------------------
#define REG_CONTROL_METHODS(name) \
    engine->RegisterObjectMethod(name, "string get_label() const property", asFUNCTION(wx_control_get_label), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_label(const string &in label) property", asFUNCTION(wx_control_set_label), asCALL_CDECL_OBJFIRST);

// ---------------------------------------------------------------------------
// REG_SIZER_METHODS — wxSizer surface.
// ---------------------------------------------------------------------------
#define REG_SIZER_METHODS(name) \
    engine->RegisterObjectMethod(name, "void add(wx_window@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_add_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void add(wx_sizer@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_add_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void add_spacer(int size)", asMETHOD(wxSizer, AddSpacer), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void add_stretch_spacer(int proportion = 1)", asMETHOD(wxSizer, AddStretchSpacer), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void insert(int index, wx_window@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_insert_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void insert(int index, wx_sizer@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_insert_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void prepend(wx_window@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_prepend_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void prepend(wx_sizer@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_prepend_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool detach(wx_window@)", asFUNCTION(wx_sizer_detach_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool detach(wx_sizer@)", asFUNCTION(wx_sizer_detach_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool detach(int index)", asFUNCTION(wx_sizer_detach_index), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool show(wx_window@, bool show = true, bool recursive = false)", asFUNCTION(wx_sizer_show_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool show(wx_sizer@, bool show = true, bool recursive = false)", asFUNCTION(wx_sizer_show_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool hide(wx_window@, bool recursive = false)", asFUNCTION(wx_sizer_hide_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool hide(wx_sizer@, bool recursive = false)", asFUNCTION(wx_sizer_hide_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "wx_sizer_item@ get_item(wx_window@)", asFUNCTION(wx_sizer_get_item_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "wx_sizer_item@ get_item(wx_sizer@)", asFUNCTION(wx_sizer_get_item_sz), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "wx_sizer_item@ get_item(int index)", asFUNCTION(wx_sizer_get_item_index), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "int find(wx_window@)", asFUNCTION(wx_sizer_find_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "int find(wx_sizer@)", asFUNCTION(wx_sizer_find_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool replace(wx_window@, wx_window@, bool recursive = false)", asFUNCTION(wx_sizer_replace_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool replace(wx_sizer@, wx_sizer@, bool recursive = false)", asFUNCTION(wx_sizer_replace_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void clear(bool delete_windows = false)", asMETHOD(wxSizer, Clear), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void layout()", asMETHOD(wxSizer, Layout), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void get_position(int &out width, int &out height)", asFUNCTION(wx_sizer_get_position), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void get_size(int &out width, int &out height)", asFUNCTION(wx_sizer_get_size), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "int get_item_count()", asMETHOD(wxSizer, GetItemCount), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_empty()", asMETHOD(wxSizer, IsEmpty), asCALL_THISCALL);

// ---------------------------------------------------------------------------
// REG_TEXT_ENTRY_METHODS — wxTextEntry mix-in surface, applied to any
// wx_window-derived type that is also a wxTextEntry (currently
// wx_text_entry and wx_text_control). The wrappers take wxWindow* and
// dynamic_cast internally; see helpers.cpp/GetEntry.
// ---------------------------------------------------------------------------
#define REG_TEXT_ENTRY_METHODS(name) \
    engine->RegisterObjectMethod(name, "string get_value() const property", asFUNCTION(wx_text_entry_get_value), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_value(const string &in value) property", asFUNCTION(wx_text_entry_set_value), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void write_text(const string &in text)", asFUNCTION(wx_text_entry_write_text), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void append_text(const string &in text)", asFUNCTION(wx_text_entry_append_text), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void clear()", asFUNCTION(wx_text_entry_clear), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void copy()", asFUNCTION(wx_text_entry_copy), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void cut()", asFUNCTION(wx_text_entry_cut), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void paste()", asFUNCTION(wx_text_entry_paste), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void remove(int from, int to)", asFUNCTION(wx_text_entry_remove), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void undo()", asFUNCTION(wx_text_entry_undo), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void redo()", asFUNCTION(wx_text_entry_redo), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool can_copy()", asFUNCTION(wx_text_entry_can_copy), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool can_cut()", asFUNCTION(wx_text_entry_can_cut), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool can_paste()", asFUNCTION(wx_text_entry_can_paste), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool can_undo()", asFUNCTION(wx_text_entry_can_undo), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool can_redo()", asFUNCTION(wx_text_entry_can_redo), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "string get_range(int from, int to)", asFUNCTION(wx_text_entry_get_range), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "string get_string_selection()", asFUNCTION(wx_text_entry_get_string_selection), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void get_selection(int &out from, int &out to)", asFUNCTION(wx_text_entry_get_selection), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_selection(int from, int to)", asFUNCTION(wx_text_entry_set_selection), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool is_editable()", asFUNCTION(wx_text_entry_is_editable), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_editable(bool editable)", asFUNCTION(wx_text_entry_set_editable), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool is_empty()", asFUNCTION(wx_text_entry_is_empty), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void replace(int from, int to, const string &in text)", asFUNCTION(wx_text_entry_replace), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void select_all()", asFUNCTION(wx_text_entry_select_all), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void select_none()", asFUNCTION(wx_text_entry_select_none), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_max_length(int length)", asFUNCTION(wx_text_entry_set_max_length), asCALL_CDECL_OBJFIRST);

// ---------------------------------------------------------------------------
// REGISTER_WX_* — register a concrete wx ref type, install AddRef/Release,
// install the wx_window/wx_control/... implicit-cast operators, and bring
// in the methods inherited from the relevant base class.
// ---------------------------------------------------------------------------
#define REGISTER_WX_WINDOW(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    REG_BASE_REF(as_name); \
    if (std::string(as_name) != "wx_window") { \
        engine->RegisterObjectMethod(as_name, "wx_window@ opImplCast()", asFUNCTION(to_window<wx_type>), asCALL_CDECL_OBJFIRST); \
    } \
    REG_WINDOW_METHODS(as_name);

#define REGISTER_WX_TLW(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    REG_BASE_REF(as_name); \
    if (std::string(as_name) != "wx_top_level_window") { \
        engine->RegisterObjectMethod(as_name, "wx_top_level_window@ opImplCast()", asFUNCTION(to_tlw<wx_type>), asCALL_CDECL_OBJFIRST); \
    } \
    engine->RegisterObjectMethod(as_name, "wx_window@ opImplCast()", asFUNCTION(to_window<wx_type>), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod("wx_window", as_name "@ opCast()", asFUNCTION(window_to_derived<wx_type>), asCALL_CDECL_OBJLAST); \
    REG_WINDOW_METHODS(as_name); \
    REG_TLW_METHODS(as_name);

#define REGISTER_WX_CONTROL(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    REG_BASE_REF(as_name); \
    if (std::string(as_name) != "wx_control") { \
        engine->RegisterObjectMethod(as_name, "wx_control@ opImplCast()", asFUNCTION(to_control<wx_type>), asCALL_CDECL_OBJFIRST); \
    } \
    engine->RegisterObjectMethod(as_name, "wx_window@ opImplCast()", asFUNCTION(to_window<wx_type>), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod("wx_window", as_name "@ opCast()", asFUNCTION(window_to_derived<wx_type>), asCALL_CDECL_OBJLAST); \
    REG_WINDOW_METHODS(as_name); \
    REG_CONTROL_METHODS(as_name);

#define REGISTER_WX_SIZER(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    REG_BASE_REF(as_name); \
    if (std::string(as_name) != "wx_sizer") { \
        engine->RegisterObjectMethod(as_name, "wx_sizer@ opImplCast()", asFUNCTION(to_sizer<wx_type>), asCALL_CDECL_OBJFIRST); \
    } \
    REG_SIZER_METHODS(as_name);

void register_key_codes(asIScriptEngine* engine) {
    engine->RegisterEnum("wx_key_code");
    #define REG_KEY(name) engine->RegisterEnumValue("wx_key_code", #name, name)
    REG_KEY(WXK_NONE);
    for (char c = 'A'; c <= 'Z'; ++c) {
        std::string name = "WXK_";
        name += c;
        engine->RegisterEnumValue("wx_key_code", name.c_str(), c);
    }
    for (int i = 0; i <= 9; ++i) {
        std::string name = "WXK_" + std::to_string(i);
        engine->RegisterEnumValue("wx_key_code", name.c_str(), '0' + i);
    }
    REG_KEY(WXK_BACK); REG_KEY(WXK_TAB); REG_KEY(WXK_RETURN);
    REG_KEY(WXK_ESCAPE); REG_KEY(WXK_SPACE); REG_KEY(WXK_DELETE);
    REG_KEY(WXK_START); REG_KEY(WXK_LBUTTON); REG_KEY(WXK_RBUTTON);
    REG_KEY(WXK_CANCEL); REG_KEY(WXK_MBUTTON); REG_KEY(WXK_CLEAR);
    REG_KEY(WXK_SHIFT); REG_KEY(WXK_ALT); REG_KEY(WXK_CONTROL);
    REG_KEY(WXK_RAW_CONTROL); REG_KEY(WXK_MENU); REG_KEY(WXK_PAUSE);
    REG_KEY(WXK_CAPITAL); REG_KEY(WXK_END); REG_KEY(WXK_HOME);
    REG_KEY(WXK_LEFT); REG_KEY(WXK_UP); REG_KEY(WXK_RIGHT); REG_KEY(WXK_DOWN);
    REG_KEY(WXK_SELECT); REG_KEY(WXK_PRINT); REG_KEY(WXK_EXECUTE);
    REG_KEY(WXK_SNAPSHOT); REG_KEY(WXK_INSERT); REG_KEY(WXK_HELP);
    REG_KEY(WXK_NUMPAD0); REG_KEY(WXK_NUMPAD1); REG_KEY(WXK_NUMPAD2);
    REG_KEY(WXK_NUMPAD3); REG_KEY(WXK_NUMPAD4); REG_KEY(WXK_NUMPAD5);
    REG_KEY(WXK_NUMPAD6); REG_KEY(WXK_NUMPAD7); REG_KEY(WXK_NUMPAD8);
    REG_KEY(WXK_NUMPAD9);
    REG_KEY(WXK_MULTIPLY); REG_KEY(WXK_ADD); REG_KEY(WXK_SEPARATOR);
    REG_KEY(WXK_SUBTRACT); REG_KEY(WXK_DECIMAL); REG_KEY(WXK_DIVIDE);
    REG_KEY(WXK_F1); REG_KEY(WXK_F2); REG_KEY(WXK_F3); REG_KEY(WXK_F4);
    REG_KEY(WXK_F5); REG_KEY(WXK_F6); REG_KEY(WXK_F7); REG_KEY(WXK_F8);
    REG_KEY(WXK_F9); REG_KEY(WXK_F10); REG_KEY(WXK_F11); REG_KEY(WXK_F12);
    REG_KEY(WXK_NUMLOCK); REG_KEY(WXK_SCROLL);
    REG_KEY(WXK_PAGEUP); REG_KEY(WXK_PAGEDOWN);
    REG_KEY(WXK_WINDOWS_LEFT); REG_KEY(WXK_WINDOWS_RIGHT);
    #undef REG_KEY
}

} // namespace

void register_all_types(asIScriptEngine* engine) {
    engine->RegisterEnum("wx_orientation");
    engine->RegisterEnumValue("wx_orientation", "WX_VERTICAL", wxVERTICAL);
    engine->RegisterEnumValue("wx_orientation", "WX_HORIZONTAL", wxHORIZONTAL);
    engine->RegisterEnumValue("wx_orientation", "WX_BOTH", wxBOTH);

    engine->RegisterEnum("wx_direction");
    engine->RegisterEnumValue("wx_direction", "WX_LEFT", wxLEFT);
    engine->RegisterEnumValue("wx_direction", "WX_RIGHT", wxRIGHT);
    engine->RegisterEnumValue("wx_direction", "WX_TOP", wxTOP);
    engine->RegisterEnumValue("wx_direction", "WX_BOTTOM", wxBOTTOM);
    engine->RegisterEnumValue("wx_direction", "WX_ALL", wxALL);

    engine->RegisterEnum("wx_alignment");
    engine->RegisterEnumValue("wx_alignment", "WX_ALIGN_LEFT", wxALIGN_LEFT);
    engine->RegisterEnumValue("wx_alignment", "WX_ALIGN_RIGHT", wxALIGN_RIGHT);
    engine->RegisterEnumValue("wx_alignment", "WX_ALIGN_TOP", wxALIGN_TOP);
    engine->RegisterEnumValue("wx_alignment", "WX_ALIGN_BOTTOM", wxALIGN_BOTTOM);
    engine->RegisterEnumValue("wx_alignment", "WX_ALIGN_CENTER", wxALIGN_CENTER);
    engine->RegisterEnumValue("wx_alignment", "WX_ALIGN_CENTER_HORIZONTAL", wxALIGN_CENTER_HORIZONTAL);
    engine->RegisterEnumValue("wx_alignment", "WX_ALIGN_CENTER_VERTICAL", wxALIGN_CENTER_VERTICAL);

    engine->RegisterEnum("wx_sizer_flag");
    engine->RegisterEnumValue("wx_sizer_flag", "WX_EXPAND", wxEXPAND);
    engine->RegisterEnumValue("wx_sizer_flag", "WX_SHAPED", wxSHAPED);
    engine->RegisterEnumValue("wx_sizer_flag", "WX_FIXED_MINSIZE", wxFIXED_MINSIZE);
    engine->RegisterEnumValue("wx_sizer_flag", "WX_RESERVE_SPACE_EVEN_IF_HIDDEN", wxRESERVE_SPACE_EVEN_IF_HIDDEN);

    engine->RegisterEnum("wx_event_type");
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_ACTIVATE", wxEVT_ACTIVATE);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_CHILD_FOCUS", wxEVT_CHILD_FOCUS);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_CONTEXT_MENU", wxEVT_CONTEXT_MENU);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_ERASE_BACKGROUND", wxEVT_ERASE_BACKGROUND);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_SET_FOCUS", wxEVT_SET_FOCUS);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_KILL_FOCUS", wxEVT_KILL_FOCUS);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_KEY_DOWN", wxEVT_KEY_DOWN);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_KEY_UP", wxEVT_KEY_UP);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_CHAR", wxEVT_CHAR);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_CHAR_HOOK", wxEVT_CHAR_HOOK);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_LEFT_DOWN", wxEVT_LEFT_DOWN);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_LEFT_UP", wxEVT_LEFT_UP);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_RIGHT_DOWN", wxEVT_RIGHT_DOWN);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_RIGHT_UP", wxEVT_RIGHT_UP);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MIDDLE_DOWN", wxEVT_MIDDLE_DOWN);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MIDDLE_UP", wxEVT_MIDDLE_UP);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_AUX1_DOWN", wxEVT_AUX1_DOWN);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_AUX1_UP", wxEVT_AUX1_UP);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_AUX2_DOWN", wxEVT_AUX2_DOWN);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_AUX2_UP", wxEVT_AUX2_UP);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MOTION", wxEVT_MOTION);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MOUSEWHEEL", wxEVT_MOUSEWHEEL);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_LEFT_DCLICK", wxEVT_LEFT_DCLICK);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MIDDLE_DCLICK", wxEVT_MIDDLE_DCLICK);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_RIGHT_DCLICK", wxEVT_RIGHT_DCLICK);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_AUX1_DCLICK", wxEVT_AUX1_DCLICK);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_AUX2_DCLICK", wxEVT_AUX2_DCLICK);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_ENTER_WINDOW", wxEVT_ENTER_WINDOW);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_LEAVE_WINDOW", wxEVT_LEAVE_WINDOW);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MAGNIFY", wxEVT_MAGNIFY);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MOUSE_CAPTURE_LOST", wxEVT_MOUSE_CAPTURE_LOST);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MOUSE_CAPTURE_CHANGED", wxEVT_MOUSE_CAPTURE_CHANGED);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_PAINT", wxEVT_PAINT);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_SIZE", wxEVT_SIZE);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_SYS_COLOUR_CHANGED", wxEVT_SYS_COLOUR_CHANGED);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_CLOSE_WINDOW", wxEVT_CLOSE_WINDOW);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_ICONIZE", wxEVT_ICONIZE);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MENU_OPEN", wxEVT_MENU_OPEN);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MENU_CLOSE", wxEVT_MENU_CLOSE);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_MENU_HIGHLIGHT", wxEVT_MENU_HIGHLIGHT);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_BUTTON", wxEVT_BUTTON);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_CHECKBOX", wxEVT_CHECKBOX);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_TEXT", wxEVT_TEXT);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_TEXT_ENTER", wxEVT_TEXT_ENTER);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_TEXT_URL", wxEVT_TEXT_URL);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_TEXT_MAXLEN", wxEVT_TEXT_MAXLEN);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_RADIOBUTTON", wxEVT_RADIOBUTTON);

    engine->RegisterEnum("wx_style");
    // wx_window
    engine->RegisterEnumValue("wx_style", "WX_BORDER_NONE", wxBORDER_NONE);
    engine->RegisterEnumValue("wx_style", "WX_BORDER_STATIC", wxBORDER_STATIC);
    engine->RegisterEnumValue("wx_style", "WX_BORDER_SIMPLE", wxBORDER_SIMPLE);
    engine->RegisterEnumValue("wx_style", "WX_BORDER_RAISED", wxBORDER_RAISED);
    engine->RegisterEnumValue("wx_style", "WX_BORDER_SUNKEN", wxBORDER_SUNKEN);
    engine->RegisterEnumValue("wx_style", "WX_BORDER_THEME", wxBORDER_THEME);
    engine->RegisterEnumValue("wx_style", "WX_HSCROLL", wxHSCROLL);
    engine->RegisterEnumValue("wx_style", "WX_VSCROLL", wxVSCROLL);
    engine->RegisterEnumValue("wx_style", "WX_WANTS_CHARS", wxWANTS_CHARS);
    engine->RegisterEnumValue("wx_style", "WX_TAB_TRAVERSAL", wxTAB_TRAVERSAL);
    engine->RegisterEnumValue("wx_style", "WX_CLIP_CHILDREN", wxCLIP_CHILDREN);
    engine->RegisterEnumValue("wx_style", "WX_FULL_REPAINT_ON_RESIZE", wxFULL_REPAINT_ON_RESIZE);
    // wx_frame
    engine->RegisterEnumValue("wx_style", "WX_DEFAULT_FRAME_STYLE", wxDEFAULT_FRAME_STYLE);
    engine->RegisterEnumValue("wx_style", "WX_ICONIZE", wxICONIZE);
    engine->RegisterEnumValue("wx_style", "WX_MINIMIZE", wxMINIMIZE);
    engine->RegisterEnumValue("wx_style", "WX_MAXIMIZE", wxMAXIMIZE);
    engine->RegisterEnumValue("wx_style", "WX_CAPTION", wxCAPTION);
    engine->RegisterEnumValue("wx_style", "WX_CLOSE_BOX", wxCLOSE_BOX);
    engine->RegisterEnumValue("wx_style", "WX_MINIMIZE_BOX", wxMINIMIZE_BOX);
    engine->RegisterEnumValue("wx_style", "WX_MAXIMIZE_BOX", wxMAXIMIZE_BOX);
    engine->RegisterEnumValue("wx_style", "WX_RESIZE_BORDER", wxRESIZE_BORDER);
    engine->RegisterEnumValue("wx_style", "WX_SYSTEM_MENU", wxSYSTEM_MENU);
    engine->RegisterEnumValue("wx_style", "WX_STAY_ON_TOP", wxSTAY_ON_TOP);
    engine->RegisterEnumValue("wx_style", "WX_FRAME_TOOL_WINDOW", wxFRAME_TOOL_WINDOW);
    engine->RegisterEnumValue("wx_style", "WX_FRAME_NO_TASKBAR", wxFRAME_NO_TASKBAR);
    engine->RegisterEnumValue("wx_style", "WX_NO_BORDER", wxNO_BORDER);
    // wx_button
    engine->RegisterEnumValue("wx_style", "WX_BU_LEFT", wxBU_LEFT);
    engine->RegisterEnumValue("wx_style", "WX_BU_RIGHT", wxBU_RIGHT);
    engine->RegisterEnumValue("wx_style", "WX_BU_TOP", wxBU_TOP);
    engine->RegisterEnumValue("wx_style", "WX_BU_BOTTOM", wxBU_BOTTOM);
    engine->RegisterEnumValue("wx_style", "WX_BU_EXACTFIT", wxBU_EXACTFIT);
    engine->RegisterEnumValue("wx_style", "WX_BU_NOTEXT", wxBU_NOTEXT);
    // wx_check_box
    engine->RegisterEnumValue("wx_style", "WX_CHK_2STATE", wxCHK_2STATE);
    engine->RegisterEnumValue("wx_style", "WX_CHK_3STATE", wxCHK_3STATE);
    engine->RegisterEnumValue("wx_style", "WX_CHK_ALLOW_3RD_STATE_FOR_USER", wxCHK_ALLOW_3RD_STATE_FOR_USER);
    // wx_text_control
    engine->RegisterEnumValue("wx_style", "WX_TE_MULTILINE", wxTE_MULTILINE);
    engine->RegisterEnumValue("wx_style", "WX_TE_PASSWORD", wxTE_PASSWORD);
    engine->RegisterEnumValue("wx_style", "WX_TE_READONLY", wxTE_READONLY);
    engine->RegisterEnumValue("wx_style", "WX_TE_PROCESS_ENTER", wxTE_PROCESS_ENTER);
    engine->RegisterEnumValue("wx_style", "WX_TE_PROCESS_TAB", wxTE_PROCESS_TAB);
    engine->RegisterEnumValue("wx_style", "WX_TE_RICH2", wxTE_RICH2);
    engine->RegisterEnumValue("wx_style", "WX_TE_AUTO_URL", wxTE_AUTO_URL);
    engine->RegisterEnumValue("wx_style", "WX_TE_NOHIDESEL", wxTE_NOHIDESEL);
    engine->RegisterEnumValue("wx_style", "WX_TE_NO_VSCROLL", wxTE_NO_VSCROLL);
    engine->RegisterEnumValue("wx_style", "WX_TE_LEFT", wxTE_LEFT);
    engine->RegisterEnumValue("wx_style", "WX_TE_CENTRE", wxTE_CENTRE);
    engine->RegisterEnumValue("wx_style", "WX_TE_RIGHT", wxTE_RIGHT);
    engine->RegisterEnumValue("wx_style", "WX_TE_DONTWRAP", wxTE_DONTWRAP);
    engine->RegisterEnumValue("wx_style", "WX_TE_CHARWRAP", wxTE_CHARWRAP);
    engine->RegisterEnumValue("wx_style", "WX_TE_WORDWRAP", wxTE_WORDWRAP);
    engine->RegisterEnumValue("wx_style", "WX_TE_BESTWRAP", wxTE_BESTWRAP);
    // wx_radio_button
    engine->RegisterEnumValue("wx_style", "WX_RB_GROUP", wxRB_GROUP);
    engine->RegisterEnumValue("wx_style", "WX_RB_SINGLE", wxRB_SINGLE);

    engine->RegisterEnum("wx_user_attention");
    engine->RegisterEnumValue("wx_user_attention", "WX_USER_ATTENTION_INFO", wxUSER_ATTENTION_INFO);
    engine->RegisterEnumValue("wx_user_attention", "WX_USER_ATTENTION_ERROR", wxUSER_ATTENTION_ERROR);

    engine->RegisterEnum("wx_checkbox_state");
    engine->RegisterEnumValue("wx_checkbox_state", "WX_CHK_UNCHECKED", wxCHK_UNCHECKED);
    engine->RegisterEnumValue("wx_checkbox_state", "WX_CHK_CHECKED", wxCHK_CHECKED);
    engine->RegisterEnumValue("wx_checkbox_state", "WX_CHK_UNDETERMINED", wxCHK_UNDETERMINED);

    engine->RegisterEnum("wx_navigation");
    engine->RegisterEnumValue("wx_navigation", "WX_NAVIGATION_FORWARD", 1);
    engine->RegisterEnumValue("wx_navigation", "WX_NAVIGATION_BACKWARD", 2);
    engine->RegisterEnumValue("wx_navigation", "WX_NAVIGATION_TABBING", 4);

    register_key_codes(engine);

    engine->RegisterObjectType("wx_window", 0, asOBJ_REF);
    engine->RegisterObjectType("wx_top_level_window", 0, asOBJ_REF);
    engine->RegisterObjectType("wx_control", 0, asOBJ_REF);
    engine->RegisterObjectType("wx_text_entry", 0, asOBJ_REF);
    engine->RegisterObjectType("wx_sizer", 0, asOBJ_REF);
    engine->RegisterObjectType("wx_sizer_item", 0, asOBJ_REF);
    REG_BASE_REF("wx_sizer_item");
    engine->RegisterObjectType("wx_event", 0, asOBJ_REF | asOBJ_NOCOUNT);
    engine->RegisterObjectType("wx_key_event", 0, asOBJ_REF | asOBJ_NOCOUNT);
    engine->RegisterObjectType("wx_mouse_event", 0, asOBJ_REF | asOBJ_NOCOUNT);
    engine->RegisterObjectType("wx_command_event", 0, asOBJ_REF | asOBJ_NOCOUNT);

    engine->RegisterFuncdef("void wx_callback(wx_event@)");

    REGISTER_WX_WINDOW("wx_window", wxWindow);
    REGISTER_WX_TLW("wx_top_level_window", wxTopLevelWindow);
    REGISTER_WX_CONTROL("wx_control", wxControl);
    REG_BASE_REF("wx_text_entry");
    REG_TEXT_ENTRY_METHODS("wx_text_entry");
    REGISTER_WX_SIZER("wx_sizer", wxSizer);
    REGISTER_WX_SIZER("wx_box_sizer", wxBoxSizer);
    REGISTER_WX_TLW("wx_frame", wxFrame);
    REGISTER_WX_WINDOW("wx_panel", wxPanel);
    REGISTER_WX_CONTROL("wx_button", wxButton);
    REGISTER_WX_CONTROL("wx_check_box", wxCheckBox);
    engine->RegisterObjectMethod("wx_check_box", "bool get_value() const property", asMETHOD(wxCheckBox, GetValue), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_check_box", "void set_value(bool value) property", asMETHOD(wxCheckBox, SetValue), asCALL_THISCALL);
    // get_3state_value/set_3state_value are registered as plain methods (no
    // 'property') because the derived property name '3state_value' is not a
    // valid AngelScript identifier (it starts with a digit).
    engine->RegisterObjectMethod("wx_check_box", "wx_checkbox_state get_3state_value() const", asMETHOD(wxCheckBox, Get3StateValue), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_check_box", "void set_3state_value(wx_checkbox_state state)", asMETHOD(wxCheckBox, Set3StateValue), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_check_box", "bool is_3rd_state_allowed_for_user()", asMETHOD(wxCheckBox, Is3rdStateAllowedForUser), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_check_box", "bool is_3state()", asMETHOD(wxCheckBox, Is3State), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_check_box", "bool is_checked()", asMETHOD(wxCheckBox, IsChecked), asCALL_THISCALL);
    REGISTER_WX_CONTROL("wx_static_text", wxStaticText);
    engine->RegisterObjectMethod("wx_static_text", "void wrap(int width)", asMETHOD(wxStaticText, Wrap), asCALL_THISCALL);
    REGISTER_WX_CONTROL("wx_text_control", wxTextCtrl);
    engine->RegisterObjectMethod("wx_text_control", "wx_text_entry@ opImplCast()", asFUNCTION(to_text_entry_as_win<wxTextCtrl>), asCALL_CDECL_OBJFIRST);
    REG_TEXT_ENTRY_METHODS("wx_text_control");
    REGISTER_WX_CONTROL("wx_radio_button", wxRadioButton);
    engine->RegisterObjectMethod("wx_radio_button", "bool get_value() const property", asMETHOD(wxRadioButton, GetValue), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_radio_button", "void set_value(bool value) property", asMETHOD(wxRadioButton, SetValue), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_radio_button", "wx_radio_button@ get_first_in_group()", asFUNCTION(wx_radio_button_get_first_in_group), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_radio_button", "wx_radio_button@ get_last_in_group()", asFUNCTION(wx_radio_button_get_last_in_group), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_radio_button", "wx_radio_button@ get_next_in_group()", asFUNCTION(wx_radio_button_get_next_in_group), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_radio_button", "wx_radio_button@ get_previous_in_group()", asFUNCTION(wx_radio_button_get_previous_in_group), asCALL_CDECL_OBJFIRST);

    engine->RegisterObjectMethod("wx_event", "wx_key_event@ opCast()", asFUNCTION(event_to_derived<wxKeyEvent>), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectMethod("wx_event", "wx_mouse_event@ opCast()", asFUNCTION(event_to_derived<wxMouseEvent>), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectMethod("wx_event", "wx_window@ get_event_object() const", asFUNCTION(wx_event_get_event_object), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_event", "int get_event_type() const", asMETHOD(wxEvent, GetEventType), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_event", "void skip(bool skip = true)", asMETHOD(wxEvent, Skip), asCALL_THISCALL);

    engine->RegisterObjectMethod("wx_key_event", "int get_key_code() const", asMETHOD(wxKeyEvent, GetKeyCode), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_key_event", "int get_unicode_key() const", asMETHOD(wxKeyEvent, GetUnicodeKey), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_key_event", "bool control_down() const", asFUNCTION(wx_key_event_control_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_key_event", "bool shift_down() const", asFUNCTION(wx_key_event_shift_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_key_event", "bool alt_down() const", asFUNCTION(wx_key_event_alt_down), asCALL_CDECL_OBJFIRST);

    engine->RegisterObjectMethod("wx_mouse_event", "bool button() const", asFUNCTION(wx_mouse_event_button), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool button_down() const", asFUNCTION(wx_mouse_event_button_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool button_up() const", asFUNCTION(wx_mouse_event_button_up), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool button_dclick() const", asFUNCTION(wx_mouse_event_button_dclick), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool left_down() const", asMETHOD(wxMouseEvent, LeftDown), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool left_up() const", asMETHOD(wxMouseEvent, LeftUp), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool left_dclick() const", asMETHOD(wxMouseEvent, LeftDClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool middle_down() const", asMETHOD(wxMouseEvent, MiddleDown), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool middle_up() const", asMETHOD(wxMouseEvent, MiddleUp), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool middle_dclick() const", asMETHOD(wxMouseEvent, MiddleDClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool right_down() const", asMETHOD(wxMouseEvent, RightDown), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool right_up() const", asMETHOD(wxMouseEvent, RightUp), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool right_dclick() const", asMETHOD(wxMouseEvent, RightDClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux1_down() const", asMETHOD(wxMouseEvent, Aux1Down), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux1_up() const", asMETHOD(wxMouseEvent, Aux1Up), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux1_dclick() const", asMETHOD(wxMouseEvent, Aux1DClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux2_down() const", asMETHOD(wxMouseEvent, Aux2Down), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux2_up() const", asMETHOD(wxMouseEvent, Aux2Up), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux2_dclick() const", asMETHOD(wxMouseEvent, Aux2DClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool left_is_down() const", asFUNCTION(wx_mouse_event_left_is_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool middle_is_down() const", asFUNCTION(wx_mouse_event_middle_is_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool right_is_down() const", asFUNCTION(wx_mouse_event_right_is_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux1_is_down() const", asFUNCTION(wx_mouse_event_aux1_is_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux2_is_down() const", asFUNCTION(wx_mouse_event_aux2_is_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "int get_x() const", asFUNCTION(wx_mouse_event_get_x), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "int get_y() const", asFUNCTION(wx_mouse_event_get_y), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "int get_wheel_rotation() const", asMETHOD(wxMouseEvent, GetWheelRotation), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "int get_wheel_delta() const", asMETHOD(wxMouseEvent, GetWheelDelta), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool dragging() const", asMETHOD(wxMouseEvent, Dragging), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool moving() const", asMETHOD(wxMouseEvent, Moving), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool entering() const", asMETHOD(wxMouseEvent, Entering), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool leaving() const", asMETHOD(wxMouseEvent, Leaving), asCALL_THISCALL);

    engine->RegisterObjectMethod("wx_event", "wx_command_event@ opCast()", asFUNCTION(event_to_derived<wxCommandEvent>), asCALL_CDECL_OBJLAST);
    // get_int/set_int and get_string/set_string are registered as plain
    // methods (no 'property') because the derived property names would be
    // 'int' (a reserved AngelScript keyword) and 'string' (a registered
    // type name), neither of which is usable from script as obj.int /
    // obj.string.
    engine->RegisterObjectMethod("wx_command_event", "int get_int() const", asFUNCTION(wx_command_event_get_int), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_command_event", "void set_int(int value)", asFUNCTION(wx_command_event_set_int), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_command_event", "int get_selection() const property", asMETHOD(wxCommandEvent, GetSelection), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_command_event", "string get_string() const", asFUNCTION(wx_command_event_get_string), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_command_event", "void set_string(const string &in value)", asFUNCTION(wx_command_event_set_string), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_command_event", "bool is_checked() const", asMETHOD(wxCommandEvent, IsChecked), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_command_event", "bool is_selection() const", asMETHOD(wxCommandEvent, IsSelection), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_command_event", "int get_extra_long() const property", asFUNCTION(wx_command_event_get_extra_long), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_command_event", "void set_extra_long(int value) property", asFUNCTION(wx_command_event_set_extra_long), asCALL_CDECL_OBJFIRST);

    engine->RegisterObjectMethod("wx_sizer_item", "wx_window@ get_window() const property", asFUNCTION(wx_sizer_item_get_window), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_sizer_item", "wx_sizer@ get_sizer() const property", asFUNCTION(wx_sizer_item_get_sizer), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_sizer_item", "int get_proportion() const property", asMETHOD(wxSizerItem, GetProportion), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_sizer_item", "void set_proportion(int proportion) property", asMETHOD(wxSizerItem, SetProportion), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_sizer_item", "int get_flag() const property", asMETHOD(wxSizerItem, GetFlag), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_sizer_item", "void set_flag(int flag) property", asMETHOD(wxSizerItem, SetFlag), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_sizer_item", "int get_border() const property", asMETHOD(wxSizerItem, GetBorder), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_sizer_item", "void set_border(int border) property", asMETHOD(wxSizerItem, SetBorder), asCALL_THISCALL);

    register_wx_manager(engine);
}
