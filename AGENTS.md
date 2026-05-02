# AGENTS.md

Guidance for humans and AI agents contributing to **nvgt_wx**, a wxWidgets plugin for NVGT.

## What this repo is

`nvgt_wx` is a native plugin for the NonVisual Gaming Toolkit (NVGT)
that exposes wxWidgets to AngelScript. NVGT itself uses SDL3 for its own
windowing; this plugin runs a parallel `wxApp` so scripts can build
accessible, native GUIs.

NVGT upstream: https://github.com/samtupy/nvgt
wxWidgets upstream: https://www.wxwidgets.org/

## Repository layout

```
LICENSE                MIT license.
README.md              Short description.
AGENTS.md              This file.
wx/
├── _SConscript        SCons build script. Plugged into NVGT's plugin/ tree.
├── wx.cpp             All AngelScript bindings live here.
├── dll/               Pre-built wxWidgets shared libraries (Windows).
├── lib/               Pre-built wxWidgets import libraries and setup.h
│                      (mswu/wx/setup.h).
├── include/wx/        wxWidgets headers (vendored, version 3.3.2).
└── doc/               wxWidgets-3.3.2.chm (upstream wxWidgets help, not
                       plugin docs).
```

## How the plugin is built

The plugin is a standard NVGT plugin and is built by NVGT's SCons build,
not by a top-level build script in this repository. To build:

1. Clone NVGT (`https://github.com/samtupy/nvgt`).
2. Place (or symlink) the contents of `wx/` from this repo into NVGT's
   `plugin/wx/` directory (so NVGT's SConstruct finds `plugin/wx/_SConscript`).
3. Build NVGT as documented upstream. The plugin output is
   `release/lib/nvgt_wx.<ext>`.

`_SConscript` only sets up wxWidgets include paths, libraries and
defines on Windows (`PLATFORM == "win32"`). Linux/macOS support requires
adding the matching wx setup; see `wx/_SConscript`.

The vendored wxWidgets is **3.3.2** (Unicode, MSVC, x64). Header layout
expects `setup.h` at `wx/lib/mswu/wx/setup.h`.

## Plugin entry point and the bridge to AngelScript

The plugin exposes a single C entry point through NVGT's plugin system:

```cpp
plugin_main(nvgt_plugin_shared* shared) {
    if (!prepare_plugin(shared)) return false;
    asIScriptEngine* engine = shared->script_engine;
    // ... register types, enums and methods ...
    return true;
}
```

NVGT's plugin ABI lives in
[`src/nvgt_plugin.h`](https://github.com/samtupy/nvgt/blob/main/src/nvgt_plugin.h).
The macros `plugin_main`, `prepare_plugin`, `static_plugin(...)` are
defined there.

## Architecture notes

### `WxManager` (AngelScript value type `wx`)

A single value type, exposed as `wx` in AngelScript. The constructor
brings up wxWidgets if it is not running yet (`wxApp::SetInstance` +
`wxEntryStart`). `wx::update()` must be called periodically by the
script (typically every frame) to drain the wx event queue. On Windows
it also pumps the native message loop with `PeekMessage`.

### Reference counting

The plugin maintains its own reference counter for every wx object
exposed to AngelScript:

- `g_ref_counts: std::map<void*, int>` is the script-side count.
- AngelScript types are registered as `asOBJ_REF` with custom
  `AddRef`/`Release` behaviours.
- When the script-side count hits zero:
  - top-level `wxWindow`s with no parent are `Destroy()`'d;
  - `wxSizer`s with no containing window are `delete`'d;
  - child windows are left to their parent (wxWidgets owns them).
- Every tracked window also has a `wxEVT_DESTROY` handler (`Track<T>()`)
  that erases its entry from `g_ref_counts` and `g_event_handlers` once
  wxWidgets actually destroys it.

`wxEvent` is registered as `asOBJ_REF | asOBJ_NOCOUNT` because events
only live for the duration of the dispatch. Scripts must not store
`wx_event@` references past the callback return.

### Event dispatch

Scripts subscribe to events with `wx_window.bind(event_type, callback)`.
The plugin keeps a `(void* object, int event_type) → callback` map and
binds a single C++ trampoline (`OnWxEvent`) per (object, event_type)
pair on the wxWidgets side. `OnWxEvent` calls the script callback
through a small `asIScriptContext` pool (`g_context_pool`) to amortise
context creation.

By design only one script callback may be attached per (object,
event_type) pair. Calling `bind` again replaces the previous callback;
`unbind(event_type)` removes it.

### Type hierarchy on the AngelScript side

AngelScript does not have C++-style multiple inheritance. The plugin
emulates the wxWidgets hierarchy with explicit cast methods:

- `opImplCast()` from a derived type to its base (e.g.
  `wx_button → wx_window`). Implicit so the type system can pass a
  derived widget where a base is expected (parent arguments to
  factories).
- `opCast()` on a base, returning a derived (`wx_window → wx_button`),
  implemented through `dynamic_cast`. This is explicit because it can
  fail.

`wx_text_entry` is a special case: `wxTextEntry` is a mixin (not a
`wxWindow`), so the plugin smuggles a `wxWindow*` through the
`wx_text_entry` handle and `dynamic_cast`s back to `wxTextEntry*` in
each method.

Adding a new control therefore looks like:

```cpp
// In plugin_main():
REGISTER_WX_CONTROL("wx_my_control", wxMyControl);
engine->RegisterObjectMethod("wx_my_control",
    "void do_thing()",
    asMETHOD(wxMyControl, DoThing), asCALL_THISCALL);

// And in WxManager add a factory:
wxMyControl* create_my_control(wxWindow* parent, ...) {
    return Track(new wxMyControl(parent, wxID_ANY, ...));
}
// Register it as a method on "wx".
```

`Track(...)` adds the object to `g_ref_counts` and binds the destroy
handler. `AddRef`/`Release` come from `REG_BASE_REF`.

For non-window types (sizers, sizer items) call `AddRef(ptr)` directly
instead of `Track`.

## Naming and code conventions

- AngelScript-exposed names use **`snake_case`** (`set_title`,
  `get_label`, `wx_text_control`). C++ wxWidgets identifiers stay in
  PascalCase as in upstream.
- Prefer the AngelScript `property` keyword on simple getter/setter
  pairs so scripts can write `frame.title = "..."`. Example:
  ```cpp
  "string get_title() const property"
  "void set_title(const string &in title) property"
  ```
  Getters carry `const` even when registered via `asCALL_CDECL_OBJFIRST`
  (the qualifier only affects AngelScript-side type checking and is
  required for symmetric `const`-ness across getter/setter pairs).
  Setters never carry `const`.
- **Property names are derived by stripping `get_`/`set_`.** That
  derived name must be a valid AngelScript identifier and must not
  collide with the type/keyword namespace. In practice:
  - Do **not** use `property` if the derived name would be a reserved
    AngelScript keyword (`int`, `bool`, `void`, `float`, `double`,
    `auto`, `if`, `for`, ...). Example: `int get_int() property`
    derives the property name `int`, which the parser cannot accept
    as a member name.
  - Do **not** use `property` if the derived name starts with a digit
    or any other character that is not a valid identifier start.
    Example: `get_3state_value()` cannot become a property because
    `3state_value` is not a valid identifier.
  - Be cautious if the derived name collides with a registered type
    name (e.g. `string`, `array`, `dictionary`). The parser may
    accept it, but it is a footgun for script authors and is not
    worth the saved keystrokes.
  In all of the above cases, register the method without `property`.
  The explicit `obj.get_xxx()` / `obj.set_xxx(v)` form continues to
  work. Add a one-line comment above the registration explaining why
  `property` is intentionally absent so the next contributor does not
  "fix" it.
- Strings cross the boundary as `std::string` in UTF-8, converted with
  `wxString::FromUTF8(...)` and `.utf8_str()`. Do not use
  `wxString::FromAscii` or pass `const char*` raw.
- Constants are exposed through enums named `wx_<topic>` with values
  prefixed `WX_<TOPIC>_<NAME>`. Avoid mixing unrelated bitmask families
  in the same enum.
- Comments in the source code are written in **English**.
- Keep `wx.cpp` self-contained for now (single translation unit). It
  may be split when it grows further.

## Commit and PR conventions

- Commits and pull requests are written in **English**.
- Keep changes focused; prefer multiple small PRs over one large one.
- When adding new widgets or sizers, also register the relevant style
  bits and any `wxCommandEvent` (or other event) accessors that go with
  them.

## Things to know before changing the bridge

- `OnWxEvent` calls `event.Skip(true)` **before** invoking the script
  callback, not after. The order is deliberate: even if `ctx->Execute()`
  raises a script exception, the event still propagates, so a buggy
  callback cannot accidentally swallow `EVT_CLOSE` / `EVT_KEY_DOWN` /
  etc. on the rest of the window chain. Scripts can override this from
  inside the callback by calling `e.skip(false)`.
- `wx_window_bind` must not call `wxEvtHandler::Bind(...)` more than
  once for the same `(object, event_type)` pair, otherwise wxWidgets
  fires the trampoline multiple times per event. When replacing the
  callback for a known pair, only the entry in `g_event_handlers` is
  updated; the wxWidgets-side `Bind` already exists.
- `wx_window_unbind` must call `Unbind` on the same `(event_type,
  &OnWxEvent, wxID_ANY)` triple that `Bind` was called with, otherwise
  wxWidgets will not find the handler to remove.
- `WxManager` must be registered with `asOBJ_VALUE | asOBJ_APP_CLASS_CD`
  (it has a non-trivial constructor and destructor); `asOBJ_POD` is
  incorrect.
- `wxEntryCleanup()` is currently not called. If the plugin ever gains
  a real shutdown hook it should be invoked there.
- **`asMETHOD` does not propagate C++ default arguments.** If the
  underlying C++ method has defaulted parameters and the AngelScript
  signature omits them, the C++ function still expects all parameters
  on the stack/registers and will read garbage. When wrapping such a
  method, either expose the full parameter list in AngelScript (with
  AngelScript-level defaults), or wrap it in a free function that
  fills in the default and call that via `asFUNCTION /
  asCALL_CDECL_OBJFIRST`. Existing example to revisit:
  `wxWindow::SetSizer(wxSizer*, bool deleteOld = true)`.
- `wxCommandEvent::GetSelection()` and `wxCommandEvent::GetInt()` read
  the **same** internal field (`m_commandInt`). The naming is upstream
  semantics: `GetSelection` is meaningful for list-style controls,
  `GetInt` for general payload. Both wrappers are exposed because
  upstream exposes both, but writers should pick the one that matches
  the control they are reading from.
- `wxCommandEvent::GetExtraLong()` / `SetExtraLong()` use C++ `long`,
  which is 32-bit on Windows MSVC x64 (the current build target) but
  64-bit on Linux x86_64. The AngelScript signature is registered as
  `int` to match the Windows ABI. Any future Linux build needs to
  switch to `int64` plus a `static_cast<long>` shim in a wrapper, not
  blindly bind `asMETHOD(wxCommandEvent, SetExtraLong)`.

## Roadmap (high-level)

- **Phase 0 — foundation:** value types (`wx_point/size/rect/colour/font`),
  per-control style enums, `wxEntryCleanup()` on shutdown, markdown
  documentation, CI. (`wx_command_event`, `unbind`, default `Skip()`,
  `bind` deduplication and the `WxManager` AS-flag fix have already
  landed.)
- **Phase 1 — basic GUI:** the rest of the sizers; dialogs
  (`wx_dialog`, `wx_message_dialog`, `wx_file_dialog`, …); selectors
  (`wx_choice`, `wx_combo_box`, `wx_list_box`, `wx_radio_box`); range
  controls (`wx_slider`, `wx_gauge`, `wx_spin_ctrl`); pickers; books
  (`wx_notebook`, `wx_scrolled_window`, `wx_splitter_window`); menus and
  bars (`wx_menu`, `wx_menu_bar`, `wx_tool_bar`, `wx_status_bar`);
  list/tree (`wx_list_ctrl`, `wx_tree_ctrl`, `wx_data_view_ctrl`);
  validators, accelerators, `wx_timer`; and the matching event types.
- **Phase 2 — canvas:** `wx_dc` family (`wx_paint_dc`,
  `wx_auto_buffered_paint_dc`, `wx_client_dc`, `wx_memory_dc`),
  graphics resources, drawing primitives, optionally
  `wx_graphics_context`.
- **Phase 3 — RichText:** add `wxmsw33u_richtext`, `wxmsw33u_html` and
  `wxmsw33u_xml` to the build; wrap `wx_rich_text_ctrl`,
  `wx_rich_text_attr`, `wx_rich_text_event`, file handlers, optionally
  styles and dialogs.
- **Phase 4 — advanced:** AUI, StyledTextCtrl, PropertyGrid, WebView,
  MediaCtrl, drag and drop, clipboard, XRC, configuration and locale.

## License

MIT, see `LICENSE`.
