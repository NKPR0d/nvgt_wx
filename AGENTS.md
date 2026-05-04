# AGENTS.md

Guidance for humans and AI agents contributing to **nvgt_wx**, a wxWidgets plugin for NVGT.

## Capturing knowledge (read this first)

This file is the project memory. Whenever you learn something
non-obvious about the bridge, the wxWidgets/AngelScript boundary, the
build, the CI, or the source layout — write it down here, in the
section that fits, in the same PR that introduces the change. Do not
defer it to "later" or to a separate documentation pass; "later" never
happens and the knowledge gets lost.

What belongs in AGENTS.md:

- conventions (naming, file ownership, comment language, …);
- footguns and the reason behind a non-obvious workaround
  (`asMETHOD` does not propagate C++ defaults, `nvgt_plugin.h`
  include-order contract, `WXUSINGDLL` link surface, …);
- decisions that future contributors must not silently undo
  (e.g. `Refresh` wrapper exists *because* the C++ default reads a
  garbage pointer on MSW — without that comment somebody will
  "simplify" it back to `asMETHOD(wxWindow, Refresh)`);
- API conventions visible to script authors that are not obvious
  from the registration call alone (value-type properties using
  `to_wx`/`from_wx`, the `wx_text_entry` mix-in trick, the
  per-control `wx_*_style` enum split, …);
- audit summaries of what the bridge does and does not yet expose
  on a given wx class, so the next person knows where to start.

What does *not* belong here:

- per-PR changelogs (those go in the PR description);
- TODO lists for one-off tasks (use the issue tracker / the PR);
- generated reference output (link to wx documentation instead).

If you find existing guidance that turned out to be wrong or stale,
fix it in the same PR that exposes the contradiction. Out-of-date
AGENTS.md is worse than no AGENTS.md.

## Language

- **Source code comments are written in English.** No exceptions —
  even when the author and the reviewer share another language, the
  next contributor or AI agent may not. Mixed-language comments
  inside the same file are particularly bad and must be cleaned up
  in the same PR that introduces them.
- **Commit messages and pull request descriptions are written in
  English.** Same reasoning: commit history and PR archaeology are
  the second-most-important form of project memory after this file.
- AngelScript identifiers (type names, method names, enum values)
  follow the casing rules in *Naming and code conventions* below
  and are also English.

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
├── wx.cpp             Plugin entry point. Holds only plugin_main(), which
│                      calls into src/register.cpp.
├── src/
│   ├── common.h       Shared types, globals (extern), inline helpers/
│   │                  templates, and forward declarations consumed by
│   │                  every other translation unit.
│   ├── runtime.cpp    Bridge runtime: script-context pool, AngelScript
│   │                  ref-counting table for wx objects, event-handler
│   │                  map, OnWxEvent trampoline, wx_window_bind/unbind.
│   ├── helpers.cpp    Free-function adapters (wx_*) registered via
│   │                  asCALL_CDECL_OBJFIRST. Add new wrappers here.
│   ├── manager.cpp    NVGTApp + WxManager (the value type `wx`) plus all
│   │                  create_* factories. Self-contained: exposes only
│   │                  register_wx_manager(engine).
│   └── register.cpp   Every enum, type and method registration plus the
│                      REG_* macros. register_all_types() is called from
│                      plugin_main().
├── dll/               Pre-built wxWidgets shared libraries (Windows).
├── lib/               Pre-built wxWidgets import libraries and setup.h
│                      (mswu/wx/setup.h).
├── include/wx/        wxWidgets headers (vendored, version 3.3.2).
└── doc/               wxWidgets-3.3.2.chm (upstream wxWidgets help, not
                       plugin docs).
```

When adding a new widget the change typically lives in three files:

- `manager.cpp`: a new `create_*` factory and its `RegisterObjectMethod`
  line in `register_wx_manager`.
- `helpers.cpp`: any free-function wrappers needed for properties or
  defaulted arguments, declared in `common.h`.
- `register.cpp`: a `REGISTER_WX_*` macro invocation plus per-widget
  `RegisterObjectMethod` calls.

Cross-cutting bridge changes (ref-counting, event dispatch, the context
pool) belong in `runtime.cpp`. `common.h` only owns shared declarations
and inline helpers; it should not gain function bodies.

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

### CI: `.github/workflows/build-windows.yml`

A GitHub Actions workflow runs on every push to `main` and every pull
request. It does not need NVGT in full — it only does compile + link of
the plugin sources to verify that the vendored wxWidgets headers / libs
plus the bridge code stay buildable on MSVC x64.

What the workflow does:

1. Stages the repo into `<workspace>/plugin/wx/` so the relative
   `../../src/nvgt_plugin.h` and `../../../src/nvgt_plugin.h` includes
   resolve.
2. Downloads `nvgt_plugin.h` from `samtupy/nvgt` (main branch) into
   `<workspace>/src/nvgt_plugin.h`. Not vendored — pinning would silently
   rot when upstream bumps `NVGT_PLUGIN_API_VERSION`.
3. Downloads `angelscript.h` from `codecat/angelscript-mirror` into
   `<workspace>/as_include/angelscript.h`.
4. Downloads `scriptarray.h` (`ASAddon/include/`),
   `scriptarray.cpp` (`ASAddon/src/`) and the wrapper file
   (`ASAddon/plugin/scriptarray.cpp`) from `samtupy/nvgt` into
   `<workspace>/ASAddon/{include,src,plugin}/`. The wrapper is what
   gets compiled; see "scriptarray (consumed from ASAddon)" below
   for why all three files are needed.
5. Sets up MSVC x64 via `ilammy/msvc-dev-cmd@v1`.
6. Compiles the bridge sources plus the scriptarray wrapper with the
   same flags NVGT's `SConstruct` uses (`/MT /EHsc /std:c++20 /J
   /utf-8 /Gy /GF /Zc:inline /bigobj /permissive- /O2`).
7. Links against the vendored `wxmsw33u_core.lib` + `wxbase33u.lib` and
   uploads `nvgt_wx.dll` as an artifact.

The CI deliberately does *not* link against NVGT or AngelScript libs:
- `WXUSINGDLL` means we link against import libs, the actual code lives
  in `wx/dll/wxmsw332u_core_vc14x_x64.dll` etc. at runtime.
- `nvgt_plugin.h` enables `ANGELSCRIPT_DLL_MANUAL_IMPORT` for plugins,
  so AngelScript symbols are resolved through the function-pointer
  table populated by `prepare_plugin()` at load time, not at link time.
  Headers are sufficient; no `.lib` is needed.

When changing the bridge:
- Touching `wx/include/`, `wx/lib/`, `wx/dll/` is supported but rare —
  if you bump wxWidgets, also update the `WXVER_MAJOR/_MINOR/_RELEASE`
  references in this AGENTS.md and the `wxmsw33u_core` / `wxbase33u`
  lib names in `_SConscript` and the workflow.
- If you add new source files under `wx/src/`, add them to **both**
  `wx/_SConscript` (`source=[...]`) and the `$sources` list in
  `.github/workflows/build-windows.yml`. There is no glob — a missing
  entry will silently link a smaller DLL.

## Plugin entry point and the bridge to AngelScript

The plugin exposes a single C entry point through NVGT's plugin system:

```cpp
plugin_main(nvgt_plugin_shared* shared) {
    if (!prepare_plugin(shared)) return false;
    register_all_types(shared->script_engine);
    return true;
}
```

`register_all_types()` lives in `src/register.cpp` and is the only thing
`wx.cpp` calls. All bindings live under `src/`.

NVGT's plugin ABI lives in
[`src/nvgt_plugin.h`](https://github.com/samtupy/nvgt/blob/main/src/nvgt_plugin.h).
The macros `plugin_main`, `prepare_plugin`, `static_plugin(...)` are
defined there.

### Including `nvgt_plugin.h` from a multi-file plugin

`nvgt_plugin.h` *defines* (not just declares) the
`asGetLibraryVersion / asAcquireExclusiveLock / nvgt_wait / ...`
function pointer table and `plugin_version()` whenever it is included
without `NVGT_PLUGIN_INCLUDE` defined. With the plugin split across
`wx.cpp + src/*.cpp` that would emit those symbols in every TU and the
linker rejects them as `LNK2005 already defined`. The convention in
this repo is:

- `wx.cpp` (the entry-point TU) includes `../../src/nvgt_plugin.h`
  **first**, *before* `src/common.h`, with the gate **not** set. This
  is the one TU that owns the function-pointer storage and
  `plugin_version()`.
- `src/common.h` defines `NVGT_PLUGIN_INCLUDE` before its own
  `#include "../../../src/nvgt_plugin.h"`. Every other TU only ever
  reaches `nvgt_plugin.h` through `common.h`, so they all see the
  extern-declarations form.
- The `#pragma once` inside `nvgt_plugin.h` makes the second include in
  `wx.cpp` (via `common.h`) a no-op, so the two paths do not conflict.

If you reorder the includes in `wx.cpp` (common.h first), `wx.cpp` will
also see only `extern` declarations and the linker will fail with
`unresolved external symbol asGetLibraryVersion` (and friends).
If you forget the gate in `common.h`, the linker will fail with
`LNK2005 already defined in wx.obj` for the same set of symbols.

## Architecture notes

### `WxManager` (AngelScript value type `wx`)

A single value type, exposed as `wx` in AngelScript. The constructor
brings up wxWidgets if it is not running yet (`wxApp::SetInstance` +
`wxEntryStart`). `wx::update()` must be called periodically by the
script (typically every frame) to drain the wx event queue. On Windows
it also pumps the native message loop with `PeekMessage`.

**wxEntryStart / wxEntryCleanup pairing.** `WxManager()` calls
`wxEntryStart` only when no `wxApp` is running yet, and flips
`g_wx_started_by_plugin = true` so the plugin remembers that *it* —
not NVGT or some other host — owns the wx lifecycle. `wxEntryCleanup`
is paired with that through `g_wx_lifecycle`, a static `WxLifecycleGuard`
whose destructor runs at C++ static deinitialisation:

- On Windows that fires from `DllMain` on `DLL_PROCESS_DETACH`, after
  AngelScript has released every wx object, which is the safe moment to
  tear wx down.
- Pairing the cleanup with `~WxManager()` directly would be wrong:
  WxManager is a *value* type, and a script that lets `wx` go out of
  scope while wx-windows are still alive would otherwise nuke `wxApp`
  out from under them.
- `nvgt_plugin.h` does not expose a per-plugin shutdown hook (its
  public API is just `plugin_main()` plus the function-pointer table
  via `prepare_plugin()`), so the static-destructor route is the only
  portable hook.

If a future host pre-creates a `wxApp` before our plugin is loaded,
`g_wx_started_by_plugin` stays false and the guard's destructor is a
no-op; the host owns the lifecycle in that case.

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

`wx_text_entry` and `wx_item_container` are special cases: `wxTextEntry`
and `wxItemContainer` are mixins (not `wxWindow`s), so the plugin
smuggles a `wxWindow*` through the AS-side handle and `dynamic_cast`s
back to the mixin pointer (`wxTextEntry*` / `wxItemContainer*` /
`wxItemContainerImmutable*`) in each method. The cast-helper templates
(`to_text_entry_as_win`, `to_item_container_as_win` in `common.h`) are
the `opImplCast` glue: a concrete control hands its `wxControl*` to the
helper, which slices it to `wxWindow*` for the mixin handle.

`wxComboBox` is the awkward case because it derives from BOTH mixins
*and* their AS method names overlap on `clear`, `is_empty` and
`get_string_selection`. The split between
`REG_ITEM_CONTAINER_FULL_METHODS` and `REG_ITEM_CONTAINER_BASE_METHODS`
in `register.cpp` exists to disambiguate: most concrete types apply
the FULL macro, but `wx_combo_box` only applies BASE and resurfaces
the dropped three methods under disambiguated names
(`is_list_empty` / `clear_items` / `get_item_string_selection`).
This mirrors what `wxComboBoxBase::IsListEmpty`/`IsTextEmpty` does in
C++.

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

When a wrapper returns a wxWindow that the bridge **may or may not have
created** — i.e. anything obtained via `wxWindow::GetParent`,
`GetGrandParent`, `GetChildren()` walks, sibling navigation on
`wxRadioButton`, etc. — use `EnsureTracked(ptr)` instead of bare
`AddRef(ptr)`. `EnsureTracked` binds the `wxEVT_DESTROY` handler the
**first** time the bridge sees a pointer, then on every subsequent call
just increments the refcount. Without that hook, an untracked window
would leave a stale entry in `g_ref_counts` after wxWidgets destroys
it, and the eventual `Release()` path could call `Destroy()` on a
top-level window the bridge does not own, taking down the entire
parent's child chain along with it. `EnsureTracked` is in
`common.h` next to `Track`.

**Sizer / sizer-item lifetime.** `wxSizer` and `wxSizerItem` do **not**
fire `wxEVT_DESTROY` of their own. To prevent stale entries in
`g_ref_counts` after wxWidgets tears them down, the runtime calls
`CleanupSizerEntries(sizer)` from two places (see `runtime.cpp`):

1. `OnObjectDestroyed` (the `wxEVT_DESTROY` callback for windows): when
   a wxWindow dies and its destructor is about to destroy the sizer
   it owns via `SetSizer`, the bridge walks the sizer tree and
   removes every `(wxSizer*, wxSizerItem*)` entry first.
2. `Release(...)` for a freestanding sizer (`!GetContainingWindow()`):
   before `delete sizer`, the same walk runs so the items disappear
   from `g_ref_counts` before their destructor invalidates the
   pointers.

The walk is recursive through any sub-sizers (`wxSizerItem::GetSizer()`),
so nested sizer trees are handled too. New code that destroys a sizer
through some other path (e.g. wrapping a wx call that internally swaps
sizers) must call `CleanupSizerEntries` before the destruction lands,
otherwise script-side `wx_sizer_item@` handles will silently start
pointing at freed memory.

Even with the cleanup, **scripts must not rely on a `wx_sizer_item@`
outliving the sizer it came from.** A `wx_sizer_item@` borrowed before
the cleanup is fine; one held across a `sizer.clear()`, a `frame.destroy()`
of the containing window, or any explicit `replace`/`detach` call is a
dangling pointer the bridge cannot rescue.

### scriptarray (consumed from ASAddon)

`array<T>` on the AngelScript side is provided by the AS standard
`scriptarray` add-on. NVGT registers it once on the engine before
plugins load — the plugin must NOT register it again. What the plugin
*does* need is the `class CScriptArray` definition itself, because the
selector wrappers in `helpers.cpp` (`as_array_to_wx_strings`,
`wx_strings_to_as_array`, `wx_ints_to_as_array`,
`wx_item_container_append_many`, …) call `CScriptArray::GetSize`,
`At`, `SetValue`, `Create` and friends.

The plugin does **not** vendor scriptarray. Instead it consumes the
add-on directly from NVGT's `ASAddon/` tree, the same way the upstream
`plugin/redis/` plugin does:

- `wx/src/common.h` does `#include <scriptarray.h>`. NVGT's main
  `SConstruct` adds `#ASAddon/include` to `CPPPATH` so the angled
  include resolves to `ASAddon/include/scriptarray.h`.
- `wx/_SConscript` compiles NVGT's tiny wrapper file
  `#ASAddon/plugin/scriptarray.cpp` (three lines: `#define
  NVGT_PLUGIN_INCLUDE`, `#include "../../src/nvgt_plugin.h"`,
  `#include "../src/scriptarray.cpp"`) and links the resulting
  object into `nvgt_wx.dll`. The wrapper's relative includes only
  resolve under the upstream directory layout, which is why the
  SConscript references it via the `#ASAddon/plugin/` SCons-tree
  prefix and not a copy in this repo.

This is the binary the plugin needs because the implementation is
required at link time even though the type is already registered by
NVGT — the bridge's `helpers.cpp` calls `CScriptArray::Create` /
`GetSize` / `At` / `SetValue` directly.

The CI workflow (`.github/workflows/build-windows.yml`) builds the
plugin **outside** NVGT's SCons tree. To keep CI working with this
contract it downloads three files from `samtupy/nvgt@main` and stages
them so the wrapper's relative includes resolve:

- `ASAddon/include/scriptarray.h` → put under
  `<workspace>/ASAddon/include/` and added to the `/I` list.
- `ASAddon/src/scriptarray.cpp` → put under
  `<workspace>/ASAddon/src/`. **Not** added to the source list
  directly; it is pulled in by the wrapper.
- `ASAddon/plugin/scriptarray.cpp` → put under
  `<workspace>/ASAddon/plugin/` and added to the `$sources` list as
  the cpp that cl.exe actually compiles.

These files are downloaded, not vendored, for the same reason as
`nvgt_plugin.h` and `angelscript.h`: pinning a copy here would
silently rot when NVGT bumps its scriptarray.

NVGT-specific gotchas:

1. NVGT plugins link with `ANGELSCRIPT_DLL_MANUAL_IMPORT`, so
   `asAllocMem` / `asFreeMem` are function-pointer **variables**.
   The wrapper at `ASAddon/plugin/scriptarray.cpp` `#define`s
   `NVGT_PLUGIN_INCLUDE` and pulls in `nvgt_plugin.h` *before*
   including the implementation, which is what makes those pointer
   declarations visible to scriptarray.cpp. Compiling
   `ASAddon/src/scriptarray.cpp` directly (without the wrapper)
   would fail with `C2065`/`C3861` for `asAllocMem` /
   `asAcquireExclusiveLock` / `asAtomicInc` / … in ~30 call sites
   because `<angelscript.h>` strips those declarations under
   `ANGELSCRIPT_DLL_MANUAL_IMPORT`, and the function-pointer
   replacements are declared by `nvgt_plugin.h`, not by AngelScript.
2. The wrapper must be added to **both** `wx/_SConscript`
   (`source=[..., scriptarray]`) and the `$sources` list in
   `.github/workflows/build-windows.yml`. There is no glob — a
   missing entry will silently link a `nvgt_wx.dll` whose
   `array<T>` calls hit unresolved symbols at runtime.
3. Upstream `ASAddon/src/scriptarray.cpp` initialises
   `static asALLOCFUNC_t userAlloc = asAllocMem;` at C++ static-init
   time. In a manual-import plugin DLL `asAllocMem` is `nullptr`
   until `prepare_plugin()` runs from `plugin_main()`, so this
   captures `nullptr` and a naïve reading suggests the first
   `CScriptArray::Create()` would dereference a null function
   pointer. The redis plugin upstream uses the same code and ships
   in production, so empirically this either does not bite or is
   masked by some platform-specific quirk; if `array<T>` operations
   crash on first use after this refactor, the fix is to lazy-resolve
   `userAlloc` / `userFree` through `asAllocMem` / `asFreeMem` at
   call time. The proper home for that fix is upstream NVGT, not a
   local patch on top of the downloaded copy.

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
  Read-only properties (a `get_*` without a matching `set_*`) are
  acceptable when the upstream setter has no script-level use case —
  e.g. `wx_window.id` is registered as `get_id() const property` only,
  because `wxWindow::SetId()` after construction breaks any pre-existing
  `Bind`/menu-item dispatch keyed on the original id.
- **`get_` and `set_` are the *only* property-accessor prefixes
  AngelScript understands.** `is_xxx() property`, `has_xxx() property`,
  `can_xxx() property` and similar wx-flavoured spellings are
  rejected at registration time with `asINVALID_DECLARATION`. CI
  cannot catch this — only the runtime that actually loads the
  plugin into NVGT will. If a wx accessor is naturally named
  `IsEditable`/`HasFocus`/etc., register it as
  `get_editable() const property` (paired with `set_editable`) for
  the property form, and *additionally* expose the wx-style spelling
  as a plain method (`bool is_editable() const`) so callers who
  prefer the wx idiom still work. Both registrations can point at
  the same C++ helper.
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
- **Property setters in an overload family must be the only setter
  marked `property`.** AngelScript's overload resolver is happy to
  pick the right `set_xxx(...)` based on argument type, but only one
  of those overloads can carry the `property` keyword — the one that
  the assignment-form `obj.xxx = ...` will resolve to. Example:
  `set_size(const wx_size &in size) property` is the property setter
  used by `obj.size = wx_size(...)`; `set_size(const wx_rect &in
  rect)` is registered as a regular method without `property` and
  is callable only as `obj.set_size(rect)`. Adding a second
  `property`-marked overload (or removing `property` from the
  intended one) silently breaks every script that relies on the
  assignment form. Document this next to the registration block when
  it is non-obvious.
- **`set_label` mirrors the value into the window's `name`** (for
  accessibility). The `name` property exists on every wx_window, so
  scripts that set both should set `name` *after* `label`, otherwise
  `set_label` clobbers the `name` they just assigned. The mirror is
  intentional — wx accessibility queries the window name first when
  no label is available — but the order dependency is non-obvious
  from the script side. The `set_label_text` variant (which strips
  `&` mnemonics and is meant for arbitrary user input) does **not**
  mirror into `name`, deliberately.
- Strings cross the boundary as `std::string` in UTF-8, converted with
  `wxString::FromUTF8(...)` and `.utf8_str()`. Do not use
  `wxString::FromAscii` or pass `const char*` raw.
- Constants are exposed through enums named `wx_<topic>` with values
  prefixed `WX_<TOPIC>_<NAME>`. Avoid mixing unrelated bitmask families
  in the same enum.
- Source code comments, commit messages and PR descriptions are
  in English; see the dedicated *Language* section at the top of
  this file.
- The plugin is split across `wx.cpp + src/{runtime,helpers,manager,register}.cpp`.
  When adding code, place it in the file that matches its role (see the
  "Repository layout" section). `wx.cpp` itself stays minimal — only
  the `plugin_main()` entry point and the include-order contract for
  `nvgt_plugin.h` documented above.

## Commit and PR conventions

- Language: see the *Language* section at the top of this file.
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
- `wxEntryCleanup()` is paired with `wxEntryStart()` through the
  static `WxLifecycleGuard` (`g_wx_lifecycle` in `manager.cpp`). Its
  destructor fires at C++ static deinitialisation — on Windows from
  `DllMain` on `DLL_PROCESS_DETACH` — after every wx object the
  bridge handed out has been released. See *WxManager* above for why
  pairing the cleanup with `~WxManager()` would be wrong.
- **Do not write a wrapper unless the bridge needs one.** The default
  for any new method binding is `asMETHOD(wxFoo, BarBaz)` registered
  directly through `engine->RegisterObjectMethod`. A free-function
  wrapper in `helpers.cpp` is only justified when one of the rules
  below makes the direct binding incorrect, fragile, or impossible:
    1. **C++ signature is incompatible with the AS-side surface.**
       The wx method takes a base-class pointer the bridge doesn't
       vend (`wxWindow::Reparent(wxWindowBase*)`), takes a const
       reference where the AS handle is a pointer
       (`wxTextCtrl::EmulateKeyPress(const wxKeyEvent&)`), or returns
       a value the AS-side method should drop (`wxButton::SetDefault`
       returns the previous default `wxWindow*` but scripts only
       want the side effect).
    2. **`long` appears in the parameter list or return type.** Always
       wrap, see the dedicated bullet below — `int`-vs-`long` width
       differs across LLP64 (MSW) and LP64 (Linux/macOS).
    3. **`wxString` ↔ `std::string` conversion.** AngelScript only
       knows `string`; the wrapper does the UTF-8 round-trip.
    4. **The C++ method has a default-valued parameter** that the
       AS-side surface should also default. `asMETHOD` does not
       propagate C++ defaults — see the next bullet for why this is
       a footgun, especially with pointer defaults.
    5. **The C++ method is `inline`-only in the header**, with no
       out-of-line definition the linker can grab. Direct `asMETHOD`
       on such a method has been observed to fail under MSVC's
       whole-program optimisation; the wrapper gives the linker a
       single out-of-line target. Examples already in the bridge:
       `wxGridSizer::GetEffectiveColsCount`, `wxGridSizer::CalcRowsCols`,
       `wxWindow::MoveAfterInTabOrder` (forwards to `DoMoveInTabOrder`),
       `wxButton::SetAuthNeeded` (forwards to `DoSetAuthNeeded`).
    6. **A property setter** with a wx-side `bool` return that has to
       be dropped to `void`, or a wx getter / setter wired through the
       `property` syntax that needs reshaping (see the property-setter
       bullet below).
  A wrapper that *only* adds a `if (!self) return ...;` guard is **not
  justified** — the existing direct `asMETHOD` bindings on
  `wxWindow::Show`, `IsShown`, `IsEnabled`, `HasFocus`, … do not
  null-check, and AngelScript callers already crash predictably when
  invoking through a null handle. The bridge's null-checks in helpers
  exist as a side-effect of wrappers needed for other reasons; they
  are not a reason to wrap.
- **`asMETHOD` does not propagate C++ default arguments.** If the
  underlying C++ method has defaulted parameters and the AngelScript
  signature omits them, the C++ function still expects all parameters
  on the stack/registers and will read garbage. When wrapping such a
  method, either declare the full parameter list in AngelScript (with
  AngelScript-level defaults), or wrap it in a free function that
  fills in the default and call that via `asFUNCTION /
  asCALL_CDECL_OBJFIRST`. The existing wrappers around `SetSizer`,
  `ShowFullScreen`, `wxMouseEvent::Button*` and `Refresh` are the
  canonical examples. **Pointer defaults are especially dangerous**:
  wxWidgets methods like `wxWindow::Refresh(bool, const wxRect*)` will
  dereference the pointer parameter on the MSW path, so a garbage
  pointer is not just an unused-byte read — it crashes the process.
  When adding a new registration, double-check the C++ signature for
  defaulted parameters (including ones documented only in the wx
  reference, not the header).
- `wxCommandEvent::GetSelection()` and `wxCommandEvent::GetInt()` read
  the **same** internal field (`m_commandInt`). The naming is upstream
  semantics: `GetSelection` is meaningful for list-style controls,
  `GetInt` for general payload. Both wrappers are exposed because
  upstream exposes both, but writers should pick the one that matches
  the control they are reading from.
- **C++ `long` parameters and return types must be wrapped, never
  bound through `asMETHOD` directly.** `long` is 32-bit on Windows
  MSVC x64 (the current build target, an LLP64 platform) but 64-bit
  on every LP64 Unix (Linux x86_64, macOS, *BSD, common aarch64
  toolchains). Binding the wxWidgets accessor through `asMETHOD`
  with an AS-side `int` happens to work on Windows because the two
  widths coincide; it silently truncates setters and reads garbage
  from the upper word of getters on every other platform. Always
  wrap with a free function that does an explicit `static_cast<long>`
  on the way in and `static_cast<int>` on the way out, and register
  via `asFUNCTION / asCALL_CDECL_OBJFIRST`. Affected accessors
  currently exposed: `wxCommandEvent::GetExtraLong()` /
  `SetExtraLong()`, `wxWindow::GetWindowStyleFlag()` /
  `SetWindowStyleFlag()`, `wxWindow::GetExtraStyle()` /
  `SetExtraStyle()`. Any future binding of a wx accessor whose
  C++ signature uses `long` (e.g. style-flag siblings, font weight
  enums in older wx, …) belongs in the same family. Once a 64-bit
  Unix port lands, switch the AS-side signature to `int64`; the
  wrappers already do the cast.
- **AngelScript property setters must return `void`.** Per the
  AngelScript [property accessor docs](https://www.angelcode.com/angelscript/sdk/docs/manual/doc_script_class_prop.html),
  the setter for a `property` accessor takes the value and returns
  void; mismatched return types either get rejected at registration
  time or, more dangerously, silently discard the result when the
  script writes `obj.prop = value`. wxWidgets has several setters
  that legitimately return `bool` (`wxControl::SetLabelMarkup`,
  `wxTextEntry::SetHint`, …). Two acceptable shapes:
    - Drop the `property` keyword and register the bool-returning
      method as a regular method (script writes
      `ok = ctrl.set_label_markup("...")`). This is what
      `set_label_markup` does.
    - Keep the `property` keyword but wrap to a `void`-returning
      adapter, accepting that the bool result is discarded. This
      is what `set_hint` does because the bool only signals
      "platform supports hints" and is rarely actionable.
  Pick one shape per setter and document the choice next to the
  registration; do not register `bool set_xxx(...) property`.
- **`asMETHOD` does not work on methods that the type only inherits
  via multiple inheritance.** `wxCommandEvent` derives from `wxEvent`
  *and* `wxEventBasicPayloadMixin`. Methods declared in the mixin
  (`GetInt`, `SetInt`, `GetExtraLong`, `SetExtraLong`) cannot be
  registered with `asMETHOD(wxCommandEvent, GetInt)` — MSVC refuses
  the C-style cast inside the `asMETHOD` macro because pointer-to-
  member representations differ across MI offsets. The error looks
  like `cannot convert "int (wxEventBasicPayloadMixin::*)() const" to
  "void (wxCommandEvent::*)()"`. Always wrap such methods in a free
  function and register via `asFUNCTION / asCALL_CDECL_OBJFIRST`.
  Methods declared directly on `wxCommandEvent` itself
  (`GetSelection`, `IsChecked`, `IsSelection`) bind fine with
  `asMETHOD`. The same caveat applies to any future `wx*Event`
  subclass that mixes in payload via MI.

## Value-type conventions (geometry and colour)

The bridge exposes seven value types to scripts:

- `wx_point` — alias of `wxPoint` (two `int`s, `x` / `y`).
- `wx_size`  — alias of `wxSize` (two `int`s, exposed as `width` /
  `height` to scripts; the underlying members are `wxSize::x` and
  `wxSize::y`).
- `wx_rect`  — alias of `wxRect` (`x` / `y` / `width` / `height`).
- `wx_colour` — **custom** POD struct of four `uint8_t` (`r` / `g` /
  `b` / `a`). It is *not* `wxColour`. `wxColour` is not
  layout-stable (it can carry a platform handle on MSW) and exposing
  it as a fixed-property AS value type would crash on construction
  in some builds. Convert at the boundary with `to_wx(wx_colour)` and
  `from_wx(wxColour)`; both are `inline` in `common.h`.
- `wx_font` — value-type wrapper around `wxFont` itself. wxFont's
  payload is a `wxGDIObject` pointer and the class is reference-counted
  internally, so copy/destruct are pointer-cost; `asGetTypeTraits<wxFont>()`
  derives the right CDAK trait flags automatically (do not spell them
  out by hand). Properties: `point_size` (int), `pixel_size` (`wx_size`),
  `face_name` (`string`), `family` / `style` / `weight` (int — see the
  font-enum note below), `underlined` (bool), `strikethrough` (bool).
  Plain methods: `is_ok`, `is_fixed_width`, `bold`, `italic`,
  `larger`, `smaller`, `scaled(float)`, `get_base_font`. The wxFont
  modifier methods `Underlined()` and `Strikethrough()` are intentionally
  **not** registered as AS methods because they would collide with
  the bool properties of the same name; scripts that need a copy can
  do `wx_font copy = font; copy.underlined = true;`.
- `wx_gb_position` — POD struct of two `int`s (`row` / `col`). Mirrors
  `wxGBPosition`, used by `wxGridBagSizer` to address grid cells.
  Convert at the boundary with `to_wx(wx_gb_position)` /
  `from_wx(wxGBPosition)`; both are `inline` in `common.h`.
- `wx_gb_span` — POD struct of two `int`s (`rowspan` / `colspan`),
  default-constructed to (1, 1). Mirrors `wxGBSpan`. The boundary
  helper `to_wx(wx_gb_span)` rounds non-positive members up to 1
  because `wxGBSpan` asserts on (`<= 0`); pass `wx_gb_span(1, 1)`
  for the explicit "default span" sentinel.

For `wx_gb_position` / `wx_gb_span` we do not use `offsetof` to bind
to the underlying `wxGBPosition` / `wxGBSpan`: those classes hide
their members behind `m_row`/`m_col`/`m_rowspan`/`m_colspan` getters
and the layout could change between minor wx versions. The AS-side
mirror gives the bridge a layout it controls.

Why aliases rather than fresh types for the first three: `wxPoint`,
`wxSize` and `wxRect` are documented by upstream as plain `int`
aggregates and have stable layouts across builds, so AS can address
their members directly via `offsetof`. `wxColour` is the outlier.

When wrapping a wxWidgets method that takes or returns one of these
types:

- The free-function wrapper signature uses the AS-visible name:
  ```cpp
  wx_point wx_window_get_position(wxWindow* self);
  void     wx_window_set_position(wxWindow* self, const wx_point& p);
  ```
- For `wx_colour`, run the conversion explicitly:
  ```cpp
  wx_colour wx_window_get_background_colour(wxWindow* self) {
      return from_wx(self->GetBackgroundColour());
  }
  void wx_window_set_background_colour(wxWindow* self, const wx_colour& c) {
      self->SetBackgroundColour(to_wx(c));
  }
  ```
- Register the method as a `property` so scripts can write
  `win.size = wx_size(640, 480)` instead of `win.set_size(...)`.
- Geometry getters are `const` on the AS side; setters are not.

`register_value_types(engine)` is called from the very top of
`register_all_types()` *before* any object type. AS rejects a
`RegisterObjectMethod` whose signature mentions a not-yet-registered
type, so the value types must come first. Do not move that call.

## Style enums (per-control)

Style bitmasks are split into per-control enums:

| AS enum                  | Constants live there                |
|--------------------------|-------------------------------------|
| `wx_window_style`        | `WX_BORDER_*`, `WX_HSCROLL`,        |
|                          | `WX_VSCROLL`, `WX_TAB_TRAVERSAL`,   |
|                          | `WX_NO_BORDER`, ...                 |
| `wx_frame_style`         | `WX_DEFAULT_FRAME_STYLE`,           |
|                          | `WX_CAPTION`, `WX_RESIZE_BORDER`, ...|
| `wx_button_style`        | `WX_BU_*`                           |
| `wx_check_box_style`     | `WX_CHK_2STATE`, `WX_CHK_3STATE`,   |
|                          | `WX_CHK_ALLOW_3RD_STATE_FOR_USER`   |
| `wx_static_text_style`   | `WX_ALIGN_LEFT`, `WX_ALIGN_RIGHT`,  |
|                          | `WX_ALIGN_CENTRE_HORIZONTAL`,       |
|                          | `WX_ST_NO_AUTORESIZE`,              |
|                          | `WX_ST_ELLIPSIZE_*`                 |
| `wx_text_ctrl_style`     | `WX_TE_*`                           |
| `wx_text_file_type`      | `WX_TEXT_TYPE_ANY` (selector for    |
|                          | `wx_text_control.load_file` /       |
|                          | `save_file`, not a bit flag)        |
| `wx_radio_button_style`  | `WX_RB_GROUP`, `WX_RB_SINGLE`       |
| `wx_choice_style`        | `WX_CB_SORT`                        |
| `wx_combo_box_style`     | `WX_CB_SIMPLE`, `WX_CB_DROPDOWN`,   |
|                          | `WX_CB_READONLY`, `WX_CB_SORT`,     |
|                          | `WX_TE_PROCESS_ENTER`,              |
|                          | `WX_TE_PROCESS_TAB`                 |
| `wx_list_box_style`      | `WX_LB_SINGLE`, `WX_LB_MULTIPLE`,   |
|                          | `WX_LB_EXTENDED`, `WX_LB_HSCROLL`,  |
|                          | `WX_LB_ALWAYS_SB`, `WX_LB_NEEDED_SB`,|
|                          | `WX_LB_NO_SB`, `WX_LB_SORT`,        |
|                          | `WX_LB_INT_HEIGHT`                  |
| `wx_radio_box_style`     | `WX_RA_SPECIFY_ROWS`,               |
|                          | `WX_RA_SPECIFY_COLS`,               |
|                          | `WX_RA_HORIZONTAL`, `WX_RA_VERTICAL`,|
|                          | `WX_RA_LEFTTORIGHT`,                |
|                          | `WX_RA_TOPTOBOTTOM`                 |

Two reasons not to merge them back into a single `wx_style`:

1. Several constants overlap numerically across families
   (e.g. `WX_TE_LEFT` shares a bit with `WX_BU_LEFT` but means a
   different thing), so a shared enum hides bugs.
2. New controls add their own families; mixing them into one enum
   means the script-side namespace grows without bound.

Constant **names** are unchanged across the split, only the AS enum
that owns them differs. Scripts continue to write
`WX_DEFAULT_FRAME_STYLE | WX_RESIZE_BORDER` because AS implicitly
converts enum values to `int` for `int`-typed parameters; OR-ing
across families is therefore still legal at the call site.

## Font enums (`wx_font_family` / `wx_font_style` / `wx_font_weight`)

`wxFontFamily`, `wxFontStyle` and `wxFontWeight` are strongly-typed C++
enums but the bridge exposes them as plain integer enums on the AS side
(`wx_font_family`, `wx_font_style`, `wx_font_weight`). Reasons:

- AngelScript does not have strongly-typed enums; an enum value is
  always an `int`. Registering each wx enum as a fresh AS enum is
  about as much typing as a single shared int and lets scripts pass
  the values straight through to `wx_font`'s constructor and setters.
- The wx enum constants (`wxFONTFAMILY_*`, `wxFONTSTYLE_*`,
  `wxFONTWEIGHT_*`) are documented as stable across wx releases and
  are even part of the serialized form of `wxFont` (`wxToString` /
  `wxFromString`), so propagating their numeric values through the
  bridge is safe.

The corresponding `wx_font` setters (`set_family`, `set_style`,
`set_weight`) take an `int` on the AS side and `static_cast` it back
to the strongly-typed wx enum inside the helper. Scripts that pass a
`wx_font_family` enum value implicitly convert to `int`, no extra
cast is needed in user code.

`wxFont`'s legacy constructor that takes raw ints for family / style /
weight is the only ctor exposed to AS for the same reason — it accepts
the ints natively and casts inside.

## Audit summary (current bridge surface)

- **`wxWindow`** — geometry/colour as value-type properties; `font`
  property (`wx_font` value type, get/set); show / hide / enable /
  focus / layout / centre / centre-on-parent / fit / refresh /
  refresh-rect / scroll / raise / lower / freeze / thaw / capture /
  drag-accept / dpi-scale / parent / grandparent / sizer / window-style
  / extra-style / tooltip / name / help-text / text-extent /
  client-screen conversions / reparent / move-{after,before}-in-tab-order
  / is-{top-level,being-deleted,double-buffered,modal,frozen} /
  set-double-buffered / accepts-focus / accepts-focus-from-keyboard /
  can-accept-focus / set-can-focus / warp-pointer /
  effective-min-size / register-hot-key / unregister-hot-key.
  `register_hot_key` / `unregister_hot_key` only do real work on MSW
  (the wx-side base implementation returns false on every other port);
  the bridge exposes them on every platform so script code stays
  portable.
  Not exposed yet: hit-testing, child-window enumeration, accelerator
  tables, validators, layout direction, transparency.
- **`wxTopLevelWindow`** — title / full-screen / maximize / iconize /
  centre-on-screen / enable-{close,maximize,minimize}-button / request
  user attention.
- **`wxControl`** — label / label-text / label-markup as properties,
  plus `command(wx_command_event@)` to fire programmatic events.
- **`wxTextEntry` mix-in** (consumed by `wx_text_entry` and
  `wx_text_control`) — value / change-value / write / append / clear /
  copy / cut / paste / remove / undo / redo / can-* / range / selection
  / editable / empty / replace / select-all / select-none / max-length
  / insertion point / last position / force-upper / hint / autocomplete
  (file / dir).
- **`wxSizer`** — add / insert / prepend (for windows, sizers,
  spacers, stretch-spacers); detach / remove / replace; show / hide /
  is-shown / show-all / show-items / are-any-items-shown / clear /
  delete-windows; layout / fit / fit-inside / set-size-hints /
  compute-fitting-{client,window}-size; geometry properties; min-size
  / set-item-min-size; item-count / find / get-item / is-empty.
- **`wxSizerItem`** — window / sizer / proportion / flag / border;
  size / calc-min / min-size{,-with-border} / max-size{,-with-border}
  / rect / position / spacer; set-init-size; set-ratio (size or
  float); is-{window,sizer,spacer,shown}; show; detach / delete.
- **`wxBoxSizer`** — `wxSizer` surface plus the box-specific
  `orientation` property (`get_orientation` / `set_orientation`) and
  `is_vertical()` predicate.
- **`wxGridSizer`** — `wxSizer` surface plus grid-specific properties
  (`cols`, `rows`, `vgap`, `hgap`) and the read-only effective-count
  properties (`effective_cols_count`, `effective_rows_count`) plus
  `calc_rows_cols(int &out, int &out)`.
- **`wxFlexGridSizer`** — `wxGridSizer` surface plus
  `add_growable_row` / `add_growable_col` (proportion defaulting to 0),
  `remove_growable_row` / `remove_growable_col`, `is_row_growable` /
  `is_col_growable`, `flexible_direction` property (`wxOrientation`)
  and `non_flexible_grow_mode` property (`wx_flex_sizer_grow_mode`).
- **`wxGridBagSizer`** — `wxFlexGridSizer` surface plus the
  position/span-aware `add` overloads (window / sizer / spacer),
  `get_item_position` / `set_item_position` (window / sizer / index
  variants), `get_item_span` / `set_item_span` (variants), the
  `empty_cell_size` property, `get_cell_size(row, col)`, the
  `find_item` (window / sizer / position / point) family and
  `check_for_intersection`. Scripts must use the position/span `add`
  on grid_bag — the proportion-based `add` inherited from `wxSizer`
  asserts inside wxWidgets because grid-bag insertion requires a
  `wxGBSizerItem`.
- **`wxWrapSizer`** — `wxBoxSizer` surface only. Wrap behaviour is
  controlled at construction time through `wx_wrap_sizer_flag`
  (`WX_EXTEND_LAST_ON_EACH_LINE` / `WX_REMOVE_LEADING_SPACES` /
  `WX_WRAPSIZER_DEFAULT_FLAGS`).
- **`wxStaticBox`** — registered as a `wx_control` so it can be passed
  to `wx_static_box_sizer` constructors and so events on it are
  routable through the bridge. wxStaticBox itself has no per-class
  methods; the empty `wx_static_box_style` enum is registered for
  symmetry with the other per-control style enums.
- **`wxStaticBoxSizer`** — `wxBoxSizer` surface plus the
  read-only `static_box` property (returns `wx_static_box@`,
  `EnsureTracked`-attached so the destroy hook is wired the first time
  the bridge sees the box).
- **Concrete widgets** — `wx_frame`, `wx_panel` (plus
  `set_focus_ignoring_children`), `wx_button` (plus `set_default`,
  `auth_needed` property — the auth-needed UAC shield is MSW-only,
  the wx-side accessors no-op elsewhere), `wx_check_box` (`get_value`
  / `set_value` / `Get3StateValue` / `Set3StateValue` /
  `is_3rd_state_allowed_for_user` / `is_3state` / `is_checked`),
  `wx_static_text` (`wrap`), `wx_text_control` (the full `wxTextEntry`
  surface plus the wxTextCtrl extras: `is_modified`, `mark_dirty`,
  `discard_edits`, `set_modified(bool)`, `number_of_lines` property,
  `get_line_length(int)`, `get_line_text(int)`, `xy_to_position(int,
  int)`, `position_to_xy(int, int &out, int &out)`, `show_position(int)`,
  `is_multi_line`, `is_single_line`, `load_file` / `save_file` with
  `wx_text_file_type` selector, `emulate_key_press(wx_key_event@)`),
  `wx_radio_button` (`get_value` / `set_value` /
  first/last/next/previous-in-group).
- **`wxItemContainer` mix-in** (consumed by `wx_choice`, `wx_combo_box`,
  `wx_list_box`) — read surface: `count` / `is_empty` / `get_string` /
  `set_string` / `find_string` / `selection` / `select` /
  `string_selection` / `set_string_selection` / `get_strings` (returns
  `array<string>@`) / `is_sorted`. Mutate surface: `append(string)` /
  `append(array<string>@)` / `insert(string, int)` /
  `insert(array<string>@, int)` / `set(array<string>@)` / `clear` /
  `delete_item(int)`. Same trick as `wx_text_entry`: every wrapper
  takes a `wxWindow*` and `dynamic_cast`s internally to
  `wxItemContainer{,Immutable}` (see `helpers.cpp/GetItemContainer`).
  `wx_combo_box` only inherits the non-conflicting subset because
  `wxComboBox` is also a `wxTextEntry`; the colliding three names
  (`clear` / `is_empty` / `get_string_selection`) take their text-
  entry semantics there, and the item-list semantics are exposed
  under the disambiguated names `clear_items` / `is_list_empty` /
  `get_item_string_selection` (mirrors `wxComboBoxBase::IsListEmpty`).
  wxRadioBox derives only from `wxItemContainerImmutable` and is
  *not* assignable to `wx_item_container@`; the read-only half of the
  surface is duplicated directly on `wx_radio_box`.
- **`wx_choice`** — `wx_item_container` surface plus
  `get_current_selection`, `columns` property (`get_columns` /
  `set_columns`).
- **`wx_combo_box`** — `wx_item_container` (BASE subset) and
  `wx_text_entry` surfaces, plus `get_current_selection`, `popup`,
  `dismiss`. ClientData (`SetClientData` / `GetClientData`) is not
  exposed because the bridge has no mechanism for smuggling AS
  handles through `wxObject`.
- **`wx_list_box`** — `wx_item_container` surface plus the multi-
  selection accessors (`is_selected`, `set_selection_multi(int, bool)`
  exposed under a distinct name to avoid clashing with the single-
  selection `set_selection(int)` property, `deselect`, `deselect_all`,
  `get_selections() → array<int>@`), the scroll/visibility helpers
  (`set_first_item(int)` / `set_first_item(string)` / `ensure_visible`
  / `top_item` / `count_per_page` / `append_and_ensure_visible`), and
  the predicates `has_multiple_selection` / `hit_test(wx_point)`.
- **`wx_radio_box`** — duplicated read-only `wxItemContainer` surface
  (`count` / `is_empty` / `get_string` / `set_string` / `find_string`
  / `selection` / `string_selection` / `set_string_selection`) plus
  the radiobox-specific per-item state accessors (`enable_item` /
  `show_item` / `is_item_enabled` / `is_item_shown`), layout
  accessors (`column_count` / `row_count`), per-item help text
  (`get_item_help_text` / `set_item_help_text`), per-item tooltip
  (`get_item_tool_tip` / `set_item_tool_tip` — return / accept plain
  strings; the underlying `wxToolTip*` is unwrapped, empty string
  signals "no tooltip configured" or "invalid index"), and the
  navigation helpers `get_item_from_point(wx_point)` /
  `get_next_item(int, wx_direction, int)`. No mutators (Append /
  Insert / Clear / Delete) — wxRadioBox freezes its set of buttons
  at construction.
- **Events** — `wx_event` (skip, type, event-object), `wx_key_event`
  (key code, unicode, modifier helpers), `wx_mouse_event` (xy,
  buttons, dclick), `wx_command_event` (string / int / extra-long
  / selection / is-checked / is-selection). Selector event types
  registered: `WX_EVT_CHOICE`, `WX_EVT_COMBOBOX`,
  `WX_EVT_COMBOBOX_DROPDOWN`, `WX_EVT_COMBOBOX_CLOSEUP`,
  `WX_EVT_LISTBOX`, `WX_EVT_LISTBOX_DCLICK`, `WX_EVT_RADIOBOX`. The
  combo-box also fires the existing `WX_EVT_TEXT` /
  `WX_EVT_TEXT_ENTER` from the text-entry side. Window-level event
  types: `WX_EVT_MOVE`, `WX_EVT_SHOW`, `WX_EVT_DPI_CHANGED`,
  `WX_EVT_DROP_FILES`, `WX_EVT_HOTKEY`, `WX_EVT_END_SESSION`,
  `WX_EVT_QUERY_END_SESSION` (the session-end pair fires only on the
  application's top window during OS shutdown, the move/show pair
  fires on every window that is repositioned or whose visibility
  toggles, drop-files fires when `drag_accept_files(true)` was called
  earlier on the same window, and hotkey is the MSW-only delivery
  side of `register_hot_key`).
- **`wxFont`** — exposed as the `wx_font` value type. Default + full
  ctor (size / family / style / weight / underlined / face_name) +
  copy ctor + `opAssign`. Properties: `point_size`, `pixel_size`,
  `face_name`, `family` / `style` / `weight` (int), `underlined`,
  `strikethrough`. Methods: `is_ok`, `is_fixed_width`, `bold`,
  `italic`, `larger`, `smaller`, `scaled`, `get_base_font`. Not
  exposed yet: native-font-info round-trip (`GetNativeFontInfo` /
  `wxFromString`), encoding, fractional point sizes, font-info
  builders.
- **Manager** — `wx::update`, `wx::create_{frame,button,check_box,box_sizer,
  grid_sizer,flex_grid_sizer,grid_bag_sizer,wrap_sizer,static_box,
  static_box_sizer,static_box_sizer_for_box,
  panel,static_text,text_control,radio_button,
  choice,combo_box,list_box,radio_box}`. `create_frame` takes a
  `wx_size` for the initial window size; the four selector factories
  take an optional `array<string>@ items` (defaulting to `null`) for
  the initial item list, except `create_radio_box` which requires
  the items array (an empty radio box is rejected with `nullptr`
  because wxRadioBox asserts on it). `create_radio_box` defaults
  `style` to `WX_RA_SPECIFY_COLS` to match wxWidgets' own default.

This list is the source of truth for "what works"; if you remove or
rename something here, update this section in the same PR.

## Roadmap (high-level)

- **Phase A (done) — value types and per-control style enums.**
  `wx_point/size/rect/colour` are registered as AS value types and
  every geometry / colour accessor uses them as properties; the
  monolithic `wx_style` enum is split into one enum per control
  family. Breaking change vs. earlier PRs.
- **Phase 0 (done) — remaining foundation work.** `wx_font` value
  type with full property + modifier surface, `wx_window.font`
  property, `wx_font_family` / `wx_font_style` / `wx_font_weight`
  enums, `wxEntryCleanup()` paired with `wxEntryStart()` through a
  static `WxLifecycleGuard` (the function-level destructor that
  fires on `DLL_PROCESS_DETACH`).
- **Phase 1a (this PR) — remaining sizers.** `wx_grid_sizer`,
  `wx_flex_grid_sizer`, `wx_grid_bag_sizer`, `wx_wrap_sizer`,
  `wx_static_box_sizer` (and `wx_static_box` as a control); two new
  POD value types `wx_gb_position` / `wx_gb_span` for grid-bag cell
  addressing; per-sizer style/flag enums (`wx_flex_sizer_grow_mode`,
  `wx_wrap_sizer_flag`, empty `wx_static_box_style`); the box-sizer
  orientation accessor (`get_orientation` / `set_orientation` /
  `is_vertical`) is now exposed on every `wxBoxSizer`-derived bridge
  type.
- **Phase 1b (this PR) — selectors.** `wx_choice`, `wx_combo_box`,
  `wx_list_box`, `wx_radio_box`. New `wx_item_container` mix-in
  type (modelled on `wx_text_entry`); per-control style enums
  (`wx_choice_style`, `wx_combo_box_style`, `wx_list_box_style`,
  `wx_radio_box_style`); selector event types
  (`WX_EVT_CHOICE`, `WX_EVT_COMBOBOX*`, `WX_EVT_LISTBOX*`,
  `WX_EVT_RADIOBOX`); factory methods on `wx`. Wired the bridge to
  the AS `scriptarray` add-on (consumed from NVGT's `ASAddon/`, not
  vendored) so it can take/return `array<string>@` for batch
  operations and `array<int>@` for `wx_list_box.get_selections`.
- **Phase 1 — basic GUI (in progress):** dialogs
  (`wx_dialog`, `wx_message_dialog`, `wx_file_dialog`, …); range
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
