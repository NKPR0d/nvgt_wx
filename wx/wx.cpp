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

std::map<void*, int> g_ref_counts;

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

static const int c_wxVERTICAL = wxVERTICAL;
static const int c_wxHORIZONTAL = wxHORIZONTAL;
static const int c_wxEXPAND = wxEXPAND;
static const int c_wxALL = wxALL;
static const int c_wxEVT_BUTTON = wxEVT_BUTTON;
static const int c_wxBOTH = wxBOTH;

struct ScriptEventData {
    asIScriptFunction* callback;
    asIScriptEngine* engine;
};

std::map<int, ScriptEventData> g_event_handlers;

void OnWxEvent(wxCommandEvent& event) {
    int id = event.GetId();
    if (g_event_handlers.count(id)) {
        ScriptEventData& data = g_event_handlers[id];
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
    int id = self->GetId();
    g_event_handlers[id] = { callback, callback->GetEngine() };
    self->Bind(wxEVT_BUTTON, &OnWxEvent, id);
}

void wx_window_set_tool_tip(wxWindow* self, const std::string& text) {
    if (self) self->SetToolTip(wxString::FromUTF8(text.c_str()));
}

void wx_window_set_background_colour(wxWindow* self, int r, int g, int b) {
    if (self) {
        self->SetBackgroundColour(wxColour(r, g, b));
        self->Refresh();
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

void wx_window_set_size(wxWindow* self, int w, int h) {
    if (self) self->SetSize(w, h);
}

void wx_control_set_label(wxControl* self, const std::string& label) { if (self) self->SetLabel(wxString::FromUTF8(label.c_str())); }
std::string wx_control_get_label(wxControl* self) { return self ? std::string(self->GetLabel().utf8_str()) : ""; }

void wx_frame_set_title(wxFrame* self, const std::string& title) {
    if (self) self->SetTitle(wxString::FromUTF8(title.c_str()));
}

void wx_sizer_add(wxSizer* self, wxWindow* window, int proportion, int flag, int border) {
    if (self && window) self->Add(window, proportion, flag, border);
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
        if (wxTheApp) {
            wxEventLoopBase* loop = wxEventLoop::GetActive();
            if (loop) { while (loop->Pending()) loop->Dispatch(); }
            wxTheApp->ProcessPendingEvents();
        }
    }

    wxFrame* create_frame(const std::string& title, int w, int h) {
        wxFrame* f = new wxFrame(NULL, wxID_ANY, wxString::FromUTF8(title.c_str()), wxDefaultPosition, wxSize(w, h));
        AddRef(f);
        return f;
    }

    wxButton* create_button(wxWindow* parent, const std::string& label) {
        wxButton* b = new wxButton(parent, wxID_ANY, wxString::FromUTF8(label.c_str()));
        AddRef(b);
        return b;
    }

    wxBoxSizer* create_box_sizer(int orient) {
        wxBoxSizer* b = new wxBoxSizer(orient);
        AddRef(b);
        return b;
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
    engine->RegisterObjectMethod(name, "void bind(int, wx_callback@)", asFUNCTION(wx_window_bind), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_tool_tip(const string &in)", asFUNCTION(wx_window_set_tool_tip), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_background_colour(int, int, int)", asFUNCTION(wx_window_set_background_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_foreground_colour(int, int, int)", asFUNCTION(wx_window_set_foreground_colour), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void get_size(int &out, int &out)", asFUNCTION(wx_window_get_size), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_size(int, int)", asFUNCTION(wx_window_set_size), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "bool is_shown()", asMETHOD(wxWindow, IsShown), asCALL_THISCALL); \
    engine->RegisterObjectMethod(name, "bool is_enabled()", asMETHOD(wxWindow, IsEnabled), asCALL_THISCALL);

#define REG_CONTROL_METHODS(name) \
    engine->RegisterObjectMethod(name, "string get_label()", asFUNCTION(wx_control_get_label), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(name, "void set_label(const string &in)", asFUNCTION(wx_control_set_label), asCALL_CDECL_OBJFIRST); \

#define REG_SIZER_METHODS(name) \
    engine->RegisterObjectMethod(name, "void add(wx_window@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_add), asCALL_CDECL_OBJFIRST);
#define REGISTER_WX_WINDOW(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    REG_BASE_REF(as_name); \
    engine->RegisterObjectMethod(as_name, "wx_window@ opImplCast()", asFUNCTION(to_window<wx_type>), asCALL_CDECL_OBJFIRST); \
    REG_WINDOW_METHODS(as_name);

#define REGISTER_WX_CONTROL(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    REG_BASE_REF(as_name); \
    engine->RegisterObjectMethod(as_name, "wx_control@ opImplCast()", asFUNCTION(to_control<wx_type>), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(as_name, "wx_window@ opImplCast()", asFUNCTION(to_window<wx_type>), asCALL_CDECL_OBJFIRST); \
    REG_WINDOW_METHODS(as_name); \
    REG_CONTROL_METHODS(as_name);

#define REGISTER_WX_SIZER(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    REG_BASE_REF(as_name); \
    REG_SIZER_METHODS(as_name);

plugin_main(nvgt_plugin_shared* shared) {
    if (!prepare_plugin(shared)) return false;
    asIScriptEngine* engine = shared->script_engine;

    engine->RegisterGlobalProperty("const int wx_VERTICAL", (void*)&c_wxVERTICAL);
    engine->RegisterGlobalProperty("const int wx_HORIZONTAL", (void*)&c_wxHORIZONTAL);
    engine->RegisterGlobalProperty("const int wx_BOTH", (void*)&c_wxBOTH);
    engine->RegisterGlobalProperty("const int wx_EXPAND", (void*)&c_wxEXPAND);
    engine->RegisterGlobalProperty("const int wx_ALL", (void*)&c_wxALL);
    engine->RegisterGlobalProperty("const int wx_EVT_BUTTON", (void*)&c_wxEVT_BUTTON);

    engine->RegisterFuncdef("void wx_callback()");

    engine->RegisterObjectType("wx_window", 0, asOBJ_REF);
    engine->RegisterObjectType("wx_control", 0, asOBJ_REF);

    REG_BASE_REF("wx_window");
    REG_WINDOW_METHODS("wx_window");

    REG_BASE_REF("wx_control");
    engine->RegisterObjectMethod("wx_control", "wx_window@ opImplCast()", asFUNCTION(to_window<wxControl>), asCALL_CDECL_OBJFIRST);
    REG_WINDOW_METHODS("wx_control");
    REG_CONTROL_METHODS("wx_control");

    REGISTER_WX_SIZER("wx_box_sizer", wxBoxSizer);

    REGISTER_WX_WINDOW("wx_frame", wxFrame);
    engine->RegisterObjectMethod("wx_frame", "void set_title(const string &in)", asFUNCTION(wx_frame_set_title), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_frame", "void set_sizer(wx_box_sizer@)", asMETHOD(wxFrame, SetSizer), asCALL_THISCALL);

    REGISTER_WX_CONTROL("wx_button", wxButton);

    engine->RegisterObjectType("wx", sizeof(WxManager), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C);
    engine->RegisterObjectBehaviour("wx", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(WxConstructor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(WxDestructor), asCALL_CDECL_OBJLAST);
    
    engine->RegisterObjectMethod("wx", "void update()", asMETHOD(WxManager, update), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_frame@ create_frame(const string &in, int, int)", asMETHOD(WxManager, create_frame), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_button@ create_button(wx_window@, const string &in)", asMETHOD(WxManager, create_button), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_box_sizer@ create_box_sizer(int)", asMETHOD(WxManager, create_box_sizer), asCALL_THISCALL);

    return true;
}