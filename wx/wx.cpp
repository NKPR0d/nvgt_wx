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
    if (asAtomicDec(g_ref_counts[ptr]) <= 0) {
        g_ref_counts.erase(ptr);
        for (auto it = g_event_handlers.begin(); it != g_event_handlers.end(); ) {
            if (it->first.first == ptr) {
                it->second.callback->Release();
                it = g_event_handlers.erase(it);
            } else {
                ++it;
            }
        }
        wxObject* wx_obj = static_cast<wxObject*>(ptr);
        wxWindow* win = dynamic_cast<wxWindow*>(wx_obj);
        if (win) {
            if (!win->GetParent()) win->Destroy();
            return;
        }
        wxSizer* sizer = dynamic_cast<wxSizer*>(wx_obj);
        if (sizer) {
            delete sizer;
        }
    }
}

template<typename T> wxWindow* to_window(T* obj) { if (obj) AddRef(obj); return static_cast<wxWindow*>(obj); }
template<typename T> wxControl* to_control(T* obj) { if (obj) AddRef(obj); return static_cast<wxControl*>(obj); }
template<typename T> wxSizer* to_sizer(T* obj) { if (obj) AddRef(obj); return static_cast<wxSizer*>(obj); }
template<typename T> wxTopLevelWindow* to_tlw(T* obj) { if (obj) AddRef(obj); return static_cast<wxTopLevelWindow*>(obj); }

void OnWxEvent(wxEvent& event) {
    void* self = event.GetEventObject();
    int type = event.GetEventType();    
    EventKey key = { self, type };
    auto it = g_event_handlers.find(key);
    if (it != g_event_handlers.end()) {
        ScriptEventData& data = it->second;
        asIScriptContext* ctx = GetContextFromPool(data.engine);
        if (ctx) {
            ctx->Prepare(data.callback);
            ctx->Execute();
            ReturnContextToPool(ctx);
        }
    }
    event.Skip();
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

void wx_window_get_size(wxWindow* self, int &out_w, int &out_h) {
    if (self) self->GetSize(&out_w, &out_h);
}

wxSizer* wx_window_get_sizer(wxWindow* self) {
    wxSizer* s = self->GetSizer();
    if (s) AddRef(s);
    return s;
}

void wx_window_set_size(wxWindow* self, int w, int h) {
    if (self) self->SetSize(w, h);
}

std::string wx_control_get_label(wxControl* self) { return self ? std::string(self->GetLabel().utf8_str()) : ""; }
void wx_control_set_label(wxControl* self, const std::string& label) { if (self) self->SetLabel(wxString::FromUTF8(label.c_str())); }

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

void wx_text_ctrl_set_value(wxTextCtrl* self, const std::string& value) {
    if (self) self->SetValue(wxString::FromUTF8(value.c_str()));
}

std::string wx_text_ctrl_get_value(wxTextCtrl* self) {
    return self ? std::string(self->GetValue().utf8_str()) : "";
}

void wx_text_ctrl_write_text(wxTextCtrl* self, const std::string& text) {
    if (self) self->WriteText(wxString::FromUTF8(text.c_str()));
}

void wx_text_ctrl_append_text(wxTextCtrl* self, const std::string& text) {
    if (self) self->AppendText(wxString::FromUTF8(text.c_str()));
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
        wxEventLoopBase *loop = wxEventLoop::GetActive();
        bool created = false;
        if (!loop) {
            loop = new wxGUIEventLoop();
            wxEventLoop::SetActive(loop);
            created = true;
        }
        while (loop && loop->Pending()) {
            loop->Dispatch();
        }
        if (created) {
            wxEventLoop::SetActive(NULL);
            delete loop;
        }
    }
    wxFrame* create_frame(const std::string& title, int w, int h, long style = wxDEFAULT_FRAME_STYLE) {
        wxFrame* f = new wxFrame(NULL, wxID_ANY, wxString::FromUTF8(title.c_str()), 
                                 wxDefaultPosition, wxSize(w, h), style);
        AddRef(f);
        return f;
    }

    wxButton* create_button(wxWindow* parent, const std::string& label, long style = 0) {
        wxButton* b = new wxButton(parent, wxID_ANY, wxString::FromUTF8(label.c_str()), 
                                   wxDefaultPosition, wxDefaultSize, style);
        AddRef(b);
        return b;
    }

    wxBoxSizer* create_box_sizer(int orient) {
        wxBoxSizer* b = new wxBoxSizer(orient);
        AddRef(b);
        return b;
    }

    wxPanel* create_panel(wxWindow* parent, long style = wxTAB_TRAVERSAL) {
        wxPanel* p = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, style);
        AddRef(p);
        return p;
    }

    wxStaticText* create_static_text(wxWindow* parent, const std::string& label, long style = 0) {
        wxStaticText* t = new wxStaticText(parent, wxID_ANY, wxString::FromUTF8(label.c_str()), 
                                           wxDefaultPosition, wxDefaultSize, style);
        AddRef(t);
        return t;
    }

    wxTextCtrl* create_text_ctrl(wxWindow* parent, const std::string& value = "", long style = 0) {
        wxTextCtrl* t = new wxTextCtrl(parent, wxID_ANY, wxString::FromUTF8(value.c_str()), 
                                       wxDefaultPosition, wxDefaultSize, style);
        AddRef(t);
        return t;
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
    engine->RegisterObjectMethod(name, "bool enable(bool e = true)", asMETHOD(wxWindow, Enable), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool disable()", asMETHOD(wxWindow, Disable), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool has_focus()", asMETHOD(wxWindow, HasFocus), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void set_focus()", asMETHOD(wxWindow, SetFocus), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void layout()", asMETHOD(wxWindow, Layout), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void center(int direction = 3)", asMETHOD(wxWindow, Center), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "int get_id()", asMETHOD(wxWindow, GetId), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void refresh()", asMETHOD(wxWindow, Refresh), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void update()", asMETHOD(wxWindow, Update), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool destroy()", asMETHOD(wxWindow, Destroy), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void bind(wx_event_type, wx_callback@)", asFUNCTION(wx_window_bind), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "string get_tool_tip()", asFUNCTION(wx_window_get_tool_tip), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_tool_tip(const string &in)", asFUNCTION(wx_window_set_tool_tip), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void get_background_colour(int &out, int &out, int &out)", asFUNCTION(wx_window_get_background_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_background_colour(int, int, int)", asFUNCTION(wx_window_set_background_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void get_foreground_colour(int &out, int &out, int &out)", asFUNCTION(wx_window_get_foreground_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_foreground_colour(int, int, int)", asFUNCTION(wx_window_set_foreground_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "wx_sizer@ get_sizer()", asFUNCTION(wx_window_get_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_sizer(wx_sizer@)", asMETHOD(wxWindow, SetSizer), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "void get_size(int &out, int &out)", asFUNCTION(wx_window_get_size), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_size(int, int)", asFUNCTION(wx_window_set_size), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool is_shown()", asMETHOD(wxWindow, IsShown), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_enabled()", asMETHOD(wxWindow, IsEnabled), asCALL_THISCALL);

#define REG_TLW_METHODS(name) \
    engine->RegisterObjectMethod(name, "string get_title()", asFUNCTION(wx_tlw_get_title), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_title(const string &in)", asFUNCTION(wx_tlw_set_title), asCALL_CDECL_OBJFIRST); \
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
    engine->RegisterObjectMethod(name, "void set_label(const string &in)", asFUNCTION(wx_control_set_label), asCALL_CDECL_OBJFIRST); \

#define REG_SIZER_METHODS(name) \
    engine->RegisterObjectMethod(name, "void add(wx_window@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_add_window), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void add(wx_sizer@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_add_sizer), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void layout()", asMETHOD(wxSizer, Layout), asCALL_THISCALL);

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
    REG_WINDOW_METHODS(as_name); \
    REG_TLW_METHODS(as_name);

#define REGISTER_WX_CONTROL(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    REG_BASE_REF(as_name); \
    if (std::string(as_name) != "wx_control") { \
        engine->RegisterObjectMethod(as_name, "wx_control@ opImplCast()", asFUNCTION(to_control<wx_type>), asCALL_CDECL_OBJFIRST); \
    } \
    engine->RegisterObjectMethod(as_name, "wx_window@ opImplCast()", asFUNCTION(to_window<wx_type>), asCALL_CDECL_OBJFIRST); \
    REG_WINDOW_METHODS(as_name); \
    REG_CONTROL_METHODS(as_name);

#define REGISTER_WX_SIZER(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    REG_BASE_REF(as_name); \
    if (std::string(as_name) != "wx_sizer") { \
        engine->RegisterObjectMethod(as_name, "wx_sizer@ opImplCast()", asFUNCTION(to_sizer<wx_type>), asCALL_CDECL_OBJFIRST); \
    } \
    REG_SIZER_METHODS(as_name);

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
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_BUTTON", wxEVT_BUTTON);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_TEXT", wxEVT_TEXT);
    engine->RegisterEnumValue("wx_event_type", "WX_EVT_TEXT_ENTER", wxEVT_TEXT_ENTER);

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
//wx_text_control
    engine->RegisterEnumValue("wx_style", "WX_TE_MULTILINE", wxTE_MULTILINE);
    engine->RegisterEnumValue("wx_style", "WX_TE_PASSWORD", wxTE_PASSWORD);
    engine->RegisterEnumValue("wx_style", "WX_TE_READONLY", wxTE_READONLY);
    engine->RegisterEnumValue("wx_style", "WX_TE_PROCESS_ENTER", wxTE_PROCESS_ENTER);

    engine->RegisterEnum("wx_user_attention");
    engine->RegisterEnumValue("wx_user_attention", "WX_USER_ATTENTION_INFO", wxUSER_ATTENTION_INFO);
    engine->RegisterEnumValue("wx_user_attention", "WX_USER_ATTENTION_ERROR", wxUSER_ATTENTION_ERROR);

    engine->RegisterFuncdef("void wx_callback()");

    engine->RegisterObjectType("wx_window", 0, asOBJ_REF);
    engine->RegisterObjectType("wx_top_level_window", 0, asOBJ_REF);
    engine->RegisterObjectType("wx_control", 0, asOBJ_REF);
    engine->RegisterObjectType("wx_sizer", 0, asOBJ_REF);

    REGISTER_WX_WINDOW("wx_window", wxWindow);

    REGISTER_WX_TLW("wx_top_level_window", wxTopLevelWindow);
    REGISTER_WX_CONTROL("wx_control", wxControl);

    REGISTER_WX_SIZER("wx_sizer", wxSizer);

    REGISTER_WX_SIZER("wx_box_sizer", wxBoxSizer);

    REGISTER_WX_TLW("wx_frame", wxFrame);
    REGISTER_WX_WINDOW("wx_panel", wxPanel);

    REGISTER_WX_CONTROL("wx_button", wxButton);
    REGISTER_WX_CONTROL("wx_static_text", wxStaticText);
    REGISTER_WX_CONTROL("wx_text_ctrl", wxTextCtrl);
    engine->RegisterObjectMethod("wx_text_ctrl", "string get_value()", asFUNCTION(wx_text_ctrl_get_value), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_text_ctrl", "void set_value(const string &in)", asFUNCTION(wx_text_ctrl_set_value), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_text_ctrl", "void write_text(const string &in)", asFUNCTION(wx_text_ctrl_write_text), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_text_ctrl", "void append_text(const string &in)", asFUNCTION(wx_text_ctrl_append_text), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_text_ctrl", "void clear()", asMETHOD(wxTextCtrl, Clear), asCALL_THISCALL);

    engine->RegisterObjectType("wx", sizeof(WxManager), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C);
    engine->RegisterObjectBehaviour("wx", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(WxConstructor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(WxDestructor), asCALL_CDECL_OBJLAST);
    
    engine->RegisterObjectMethod("wx", "void update()", asMETHOD(WxManager, update), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_frame@ create_frame(const string &in, int, int, int style = WX_DEFAULT_FRAME_STYLE)", 
    asMETHOD(WxManager, create_frame), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_button@ create_button(wx_window@, const string &in, int style = 0)", 
    asMETHOD(WxManager, create_button), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_box_sizer@ create_box_sizer(int)", asMETHOD(WxManager, create_box_sizer), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_panel@ create_panel(wx_window@, int style = WX_TAB_TRAVERSAL)", 
    asMETHOD(WxManager, create_panel), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_static_text@ create_static_text(wx_window@, const string &in, int style = 0)", asMETHOD(WxManager, create_static_text), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_text_ctrl@ create_text_ctrl(wx_window@, const string &in = \"\", int style = 0)", asMETHOD(WxManager, create_text_ctrl), asCALL_THISCALL);
    return true;
}