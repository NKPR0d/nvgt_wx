#include <string>
#include <vector>
#include <map>
#include "../../src/nvgt_plugin.h"
#include <wx/wx.h>
#include <wx/evtloop.h>
std::vector<asIScriptContext*> g_context_pool;
asIScriptContext* GetContextFromPool(asIScriptEngine* engine) {
    if (!g_context_pool.empty()) {
        asIScriptContext* ctx = g_context_pool.back();
        g_context_pool.pop_back();
        return ctx;
    }
    return engine->CreateContext();
}

void ReturnContextToPool(asIScriptContext* ctx) {
    if (ctx) {
        ctx->Unprepare();
        g_context_pool.push_back(ctx);
    }
}

struct ScriptEventData {
    asIScriptFunction* callback;
    asIScriptEngine* engine;
};

std::map<void*, int> g_ref_counts;
typedef std::pair<void*, int> EventKey;
std::map<EventKey, ScriptEventData> g_event_handlers;

void CleanupHandlersFor(void* ptr) {
    for (auto it = g_event_handlers.begin(); it != g_event_handlers.end(); ) {
        if (it->first.first == ptr) {
            if (it->second.callback) {
                it->second.callback->Release();
            }
            it = g_event_handlers.erase(it);
        } else {
            ++it;
        }
    }
}

void AddRef(void* ptr) {
    if (!ptr) return;
    if (g_ref_counts.find(ptr) == g_ref_counts.end()) {
        g_ref_counts[ptr] = 1;
    } else {
asAtomicInc(g_ref_counts[ptr]);
    }
}

void Release(void* ptr) {
    if (!ptr) return;
    auto it = g_ref_counts.find(ptr);
    if (it == g_ref_counts.end()) return; 
    if (asAtomicDec(it->second) <= 0) {
        g_ref_counts.erase(it);
        CleanupHandlersFor(ptr);
        wxObject* wx_obj = static_cast<wxObject*>(ptr);
        if (wx_obj->IsKindOf(CLASSINFO(wxWindow))) {
            wxWindow* win = static_cast<wxWindow*>(wx_obj);
            if (win && !win->GetParent()) {
                win->Destroy();
            }
        } 
        else if (wx_obj->IsKindOf(CLASSINFO(wxSizer))) {
            wxSizer* sizer = static_cast<wxSizer*>(wx_obj);
            if (sizer && !sizer->GetContainingWindow()) {
                delete sizer;
            }
        }
    }
}
class ContextGuard {
    asIScriptContext* ctx;
public:
    ContextGuard(asIScriptEngine* eng) {
        ctx = GetContextFromPool(eng);
    }
    ~ContextGuard() {
        if (ctx) ReturnContextToPool(ctx);
    }
    asIScriptContext* get() { return ctx; }
};

template<typename T> wxWindow* to_window(T* obj) { return static_cast<wxWindow*>(obj); }
template<typename T> wxControl* to_control(T* obj) { return static_cast<wxControl*>(obj); }
template<typename T> wxSizer* to_sizer(T* obj) { return static_cast<wxSizer*>(obj); }
template<typename T> wxTopLevelWindow* to_tlw(T* obj) { return static_cast<wxTopLevelWindow*>(obj); }
template<typename T> wxWindow* to_text_entry_as_win(T* obj) { return static_cast<wxWindow*>(obj); }

void OnObjectDestroyed(wxWindowDestroyEvent& event) {
    void* ptr = event.GetEventObject();    
    CleanupHandlersFor(ptr);
    g_ref_counts.erase(ptr);
}

template<typename T>
T* Track(T* obj) {
    if (obj) {
        wxWindow* win = dynamic_cast<wxWindow*>(obj);
        if (win) win->Bind(wxEVT_DESTROY, &OnObjectDestroyed);
        AddRef(obj);
    }
    return obj;
}

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

wxWindow* wx_event_get_event_object(wxEvent* self) {
    wxWindow* win = dynamic_cast<wxWindow*>(self->GetEventObject());
    if (win) AddRef(win);
    return win;
}

bool wx_key_event_control_down(wxKeyEvent* self) {
    return self->ControlDown();
}

bool wx_key_event_shift_down(wxKeyEvent* self) {
    return self->ShiftDown();
}

bool wx_key_event_alt_down(wxKeyEvent* self) {
    return self->AltDown();
}

int wx_mouse_event_get_x(wxMouseEvent* self) { return self->GetX(); }
int wx_mouse_event_get_y(wxMouseEvent* self) { return self->GetY(); }
bool wx_mouse_event_left_is_down(wxMouseEvent* self) { return self->LeftIsDown(); }
bool wx_mouse_event_middle_is_down(wxMouseEvent* self) { return self->MiddleIsDown(); }
bool wx_mouse_event_right_is_down(wxMouseEvent* self) { return self->RightIsDown(); }
bool wx_mouse_event_aux1_is_down(wxMouseEvent* self) { return self->Aux1IsDown(); }
bool wx_mouse_event_aux2_is_down(wxMouseEvent* self) { return self->Aux2IsDown(); }

void OnWxEvent(wxEvent& event) {
    auto it = g_event_handlers.find({event.GetEventObject(), event.GetEventType()});
    if (it != g_event_handlers.end()) {
        ContextGuard guard(it->second.engine);
        if (auto ctx = guard.get()) {
            ctx->Prepare(it->second.callback);
            ctx->SetArgObject(0, &event);            
            ctx->Execute();
        }
    }
}

void wx_window_bind(wxWindow* self, int event_type, asIScriptFunction* callback) {
    if (!self || !callback) return;
    EventKey key = { (void*)self, event_type };
    if (g_event_handlers.count(key)) {
        g_event_handlers[key].callback->Release();
    }
    callback->AddRef();
    g_event_handlers[key] = { callback, callback->GetEngine() };
    self->Bind(static_cast<wxEventType>(event_type), &OnWxEvent, wxID_ANY);
}

std::string wx_window_get_tool_tip(wxWindow* self) {
    return self ? std::string(self->GetToolTipText().utf8_str()) : "";
}

void wx_window_set_tool_tip(wxWindow* self, const std::string& text) {
    if (self) self->SetToolTip(wxString::FromUTF8(text.c_str()));
}

void wx_window_get_background_colour(wxWindow* self, int &r, int &g, int &b) {
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

void wx_window_get_foreground_colour(wxWindow* self, int &r, int &g, int &b) {
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

void wx_window_get_size(wxWindow* self, int &out_w, int &out_h) {
    if (self) self->GetSize(&out_w, &out_h);
}

void wx_window_set_size(wxWindow* self, int w, int h) {
    if (self) self->SetSize(w, h);
}

wxSizer* wx_window_get_sizer(wxWindow* self) {
    wxSizer* s = self->GetSizer();
    if (s) AddRef(s);
    return s;
}

std::string wx_control_get_label(wxControl* self) { return self ? std::string(self->GetLabel().utf8_str()) : ""; }

void wx_control_set_label(wxControl* self, const std::string& label) {
    if (self) {
        wxString wx_s = wxString::FromUTF8(label.c_str());        
        self->SetLabel(wx_s);        
        self->SetName(wx_s);
    }
}

std::string wx_tlw_get_title(wxTopLevelWindow* self) {
    return self ? std::string(self->GetTitle().utf8_str()) : "";
}

void wx_tlw_set_title(wxTopLevelWindow* self, const std::string& title) {
    if (self) self->SetTitle(wxString::FromUTF8(title.c_str()));
}

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

class NVGTApp : public wxApp {
public:
    bool OnInit() override { return true; }
};

class WxManager {
public:
    WxManager() {
        if (!wxTheApp) {
            wxApp::SetInstance(new NVGTApp());
            int argc = 0;
            char** argv = nullptr;
            wxEntryStart(argc, argv);
            wxTheApp->CallOnInit();
        }
    }
    void update() {
        if (!wxTheApp) return;
        wxTheApp->ProcessPendingEvents();
#ifdef _WIN32
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            HWND active_hwnd = GetActiveWindow();
            if (active_hwnd && IsDialogMessage(active_hwnd, &msg)) continue;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
#endif
    }

    wxFrame* create_frame(const std::string& title, int w, int h, long style = wxDEFAULT_FRAME_STYLE) {
        return Track(new wxFrame(NULL, wxID_ANY, wxString::FromUTF8(title.c_str()), wxDefaultPosition, wxSize(w, h), style));
    }

    wxButton* create_button(wxWindow* parent, const std::string& label, long style = 0) {
        wxString wx_s = wxString::FromUTF8(label.c_str());
        return Track(new wxButton(parent, wxID_ANY, wx_s, wxDefaultPosition, wxDefaultSize, style, wxDefaultValidator, wx_s));
    }

    wxBoxSizer* create_box_sizer(int orient) {
        wxBoxSizer* b = new wxBoxSizer(orient);
        AddRef(b);
        return b;
    }

    wxPanel* create_panel(wxWindow* parent, long style = wxTAB_TRAVERSAL) {
        return Track(new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, style));
    }

    wxStaticText* create_static_text(wxWindow* parent, const std::string& label, long style = 0) {
        wxString wx_s = wxString::FromUTF8(label.c_str());
        return Track(new wxStaticText(parent, wxID_ANY, wx_s, wxDefaultPosition, wxDefaultSize, style, wx_s));
    }

    wxTextCtrl* create_text_control(wxWindow* parent, const std::string& value = "", long style = 0) {
        return Track(new wxTextCtrl(parent, wxID_ANY, wxString::FromUTF8(value.c_str()), wxDefaultPosition, wxDefaultSize, style));
    }

    wxCheckBox* create_check_box(wxWindow* parent, const std::string& label, long style = 0) {
        wxString wx_s = wxString::FromUTF8(label.c_str());
        return Track (new wxCheckBox(parent, wxID_ANY, wx_s, wxDefaultPosition, wxDefaultSize, style, wxDefaultValidator, wx_s));
    }
};

void WxConstructor(WxManager* self) { new(self) WxManager(); }
void WxDestructor(WxManager* self) { self->~WxManager(); }

#define REG_BASE_REF(name) \
    engine->RegisterObjectBehaviour(name, asBEHAVE_ADDREF, "void f()", asFUNCTION(AddRef), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectBehaviour(name, asBEHAVE_RELEASE, "void f()", asFUNCTION(Release), asCALL_CDECL_OBJFIRST);

#define REG_WINDOW_METHODS(name) \
    engine->RegisterObjectMethod(name, "bool show(bool visible = true)", asMETHOD(wxWindow, Show), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool hide()", asMETHOD(wxWindow, Hide), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool enable(bool enable = true)", asMETHOD(wxWindow, Enable), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool disable()", asMETHOD(wxWindow, Disable), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool has_focus()", asMETHOD(wxWindow, HasFocus), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void set_focus()", asMETHOD(wxWindow, SetFocus), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void layout()", asMETHOD(wxWindow, Layout), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void center(int direction = 3)", asMETHOD(wxWindow, Center), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "int get_id()", asMETHOD(wxWindow, GetId), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void refresh()", asMETHOD(wxWindow, Refresh), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void update()", asMETHOD(wxWindow, Update), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool destroy()", asMETHOD(wxWindow, Destroy), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool close(bool force = false)", asMETHOD(wxWindow, Close), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void bind(wx_event_type, wx_callback@)", asFUNCTION(wx_window_bind), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool navigate(wx_navigation flag = WX_NAVIGATION_FORWARD)", asMETHOD(wxWindow, Navigate), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "string get_tool_tip()", asFUNCTION(wx_window_get_tool_tip), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_tool_tip(const string &in tool_tip)", asFUNCTION(wx_window_set_tool_tip), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void unset_tool_tip()", asMETHOD(wxWindow, UnsetToolTip), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void get_background_colour(int &out r, int &out g, int &out b)", asFUNCTION(wx_window_get_background_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_background_colour(int r, int g, int b)", asFUNCTION(wx_window_set_background_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void get_foreground_colour(int &out r, int &out g, int &out b)", asFUNCTION(wx_window_get_foreground_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_foreground_colour(int r, int g, int b)", asFUNCTION(wx_window_set_foreground_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "wx_sizer@ get_sizer()", asFUNCTION(wx_window_get_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_sizer(wx_sizer@)", asMETHOD(wxWindow, SetSizer), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void get_position(int &out width, int &out height)", asFUNCTION(wx_window_get_position), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_position(int width, int height)", asFUNCTION(wx_window_set_position), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void get_size(int &out width, int &out height)", asFUNCTION(wx_window_get_size), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_size(int width, int height)", asFUNCTION(wx_window_set_size), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool is_shown()", asMETHOD(wxWindow, IsShown), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_enabled()", asMETHOD(wxWindow, IsEnabled), asCALL_THISCALL);

#define REG_TLW_METHODS(name) \
    engine->RegisterObjectMethod(name, "string get_title()", asFUNCTION(wx_tlw_get_title), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_title(const string &in title)", asFUNCTION(wx_tlw_set_title), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool full_screen(bool show)", asMETHOD(wxTopLevelWindow, ShowFullScreen), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void maximize(bool maximize = true)", asMETHOD(wxTopLevelWindow, Maximize), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void iconize(bool iconize = true)", asMETHOD(wxTopLevelWindow, Iconize), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_full_screen()", asMETHOD(wxTopLevelWindow, IsFullScreen), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_maximized()", asMETHOD(wxTopLevelWindow, IsMaximized), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_iconized()", asMETHOD(wxTopLevelWindow, IsIconized), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool enable_close_button(bool enable = true)", asMETHOD(wxTopLevelWindow, EnableCloseButton), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool enable_maximize_button(bool enable = true)", asMETHOD(wxTopLevelWindow, EnableMaximizeButton), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool enable_minimize_button(bool enable = true)", asMETHOD(wxTopLevelWindow, EnableMinimizeButton), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void request_user_attention(wx_user_attention flags = WX_USER_ATTENTION_INFO)", asMETHOD(wxTopLevelWindow, RequestUserAttention), asCALL_THISCALL);

#define REG_CONTROL_METHODS(name) \
    engine->RegisterObjectMethod(name, "string get_label()", asFUNCTION(wx_control_get_label), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_label(const string &in label)", asFUNCTION(wx_control_set_label), asCALL_CDECL_OBJFIRST); \

#define REG_SIZER_METHODS(name) \
    engine->RegisterObjectMethod(name, "void add(wx_window@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_add_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void add(wx_sizer@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_add_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void add_spacer(int size)", asMETHOD(wxSizer, AddSpacer), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void add_stretch_spacer(int proportion = 1)", asMETHOD(wxSizer, AddStretchSpacer), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void insert(int index, wx_window@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_insert_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void insert(int index, wx_sizer@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_insert_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void prepend(wx_window@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_prepend_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void prepend(wx_sizer@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_prepend_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool detach(int index)", asFUNCTION(wx_sizer_detach_index), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool detach(wx_window@)", asFUNCTION(wx_sizer_detach_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool detach(wx_sizer@)", asFUNCTION(wx_sizer_detach_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool show(wx_window@, bool show = true, bool recursive = false)", asFUNCTION(wx_sizer_show_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool show(wx_sizer@, bool show = true, bool recursive = false)", asFUNCTION(wx_sizer_show_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool hide(wx_window@, bool recursive = false)", asFUNCTION(wx_sizer_hide_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool hide(wx_sizer@, bool recursive = false)", asFUNCTION(wx_sizer_hide_sizer), asCALL_CDECL_OBJFIRST); \
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

#define REG_TEXT_ENTRY_METHODS(name) \
    engine->RegisterObjectMethod(name, "string get_value()", asFUNCTION(wx_text_entry_get_value), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_value(const string &in value)", asFUNCTION(wx_text_entry_set_value), asCALL_CDECL_OBJFIRST); \
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

plugin_main(nvgt_plugin_shared* shared) {
    if (!prepare_plugin(shared)) return false;
    asIScriptEngine* engine = shared->script_engine;
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

    engine->RegisterEnum("wx_style");
//wx_window
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
//wx_frame
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
//wx_button
    engine->RegisterEnumValue("wx_style", "WX_BU_LEFT", wxBU_LEFT);
    engine->RegisterEnumValue("wx_style", "WX_BU_RIGHT", wxBU_RIGHT);
    engine->RegisterEnumValue("wx_style", "WX_BU_TOP", wxBU_TOP);
    engine->RegisterEnumValue("wx_style", "WX_BU_BOTTOM", wxBU_BOTTOM);
    engine->RegisterEnumValue("wx_style", "WX_BU_EXACTFIT", wxBU_EXACTFIT);
    engine->RegisterEnumValue("wx_style", "WX_BU_NOTEXT", wxBU_NOTEXT);
//wx_check_box
    engine->RegisterEnumValue("wx_style", "WX_CHK_2STATE", wxCHK_2STATE);
    engine->RegisterEnumValue("wx_style", "WX_CHK_3STATE", wxCHK_3STATE);
    engine->RegisterEnumValue("wx_style", "WX_CHK_ALLOW_3RD_STATE_FOR_USER", wxCHK_ALLOW_3RD_STATE_FOR_USER);
//wx_text_control
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
    engine->RegisterObjectType("wx_event", 0, asOBJ_REF | asOBJ_NOCOUNT);
    engine->RegisterObjectType("wx_key_event", 0, asOBJ_REF | asOBJ_NOCOUNT);
    engine->RegisterObjectType("wx_mouse_event", 0, asOBJ_REF | asOBJ_NOCOUNT);

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
    engine->RegisterObjectMethod("wx_check_box", "bool get_value()", asMETHOD(wxCheckBox, GetValue), asCALL_THISCALL);    
    engine->RegisterObjectMethod("wx_check_box", "void set_value(bool value)", asMETHOD(wxCheckBox, SetValue), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_check_box", "wx_checkbox_state get_3state_value()", asMETHOD(wxCheckBox, Get3StateValue), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_check_box", "void set_3state_value(wx_checkbox_state state)", asMETHOD(wxCheckBox, Set3StateValue), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_check_box", "bool is_3rd_state_allowed_for_user()", asMETHOD(wxCheckBox, Is3rdStateAllowedForUser), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_check_box", "bool is_3state()", asMETHOD(wxCheckBox, Is3State), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_check_box", "bool is_checked()", asMETHOD(wxCheckBox, IsChecked), asCALL_THISCALL);
    REGISTER_WX_CONTROL("wx_static_text", wxStaticText);
    engine->RegisterObjectMethod("wx_static_text", "void wrap(int width)", asMETHOD(wxStaticText, Wrap), asCALL_THISCALL);
    REGISTER_WX_CONTROL("wx_text_control", wxTextCtrl);
    engine->RegisterObjectMethod("wx_text_control", "wx_text_entry@ opImplCast()", asFUNCTION(to_text_entry_as_win<wxTextCtrl>), asCALL_CDECL_OBJFIRST); \
    REG_TEXT_ENTRY_METHODS("wx_text_control");

    engine->RegisterObjectMethod("wx_event", "wx_key_event@ opCast()", asFUNCTION(event_to_derived<wxKeyEvent>), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectMethod("wx_event", "wx_mouse_event@ opCast()", asFUNCTION(event_to_derived<wxMouseEvent>), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectMethod("wx_event", "wx_window@ get_event_object()", asFUNCTION(wx_event_get_event_object), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_event", "int get_event_type()", asMETHOD(wxEvent, GetEventType), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_event", "void skip(bool skip = true)", asMETHOD(wxEvent, Skip), asCALL_THISCALL);

    engine->RegisterObjectMethod("wx_key_event", "int get_key_code()", asMETHOD(wxKeyEvent, GetKeyCode), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_key_event", "int get_unicode_key()", asMETHOD(wxKeyEvent, GetUnicodeKey), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_key_event", "bool control_down()", asFUNCTION(wx_key_event_control_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_key_event", "bool shift_down()", asFUNCTION(wx_key_event_shift_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_key_event", "bool alt_down()", asFUNCTION(wx_key_event_alt_down), asCALL_CDECL_OBJFIRST);

    engine->RegisterObjectMethod("wx_mouse_event", "bool button()", asMETHOD(wxMouseEvent, Button), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool button_down()", asMETHOD(wxMouseEvent, ButtonDown), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool button_up()", asMETHOD(wxMouseEvent, ButtonUp), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool button_dclick()", asMETHOD(wxMouseEvent, ButtonDClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool left_down()", asMETHOD(wxMouseEvent, LeftDown), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool left_up()", asMETHOD(wxMouseEvent, LeftUp), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool left_dclick()", asMETHOD(wxMouseEvent, LeftDClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool middle_down()", asMETHOD(wxMouseEvent, MiddleDown), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool middle_up()", asMETHOD(wxMouseEvent, MiddleUp), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool middle_dclick()", asMETHOD(wxMouseEvent, MiddleDClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool right_down()", asMETHOD(wxMouseEvent, RightDown), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool right_up()", asMETHOD(wxMouseEvent, RightUp), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool right_dclick()", asMETHOD(wxMouseEvent, RightDClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux1_down()", asMETHOD(wxMouseEvent, Aux1Down), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux1_up()", asMETHOD(wxMouseEvent, Aux1Up), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux1_dclick()", asMETHOD(wxMouseEvent, Aux1DClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux2_down()", asMETHOD(wxMouseEvent, Aux2Down), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux2_up()", asMETHOD(wxMouseEvent, Aux2Up), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux2_dclick()", asMETHOD(wxMouseEvent, Aux2DClick), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool left_is_down()", asFUNCTION(wx_mouse_event_left_is_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool middle_is_down()", asFUNCTION(wx_mouse_event_middle_is_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool right_is_down()", asFUNCTION(wx_mouse_event_right_is_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux1_is_down()", asFUNCTION(wx_mouse_event_aux1_is_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "bool aux2_is_down()", asFUNCTION(wx_mouse_event_aux2_is_down), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "int get_x()", asFUNCTION(wx_mouse_event_get_x), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "int get_y()", asFUNCTION(wx_mouse_event_get_y), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_mouse_event", "int get_wheel_rotation()", asMETHOD(wxMouseEvent, GetWheelRotation), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "int get_wheel_delta()", asMETHOD(wxMouseEvent, GetWheelDelta), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool dragging()", asMETHOD(wxMouseEvent, Dragging), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool moving()", asMETHOD(wxMouseEvent, Moving), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool entering()", asMETHOD(wxMouseEvent, Entering), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx_mouse_event", "bool leaving()", asMETHOD(wxMouseEvent, Leaving), asCALL_THISCALL);

    engine->RegisterObjectType("wx", sizeof(WxManager), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C);
    engine->RegisterObjectBehaviour("wx", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(WxConstructor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(WxDestructor), asCALL_CDECL_OBJLAST);
    
    engine->RegisterObjectMethod("wx", "void update()", asMETHOD(WxManager, update), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_frame@ create_frame(const string &in title, int width, int height, int style = WX_DEFAULT_FRAME_STYLE)", asMETHOD(WxManager, create_frame), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_button@ create_button(wx_window@, const string &in label, int style = 0)", asMETHOD(WxManager, create_button), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_check_box@ create_check_box(wx_window@, const string &in label, int style = 0)", asMETHOD(WxManager, create_check_box), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_box_sizer@ create_box_sizer(int orientation)", asMETHOD(WxManager, create_box_sizer), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_panel@ create_panel(wx_window@, int style = WX_TAB_TRAVERSAL)", asMETHOD(WxManager, create_panel), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_static_text@ create_static_text(wx_window@, const string &in label, int style = 0)", asMETHOD(WxManager, create_static_text), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_text_control@ create_text_control(wx_window@, const string &in text = \"\", int style = 0)", asMETHOD(WxManager, create_text_control), asCALL_THISCALL);
    return true;
}