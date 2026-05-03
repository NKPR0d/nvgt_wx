// WxManager: the value-type the script side sees as `wx`. The constructor
// boots wxWidgets via wxEntryStart/CallOnInit; the update() method pumps
// the message loop; the create_* methods are factories for every wx widget
// the bridge exposes. Everything in this file is private to the
// translation unit except register_wx_manager(), which register.cpp calls
// into to register the type and its methods on the AngelScript engine.
//
// wxEntryCleanup() is paired with wxEntryStart() through a static guard
// (`g_wx_lifecycle`, below). The guard's destructor runs at C++ static
// deinitialisation — on Windows that fires from DllMain on
// DLL_PROCESS_DETACH, after AngelScript has released every wx object,
// which is the safe moment to tear wx down. Pairing it with the
// WxManager destructor instead would be wrong: WxManager is a value
// type, and a script that lets `wx` go out of scope while wx-windows are
// still alive would otherwise nuke wxApp under them.

#include "common.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

class NVGTApp : public wxApp {
public:
    bool OnInit() override { return true; }
};

// Tracks whether *this plugin* started wxWidgets. NVGT itself does not
// run a wxApp, so under normal use this flips to true on the first
// WxManager() constructor call. If a future host pre-creates a wxApp,
// the plugin defers to it and the lifecycle guard below stays a no-op.
bool g_wx_started_by_plugin = false;

class WxLifecycleGuard {
public:
    WxLifecycleGuard() = default;
    ~WxLifecycleGuard() {
        if (g_wx_started_by_plugin && wxTheApp) {
            // Drain anything wxWidgets queued during teardown so
            // wxEntryCleanup sees a quiescent message pump.
            wxTheApp->ProcessPendingEvents();
            wxEntryCleanup();
            g_wx_started_by_plugin = false;
        }
    }
};
WxLifecycleGuard g_wx_lifecycle;

class WxManager {
public:
    WxManager() {
        if (!wxTheApp) {
            wxApp::SetInstance(new NVGTApp());
            int argc = 0;
            char** argv = nullptr;
            wxEntryStart(argc, argv);
            wxTheApp->CallOnInit();
            g_wx_started_by_plugin = true;
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

    // wxGridSizer ctor variants. The plugin exposes the (rows, cols, vgap, hgap)
    // form because it covers everything the (cols, vgap, hgap) and (cols, gap)
    // forms do (pass rows=0 for "auto-derive"); collapsing them into a single
    // factory keeps the AS-side surface unambiguous and lets argument defaults
    // work without overload resolution.
    wxGridSizer* create_grid_sizer(int rows, int cols, int vgap = 0, int hgap = 0) {
        wxGridSizer* s = new wxGridSizer(rows, cols, vgap, hgap);
        AddRef(s);
        return s;
    }

    // wxFlexGridSizer mirrors the wxGridSizer ctor form. Flexible-direction
    // and non-flexible-grow-mode are runtime properties; scripts set them
    // after construction via the registered methods.
    wxFlexGridSizer* create_flex_grid_sizer(int rows, int cols, int vgap = 0, int hgap = 0) {
        wxFlexGridSizer* s = new wxFlexGridSizer(rows, cols, vgap, hgap);
        AddRef(s);
        return s;
    }

    // wxGridBagSizer's only ctor takes vgap and hgap (note the wx parameter
    // order is (vgap, hgap) — opposite of how grid_sizer's ctor parameters
    // are listed in the wx documentation table). Items are added through
    // Add(window, wxGBPosition, wxGBSpan, …) overloads, registered as
    // gb.add(...) on the AS side.
    wxGridBagSizer* create_grid_bag_sizer(int vgap = 0, int hgap = 0) {
        wxGridBagSizer* s = new wxGridBagSizer(vgap, hgap);
        AddRef(s);
        return s;
    }

    // wxWrapSizer takes an orientation and a flag bitmask. The wx default for
    // flags is wxWRAPSIZER_DEFAULT_FLAGS (extend-last + remove-leading-spaces),
    // which is what scripts almost always want; expose it as the AS-side
    // default so `wx.create_wrap_sizer(WX_HORIZONTAL)` does the right thing.
    wxWrapSizer* create_wrap_sizer(int orient = wxHORIZONTAL,
                                   int flags = wxWRAPSIZER_DEFAULT_FLAGS) {
        wxWrapSizer* s = new wxWrapSizer(orient, flags);
        AddRef(s);
        return s;
    }

    // wxStaticBox is registered as a control even though scripts most often
    // never reference it by handle: wxStaticBoxSizer takes an existing box
    // (first ctor form) or creates one internally (second ctor form). The
    // factory exists so a script that wants the first form can build the
    // box up-front and re-use it.
    wxStaticBox* create_static_box(wxWindow* parent, const std::string& label, long style = 0) {
        wxString wx_s = wxString::FromUTF8(label.c_str());
        return Track(new wxStaticBox(parent, wxID_ANY, wx_s, wxDefaultPosition, wxDefaultSize, style, wx_s));
    }

    // wxStaticBoxSizer has two ctors. The (orient, parent, label) form is the
    // common one — wxWidgets creates the static box internally and parents it
    // to `parent`, so the box dies with the parent and the bridge does not
    // own it. The (box, orient) form takes an existing wxStaticBox; the box
    // must already be a child of the window the sizer will live in. Both are
    // exposed so scripts can pick whichever fits their construction order.
    wxStaticBoxSizer* create_static_box_sizer(int orient, wxWindow* parent,
                                              const std::string& label = "") {
        wxStaticBoxSizer* s = new wxStaticBoxSizer(orient, parent, wxString::FromUTF8(label.c_str()));
        AddRef(s);
        return s;
    }

    wxStaticBoxSizer* create_static_box_sizer_for_box(wxStaticBox* box, int orient) {
        wxStaticBoxSizer* s = new wxStaticBoxSizer(box, orient);
        AddRef(s);
        return s;
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
    engine->RegisterObjectMethod("wx", "wx_grid_sizer@ create_grid_sizer(int rows, int cols, int vgap = 0, int hgap = 0)", asMETHOD(WxManager, create_grid_sizer), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_flex_grid_sizer@ create_flex_grid_sizer(int rows, int cols, int vgap = 0, int hgap = 0)", asMETHOD(WxManager, create_flex_grid_sizer), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_grid_bag_sizer@ create_grid_bag_sizer(int vgap = 0, int hgap = 0)", asMETHOD(WxManager, create_grid_bag_sizer), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_wrap_sizer@ create_wrap_sizer(int orient = WX_HORIZONTAL, int flags = WX_WRAPSIZER_DEFAULT_FLAGS)", asMETHOD(WxManager, create_wrap_sizer), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_static_box@ create_static_box(wx_window@, const string &in label, int style = 0)", asMETHOD(WxManager, create_static_box), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_static_box_sizer@ create_static_box_sizer(int orient, wx_window@, const string &in label = \"\")", asMETHOD(WxManager, create_static_box_sizer), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_static_box_sizer@ create_static_box_sizer_for_box(wx_static_box@, int orient)", asMETHOD(WxManager, create_static_box_sizer_for_box), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_panel@ create_panel(wx_window@, int style = WX_TAB_TRAVERSAL)", asMETHOD(WxManager, create_panel), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_static_text@ create_static_text(wx_window@, const string &in label, int style = 0)", asMETHOD(WxManager, create_static_text), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_text_control@ create_text_control(wx_window@, const string &in text = \"\", int style = 0)", asMETHOD(WxManager, create_text_control), asCALL_THISCALL);
    engine->RegisterObjectMethod("wx", "wx_radio_button@ create_radio_button(wx_window@, const string &in label, int style = 0)", asMETHOD(WxManager, create_radio_button), asCALL_THISCALL);
}
