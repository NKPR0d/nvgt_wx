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

template<typename T>
wxWindow* to_window(T* obj) {
    if (!obj) return nullptr;
    AddRef(obj);
    return static_cast<wxWindow*>(obj);
}

static const int c_wxVERTICAL = wxVERTICAL;
static const int c_wxHORIZONTAL = wxHORIZONTAL;
static const int c_wxEXPAND = wxEXPAND;
static const int c_wxALL = wxALL;
static const int c_wxEVT_BUTTON = wxEVT_BUTTON;

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

void wx_window_show(wxWindow* self, bool s) {
    if (self) self->Show(s);
}

void wx_window_hide(wxWindow* self) { 
    if (self) self->Hide(); 
}

void wx_window_bind(wxWindow* self, int event_type, asIScriptFunction* callback) {
    if (!self || !callback) return;
    int id = self->GetId();
    g_event_handlers[id] = { callback, callback->GetEngine() };
    self->Bind(wxEVT_BUTTON, &OnWxEvent, id);
}

void wx_frame_set_title(wxFrame* self, const std::string& title) {
    if (self) self->SetTitle(wxString::FromUTF8(title.c_str()));
}

void wx_sizer_add(wxSizer* self, wxWindow* window, int proportion = 0, int flag = 0, int border = 0) {
    if (self && window) self->Add(window, proportion, flag, border);
}
class MiniApp : public wxApp {
public:
    bool OnInit() override { return true; }
};

class WxManager {
public:
    WxManager() {
        if (!wxTheApp) {
            wxApp::SetInstance(new MiniApp());
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

#define REGISTER_WX_TYPE(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    engine->RegisterObjectBehaviour(as_name, asBEHAVE_ADDREF, "void f()", asFUNCTION(AddRef), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectBehaviour(as_name, asBEHAVE_RELEASE, "void f()", asFUNCTION(Release), asCALL_CDECL_OBJFIRST); \
    if (std::string(as_name) != "wx_window") { \
        engine->RegisterObjectMethod(as_name, "wx_window@ opImplCast()", asFUNCTION(to_window<wx_type>), asCALL_CDECL_OBJFIRST); \
    } \
    engine->RegisterObjectMethod(as_name, "void show(bool visible = true)", asFUNCTION(wx_window_show), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(as_name, "void hide()", asFUNCTION(wx_window_hide), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(as_name, "void bind(int, wx_callback@)", asFUNCTION(wx_window_bind), asCALL_CDECL_OBJFIRST);

#define REGISTER_SIZER_TYPE(as_name, wx_type) \
    engine->RegisterObjectType(as_name, 0, asOBJ_REF); \
    engine->RegisterObjectBehaviour(as_name, asBEHAVE_ADDREF, "void f()", asFUNCTION(AddRef), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectBehaviour(as_name, asBEHAVE_RELEASE, "void f()", asFUNCTION(Release), asCALL_CDECL_OBJFIRST); \
    engine->RegisterObjectMethod(as_name, "void add(wx_window@, int proportion = 0, int flag = 0, int border = 0)", asFUNCTION(wx_sizer_add), asCALL_CDECL_OBJFIRST);

plugin_main(nvgt_plugin_shared* shared) {
    if (!prepare_plugin(shared)) return false;
    asIScriptEngine* engine = shared->script_engine;

    engine->RegisterGlobalProperty("const int wx_VERTICAL", (void*)&c_wxVERTICAL);
    engine->RegisterGlobalProperty("const int wx_HORIZONTAL", (void*)&c_wxHORIZONTAL);
    engine->RegisterGlobalProperty("const int wx_EXPAND", (void*)&c_wxEXPAND);
    engine->RegisterGlobalProperty("const int wx_ALL", (void*)&c_wxALL);
    engine->RegisterGlobalProperty("const int wx_EVT_BUTTON", (void*)&c_wxEVT_BUTTON);

    engine->RegisterFuncdef("void wx_callback()");

    REGISTER_WX_TYPE("wx_window", wxWindow);
    REGISTER_WX_TYPE("wx_frame", wxFrame);
    REGISTER_WX_TYPE("wx_button", wxButton);

    REGISTER_SIZER_TYPE("wx_box_sizer", wxBoxSizer);

    engine->RegisterObjectMethod("wx_frame", "void set_title(const string &in)", asFUNCTION(wx_frame_set_title), asCALL_CDECL_OBJFIRST);
    engine->RegisterObjectMethod("wx_frame", "void set_sizer(wx_box_sizer@)", asMETHOD(wxFrame, SetSizer), asCALL_THISCALL);

    engine->RegisterObjectType("wx", sizeof(WxManager), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C);
    engine->RegisterObjectBehaviour("wx", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(WxConstructor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(WxDestructor), asCALL_CDECL_OBJLAST);
    
    engine->RegisterObjectMethod("wx", "void update()", asMETHOD(WxManager, update), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_frame@ create_frame(const string &in, int, int)", asMETHOD(WxManager, create_frame), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_button@ create_button(wx_window@, const string &in)", asMETHOD(WxManager, create_button), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_box_sizer@ create_box_sizer(int)", asMETHOD(WxManager, create_box_sizer), asCALL_THISCALL);

    return true;
}