// WxManager: the value-type the script side sees as `wx`. The constructor
// boots wxWidgets via wxEntryStart/CallOnInit; the update() method pumps
// the message loop; the create_* methods are factories for every wx widget
// the bridge exposes. Everything in this file is private to the
// translation unit except register_wx_manager(), which register.cpp calls
// into to register the type and its methods on the AngelScript engine.

#include "common.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

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

    wxFrame* create_frame(const std::string& title, const wx_size& size, long style = wxDEFAULT_FRAME_STYLE) {
        return Track(new wxFrame(NULL, wxID_ANY, wxString::FromUTF8(title.c_str()), wxDefaultPosition, size, style));
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
        return Track(new wxCheckBox(parent, wxID_ANY, wx_s, wxDefaultPosition, wxDefaultSize, style, wxDefaultValidator, wx_s));
    }

    wxRadioButton* create_radio_button(wxWindow* parent, const std::string& label, long style = 0) {
        wxString wx_s = wxString::FromUTF8(label.c_str());
        return Track(new wxRadioButton(parent, wxID_ANY, wx_s, wxDefaultPosition, wxDefaultSize, style, wxDefaultValidator, wx_s));
    }
};

void WxConstructor(WxManager* self) { new(self) WxManager(); }
void WxDestructor(WxManager* self) { self->~WxManager(); }

} // namespace

void register_wx_manager(asIScriptEngine* engine) {
    engine->RegisterObjectType("wx", sizeof(WxManager), asOBJ_VALUE | asOBJ_APP_CLASS_CD);
    engine->RegisterObjectBehaviour("wx", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(WxConstructor), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("wx", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(WxDestructor), asCALL_CDECL_OBJLAST);

    engine->RegisterObjectMethod("wx", "void update()", asMETHOD(WxManager, update), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_frame@ create_frame(const string &in title, const wx_size &in size, int style = WX_DEFAULT_FRAME_STYLE)", asMETHOD(WxManager, create_frame), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_button@ create_button(wx_window@, const string &in label, int style = 0)", asMETHOD(WxManager, create_button), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_check_box@ create_check_box(wx_window@, const string &in label, int style = 0)", asMETHOD(WxManager, create_check_box), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_box_sizer@ create_box_sizer(int orientation)", asMETHOD(WxManager, create_box_sizer), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_panel@ create_panel(wx_window@, int style = WX_TAB_TRAVERSAL)", asMETHOD(WxManager, create_panel), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_static_text@ create_static_text(wx_window@, const string &in label, int style = 0)", asMETHOD(WxManager, create_static_text), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_text_control@ create_text_control(wx_window@, const string &in text = \"\", int style = 0)", asMETHOD(WxManager, create_text_control), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_radio_button@ create_radio_button(wx_window@, const string &in label, int style = 0)", asMETHOD(WxManager, create_radio_button), asCALL_THISCALL);
}
